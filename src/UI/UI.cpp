#include "UI/UI.h"
#include "UI/Translations.h"
#include "Body/Body.h"
#include "Body/Event.h"
#include "PresetManager/PresetManager.h"
#include "JSONParser/JSONParser.h"

namespace {
    std::atomic<std::uint32_t> g_hotkeyScanCode{0x18};

    void CenteredText(const char* text, float fontScale = 1.0f) {
        const float windowWidth = ImGuiMCP::GetWindowSize().x;
        ImGuiMCP::SetWindowFontScale(fontScale);
        const float textWidth = ImGuiMCP::CalcTextSize(text).x;
        ImGuiMCP::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGuiMCP::Text("%s", text);
        ImGuiMCP::SetWindowFontScale(1.0f);
    }

    bool CenteredButton(const char* label) {
        const float windowWidth = ImGuiMCP::GetWindowSize().x;
        const auto* style = ImGuiMCP::GetStyle();
        const float buttonWidth = ImGuiMCP::CalcTextSize(label).x + style->FramePadding.x * 2.0f;
        ImGuiMCP::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
        return ImGuiMCP::Button(label);
    }

    bool __stdcall OnHotkeyInput(RE::InputEvent* a_event) {
        const auto scanCode = g_hotkeyScanCode.load();

        if (scanCode == 0 || !a_event) {
            return false;
        }

        if (auto* button = a_event->AsButtonEvent()) {
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                button->IsDown() &&
                button->GetIDCode() == scanCode) {

                if (!UI::PresetList::Window) {
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
}

void UI::PresetList::SetHotkeyScanCode(std::uint32_t scanCode) {
    g_hotkeyScanCode.store(scanCode);
}

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        RE::DebugMessageBox(UI::Translations::Get("obody_skse_menu_framework_failure").c_str());
    }

    PresetList::Window = SKSEMenuFramework::AddWindow(PresetList::Render);

    static auto* inputHandle = SKSEMenuFramework::AddInputEvent(OnHotkeyInput);
}

void __stdcall UI::PresetList::Render() {
    bool isOpen = Window->IsOpen.load();

    if (!isOpen) {
        return;
    }

    auto* viewport = ImGuiMCP::GetMainViewport();
    const auto center = ImGuiMCP::ImGuiViewportManager::GetCenter(viewport);
    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_Appearing, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(
        ImGuiMCP::ImVec2{viewport->Size.x * 0.35f, viewport->Size.y * 0.5f}, ImGuiMCP::ImGuiCond_Appearing);

    if (!ImGuiMCP::Begin("OBody##OBodyPresetWindow", &isOpen)) {
        ImGuiMCP::End();
        Window->IsOpen = isOpen;
        return;
    }

    static char buf[128] = "";
    static bool wasOpen = false;

    // This is to track the state of the menu, so scroll position and search input is reset between calls
    if (!wasOpen) {
        buf[0] = '\0';
        ImGuiMCP::SetScrollY(0.0f);
        wasOpen = true;
    }

    RE::Actor* actor = Event::OBodyEventHandler::GetSingleton()->GetCurrentCrosshairActor().get();

    if (actor == nullptr || !actor->HasKeywordString("ActorTypeNPC") || actor->IsChild()) {
        actor = RE::PlayerCharacter::GetSingleton();
    }

    auto a_presetName = ActorTracker::Registry::GetInstance().GetPresetNameForActor(actor, Body::OBody::IsFemale(actor));
    const auto& obody{Body::OBody::GetInstance()};

    const std::string presetLine = UI::Translations::Get("obody_current_preset") + ": " +
                                a_presetName.value_or(UI::Translations::Get("obody_none"));

    CenteredText(actor->GetDisplayFullName(), 1.4f);
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.7f, 0.7f, 0.75f, 1.0f});
    CenteredText(presetLine.c_str(), 1.0f);
    ImGuiMCP::PopStyleColor();

    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();

    if (CenteredButton(UI::Translations::Get("obody_reset_morphs_button").c_str())) {
        obody.AssignPresetToActor(actor, "", true, false);
        isOpen = false;
    }

    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    ImGuiMCP::TextUnformatted(UI::Translations::Get("obody_search_presets").c_str());
    ImGuiMCP::SameLine();

    if (ImGuiMCP::IsWindowAppearing()) {
        ImGuiMCP::SetKeyboardFocusHere();
    }

    ImGuiMCP::InputText("##Search", buf, sizeof(buf));

    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

    const auto& presetDistributionConfig{Parser::JSONParser::GetInstance().presetDistributionConfig};
    const auto showBlacklistedPresetsItr{presetDistributionConfig.FindMember("blacklistedPresetsShowInOBodyMenu")};
    const auto end{presetDistributionConfig.MemberEnd()};

    bool showBlacklistedPresets{false};
    if (showBlacklistedPresetsItr != end && showBlacklistedPresetsItr->value.IsBool()) {
        showBlacklistedPresets = showBlacklistedPresetsItr->value.GetBool();
    } else {
        logger::info(
            "Failed to read blacklistedPresetsShowInOBodyMenu key. Defaulting to showing the blacklisted presets "
            "in OBody menu.");
    }

    auto& base_presets = (Body::OBody::IsFemale(actor)
                            ? (showBlacklistedPresets ? presetContainer.allFemalePresets : presetContainer.femalePresets)
                            : (showBlacklistedPresets ? presetContainer.allMalePresets : presetContainer.malePresets));

    auto presets_to_show = base_presets
        | std::views::filter([](const PresetManager::Preset& preset) {
            if (buf[0] == '\0') return true;
            std::string nameLower = preset.name;
            std::string queryLower = buf;
            std::ranges::transform(nameLower, nameLower.begin(), [](unsigned char c){ return std::tolower(c); });
            std::ranges::transform(queryLower, queryLower.begin(), [](unsigned char c){ return std::tolower(c); });
            return nameLower.find(queryLower) != std::string::npos;
        })
        | std::views::transform(&PresetManager::Preset::name);

    int idx = 0;

    // this is here to later test glyphs sets
    // ImGuiMCP::Text("你好, 안녕하세요, こんにちは, Привет");

    for (const auto& i : presets_to_show) {
        ImGuiMCP::PushID(idx);
        if (ImGuiMCP::Selectable(i.c_str())) {
            obody.GenerateBodyByName(actor, i, &obody.specialPapyrusPluginInterface);
            isOpen = false;
            break;
        }
        ImGuiMCP::PopID();
        ++idx;
    }

    if (!isOpen) {
        wasOpen = false;
    }

    ImGuiMCP::End();
    Window->IsOpen = isOpen;
}
