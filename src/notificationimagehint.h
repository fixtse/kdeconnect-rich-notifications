// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <dbus/dbus.h>

// Decode the freedesktop notification (iiibiiay) image-data structure.
QImage notificationImageFromDBus(DBusMessageIter *structure);
