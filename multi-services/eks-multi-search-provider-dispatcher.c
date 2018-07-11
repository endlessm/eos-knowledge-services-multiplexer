/* Copyright 2018 Endless Mobile, Inc.
 *
 * eos-multi-services-dispatcher is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * eos-multi-services-dispatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with eos-companion-app-service.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <glib.h>

#include <stdlib.h>
#include <string.h>

static char *opt_arch;
static char *opt_services_version;

static GOptionEntry entries[] = {
  { "arch", 's', 0, G_OPTION_ARG_STRING, &opt_arch, "The arch to use when searching library paths with an arch-triple", "VERSION" },
  { "services-version", 's', 0, G_OPTION_ARG_STRING, &opt_services_version, "The eks-search-provider version to use", "VERSION" },
  { NULL }
};

static GSubprocess *
spawnv_with_appended_paths_and_fds (const char * const  *argv,
                                    const char * const  *executable_paths,
                                    const char * const  *ld_library_paths,
                                    const char * const  *xdg_data_dirs,
                                    GError             **error)
{
  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_INHERIT_FDS);
  g_autofree char *executable_path_variable = g_strjoinv (":", (GStrv) executable_paths);
  g_autofree char *ld_library_path_variable = g_strjoinv (":", (GStrv) ld_library_paths);
  g_autofree char *xdg_data_dirs_variable = g_strjoinv (":", (GStrv) xdg_data_dirs);

  g_subprocess_launcher_setenv (launcher, "PATH", executable_path_variable, TRUE);
  g_subprocess_launcher_setenv (launcher, "LD_LIBRARY_PATH", ld_library_path_variable, TRUE);
  g_subprocess_launcher_setenv (launcher, "XDG_DATA_DIRS", xdg_data_dirs_variable, TRUE);

  return g_subprocess_launcher_spawnv (launcher, argv, error); 
}

/* Try and find an installed and mounted SDK with the highest priority,
 * the first item having the highest priority. */
static const char *
find_sdk_with_highest_priority (const char * const  *sdks_in_priority_order,
                                const char          *services_version,
                                GError             **error)
{
  const char * const *iter = sdks_in_priority_order;

  for (; *iter != NULL; ++iter)
    {
      g_autoptr(GError) local_error = NULL;
      g_autoptr(GFile) sdk_dir = g_file_new_for_path (*iter);
      g_autoptr(GFileEnumerator) sdk_enumerator = g_file_enumerate_children (sdk_dir,
                                                                             G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                                             G_FILE_QUERY_INFO_NONE,
                                                                             NULL,
                                                                             error);

      if (sdk_enumerator == NULL)
        return NULL;

      g_autoptr(GFileInfo) first_file_info = g_file_enumerator_next_file (sdk_enumerator,
                                                                          NULL,
                                                                          &local_error);

      if (local_error != NULL)
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
          return NULL;
        }

      if (first_file_info != NULL)
        return *iter;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_FAILED,
               "Could not find candidate SDK for services version %s",
               services_version);
  return NULL;
}

static void
create_paths_for_prefixes (const char *binary,
                           const char *sdk_prefix,
                           const char *services_prefix,
                           const char *arch,
                           GStrv      *out_argv,
                           GStrv      *out_executable_paths,
                           GStrv      *out_ld_library_paths,
                           GStrv      *out_xdg_data_dirs)
{
  g_autoptr(GPtrArray) argv = g_ptr_array_new_with_free_func (g_free);
  g_autoptr(GPtrArray) executable_paths = g_ptr_array_new_with_free_func (g_free);
  g_autoptr(GPtrArray) ld_library_paths = g_ptr_array_new_with_free_func (g_free);
  g_autoptr(GPtrArray) xdg_data_dirs = g_ptr_array_new_with_free_func (g_free);

  g_ptr_array_add (argv, g_build_filename (services_prefix, "bin", binary, NULL));
  g_ptr_array_add (argv, NULL);

  g_ptr_array_add (executable_paths, g_build_filename (services_prefix, "bin", NULL));
  g_ptr_array_add (executable_paths, g_build_filename (sdk_prefix, "bin", NULL));
  g_ptr_array_add (executable_paths, NULL);

  g_ptr_array_add (ld_library_paths, g_build_filename (services_prefix, "lib", NULL));
  g_ptr_array_add (ld_library_paths, g_build_filename (sdk_prefix, "lib", NULL));

  /* If we specified an arch, also add an arch-triple library path */
  if (arch != NULL)
    {
      g_autofree char *arch_triple = g_strdup_printf ("%s-linux-gnu", arch);
      g_ptr_array_add (ld_library_paths,
                       g_build_filename (sdk_prefix, "lib", arch_triple, NULL));
    }

  g_ptr_array_add (ld_library_paths, NULL);

  g_ptr_array_add (xdg_data_dirs, g_build_filename (services_prefix, "share", NULL));
  g_ptr_array_add (xdg_data_dirs, g_build_filename (sdk_prefix, "share", NULL));
  g_ptr_array_add (xdg_data_dirs, NULL);

  *out_argv = (GStrv) g_ptr_array_free (g_steal_pointer (&argv), FALSE);
  *out_executable_paths = (GStrv) g_ptr_array_free (g_steal_pointer (&executable_paths), FALSE);
  *out_ld_library_paths = (GStrv) g_ptr_array_free (g_steal_pointer (&ld_library_paths), FALSE);
  *out_xdg_data_dirs = (GStrv) g_ptr_array_free (g_steal_pointer (&xdg_data_dirs), FALSE);
}

