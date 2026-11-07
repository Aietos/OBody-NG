#pragma once
#include "SKSEMenuFramework.h"

namespace UI {
    void Register();

    namespace PresetList {
        void __stdcall Render();
        inline MENU_WINDOW Window;

        void SetHotkeyScanCode(std::uint32_t scanCode);

        std::uint32_t GetHotkeyScanCode();

        void SetPreviewEnabled(bool a_enabled);
        bool IsPreviewEnabled();
    }  // namespace PresetList
};  // namespace UI
