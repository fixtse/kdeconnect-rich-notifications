// SPDX-License-Identifier: GPL-3.0-only
#include "../src/windows/nativebackend.h"
#include <windows.h>
#include <appmodel.h>
#include <iostream>
using namespace RichNotifications::Windows;
int main()
{
    UINT32 length = 0;
    if (GetCurrentPackageFamilyName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE) {
        std::cerr << "This test requires an unpackaged process\n";
        return 1;
    }
    int statuses = 0;
    bool unexpected = false;
    Backend backend([&](Notification) { unexpected = true; }, [&](Access access) {
        ++statuses;
        if (access != Access::NeedsPackage) unexpected = true;
    });
    backend.start();
    backend.start(); // Idempotent: no duplicate permission request or subscription.
    backend.stop();
    backend.stop();
    backend.start(); // A stopped listener cannot be revived accidentally.
    if (unexpected || statuses != 1) return 1;
    return 0;
}
