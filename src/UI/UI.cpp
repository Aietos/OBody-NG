#include "UI/UI.h"
#include "UI/Translations.h"
#include "UI/GamepadCamera.h"
#include "UI/JsonView.h"
#include "UI/NpcList.h"
#include "UI/PresetsTab.h"
#include "UI/PreviewPanel.h"
#include "UI/Util.h"
#include "Body/Event.h"
#include "JSONParser/JSONParser.h"

namespace {
    std::atomic<std::uint32_t> g_hotkeyScanCode{0x18};
    std::atomic_bool g_previewEnabled{true};

    constexpr auto kNumKeyboardKeys{static_cast<std::uint32_t>(SKSE::InputMap::kMacro_NumKeyboardKeys)};
    constexpr auto kNumMouseButtons{static_cast<std::uint32_t>(SKSE::InputMap::kMacro_NumMouseButtons)};
    constexpr auto kMouseButtonOffset{static_cast<std::uint32_t>(SKSE::InputMap::kMacro_MouseButtonOffset)};
    constexpr auto kInvalidKeyCode{static_cast<std::uint32_t>(SKSE::InputMap::kMaxMacros)};

    std::uint32_t ToMacroKeyCode(const RE::ButtonEvent& a_button) {
        const auto id = a_button.GetIDCode();
        switch (a_button.GetDevice()) {
            case RE::INPUT_DEVICE::kKeyboard:
                return id < kNumKeyboardKeys ? id : kInvalidKeyCode;
            case RE::INPUT_DEVICE::kMouse:
                return id < kNumMouseButtons ? kMouseButtonOffset + id : kInvalidKeyCode;
            case RE::INPUT_DEVICE::kGamepad:
                return SKSE::InputMap::GamepadMaskToKeycode(id);
            default:
                return kInvalidKeyCode;
        }
    }

    bool IsGameTextEntryActive() {
        const auto* controlMap = RE::ControlMap::GetSingleton();
        return controlMap && controlMap->GetRuntimeData().textEntryCount > 0;
    }

    bool __stdcall OnHotkeyInput(RE::InputEvent* a_event) {
        const auto scanCode = g_hotkeyScanCode.load();

        if (scanCode == 0 || !a_event) {
            return false;
        }

        if (auto* button = a_event->AsButtonEvent()) {
            if (button->IsDown() && ToMacroKeyCode(*button) == scanCode) {
                if (!UI::PresetList::Window) {
                    return false;
                }

                if (IsGameTextEntryActive()) {
                    return false;
                }

                const bool isCurrentlyOpen = UI::PresetList::Window->IsOpen.load();

                if (!isCurrentlyOpen) {
                    auto* ui = RE::UI::GetSingleton();
                    if (ui && (ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME))) {
                        return false;
                    }
                }

                UI::PresetList::Window->IsOpen = !isCurrentlyOpen;
            }
        }
        return false;
    }

    void __stdcall OnMenuFrameworkEvent(const SKSEMenuFramework::Model::EventType a_type) {
        // The window can be closed without rendering again, by the hotkey or by SKSE Menu Framework itself
        if (UI::PresetList::Window && !UI::PresetList::Window->IsOpen.load()) {
            UI::PreviewPanel::End();
        }

        if (a_type == SKSEMenuFramework::Model::EventType::kCloseMenu) {
            UI::PreviewPanel::OnMenuClosed();
        }
    }

    // The actor the menu is for, picked when it opens: the NPC in the crosshair, or the player. The NPC tab can
    // pick another. It's kept while the menu is open, because the preview's camera changes what the crosshair is on
    RE::ActorHandle g_target;
    bool g_selectPresetsTab{false};
    bool g_selectNpcTab{false};

    void SelectTarget(RE::Actor* a_actor, const bool a_menuOpened = false) {
        const bool another{a_menuOpened || g_target.get().get() != a_actor};
        g_target = a_actor->GetHandle();
        UI::PreviewPanel::OnTargetSelected(another);
    }
}  // namespace

void UI::PresetList::SetHotkeyScanCode(std::uint32_t scanCode) {
    // 0x01 is Esc, we treat that as unbinding/disabling the hotkey
    g_hotkeyScanCode.store(scanCode == 0x01 ? 0 : scanCode);
}

std::uint32_t UI::PresetList::GetHotkeyScanCode() { return g_hotkeyScanCode.load(); }

void UI::PresetList::SetPreviewEnabled(const bool a_enabled) { g_previewEnabled.store(a_enabled); }

