# KDE Connect Rich Notifications

A separate desktop plugin for KDE Connect. Share notification text with
**chat avatars, website icons and screenshot previews**, directly over the
existing paired, encrypted KDE Connect connection. Compatible receivers include
Android and [KDE for Home Assistant](https://github.com/fixtse/kdeforhomeassistant).
No MQTT bridge or separate image server is required by this plugin.

Plugin ID: `kdeconnect_richnotifications` · Display name: **Rich notifications**

This is an independent, community plugin derived from KDE Connect. It is not an
official KDE release. Original attribution and licenses are retained; see
[NOTICE.md](NOTICE.md) and [LICENSE](LICENSE).

Linux supports the rich images described above. An **experimental Windows source
port** forwards text and application logos with a suitably packaged KDE Connect
host. See the [Windows build and packaging guide](docs/WINDOWS.md) for requirements
and limitations. macOS is not supported.

## Linux requirements and compatibility

- Linux desktop with a running session D-Bus and native desktop notifications.
- KDE Connect built with Qt 6 (Qt >= 6.7), C++20 compiler, CMake >= 3.22 and Ninja.
- KDE Frameworks 6: CoreAddons, I18n, IconThemes, Config, Service; libdbus headers.
- Python 3 and `busctl` for the optional user installer and activation helper.
- KDE Connect source **matching your installed KDE Connect version**. Only its
  headers are used; the daemon and core library are not rebuilt or replaced.

**Tested: KDE Connect 26.08.3 / Qt 6.12 on Arch Linux x86_64.** Other Linux
configurations are source-build targets, not verified compatibility claims.
There is no stable third-party KDE Connect plugin ABI. Rebuild after KDE Connect
or Qt upgrades; the installer rejects a different KDE Connect version.
Prebuilt universal binaries would be unsafe, so this project distributes source.

## Build

On Arch Linux (including CachyOS):

```sh
sudo pacman -S --needed base-devel git cmake ninja extra-cmake-modules kdeconnect qt6-tools
```

The `kdeconnect` dependencies supply the relevant Qt and KDE Frameworks libraries.
Other distributions need their corresponding **Qt 6 / KF6 development packages**;
Qt 5 installations are not supported.

```sh
git clone https://github.com/fixtse/kdeconnect-rich-notifications.git
cd kdeconnect-rich-notifications
kdeconnect-cli --version

# Use the version printed above. The example matches the tested version.
git clone --depth 1 --branch v26.08.3 \
  https://invent.kde.org/network/kdeconnect-kde.git deps/kdeconnect
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DKDECONNECT_SOURCE_DIR="$PWD/deps/kdeconnect"
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -v
```

If your distribution strips the unversioned core library symlink, CMake also
looks for `libkdeconnectcore.so.<major>`. For a custom package layout specify
`-DKDECONNECT_CORE_LIBRARY=/absolute/path/to/libkdeconnectcore.so.<major>`.

## Add the plugin to KDE Connect

Run these as your normal desktop user, **without sudo**:

```sh
python3 scripts/manage.py install build
python3 scripts/manage.py restart
kdeconnect-cli -l
```

The installer puts the module in `~/.local/lib/qt6/plugins/kdeconnect/`, installs
its settings page, and adds user-level D-Bus/autostart launchers. This makes Qt
find the plugin on both login and D-Bus activation. System package files remain
untouched. Existing launchers cause a refusal; inspect them and use
`install build --replace-discovery` to back them up before replacing them.

Wait until your intended receiver is **paired and reachable**, then substitute
its ID from `kdeconnect-cli -l`:

```sh
python3 scripts/manage.py enable DEVICE_ID
```

This enables Rich notifications and disables the stock **Send notifications**
plugin **only for that device**. It checks that the new plugin actually loaded
and restores the previous switches if activation fails. Do not enable both
senders for one device: they would send duplicate notifications.

Alternatively, after installation and restart open **KDE Connect settings ->
paired device -> plugins**, disable **Send notifications**, and enable
**Rich notifications**. Its settings button controls icons, body text and urgency.

Create a new notification to test it. Chat avatars or website images are used
when the originating app supplies them. Otherwise the application icon is used.
The plugin does not download avatars or favicons from online accounts.

### Home Assistant

Pair the desktop directly with KDE for Home Assistant. Enable sharing for that
desktop and choose apps in its sidebar panel. Use the integration's ESPHome
display blueprint for a knob. The receiver must provide a URL the display can
reach; set the blueprint's icon-server URL to HA's LAN address when needed.

Conversation history is displayed when the sending application/device provides
it. This plugin does not scrape chat history or combine unrelated notifications.

### Migration from the earlier patched sender

Install with `--replace-discovery` if a prior user launcher is already present.
Run `enable DEVICE_ID` to switch that receiver away from `kdeconnect_sendnotifications`.
An old user override named `kdeconnect_sendnotifications.so` may stay on disk for
rollback; it is no longer used for that receiver. Do not delete system plugins.

## Upgrade or remove

After a KDE Connect upgrade, disable the plugin if it stops loading, get matching
source headers, rebuild in a fresh build directory, then rerun `install`,
`restart`, and `enable`. Backups remain the originals across plugin upgrades.

```sh
# Restore the previous sender switches for one device:
python3 scripts/manage.py disable DEVICE_ID

# Restore recorded switches and pre-install launcher files, and remove the plugin:
python3 scripts/manage.py uninstall
python3 scripts/manage.py restart
```

The helper refuses to overwrite files you edited after installation. Its backup
manifest is at `~/.local/state/kdeconnect-rich-notifications/install.json`.
Manual switch changes made in KDE Connect's UI are not recorded by the helper;
restore those yourself before uninstalling. If upgrading KDE Connect makes the
old plugin incompatible, disable Rich notifications in the UI first.

## Troubleshooting

- **Plugin missing:** restart KDE Connect, check the user launchers and
  `QT_PLUGIN_PATH`, and confirm the source headers match the installed version.
- **Enable fails:** confirm the receiver is reachable. Inspect
  `~/.local/state/kdeconnect-rich-notifications/daemon.log` and the user journal.
- **Duplicates:** disable the stock sender for that receiver and any older MQTT
  bridge forwarding the same app/device.
- **Only the application icon:** the app may not supply a notification image.
  Raw `image-data`, legacy aliases and local `image-path` hints are supported.
- **Knob cannot fetch an image:** check the receiver's LAN image URL; this plugin
  sends images over KDE Connect and does not host an HTTP server.

## Development

`.devcontainer/` defines an Arch development environment. Build the container,
mount this repository and matching upstream source, and run the CMake/CTest and
Python commands above. No credentials or forwarded ports are needed for tests.
GUI pairing and real notification testing require a desktop session outside the
container. See [docs/VALIDATION.md](docs/VALIDATION.md) for recorded checks.
