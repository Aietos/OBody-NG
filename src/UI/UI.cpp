#include "UI/UI.h"
#include "UI/Translations.h"
#include "UI/ConfigEditor.h"
#include "UI/ConfigTabs.h"
#include "UI/GamepadCamera.h"
#include "UI/NpcList.h"
#include "UI/PresetsTab.h"
#include "UI/PreviewPanel.h"
#include "UI/Util.h"
#include "UI/Widgets.h"
#include "Body/Event.h"
#include "API/OSkin.h"
#include "Preview/Preview.h"

namespace {
    std::atomic<std::uint32_t> g_hotkeyScanCode{0x18};
    std::atomic_bool g_previewEnabled{true};

    bool g_closeAfterTextInput{false};

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

    // The game's idle camera starts spinning around the player when nothing has been pressed for fAutoVanityModeDelay,
    // which happens while the menu is open. The camera keeps its own allowAutoVanityMode, set from Skyrim.ini's
    // bDisableAutoVanityMode, so changing that setting while playing does nothing. The camera's vanity mode is
    // turned off while the menu is open, and put back as it was when it closes
    std::optional<bool> g_autoVanityAllowedBefore;

    void DisableAutoVanity() {
        auto* const camera{RE::PlayerCamera::GetSingleton()};

        if (!camera) return;

        auto& allowed{camera->GetRuntimeData2().allowAutoVanityMode};

        if (!g_autoVanityAllowedBefore) {
            g_autoVanityAllowedBefore = allowed;
            logger::info("The idle camera is off while the menu is open, it was {}", allowed ? "on" : "off already");
        }

        allowed = false;
    }

    void RestoreAutoVanity() {
        if (!g_autoVanityAllowedBefore) return;

        if (auto* const camera{RE::PlayerCamera::GetSingleton()}) {
            camera->GetRuntimeData2().allowAutoVanityMode = *g_autoVanityAllowedBefore;
        }

        g_autoVanityAllowedBefore.reset();
    }

    bool IsGameTextEntryActive() {
        const auto* controlMap = RE::ControlMap::GetSingleton();
        return controlMap && controlMap->GetRuntimeData().textEntryCount > 0;
    }

    constexpr std::uint32_t kVRTriggerButton{33};

    bool IsVRController(const RE::INPUT_DEVICE a_device) {
        if (!REL::Module::IsVR()) return false;

        switch (a_device) {
            case RE::INPUT_DEVICE::kVivePrimary:
            case RE::INPUT_DEVICE::kViveSecondary:
            case RE::INPUT_DEVICE::kOculusPrimary:
            case RE::INPUT_DEVICE::kOculusSecondary:
            case RE::INPUT_DEVICE::kWMRPrimary:
            case RE::INPUT_DEVICE::kWMRSecondary:
                return true;
            default:
                return false;
        }
    }

    // The menu's left mouse button is held by a VR trigger
    bool g_vrTriggerDown{false};

    // Hacky attempt to workaround an issue with SKSE Menu Framework, where the VR controller wand trigger
    // isn't being properly registered as an action click, preventing VR wand from interacting with OBody's menu.
    void ClickWithVRTrigger(const RE::ButtonEvent& a_button) {
        if (!IsVRController(a_button.GetDevice()) || a_button.GetIDCode() != kVRTriggerButton) return;

        const bool open{UI::PresetList::Window && UI::PresetList::Window->IsOpen.load()};

        if (a_button.IsDown() && open && !g_vrTriggerDown) {
            static bool logged{false};

            if (!logged) {
                logger::info("VR Trigger click detected, translating to left mouse button click");
                logged = true;
            }

            ImGuiMCP::ImGuiIOManager::AddMouseButtonEvent(ImGuiMCP::GetIO(), 0, true);
            g_vrTriggerDown = true;
        } else if (!a_button.IsPressed() && g_vrTriggerDown) {
            ImGuiMCP::ImGuiIOManager::AddMouseButtonEvent(ImGuiMCP::GetIO(), 0, false);
            g_vrTriggerDown = false;
        }
    }

    bool __stdcall OnHotkeyInput(RE::InputEvent* a_event) {
        if (!a_event) {
            return false;
        }

        if (const auto* const button{a_event->AsButtonEvent()}) {
            ClickWithVRTrigger(*button);
        }

        const auto scanCode = g_hotkeyScanCode.load();

        if (scanCode == 0) {
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
            RestoreAutoVanity();
        }

        if (a_type == SKSEMenuFramework::Model::EventType::kCloseMenu) {
            UI::PreviewPanel::OnMenuClosed();
        }

        Preview::Session::GetInstance().RunWaitingIfTimeRuns();
    }

    RE::ActorHandle g_openFor;

    // The actor to open OSkin's menu on
    RE::ActorHandle g_switchTo;