static gboolean
dispatch_correct_service (const char   *services_version,
                          const char   *arch,
                          GError      **error)
{
  /* We keep track of this and then free it, the exec'd service
   * will become a child of init and inherit all our fds, thus
   * consuming the d-bus traffic that was destined for this process. */
  g_autoptr(GSubprocess) subprocess = NULL;

  g_auto(GStrv) argv = NULL;
  g_auto(GStrv) executable_paths = NULL;
  g_auto(GStrv) ld_library_paths = NULL;
  g_auto(GStrv) xdg_data_dirs = NULL;

  if (g_strcmp0 (services_version, "1") == 0)
    {
      const char * const candidate_sdks[] = {
        "/app/sdk/1",
        "/app/sdk/0",
        NULL
      };
      const char *sdk_path = find_sdk_with_highest_priority (candidate_sdks,
                                                             services_version,
                                                             error);

      if (sdk_path == NULL)
        return FALSE;

      create_paths_for_prefixes ("eks-search-provider-v1",
                                 sdk_path,
                                 "/app/eos-knowledge-services/1",
                                 arch,
                                 &argv,
                                 &executable_paths,
                                 &ld_library_paths,
                                 &xdg_data_dirs);
    }
  else if (g_strcmp0 (services_version, "2") == 0)
    {
      const char * const candidate_sdks[] = {
        "/app/sdk/3",
        "/app/sdk/2",
        NULL
      };
      const char *sdk_path = find_sdk_with_highest_priority (candidate_sdks,
                                                             services_version,
                                                             error);

      if (sdk_path == NULL)
        return FALSE;

      create_paths_for_prefixes ("eks-search-provider-v2",
                                 sdk_path,
                                 "/app/eos-knowledge-services/2",
                                 arch,
                                 &argv,
                                 &executable_paths,
                                 &ld_library_paths,
                                 &xdg_data_dirs);
    }
  else if (g_strcmp0 (services_version, "3") == 0)
    {
      const char * const candidate_sdks[] = {
        "/app/sdk/5",
        "/app/sdk/4",
        NULL
      };
      const char *sdk_path = find_sdk_with_highest_priority (candidate_sdks,
                                                             services_version,
                                                             error);

      if (sdk_path == NULL)
        return FALSE;

      create_paths_for_prefixes ("eks-search-provider-v3",
                                 sdk_path,
                                 "/app/eos-knowledge-services/3",
                                 arch,
                                 &argv,
                                 &executable_paths,
                                 &ld_library_paths,
                                 &xdg_data_dirs);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Don't know how to spawn services version %s",
                   services_version);
      return FALSE;
    }

  subprocess = spawnv_with_appended_paths_and_fds ((const char * const *) argv,
                                                   (const char * const *) executable_paths,
                                                   (const char * const *) ld_library_paths,
                                                   (const char * const *) xdg_data_dirs,
                                                   error);
  return subprocess != NULL;
}

int
main (int    argc,
      char **argv)
{
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GOptionContext) context = g_option_context_new ("- multiplexer for EknServices");

  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &local_error))
    {
      g_message ("Option parsing failed: %s\n", local_error->message);
      return EXIT_FAILURE;
    }

  if (!dispatch_correct_service (opt_services_version,
                                 opt_arch,
                                 &local_error))
    {
      g_message ("Failed to dispatch correct service for version %s: %s",
                 opt_services_version,
                 local_error->message);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
