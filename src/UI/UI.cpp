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

    // PresetContainer keeps non-blacklisted presets first and blacklisted ones last, and
    // PresetContainer::AssignPresetIndexes relies on that order, so we must order the presets alphabetically ourselves
    struct PresetMenuEntry {
        std::string name;         // exact preset name, the only thing used for lookups
        std::string displayName;  // name without leading/trailing whitespace, shown in the list
        std::wstring searchText;  // displayName as lowercase UTF-16, used for searching
        std::wstring sortKey;     // displayName as UTF-16 without leading non-letters/digits, used for ordering
        bool hasAlnum;            // false for names with no letters/digits at all, so they go last
    };

    struct PresetMenuCache {
        std::vector<PresetMenuEntry> entries;
        bool valid{false};
    };

    std::array<PresetMenuCache, 4> g_presetMenuCache;

    std::string_view TrimWhitespace(std::string_view text) {
        constexpr std::string_view whitespace{" \t\r\n\f\v"};
        const auto first = text.find_first_not_of(whitespace);
        if (first == std::string_view::npos) {
            return {};
        }
        const auto last = text.find_last_not_of(whitespace);
        return text.substr(first, last - first + 1);
    }

    std::wstring Utf8ToWide(std::string_view text) {
        if (text.empty()) {
            return {};
        }
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0) {
            return {};
        }
        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    // Unicode-aware lowercase, invariant locale is needed so non-Latin systems don't break on latin letters
    std::wstring ToLowerWide(const std::wstring& text) {
        if (text.empty()) {
            return {};
        }
        std::wstring lower(text.size(), L'\0');
        const int length = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, text.data(),
                                         static_cast<int>(text.size()), lower.data(), static_cast<int>(lower.size()),
                                         nullptr, nullptr, 0);
        if (length <= 0) {
            return text;
        }
        lower.resize(static_cast<std::size_t>(length));
        return lower;
    }

    // Locale-aware, case-insensitive comparison so that accented letters sort next to their base letter
    int CompareLinguistic(const std::wstring& a, const std::wstring& b) {
        const int result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS, a.data(),
                                           static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), nullptr,
                                           nullptr, 0);
        if (result == 0) {
            return a.compare(b);  // fallback in case the API fails, realistically it shouldn't happen
        }
        return result - CSTR_EQUAL;
    }

    std::vector<PresetMenuEntry> BuildSortedPresetEntries(const PresetManager::PresetSet& presets) {
        std::vector<PresetMenuEntry> entries;
        entries.reserve(presets.size());

        for (const auto& preset : presets) {
            const auto trimmed = TrimWhitespace(preset.name);
            const std::wstring wide = Utf8ToWide(trimmed);

            PresetMenuEntry entry;
            entry.name = preset.name;
            entry.displayName = trimmed.empty() ? preset.name : std::string{trimmed};
            entry.searchText = ToLowerWide(wide);

            // Skip leading characters that aren't letters or digits for ordering,
            // so "(HIMBO) Foo" and "--((HIMBO) Foo" both sort under H.
            std::size_t firstAlnum = wide.size();
            if (!wide.empty()) {
                std::vector<WORD> types(wide.size());
                if (GetStringTypeW(CT_CTYPE1, wide.data(), static_cast<int>(wide.size()), types.data())) {
                    for (std::size_t i = 0; i < types.size(); ++i) {
                        if (types[i] & (C1_ALPHA | C1_DIGIT)) {
                            firstAlnum = i;
                            break;
                        }
                    }
                }
            }

            entry.hasAlnum = firstAlnum < wide.size();
            entry.sortKey = entry.hasAlnum ? wide.substr(firstAlnum) : wide;

            entries.push_back(std::move(entry));
        }

        std::ranges::stable_sort(entries, [](const PresetMenuEntry& a, const PresetMenuEntry& b) {
            if (a.hasAlnum != b.hasAlnum) {
                return a.hasAlnum;  // names with letters/digits first, symbol-only names last
            }
            if (const int cmp = CompareLinguistic(a.sortKey, b.sortKey); cmp != 0) {
                return cmp < 0;
            }
            return a.name < b.name;
        });

        return entries;
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
    // 0x01 is Esc, we treat that as unbinding/disabling the hotkey
    g_hotkeyScanCode.store(scanCode == 0x01 ? 0 : scanCode);
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
        for (auto& cache : g_presetMenuCache) {
            cache.valid = false;
        }
    }

    // Esc closes the menu
    if (ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false)) {
        isOpen = false;
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
                Body::OnActorPresetChangedWithoutGeneration.SendEvent(actor, "");
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

            const bool isFemale = Body::OBody::IsFemale(actor);
            auto& menuCache = g_presetMenuCache[(isFemale ? 2 : 0) + (showBlacklistedPresets ? 1 : 0)];

            if (!menuCache.valid) {
                const auto& basePresets = isFemale
                                              ? (showBlacklistedPresets ? presetContainer.allFemalePresets : presetContainer.femalePresets)
                                              : (showBlacklistedPresets ? presetContainer.allMalePresets : presetContainer.malePresets);
                menuCache.entries = BuildSortedPresetEntries(basePresets);
                menuCache.valid = true;
            }

            const std::wstring query = ToLowerWide(Utf8ToWide(buf));

            int idx = 0;

            // this is here to later test glyphs sets
            // ImGuiMCP::Text("你好, 안녕하세요, こんにちは, Привет");

            for (const auto& entry : menuCache.entries) {
                if (!query.empty() && entry.searchText.find(query) == std::wstring::npos) {
                    continue;
                }

                ImGuiMCP::PushID(idx);
                if (ImGuiMCP::Selectable(entry.displayName.c_str())) {
                    obody.GenerateBodyByName(actor, entry.name, &obody.specialPapyrusPluginInterface);
                    Body::OnActorPresetChangedWithoutGeneration.SendEvent(actor, entry.name);
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
