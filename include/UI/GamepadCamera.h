#pragma once
#include "UI/Widgets.h"

namespace UI::GamepadCamera {
    void OnMenuOpened();

    void Control();

    // To check if gamepad is in use (if not, it's mouse+keyboard)
    bool IsInUse();

    std::vector<Widgets::ControlHint> Hints();
}  // namespace UI::GamepadCamera
