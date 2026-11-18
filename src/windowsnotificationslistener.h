// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "notificationslistener.h"
#include "windows/nativebackend.h"
#include <QSet>
#include <memory>

class WindowsNotificationsListener : public NotificationsListener
{
    Q_OBJECT
public:
    explicit WindowsNotificationsListener(KdeConnectPlugin *plugin);
    ~WindowsNotificationsListener() override;
private:
    void publish(RichNotifications::Windows::Notification notification);
    std::unique_ptr<RichNotifications::Windows::Backend> backend;
    QSet<QString> forwarded;
};
