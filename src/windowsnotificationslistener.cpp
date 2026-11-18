// SPDX-License-Identifier: GPL-3.0-only
#include "windowsnotificationslistener.h"
#include "notifyingapplication.h"
#include "plugin_sendnotifications_debug.h"
#include <QCoreApplication>
#include <QImage>
#include <QPointer>
#include <core/kdeconnectplugin.h>

using namespace RichNotifications::Windows;
WindowsNotificationsListener::WindowsNotificationsListener(KdeConnectPlugin *plugin)
    : NotificationsListener(plugin)
{
    QPointer<WindowsNotificationsListener> guard(this);
    backend = std::make_unique<Backend>([guard](Notification notification) {
        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, notification = std::move(notification)]() mutable {
            if (guard) guard->publish(std::move(notification));
        }, Qt::QueuedConnection);
    }, [guard](Access access) {
        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, access] {
            if (!guard) return;
            switch (access) {
            case Access::Allowed:
                qCInfo(KDECONNECT_PLUGIN_SENDNOTIFICATIONS) << "Windows notification access granted";
                break;
            case Access::NeedsPackage:
                qCWarning(KDECONNECT_PLUGIN_SENDNOTIFICATIONS) << "A packaged KDE Connect host with userNotificationListener capability is required";
                break;
            case Access::Denied:
                qCWarning(KDECONNECT_PLUGIN_SENDNOTIFICATIONS) << "Windows notification access was not granted; check Windows privacy settings";
                break;
            case Access::Error:
                qCWarning(KDECONNECT_PLUGIN_SENDNOTIFICATIONS) << "Windows notification listener failed; check package capability and permissions";
                break;
            }
        }, Qt::QueuedConnection);
    });
    backend->start();
}
WindowsNotificationsListener::~WindowsNotificationsListener() = default;
void WindowsNotificationsListener::publish(Notification notification)
{
    const QString id = QStringLiteral("windows:%1").arg(notification.id);
    if (notification.removed) {
        if (forwarded.remove(id)) {
            NetworkPacket packet(PACKET_TYPE_NOTIFICATION, {{QStringLiteral("id"), id}, {QStringLiteral("isCancel"), true}});
            m_plugin->sendPacket(packet);
        }
        return;
    }
    const auto app = QString::fromStdWString(notification.appName);
    const auto title = QString::fromStdWString(notification.title);
    const auto text = m_plugin->config()->getBool(QStringLiteral("generalIncludeBody"), true)
        ? QString::fromStdWString(notification.text) : QString();
    const auto ticker = text.isEmpty() ? title : title + QStringLiteral(": ") + text;
    if (!checkApplicationName(app, QString()) || checkIsInBlacklist(app, ticker)) return;
    NetworkPacket packet(PACKET_TYPE_NOTIFICATION, {
        {QStringLiteral("id"), id}, {QStringLiteral("appName"), app},
        {QStringLiteral("title"), title}, {QStringLiteral("text"), text},
        {QStringLiteral("ticker"), ticker}, {QStringLiteral("silent"), false},
        {QStringLiteral("isClearable"), true}});
    if (m_plugin->config()->getBool(QStringLiteral("generalSynchronizeIcons"), true) && !notification.logo.empty()) {
        const auto image = QImage::fromData(notification.logo.data(), static_cast<int>(notification.logo.size()));
        if (!image.isNull()) {
            auto payload = iconFromQImage(image);
            if (payload) packet.setPayload(payload, payload->size());
        }
    }
    forwarded.insert(id);
    while (forwarded.size() > 1024) forwarded.erase(forwarded.begin());
    m_plugin->sendPacket(packet);
}
#include "moc_windowsnotificationslistener.cpp"
