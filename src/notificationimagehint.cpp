// SPDX-License-Identifier: GPL-2.0-or-later
#include "notificationimagehint.h"
#include <cstring>
#include <cstdint>

QImage notificationImageFromDBus(DBusMessageIter *structure)
{
    char *signature = dbus_message_iter_get_signature(structure);
    const bool valid = signature && std::strcmp(signature, "(iiibiiay)") == 0;
    dbus_free(signature);
    if (!valid) {
        return {};
    }
    DBusMessageIter fields;
    dbus_message_iter_recurse(structure, &fields);
    auto integer = [&]() {
        dbus_int32_t value;
        dbus_message_iter_get_basic(&fields, &value);
        dbus_message_iter_next(&fields);
        return value;
    };
    const int width = integer();
    const int height = integer();
    const int stride = integer();
    dbus_bool_t alpha;
    dbus_message_iter_get_basic(&fields, &alpha);
    dbus_message_iter_next(&fields);
    const int bits = integer();
    const int channels = integer();
    DBusMessageIter bytes;
    dbus_message_iter_recurse(&fields, &bytes);
    const unsigned char *pixels = nullptr;
    int size = 0;
    dbus_message_iter_get_fixed_array(&bytes, &pixels, &size);
    if (width <= 0 || width > 2048 || height <= 0 || height > 2048
        || bits != 8 || (channels != 3 && channels != 4) || (alpha && channels != 4)
        || stride < width * channels || !pixels
        || int64_t(stride) * (height - 1) + width * channels > size) {
        return {};
    }
    QImage image(width, height, QImage::Format_ARGB32);
    if (image.isNull()) {
        return {};
    }
    for (int y = 0; y < height; ++y) {
        auto *destination = reinterpret_cast<QRgb *>(image.scanLine(y));
        const auto *row = pixels + int64_t(y) * stride;
        for (int x = 0; x < width; ++x) {
            const auto *pixel = row + x * channels;
            destination[x] = qRgba(pixel[0], pixel[1], pixel[2], alpha ? pixel[3] : 255);
        }
    }
    return image;
}
