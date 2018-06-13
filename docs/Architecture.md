# Introduction
This document is the technical reference for how EknServices will be used in
the [Discovery Feed](https://github.com/endlessm/eos-discovery-feed) and the
[Companion App Service](https://github.com/endlessm/eos-companion-app-integration)
going forward. It describes the API that will be exposed and how we will achieve
better compatibility with multiple SDK versions going forward.

# Purpose and Problem Definition
Previously, the Companion App Service linked to the Platform SDK and
ekncontent directly. This was not a great situation to be in, since it means
that we had a hard-dependency on apps from a single SDK version. It is against
the SDK’s design to try and support apps from multiple SDK versions
concurrently - the database schema and ekncontent API/ABI is liable to change
between SDK versions.

The Companion App Service thus needs to make use of the standard mechanism
used to achieve multi-SDK compatibility, which is EknServices. That imports
along with it several other concerns.

First, the way EknServices is currently used, which is to provide a narrow
consumer-specific API on top of the existing query interface, has proven to be
a poor developer experience. In the case of the Discovery Feed, code was often
duplicated within EknServices itself to support slightly varying queries
because the external API provided was not flexible enough. This led to the
kind of problems that code duplication tends to cause - inconsistency in what
should be shared implementation and a "write-twice, debug-in-two-places"
problem in development. It would be much nicer if EknServices exposed a query
API that was stable across SDK versions but still flexible enough to prevent
the need to constantly make changes to EknServices to implement features in
the Discovery Feed or Companion App Service.

Second, EknServices runs as a session-level service, but due to the fact that
it listens on a system-global socket, Companion App Service runs as a
system-level service. Thus, without a bit of extra work, the two aren’t
compatible. This can be solved fairly easily, but it’s worth noting.

Third, there were concerns about the way that EknServices has been bundled in
the past. Apps that depend on a given runtime also depend on EknServices for
that runtime in order to provide the shell search and discovery feed
functionality. Previously we reimplemented dependency tracking in the OS
[itself](https://github.com/endlessm/gnome-software/blob/eos3.3/plugins/eos/gs-plugin-eos.c#L1455)
in order to ensure that the right EknServices is installed whenever an app
brings in a given SDK. If an app tried to pull in a newer SDK version than
GNOME-Software (bundled with the OS) knows about at the time, the user could end
up without the corresponding EknServices installed. Also, flatpak had no idea
about this dependency, so if the user installed an App or SDK manually through
flatpak they end up without the corresponding EknServices installed.

Fourth, we had to bundle every SDK and corresponding EknServices for every
app we include on an image. If all the apps get uninstalled, we should be able
to uninstall the corresponding SDK automatically, but we couldn’t do that
because the old EknServices is still sticking around.

This document attempts to comprehensively address all the issues and the
proposed solution set.

# Assumptions
1. EknServices is not going away anytime soon.
2. We’re going to keep shipping new SDK releases.
3. Once an API lands in EknServices, we don’t make backward-incompatible changes
   to it unless we give the API a new name.
4. Consumers of EknServices will always have a session bus available to them.
5. Consumers of EknServices might be bundled with the OS or might be flatpaks.

# Requirements

## Don’t break the existing apps, Discovery Feed, Shell Search Providers or Companion App Service
Whatever happens, we shouldn’t break these things with an update. To the
extent that they might rely on an old API, any new implementation here should
provide the old API and satisfy all the old API’s implicit contracts.

## Companion App Service not dependent on apps from a particular SDK version
Right now, the Companion App Service is linked directly to a specific SDK
version. This is highly undesirable for the reasons stated above. Any new
implementation should not require that the Companion App Service has a
dependency on apps from a specific SDK version.

Of course, this doesn’t prevent us from having the Companion App Service
flatpak linked to a particular SDK version, but it should still work with all
apps regardless of what version it is linked to.

## Discovery Feed not dependent on a particular SDK version
As above, the Discovery Feed should not gain a dependency on a particular SDK
version. It doesn’t have that right now, but any new solution shouldn’t import
such a dependency.

## Less Development Churn
Any new solution should optimize in favor of reducing developer churn as
opposed to providing a narrow API for a particular use case. Ideally,
implementing new features in either the
Discovery Feed or Companion App Service should not require subsequent changes in
EknServices. Thus, the exposed API needs to be flexible enough to deal with
anticipated future needs.

## No version-dependent dependency tracking in the OS
Any new solution should obviate the need to keep doing dependency tracking of
EknServices versions for different SDK versions, but we should still have
EknServices functionality for each installed SDK.

## Don’t waste disk space
Any new solution should also not require every SDK version to be installed
regardless of whether corresponding apps are installed.


# Multiple EknServices in one Flatpak (MultiEknServices)
In order to satisfy requirements
[No Dependency Tracking in the OS](#no-dependency-tracking-in-the-os)
and [Don’t Waste Disk Space](#don't-waste-disk-space) we will bundle all of the
EknServices into a single flatpak. The way this works is that each EknServices
is copied into in a separate prefix in a "EknServicesMultiplexer" flatpak.
SDKs are mounted as extensions. If an SDK isn’t available, then it won’t be
mounted.

The EknServicesMultiplexer flatpak itself exports all EknServices bus names
(e.g., com.endlessm.EknServices, com.endlessm.EknServices2) which each invoke
a helper binary, eks-multi-services-dispatcher with a --services-version
argument that looks up the best SDK version for the requested EknServices
version, sets LD_LIBRARY_PATH etc and then exec()’s (without O_CLOEXEC) the
relevant eks-search-provider binary which actually calls
g_dbus_register_subtree. It is a caller error if the caller invokes an
EknServices version that cannot be loaded, since the caller is supposed to
work out which EknServices to launch based on introspecting the relevant app
(e.g., through a Shell Search Provider or a Discovery Feed Content Provider).
It is assumed that if an app was installed, then the correct SDK version is
also installed for that app, thus the relevant version of EknServices would
also work for that app.

The "best" SDK version in this case is the highest installed SDK version which
supports the requested EknServices version.

As an example, the filesystem layout for com.endlessm.EknServicesMultiplexer
would look as follows

    /app/sdk/1/
    /app/sdk/2/
    /app/sdk/3/
    /app/sdk/4/
    /app/share/dbus-1/com.endlessm.EknServices.SearchProviderV1.service (Exec=eks-multi-services-dispatcher --services-version 1)
    /app/share/dbus-1/com.endlessm.EknServices2.SearchProviderV2.service (Exec=eks-multi-services-dispatcher --services-version 3)
    /app/eos-knowledge-services/1 (copy of /app tree for com.endlessm.EknServices.Extension, without debug symbols)
    /app/eos-knowledge-services/2 (copy of /app tree for com.endlessm.EknServices2.Extension, without debug symbols)
    /app/eos-knowledge-services/3 (copy of /app tree for com.endlessm.EknServices3.Extension, without debug symbols)
    /app/lib/debug/eos-knowledge-services/1 (copy of /app/lib/debug for com.endlessm.EknServices.Extension)
    /app/lib/debug/eos-knowledge-services/2 (copy of /app/lib/debug for com.endlessm.EknServices2.Extension)
    /app/lib/debug/eos-knowledge-services/3 (copy of /app/lib/debug for com.endlessm.EknServices3.Extension)

There are three notable implementation details here.

### EknServices becomes an extension
Only runtimes can be extensions in flatpak. EknServices is bundled as an app,
so this presents an obvious problem. The solution is to build an extension,
e.g., com.endlessm.EknServices.Extension when building each EknServices which
contains the entire /app tree, exported to an extension point like
/app/build/eos-knowledge-services/1. The extension can then be mounted in
another flatpak.

### BaseEknServicesMultiplexer
Declared extensions for a flatpak are not mounted at build time, except if the
extension was for a "parent" flatpak specified with “base” and the extension
was requested to be mounted with “base-extensions”.

Because of this, we need to have a BaseEknServicesMultiplexer flatpak that
**only** declares the extension points and the extensions that can satisfy
them. EknServicesMultiplexer then extends BaseExtenServicesMultiplexer and
uses "base-extensions" to ensure those extensions are available at build time.

### Extension contents are copied into EknServicesMultiplexer
Instead of each EknServices extension being mounted at runtime, the contents
themselves are copied into EknServicesMultiplexer. This way, the user only has
to continue to upgrade EknServicesMultiplexer in order to get support for
newer EknServices versions. There will be no need for a separate part of the
OS to ensure that the corresponding EknServices flatpak is installed.

## All old EknServices versions conflict with EknServicesMultiplexer
Because MultiEknServices exports the same D-Bus services that the old
EknServices flatpaks do, the two are in conflict. Along with an OS update that
adds com.endlessm.MultiEknServices to "Endless Platform", we should also ship
entries in the flatpak autoinstaller to remove the old EknServices flatpaks
and install MultiEknServices.

The OS image should also be built to include EknServicesMultiplexer.

## Jenkins flow for EknServices and EknServicesMultiplexer
Since the contents of each EknServices version is copied into
EknServicesMultiplexer, the normal Jenkins flow will not be sufficient to
ensure that a change to eos-knowledge-services will be made available to users
through a flatpak update. EknServicesMutliplexer must also be rebuilt, but
only once the corresponding EknServices.Extension flatpak has been built,
deployed and uploaded to the OSTree repo.

Thus a custom Jenkins flow will be required to ensure that these steps take
place whenever an EknServices flatpak is rebuilt.