bool UI::PresetList::IsPreviewEnabled() { return g_previewEnabled.load(); }

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        RE::DebugMessageBox(UI::Translations::Get("obody_skse_menu_framework_failure").c_str());
    }

    PresetList::Window = SKSEMenuFramework::AddWindow(PresetList::Render);

    static auto* inputHandle = SKSEMenuFramework::AddInputEvent(OnHotkeyInput);
    static auto* menuEventHandle = SKSEMenuFramework::AddEvent(OnMenuFrameworkEvent, 0.0f);
}

void __stdcall UI::PresetList::Render() {
    bool isOpen = Window->IsOpen.load();

    if (!isOpen) {
        PreviewPanel::End();
        return;
    }

    auto* viewport = ImGuiMCP::GetMainViewport();

    PreviewPanel::RenderOverlay(viewport);

    const auto center = ImGuiMCP::ImGuiViewportManager::GetCenter(viewport);

    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_FirstUseEver, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{viewport->Size.x * 0.35f, viewport->Size.y * 0.5f},
                                ImGuiMCP::ImGuiCond_FirstUseEver);

    PreviewPanel::PlaceWindow(viewport);

    if (!ImGuiMCP::Begin("OBody##OBodyPresetWindow", &isOpen)) {
        ImGuiMCP::End();
        Window->IsOpen = isOpen;
        return;
    }

    PreviewPanel::TrackWindow();

    const bool justOpened = ImGuiMCP::IsWindowAppearing();

    if (justOpened) {
        ImGuiMCP::SetScrollY(0.0f);
        PresetsTab::Reset();

        RE::Actor* crosshairActor{Event::OBodyEventHandler::GetSingleton()->GetCurrentCrosshairActor().get()};
        if (Util::IsPresetTarget(crosshairActor)) {
            SelectTarget(crosshairActor, true);
        } else {
            SelectTarget(RE::PlayerCharacter::GetSingleton(), true);
        }

        GamepadCamera::OnMenuOpened();
    }

    // Esc closes the menu, also after dragging on the preview's overlay
    if ((ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) ||
         PreviewPanel::IsOverlayFocused()) &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false)) {
        isOpen = false;
    }

    // The NPC picked may have been unloaded or deleted since
    RE::NiPointer<RE::Actor> target{g_target.get()};

    if (!target) {
        SelectTarget(RE::PlayerCharacter::GetSingleton());
        target = g_target.get();
    }
    RE::Actor* actor{target.get()};

    PreviewPanel::Manage(actor);

    if (ImGuiMCP::BeginTabBar("##OBodyTabs")) {
        // Always open Presets tab by default
        const bool selectPresetsTab{justOpened || g_selectPresetsTab};
        g_selectPresetsTab = false;

        if (ImGuiMCP::BeginTabItem(
                UI::Translations::Get("obody_presets_tab").c_str(), nullptr,
                selectPresetsTab ? ImGuiMCP::ImGuiTabItemFlags_SetSelected : ImGuiMCP::ImGuiTabItemFlags_None)) {
            if (PresetsTab::Render(actor)) {
                // An NPC that isn't loaded can't be seen, the menu goes back to the NPCs to pick the next one
                if (!actor->Is3DLoaded()) {
                    g_selectNpcTab = true;
                } else if (!PreviewPanel::KeepsMenuOpen()) {
                    isOpen = false;
                }
            }
            ImGuiMCP::EndTabItem();
        }

        const bool selectNpcTab{g_selectNpcTab};
        g_selectNpcTab = false;

        // The nearby NPCs are scanned each time the tab is opened
        static bool npcTabWasOpen{false};

        if (ImGuiMCP::BeginTabItem(
                UI::Translations::Get("obody_npcs_tab").c_str(), nullptr,
                selectNpcTab ? ImGuiMCP::ImGuiTabItemFlags_SetSelected : ImGuiMCP::ImGuiTabItemFlags_None)) {
            if (auto* const picked{NpcList::Render(actor, !npcTabWasOpen || justOpened)}) {
                SelectTarget(picked);
                g_selectPresetsTab = true;
            }
            npcTabWasOpen = true;
            ImGuiMCP::EndTabItem();
        } else {
            npcTabWasOpen = false;
        }

        if (ImGuiMCP::BeginTabItem(UI::Translations::Get("obody_config_viewer_tab").c_str())) {
            JsonView::Render(Parser::JSONParser::GetInstance().presetDistributionConfig);
            ImGuiMCP::EndTabItem();
        }

        ImGuiMCP::EndTabBar();
    }

    ImGuiMCP::End();
    Window->IsOpen = isOpen;

    if (!isOpen) {
        PreviewPanel::End();
    }
}
