# Design and scope

The module has its own KDE Connect ID (`kdeconnect_richnotifications`). KDE
Connect's existing daemon owns discovery, pairing, certificates and transport;
the plugin observes the desktop session's freedesktop notification calls and
sends standard `kdeconnect.notification` packets through that daemon.

Image priority is the notification's raw image-data or local image-path, then
its application icon. Raw RGB/RGBA decoding checks dimensions, stride, channel
count and buffer length. Images are bounded to 256 x 256 by the notification
manager, encoded as PNG and transferred through KDE Connect's payload channel.

Each notification owns its image. Copies queued between the D-Bus listener and
GUI thread retain that image; an unrelated notification cannot access an old
picture through the initial notification ID 0. Replacement notifications also
send their current image. Outgoing IDs distinguish apps, generated IDs and
replacement IDs.

The plugin does not implement notification-server method-return tracking. Its
first generated ID and a later desktop replacement ID may differ; this remains
a limitation for consumers trying to identify the first update precisely.
Remote dismissal and requests to replay existing notifications are not
implemented. This plugin focuses on forwarding new notifications and updates.

The native plugin interface is an internal KDE Connect API. Building requires
matching upstream headers and the installed core library. The installer checks
the exact KDE Connect version, and the Qt plugin loader performs its own binary
compatibility checks. An update can still require rebuilding or code changes.

The source includes the small notification-manager subset used by upstream's
sendnotifications plugin. It is intentionally retained with its original
license notices; the rest of KDE Connect is not bundled or built. The copied
subset is maintained here as independent derived work.
