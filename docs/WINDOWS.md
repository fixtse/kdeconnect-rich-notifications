# Windows port (experimental)

The Windows implementation uses Microsoft's `UserNotificationListener` API.
It sends new toast notifications, updated text, application logos and removal
packets over the existing paired KDE Connect connection. IDs are strings, and
asynchronous logo reads are discarded when a newer update/removal supersedes
them. Existing Action Center history is not replayed when enabling the plugin.

## Platform requirements

- Windows 10 version 2004 or later, or Windows 11.
- A **packaged Qt 6 / KF6 KDE Connect build** with the
  `userNotificationListener` capability in its package manifest.
- User permission to read notifications, requested by the plugin on first use.
- A matching MSVC/architecture Qt/KDE SDK, Windows SDK with C++/WinRT headers,
  CMake 3.22+, and matching KDE Connect source headers and core import library.

This is a source port for compatible KDE Connect package builders. It is not a
DLL that can be dropped into every existing Windows installation. An unpackaged
host cannot request the required permission; the plugin reports that constraint
in the KDE Connect log. A plugin DLL cannot add capabilities to its host package.
Do not modify a signed Store installation under WindowsApps.

In the **host package's** manifest, declare the uap3 namespace
as required by your packaging toolchain, then include the notification-listener
capability (the namespace is `http://schemas.microsoft.com/appx/manifest/uap/windows10/3`):

```xml
<Package xmlns:uap3="http://schemas.microsoft.com/appx/manifest/uap/windows10/3"
         IgnorableNamespaces="uap3">
  <!-- Existing package metadata, applications and capabilities remain here. -->
  <Capabilities>
    <uap3:Capability Name="userNotificationListener" />
  </Capabilities>
</Package>
```

Merge this declaration into the existing manifest; this fragment is not a
complete installable manifest. Rebuild, sign and install your own package using
its normal packaging process. All KDE Connect child processes that load the
plugin must retain the package identity. See Microsoft's
[notification listener documentation](https://learn.microsoft.com/en-us/windows/apps/develop/notifications/app-notifications/notification-listener).

## Build and add to your package

Use the same Craft/MSVC developer environment that built your KDE Connect host.
Build Qt 6/KF6 dependencies and KDE Connect first; preserve their development
headers and import libraries. This repository does not provide those SDKs.

```powershell
git clone https://github.com/fixtse/kdeconnect-rich-notifications.git
cd kdeconnect-rich-notifications
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/CraftRoot `
  -DKDECONNECT_SOURCE_DIR=C:/src/kdeconnect-kde `
  -DKDECONNECT_CORE_LIBRARY=C:/CraftRoot/lib/kdeconnectcore.lib
cmake --build build
cmake --install build --prefix C:/staging/rich-notifications
```

Use source matching the host version exactly. Copy the staged
`plugins/kdeconnect/kdeconnect_richnotifications.dll` into the host package's Qt
plugin directory `kdeconnect/`, and `share/kdeconnect/kdeconnect_richnotifications_config.qml`
into its existing KDE Connect data directory. Bundle them in your rebuilt host
package before signing. No files in an installed signed package are overwritten.
Rebuild this module whenever KDE Connect, Qt, compiler ABI or architecture changes.
The Linux `scripts/manage.py` helper is not used on Windows.

Restart the packaged host. In the paired device's KDE Connect plugin settings,
disable **Send notifications** and enable **Rich notifications**. Approve the
Windows notification-access prompt. Generate a new toast to test forwarding,
then dismiss it to test removal. Check Windows notification access/privacy
settings and KDE Connect logs if permission is refused.

## Scope and validation

Windows exposes application logos and text through the public listener API.
It does **not** expose the toast's chat avatar, website image, screenshot or hero
attachment through `NotificationBinding`. This port does not scrape application
databases or private Windows notification storage. It cannot provide Linux's
full image support through this API. Persistent-only and urgency settings are
hidden on Windows because that metadata is unavailable. Reply/actions and remote
dismissal are not implemented. Desktop conversation grouping remains a receiver
feature in KDE for Home Assistant.

CI compiles the native C++/WinRT backend on Windows and tests refusal from an
unpackaged process. It does not validate a complete KDE SDK DLL or interactive
permission/forwarding behavior. Full packaged-host runtime validation is still
required before treating this port as production-ready.
