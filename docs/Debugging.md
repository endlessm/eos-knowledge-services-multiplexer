# Debugging Guide for EOS Knowledge Services Multiplexer
This is a debugging guide for the `EknServicesMultiplexer` flatpak.
The flatpak itself is built in a special way and this leads itself
to special considerations when debugging it.

## No Debug Symbols
The `EknServicesMultiplexer` flatpak is actually a composition
of the N separately versioned `eks-search-provider` binaries
with their corresponding runtimes mounted as extensions at
`/app/sdk/N`. A single binary, `eks-multi-search-provider-dispatcher`
sets `LD_LIBRARY_PATH` to the correct SDK and then runs the
corresponding `eks-search-provider` binary when activated.

Because the `eks-search-provider` binaries are copied in,
there isn't any way to get their debug symbols and so trying
to debug them directly is not going to be very useful.

### Installing `EknServices` independently
Since the "independent" `com.endlessm.EknServicesN` flatpaks
use the exact same binaries as the ones copied into the
multiplexer, its probable that the bug is reproducible
on one of those flatpaks. Those flatpaks *do* have a
debuginfo extension, which makes debugging a lot easier.

To install the "independent" flatpak along with its
SDK, use something like:

    $ flatpak install eos-sdk com.endlessm.apps.Sdk//3
    $ flatpak install eos-sdk com.endlessm.apps.Sdk.Debug//3
    $ flatpak install eos-sdk com.endlessm.EknServices3

The "independent" flatpak will "supercede" `EknServicesMultiplexer`
in that its exports will overwrite whatever `EknServicesMultiplexer`
had in place, meaning that future activations of the
`com.endlessm.EknServices3` name would spawn the
`com.endlessm.EknServices3` flatpak.

You can also debug the `EknServices3` flatpak by running it
with the `--devel` flag:

    $ flatpak run --devel --command=/bin/bash com.endlessm.EknServices3
    $ gdb eks-search-provider-v3
    (gdb) r

### Building `EknServices` independently
If you need to build and make changes to `EknServices`, use the
`build-flatpak.sh` script included in the `eos-knowledge-services`
repository. That will build both the "independent"
`com.endlessm.EknServicesN` and `com.endlessm.EknServicesN.Extension`
flatpaks. The latter is used when `EknServicesMultiplexer` is
rebuilt, the former can be installed as an "independent" version
of `EknServices` and debugged as such.

### Bug only reproducible on `EknServicesMultiplexer`
On some occassions you might be unlucky enough to only reproduce
the bug when running with `EknServicesMultiplexer`, where
debug symbols are not availble. Its worth noting that bugs
reproducible under this circumstance are almost always
latent memory corruption issues which are present, but not
realized in the corresponding "independent" `EknServices`
flatpak.

Unfortunately, you will need to resort to `printf`-style debugging
to try and work out what is happening. Also refer to the
below list of common memory related pitfalls to try and find
the bug by code inspection.

## Common Pitfalls

### Incorrect `LD_LIBRARY_PATH`
When adding a new `EknServices` version its quite likely
that you copy-pasted a new branch path into
`eks-multi-search-provider-dispatcher`. Make sure that the
configured `LD_LIBRARY_PATH` points to the SDK version that
the relevant binary was built against, or you'll encounter
ABI mismatch issues.

### No terminating `NULL` sentinel
Some GLib functions use varargs to take an indeterminate
number of arguments, looped through at runtime. By convention,
`NULL` needs to be passed as the last argument so that
the function knows when to stop processing arguments. If
the caller does not pass `NULL`, uninitialized memory will
be read and the behavior will not be defined. Watch
out especially for `g_object_new` and `g_build_filename`.

### `GVariant` misuse
Since `EknServices` runs as a D-Bus service, it uses
`GVariant` heavily in order to serialize messages into
a wire format. Unfortunately, the `GVariant` API has
a reputation for being easy to use incorrectly, making
undefined behaviour due to use-after-free errors
far more common.

#### Floating vs Non-Floating References
Be aware that most `GVariant` functions that create
a new `GVariant` will return a `GVariant` with a
[floating reference](https://developer.gnome.org/glib/stable/glib-GVariant.html#g-variant-ref-sink).

"Floating references" are like a one-time `transfer full`.
If a caller expects to be the first one to take ownership
of a `GVariant` then it should always call `g_variant_ref_sink`
on it. The effect will either be to remove the floating reference
and keep the reference count as-is, or to increment the reference
count if there is no floating reference. This is especially
important when combining `GVariant` with `g_autoptr` - if
you assign a newly created `GVariant` to a `g_autoptr` you
should almost always sink the floating reference:

    g_autoptr(GVariant) v = g_variant_ref_sink (g_variant_new_string ("string"));

Consider what happens if the floating reference is not sunk
and the variant is passed to a function that sinks the floating
reference:

    g_autoptr(GVariant) response = g_variant_new_string ("string") // rc: 1

    ...
    // Floating reference sunk, reference count does not increase
    // and response is unreffed after returning.
    g_dbus_method_invocation_return_value (invocation, response) // rc: 0

    // g_autoptr attempts to unref the variant again,
    // causing a use-after-free

#### Misuse of `GVariantBuilder`
`GVariantBuilder` is usually used on the stack, meaning that its
contents are uninitialized until initialized with
`g_variant_builder_init`.

If you are using `g_auto` with `GVariantBuilder` then you must
ensure that `g_variant_builder_init` is called before the function
returns, or that it is initialized with
[`G_VARIANT_BUILDER_INIT`](https://developer.gnome.org/glib/stable/glib-GVariant.html#G-VARIANT-BUILDER-INIT:CAPS).

If you don't do this, then the `g_auto` cleanup func for `GVariantBuilder`
may call `g_variant_builder_clear`, which will attempt to free uninitialized
memory.
