#pragma once
#include "SKSEMenuFramework.h"
#include "UI/PreviewPanel.h"

namespace UI {
    void Register();

    namespace PresetList {
        void __stdcall Render();
        inline MENU_WINDOW Window;

        void SetHotkeyScanCode(std::uint32_t scanCode);

        std::uint32_t GetHotkeyScanCode();

        void SetPreviewEnabled(bool a_enabled);
        bool IsPreviewEnabled();

        // Opens the menu on the actor, for another mod's menu. a_state is how that menu showed them, or null.
        bool OpenFor(RE::Actor* a_actor, const OBody::API::MenuState* a_state);

        // Closes the menu and opens OSkin's menu on the actor
        void OpenInOSkin(RE::Actor* a_actor);
    }  // namespace PresetList
};  // namespace UI
