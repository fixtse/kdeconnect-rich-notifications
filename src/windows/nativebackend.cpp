// SPDX-License-Identifier: GPL-3.0-only
#include "nativebackend.h"
#include <windows.h>
#include <appmodel.h>
#include <roapi.h>
#include <atomic>
#include <map>
#include <mutex>
#include <utility>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include <winrt/Windows.UI.Notifications.h>

namespace RichNotifications::Windows {
namespace Native = winrt::Windows;
using Native::UI::Notifications::Management::UserNotificationListener;
using Native::UI::Notifications::Management::UserNotificationListenerAccessStatus;
using Native::UI::Notifications::UserNotificationChangedKind;

struct Backend::State : std::enable_shared_from_this<State> {
    Changed changed;
    Status status;
    std::atomic<bool> active{true};
    std::mutex mutex;
    UserNotificationListener listener{nullptr};
    winrt::event_token token{};
    bool subscribed = false;
    uint64_t sequence = 0;
    std::map<uint32_t, uint64_t> revisions;
    std::wstring ownFamily;

    static winrt::fire_and_forget notification(std::shared_ptr<State> self, uint32_t id, UserNotificationChangedKind kind)
    {
        try {
            uint64_t revision;
            {
                std::lock_guard lock(self->mutex);
                if (!self->active) co_return;
                revision = ++self->sequence;
                self->revisions[id] = revision;
                while (self->revisions.size() > 1024) self->revisions.erase(self->revisions.begin());
            }
            Notification result;
            result.id = id;
            if (kind == UserNotificationChangedKind::Removed) {
                result.removed = true;
            } else {
                if (self->listener.GetAccessStatus() != UserNotificationListenerAccessStatus::Allowed) co_return;
                auto source = self->listener.GetNotification(id);
                if (!source) co_return;
                result.appId = source.AppInfo().AppUserModelId().c_str();
                // Do not echo KDE Connect's own received notifications back out.
                if (result.appId.starts_with(self->ownFamily + L"!")) co_return;
                result.appName = source.AppInfo().DisplayInfo().DisplayName().c_str();
                auto binding = source.Notification().Visual().GetBinding(Native::UI::Notifications::KnownNotificationBindings::ToastGeneric());
                if (!binding) co_return;
                auto texts = binding.GetTextElements();
                if (texts.Size() == 0) co_return;
                result.title = texts.GetAt(0).Text().c_str();
                for (uint32_t i = 1; i < texts.Size(); ++i) {
                    if (!result.text.empty()) result.text += L"\n";
                    result.text += texts.GetAt(i).Text().c_str();
                }
                // The public listener API exposes the app logo, not toast image attachments.
                try {
                    auto logo = source.AppInfo().DisplayInfo().GetLogo({96, 96});
                    if (logo) {
                        auto stream = co_await logo.OpenReadAsync();
                        constexpr uint64_t maximum = 1024 * 1024;
                        if (stream.Size() > 0 && stream.Size() <= maximum) {
                            Native::Storage::Streams::DataReader reader(stream);
                            const auto count = static_cast<uint32_t>(stream.Size());
                            const auto loaded = co_await reader.LoadAsync(count);
                            if (loaded == count) {
                                result.logo.resize(loaded);
                                reader.ReadBytes(result.logo);
                            }
                        }
                    }
                } catch (const winrt::hresult_error &) {
                    // Missing or unreadable logos must not drop the text.
                }
            }
            bool current;
            {
                std::lock_guard lock(self->mutex);
                auto found = self->revisions.find(id);
                current = self->active && found != self->revisions.end() && found->second == revision;
            }
            if (current) self->changed(std::move(result));
        } catch (const winrt::hresult_error &) {
            // A toast can disappear while its snapshot is being read.
        } catch (...) {
            if (self->active) self->status(Access::Error);
        }
    }

    static winrt::fire_and_forget request(std::shared_ptr<State> self)
    {
        try {
            self->listener = UserNotificationListener::Current();
            auto access = self->listener.GetAccessStatus();
            if (access != UserNotificationListenerAccessStatus::Allowed) {
                access = co_await self->listener.RequestAccessAsync();
            }
            if (!self->active) co_return;
            if (access != UserNotificationListenerAccessStatus::Allowed) {
                self->status(Access::Denied);
                co_return;
            }
            {
                std::lock_guard lock(self->mutex);
                if (!self->active) co_return;
                std::weak_ptr<State> weak = self;
                self->token = self->listener.NotificationChanged([weak](const auto &, const auto &args) {
                    if (auto locked = weak.lock(); locked && locked->active) {
                        notification(locked, args.UserNotificationId(), args.ChangeKind());
                    }
                });
                self->subscribed = true;
            }
            self->status(Access::Allowed);
            // Existing Action Center entries are deliberately not replayed.
        } catch (...) {
            if (self->active) self->status(Access::Error);
        }
    }
};

Backend::Backend(Changed changed, Status status) : state(std::make_shared<State>())
{
    state->changed = std::move(changed);
    state->status = std::move(status);
}
Backend::~Backend()
{
    stop();
    if (apartmentInitialized) RoUninitialize();
}
void Backend::start()
{
    if (started || !state->active) return;
    started = true;
    UINT32 length = 0;
    if (GetCurrentPackageFamilyName(&length, nullptr) != ERROR_INSUFFICIENT_BUFFER) {
        state->status(Access::NeedsPackage);
        return;
    }
    std::wstring family(length, L'\0');
    if (GetCurrentPackageFamilyName(&length, family.data()) != ERROR_SUCCESS) {
        state->status(Access::Error);
        return;
    }
    family.resize(length - 1);
    state->ownFamily = std::move(family);
    const auto initialized = RoInitialize(RO_INIT_SINGLETHREADED);
    apartmentInitialized = SUCCEEDED(initialized);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
        state->status(Access::Error);
        return;
    }
    State::request(state);
}
void Backend::stop()
{
    state->active = false;
    std::lock_guard lock(state->mutex);
    if (state->subscribed) {
        try { state->listener.NotificationChanged(state->token); } catch (...) {}
        state->subscribed = false;
    }
}
}
