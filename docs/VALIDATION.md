# Validation record - 2026-11-16

## Build and tests

Execution host: devserv, isolated checkout:
`~/agent-work/kdeconnect-rich-notifications-20261116`.
Development image: `.devcontainer/Dockerfile`, built as
`richnotifications-dev-20261116` on that host.

Headers: KDE Connect 26.08.3; only upstream headers were used. The independent
CMake build links the container's packaged KDE Connect core library, not a core
library built from our fork. Qt 6.12, GCC 16.2.1, x86_64, Arch Linux.

Executed inside the development container:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DKDECONNECT_SOURCE_DIR=/upstream
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -v
```

Results: build passed; notification image test executable passed; all four Python
installer tests passed. Coverage includes RGB/RGBA and transparency, row padding,
truncated image rejection, notification image ownership, screenshot paths,
website-image priority, install/upgrade/uninstall backup restoration, modified-file
protection, version mismatch rejection and rollback of failed device activation.

No C++ build, dependency installation or repository tests ran on the desktop
coordination host. The development container itself was built and exercised on
devserv. The real desktop checks below exercise the deployed local service.

## Desktop integration

Installed the standalone module on a Linux desktop running packaged KDE Connect
26.08.3 / Qt 6.12. Used the user installer with existing-discovery backup, restarted
KDE Connect, and activated the new plugin for its paired Home Assistant receiver.
D-Bus `loadedPlugins` included `kdeconnect_richnotifications` and excluded
`kdeconnect_sendnotifications` for that receiver.

Sent two native desktop notification fixtures:

1. A local PNG in the application-icon field.
2. A different raw RGBA image in `image-data`, together with the first PNG as
   the application icon, to test image priority.

Home Assistant received exactly one event for each, each with a downloadable PNG.
The second image contained the expected green pixels and overrode the application
icon. The ESPHome knob automation received each correct image URL. This verifies
the notification-to-display-action path; no physical screen observation was made.
The old sender override remains available on disk for rollback.

Installed module SHA256:
`7d700a40301b50a34133c6af9728e46b104a1716c705c86b0279b29c5327e777`.

## Boundaries

Other distributions, architectures, KDE Connect versions, nonstandard XDG layouts,
and other desktop notification servers have not been verified. The hosted GitHub
workflow is configured to repeat the source build against the Arch package's
matching upstream version; this record describes remote execution performed
before publication, not a claim about a future workflow run.

## Windows port — 2026-11-18

Source revision `a41257ee89cc7a7e0761b3efd7040679a1a711ce` was checked in
[GitHub Actions run 44303300584](https://github.com/fixtse/kdeconnect-rich-notifications/actions/runs/44303300584).
Both `windows-native` and `arch-linux` jobs passed. The temporary completion
notification job failed independently and has been removed.

The Windows job compiled the C++/WinRT native backend with MSVC and the Windows
SDK and passed the unpackaged-access test, including repeated start/stop calls.
The Linux job built the full plugin and passed CTest and four installer tests.
The same Linux checks also passed in the isolated remote checkout
`/home/fixt/agent-work/kdeconnect-rich-windows-20261118-checked` at that revision,
using the existing development image and matching 26.08.3 headers.

The Qt Windows adapter, complete KDE SDK DLL, package installation, permission
prompt and live notification forwarding have not been validated on Windows.
This remains an experimental source port; CI success for the native backend
is not a claim of full Windows runtime validation.
