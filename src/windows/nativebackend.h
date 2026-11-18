// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RichNotifications::Windows {
enum class Access { Allowed, Denied, NeedsPackage, Error };
struct Notification {
    uint32_t id = 0;
    bool removed = false;
    std::wstring appId;
    std::wstring appName;
    std::wstring title;
    std::wstring text;
    std::vector<uint8_t> logo;
};
class Backend {
public:
    using Changed = std::function<void(Notification)>;
    using Status = std::function<void(Access)>;
    Backend(Changed changed, Status status);
    ~Backend();
    Backend(const Backend &) = delete;
    Backend &operator=(const Backend &) = delete;
    // Call from the application's UI thread; access prompts are asynchronous.
    void start();
    void stop();
private:
    struct State;
    std::shared_ptr<State> state;
    bool started = false;
    bool apartmentInitialized = false;
};
}
