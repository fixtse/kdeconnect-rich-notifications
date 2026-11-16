// SPDX-License-Identifier: GPL-2.0-or-later
#include "notificationslistener.h"
#include "notificationimagehint.h"
#include "plugin_sendnotifications_debug.h"
#include "pw_libnotificationmanager/notification.h"
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

Q_LOGGING_CATEGORY(KDECONNECT_PLUGIN_SENDNOTIFICATIONS, "kdeconnect.plugin.sendnotifications")

class ImageEncoder : public NotificationsListener
{
public:
    using NotificationsListener::iconFromQImage;
};

class NotificationImageTest : public QObject
{
    Q_OBJECT
    static QImage decodeHint(int byteCount, int stride = 8, int channels = 3, bool hasAlpha = false)
    {
        DBusMessage *message = dbus_message_new_signal("/test", "org.fixt.Test", "Image");
        DBusMessageIter root, structure, bytes;
        dbus_message_iter_init_append(message, &root);
        dbus_message_iter_open_container(&root, DBUS_TYPE_STRUCT, nullptr, &structure);
        for (dbus_int32_t value : {2, 2, stride}) {
            dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32, &value);
        }
        dbus_bool_t alpha = hasAlpha;
        dbus_message_iter_append_basic(&structure, DBUS_TYPE_BOOLEAN, &alpha);
        for (dbus_int32_t value : {8, channels}) {
            dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32, &value);
        }
        dbus_message_iter_open_container(&structure, DBUS_TYPE_ARRAY, "y", &bytes);
        const unsigned char data[] = {255,0,0, 0,255,0, 0,0, 0,0,255, 255,255,0, 0,0};
        const unsigned char *pointer = data;
        dbus_message_iter_append_fixed_array(&bytes, DBUS_TYPE_BYTE, &pointer, byteCount);
        dbus_message_iter_close_container(&structure, &bytes);
        dbus_message_iter_close_container(&root, &structure);
        dbus_message_iter_init(message, &root);
        QImage result = notificationImageFromDBus(&root);
        dbus_message_unref(message);
        return result;
    }
private Q_SLOTS:
    void rawWebsiteIconOverridesBrowserIcon()
    {
        const auto icon = decodeHint(16);
        QVERIFY(!icon.isNull());
        QCOMPARE(icon.pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(icon.pixelColor(1, 0), QColor(Qt::green));
        QCOMPARE(icon.pixelColor(0, 1), QColor(Qt::blue));
        NotificationManager::Notification notification(0);
        QImage browser(32, 32, QImage::Format_ARGB32);
        browser.fill(Qt::black);
        notification.setImage(browser);
        notification.processHints({{QStringLiteral("image-data"), QVariant::fromValue(icon)}});
        QCOMPARE(notification.image(), icon);
        QVERIFY(ImageEncoder::iconFromQImage(notification.image()));
    }
    void preservesRawAlpha()
    {
        const auto icon = decodeHint(16, 8, 4, true);
        QVERIFY(!icon.isNull());
        QCOMPARE(icon.pixel(0, 0), qRgba(255, 0, 0, 0));
    }
    void rejectsInvalidRawImages()
    {
        QVERIFY(decodeHint(4).isNull());
        QVERIFY(decodeHint(16, 1).isNull());
    }
    void payloadContainsReadablePng()
    {
        QImage original(96, 64, QImage::Format_ARGB32);
        original.fill(Qt::blue);
        auto payload = ImageEncoder::iconFromQImage(original);
        QVERIFY(payload);
        QVERIFY(payload->size() > 0);
        // Match UploadJob: it opens the returned device for reading.
        QVERIFY(payload->open(QIODevice::ReadOnly));
        const auto png = payload->readAll();
        QVERIFY(png.startsWith("\x89PNG\r\n\x1a\n"));
        QCOMPARE(QImage::fromData(png, "PNG"), original);
    }
    void fileThumbnailIsRetained()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage original(96, 64, QImage::Format_ARGB32);
        original.fill(Qt::blue);
        const auto path = directory.filePath(QStringLiteral("thumbnail.png"));
        QVERIFY(original.save(path, "PNG"));
        NotificationManager::Notification notification(0);
        notification.processHints({{QStringLiteral("image-path"), path}});
        QCOMPARE(notification.image(), original);
        QVERIFY(ImageEncoder::iconFromQImage(notification.image()));
    }
    void notificationsOwnTheirImages()
    {
        QImage red(96, 64, QImage::Format_ARGB32);
        red.fill(Qt::red);
        QImage blue(96, 64, QImage::Format_ARGB32);
        blue.fill(Qt::blue);
        NotificationManager::Notification first(0);
        first.setImage(red);
        const auto queuedCopy = first;
        NotificationManager::Notification second(0);
        second.setImage(blue);
        QCOMPARE(first.image(), red);
        QCOMPARE(queuedCopy.image(), red);
        QCOMPARE(second.image(), blue);
        NotificationManager::Notification withoutImage(0);
        QVERIFY(withoutImage.image().isNull());
        NotificationManager::Notification replacement(42);
        replacement.setImage(red);
        NotificationManager::Notification updated(42);
        updated.setImage(blue);
        QCOMPARE(replacement.image(), red);
        QCOMPARE(updated.image(), blue);
    }
    void screenshotInApplicationIconPath()
    {
        QTemporaryDir directory;
        QImage original(96, 64, QImage::Format_ARGB32);
        original.fill(Qt::green);
        const auto path = directory.filePath(QStringLiteral("screenshot.png"));
        QVERIFY(original.save(path, "PNG"));
        NotificationManager::Notification notification(0);
        notification.setApplicationIconName(path);
        notification.processHints({});
        QCOMPARE(notification.image(), original);
    }
    void nullImageHasNoPayload()
    {
        QVERIFY(!ImageEncoder::iconFromQImage(QImage()));
    }
};
QTEST_GUILESS_MAIN(NotificationImageTest)
#include "notificationimagetest.moc"