    struct CursorRestore {
        float x{0.0f};
        float y{0.0f};
        int frames{0};
    };

    std::optional<CursorRestore> g_cursorRestore;

    HWND GetGameWindow() {
        const auto* const renderer{RE::BSGraphics::Renderer::GetSingleton()};

        return renderer ? reinterpret_cast<HWND>(renderer->GetRuntimeData().renderWindows[0].hWnd) : nullptr;
    }

    void RestoreCursor() {
        if (!g_cursorRestore) return;

        auto& restore{*g_cursorRestore};

        ImGuiMCP::ImGuiIOManager::AddMousePosEvent(ImGuiMCP::GetIO(), restore.x, restore.y);

        if (auto* const window{GetGameWindow()}) {
            POINT point{std::lround(restore.x), std::lround(restore.y)};

            if (ClientToScreen(window, &point)) {
                SetCursorPos(point.x, point.y);
            }
        }

        if (--restore.frames <= 0) {
            g_cursorRestore.reset();
        }
    }

    bool g_editorTabShown{false};

    // The actor the menu is for, picked when it opens: the NPC in the crosshair, or the player. The NPC tab can
    // pick another. It's kept while the menu is open, because the preview's camera changes what the crosshair is on
    RE::ActorHandle g_target;
    bool g_selectPresetsTab{false};
    bool g_selectNpcTab{false};

    void SelectTarget(RE::Actor* a_actor, const bool a_menuOpened = false) {
        const bool another{a_menuOpened || g_target.get().get() != a_actor};
        g_target = a_actor->GetHandle();
        UI::PreviewPanel::OnTargetSelected(another);

        if (another) {
            UI::PresetsTab::OnTargetSelected();
        }
    }
}  // namespace

void UI::PresetList::SetHotkeyScanCode(std::uint32_t scanCode) {
    // 0x01 is Esc, we treat that as unbinding/disabling the hotkey
    g_hotkeyScanCode.store(scanCode == 0x01 ? 0 : scanCode);
}

std::uint32_t UI::PresetList::GetHotkeyScanCode() { return g_hotkeyScanCode.load(); }

bool UI::PresetList::OpenFor(RE::Actor* a_actor, const OBody::API::MenuState* a_state) {
    if (!Window || !Util::IsPresetTarget(a_actor)) {
        logger::info("Another mod asked to open the preset menu on {}",
                     a_actor ? a_actor->GetDisplayFullName() : "nobody");
        return false;
    }

    g_openFor = a_actor->GetHandle();
    Window->IsOpen = true;
    logger::info("The preset menu opens on {} for another mod", a_actor->GetDisplayFullName());

    if (a_state) {
        PreviewPanel::StartNextAs(a_actor, *a_state);

        if (a_state->cursorX >= 0.0f && a_state->cursorY >= 0.0f) {
            g_cursorRestore = CursorRestore{.x = a_state->cursorX, .y = a_state->cursorY, .frames = 3};
            logger::info("Menu: the cursor goes back to ({:.0f}, {:.0f}), where the other menu had it",
                         a_state->cursorX, a_state->cursorY);
        }
    }

    return true;
}

