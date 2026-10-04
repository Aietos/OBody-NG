#include "UI/UI.h"
#include "UI/Translations.h"
#include "Body/Body.h"
#include "Body/Event.h"
#include "PresetManager/PresetManager.h"
#include "JSONParser/JSONParser.h"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

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

    namespace JsonView {
        const ImGuiMCP::ImVec4 kKey    {0.60f, 0.80f, 1.00f, 1.0f};
        const ImGuiMCP::ImVec4 kString {0.90f, 0.68f, 0.45f, 1.0f};
        const ImGuiMCP::ImVec4 kNumber {0.70f, 0.90f, 0.60f, 1.0f};
        const ImGuiMCP::ImVec4 kKeyword{0.78f, 0.60f, 0.95f, 1.0f};  // true / false / null
        const ImGuiMCP::ImVec4 kPunct  {0.62f, 0.62f, 0.66f, 1.0f};
        const ImGuiMCP::ImVec4 kDim    {0.45f, 0.45f, 0.48f, 1.0f};

        // 0 = nothing, 1 = expand all, -1 = collapse all
        int g_forceOpen = 0;

        void Seg(const std::string& text, const ImGuiMCP::ImVec4& color) {
            ImGuiMCP::SameLine(0.0f, 0.0f);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
            ImGuiMCP::TextUnformatted(text.c_str());
            ImGuiMCP::PopStyleColor();
        }

        void BeginLeafLine(const char* id) {
            ImGuiMCP::TreeNodeEx(id, ImGuiMCP::ImGuiTreeNodeFlags_Leaf | ImGuiMCP::ImGuiTreeNodeFlags_NoTreePushOnOpen);
        }

        void KeyPrefix(const char* key) {
            if (!key) return;
            Seg(std::format("\"{}\"", key), kKey);
            Seg(": ", kPunct);
        }

        void Comma(bool comma) {
            if (comma) Seg(",", kPunct);
        }

        void Scalar(const rapidjson::Value& v) {
            if (v.IsString())      Seg(std::format("\"{}\"", v.GetString()), kString);
            else if (v.IsBool())   Seg(v.GetBool() ? "true" : "false", kKeyword);
            else if (v.IsNull())   Seg("null", kKeyword);
            else if (v.IsInt64())  Seg(std::to_string(v.GetInt64()), kNumber);
            else if (v.IsUint64()) Seg(std::to_string(v.GetUint64()), kNumber);
            else                   Seg(std::format("{}", v.GetDouble()), kNumber);
        }

        void Node(const char* key, const rapidjson::Value& v, bool comma);

        void Children(const rapidjson::Value& v) {
            if (v.IsObject()) {
                for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
                    ImGuiMCP::PushID(it->name.GetString());
                    Node(it->name.GetString(), it->value, std::next(it) != v.MemberEnd());
                    ImGuiMCP::PopID();
                }
            } else if (v.IsArray()) {
                for (rapidjson::SizeType i = 0; i < v.Size(); ++i) {
                    ImGuiMCP::PushID(static_cast<int>(i));
                    Node(nullptr, v[i], i + 1 < v.Size());
                    ImGuiMCP::PopID();
                }
            }
        }

        void Node(const char* key, const rapidjson::Value& v, bool comma) {
            const bool isObj = v.IsObject();
            const bool isArr = v.IsArray();

            if ((!isObj && !isArr) || (isObj && v.MemberCount() == 0) || (isArr && v.Empty())) {
                BeginLeafLine("##leaf");
                KeyPrefix(key);
                if (isObj)      Seg("{}", kPunct);
                else if (isArr) Seg("[]", kPunct);
                else            Scalar(v);
                Comma(comma);
                return;
            }

            const char* open  = isObj ? "{" : "[";
            const char* close = isObj ? "}" : "]";

            if (g_forceOpen != 0)
                ImGuiMCP::SetNextItemOpen(g_forceOpen > 0, ImGuiMCP::ImGuiCond_Always);
            else
                ImGuiMCP::SetNextItemOpen(true, ImGuiMCP::ImGuiCond_Once);

            const bool opened = ImGuiMCP::TreeNodeEx("##node", ImGuiMCP::ImGuiTreeNodeFlags_None);
            KeyPrefix(key);
            Seg(open, kPunct);

            if (!opened) {
                const auto count = isObj ? v.MemberCount() : v.Size();
                Seg(" … ", kDim);
                Seg(close, kPunct);
                Comma(comma);
                Seg(std::format("  // {} {}", count, isObj ? "keys" : "items"), kDim);
                return;
            }

            Children(v);
            ImGuiMCP::TreePop();

            BeginLeafLine("##close");
            Seg(close, kPunct);
            Comma(comma);
        }

        void Render(const rapidjson::Value& root) {
            if (ImGuiMCP::Button(UI::Translations::Get("obody_expand_all").c_str()))   g_forceOpen = 1;
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(UI::Translations::Get("obody_collapse_all").c_str())) g_forceOpen = -1;
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button(UI::Translations::Get("obody_copy_json").c_str())) {
                rapidjson::StringBuffer sb;
                rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
                writer.SetIndent(' ', 2);
                root.Accept(writer);
                ImGuiMCP::SetClipboardText(sb.GetString());
            }
            ImGuiMCP::Separator();

            if (ImGuiMCP::BeginChild("##ConfigViewerScroll")) {
                ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_IndentSpacing, 18.0f);

                const bool isObj = root.IsObject();
                const bool isArr = root.IsArray();

                if ((isObj && root.MemberCount() > 0) || (isArr && !root.Empty())) {
                    // Root is a fixed and non-collapsible
                    BeginLeafLine("##rootOpen");
                    Seg(isObj ? "{" : "[", kPunct);

                    ImGuiMCP::Indent();
                    Children(root);
                    ImGuiMCP::Unindent();

                    BeginLeafLine("##rootClose");
                    Seg(isObj ? "}" : "]", kPunct);
                } else {
                    Node(nullptr, root, false);
                }

                ImGuiMCP::PopStyleVar();
            }
            ImGuiMCP::EndChild();

            g_forceOpen = 0;
        }
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
    static char buf[128] = "";

    bool isOpen = Window->IsOpen.load();

    if (!isOpen) {
        return;
    }

    auto* viewport = ImGuiMCP::GetMainViewport();
    const auto center = ImGuiMCP::ImGuiViewportManager::GetCenter(viewport);
    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_FirstUseEver, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(
        ImGuiMCP::ImVec2{viewport->Size.x * 0.35f, viewport->Size.y * 0.5f}, ImGuiMCP::ImGuiCond_FirstUseEver);

    if (!ImGuiMCP::Begin("OBody##OBodyPresetWindow", &isOpen)) {
        ImGuiMCP::End();
        Window->IsOpen = isOpen;
        return;
    }

    const bool justOpened = ImGuiMCP::IsWindowAppearing();

    if (justOpened) {
        buf[0] = '\0';
        ImGuiMCP::SetScrollY(0.0f);
    }

    RE::Actor* actor = nullptr;

    if (ImGuiMCP::BeginTabBar("##OBodyTabs")) {
        // Always open Presets tab by default
        if (ImGuiMCP::BeginTabItem(UI::Translations::Get("obody_presets_tab").c_str(), nullptr,
                                   justOpened ? ImGuiMCP::ImGuiTabItemFlags_SetSelected
                                              : ImGuiMCP::ImGuiTabItemFlags_None)) {
            actor = Event::OBodyEventHandler::GetSingleton()->GetCurrentCrosshairActor().get();

            if (actor == nullptr || !actor->HasKeywordString("ActorTypeNPC") || actor->IsChild()) {
                actor = RE::PlayerCharacter::GetSingleton();
            }

            const auto& obody{Body::OBody::GetInstance()};

            auto a_presetName = ActorTracker::Registry::GetInstance().GetPresetNameForActor(actor, Body::OBody::IsFemale(actor));

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

            ImGuiMCP::EndTabItem();
        }

        if (ImGuiMCP::BeginTabItem(UI::Translations::Get("obody_config_viewer_tab").c_str())) {
            JsonView::Render(Parser::JSONParser::GetInstance().presetDistributionConfig);
            ImGuiMCP::EndTabItem();
        }

        ImGuiMCP::EndTabBar();
    }

    ImGuiMCP::End();
    Window->IsOpen = isOpen;
}