void UI::PresetList::OpenInOSkin(RE::Actor* a_actor) {
    // Render writes the window's state back as it ends, so the switch happens there
    if (a_actor) {
        g_switchTo = a_actor->GetHandle();
    }
}

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
        RestoreAutoVanity();
        return;
    }

    DisableAutoVanity();

    auto* viewport = ImGuiMCP::GetMainViewport();

    PreviewPanel::RenderOverlay(viewport);

    const auto center = ImGuiMCP::ImGuiViewportManager::GetCenter(viewport);

    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_FirstUseEver, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{viewport->Size.x * 0.38f, viewport->Size.y * 0.75f},
                                ImGuiMCP::ImGuiCond_FirstUseEver);

    PreviewPanel::PlaceWindow(viewport);

    // The Config Editor's page is as tall as its rules, so the window's scrollbar would come and go with the category
    // and the search, and everything would get narrower and wider with it: on that tab, its room is always kept
    const ImGuiMCP::ImGuiWindowFlags windowFlags{g_editorTabShown ? ImGuiMCP::ImGuiWindowFlags_AlwaysVerticalScrollbar
                                                                  : ImGuiMCP::ImGuiWindowFlags_None};

    if (!ImGuiMCP::Begin("OBody##OBodyPresetWindow", &isOpen, windowFlags)) {
        ImGuiMCP::End();
        Window->IsOpen = isOpen;
        return;
    }

    PreviewPanel::TrackWindow();

    const std::array<std::string, 6> tabs{
        UI::Translations::Get("obody_presets_tab"),          UI::Translations::Get("obody_npcs_tab"),
        UI::Translations::Get("obody_settings_header"),      UI::Translations::Get("obody_editor_tab"),
        UI::Translations::Get("obody_include_files_header"), UI::Translations::Get("obody_config_viewer_tab")};
    Widgets::UpdateTextScale(tabs);
    Widgets::PushScaledSpacing();

    const bool justOpened = ImGuiMCP::IsWindowAppearing();
    RestoreCursor();

    const auto openForHandle{std::exchange(g_openFor, RE::ActorHandle{}).get()};
    RE::Actor* const openFor{openForHandle && Util::IsPresetTarget(openForHandle.get()) ? openForHandle.get()
                                                                                        : nullptr};

    if (justOpened) {
        g_closeAfterTextInput = false;
        ImGuiMCP::SetScrollY(0.0f);
        PresetsTab::Reset();
        ConfigTabs::Reset();
        ConfigEditor::Reset();

        RE::Actor* crosshairActor{Event::OBodyEventHandler::GetSingleton()->GetCurrentCrosshairActor().get()};
        if (openFor) {
            SelectTarget(openFor, true);
        } else if (Util::IsPresetTarget(crosshairActor)) {
            SelectTarget(crosshairActor, true);
        } else {
            SelectTarget(RE::PlayerCharacter::GetSingleton(), true);
        }

        GamepadCamera::OnMenuOpened();
    } else if (openFor) {
        SelectTarget(openFor);
    }

    if (openFor) {
        g_selectPresetsTab = true;
    }

    if (g_closeAfterTextInput) {
        g_closeAfterTextInput = false;
        isOpen = false;
    }

    if ((ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) ||
         PreviewPanel::IsOverlayFocused()) &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false)) {
        if (ImGuiMCP::IsPopupOpen("", ImGuiMCP::ImGuiPopupFlags_AnyPopup) || Widgets::WasComboOpen()) {
            ImGuiMCP::ClearActiveID();
            Widgets::CloseOpenCombo();
        } else if (ImGuiMCP::IsAnyItemActive()) {
            ImGuiMCP::ClearActiveID();
            g_closeAfterTextInput = true;
        } else {
            isOpen = false;
        }
    }

    // The NPC picked may have been unloaded or deleted since
    RE::NiPointer<RE::Actor> target{g_target.get()};

    if (!target) {
        SelectTarget(RE::PlayerCharacter::GetSingleton());
        target = g_target.get();
    }
    RE::Actor* actor{target.get()};

    PreviewPanel::Manage(actor);

    if (ImGuiMCP::BeginTabBar("##OBodyTabs", ImGuiMCP::ImGuiTabBarFlags_FittingPolicyScroll)) {
        // Always open Presets tab by default
        const bool selectPresetsTab{justOpened || g_selectPresetsTab};
        g_selectPresetsTab = false;

        if (ImGuiMCP::BeginTabItem(
                tabs[0].c_str(), nullptr,
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
                tabs[1].c_str(), nullptr,
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

        if (ImGuiMCP::BeginTabItem(tabs[2].c_str())) {
            ConfigTabs::RenderSettings();
            ImGuiMCP::EndTabItem();
        }

        g_editorTabShown = ImGuiMCP::BeginTabItem(tabs[3].c_str());

        if (g_editorTabShown) {
            ConfigEditor::Render(actor);
            ImGuiMCP::EndTabItem();
        }

        if (ImGuiMCP::BeginTabItem(tabs[4].c_str())) {
            ConfigTabs::RenderIncludeFiles();
            ImGuiMCP::EndTabItem();
        }

        if (ImGuiMCP::BeginTabItem(tabs[5].c_str())) {
            ConfigTabs::RenderViewer();
            ImGuiMCP::EndTabItem();
        }

        ImGuiMCP::EndTabBar();
    }

    Widgets::PopScaledSpacing();
    ImGuiMCP::End();

    // OSkin's menu was asked for
    const auto switchTo{std::exchange(g_switchTo, RE::ActorHandle{}).get()};
    OBody::API::MenuState handOff;

    if (switchTo) {
        // Taken before the preview ends, OSkin's preview starts the same way
        handOff = PreviewPanel::GetShownState(switchTo.get());

        const auto mouse{ImGuiMCP::GetMousePos()};
        handOff.cursorX = mouse.x;
        handOff.cursorY = mouse.y;

        isOpen = false;
    }

    Window->IsOpen = isOpen;
    Widgets::EndFrame();

    if (!isOpen) {
        PreviewPanel::End();
        RestoreAutoVanity();
    }

    if (!switchTo) return;

    if (API::OSkin::OpenMenu(switchTo.get(), handOff)) {
        logger::info("Switched to OSkin's menu on {}", switchTo->GetDisplayFullName());
        return;
    }

    // The preview begins again next frame
    logger::warn("OSkin's menu didn't open on {}", switchTo->GetDisplayFullName());
    Window->IsOpen = true;
}
