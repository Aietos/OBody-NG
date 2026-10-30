#include "UI/PresetsTab.h"
#include "SKSEMenuFramework.h"
#include "UI/PreviewPanel.h"
#include "UI/Translations.h"
#include "UI/UI.h"
#include "UI/Util.h"
#include "UI/Widgets.h"
#include "Body/Body.h"
#include "PresetManager/PresetManager.h"
#include "JSONParser/JSONParser.h"

namespace {
    char g_search[128] = "";

    // A preset is previewed once the mouse rests on it, so moving over the list to reach the controls doesn't change
    // what's shown. The preview waits a little more before rebuilding the body (Preview::Session::RequestPreset)
    constexpr auto kHoverRest{std::chrono::milliseconds{250}};
    // How far the mouse can move and still count as resting, in pixels
    constexpr float kHoverStillDistance{6.0f};

    // The row the mouse rests on, and since when
    struct HoverRest {
        std::string name;
        ImGuiMCP::ImVec2 anchor;
        std::chrono::steady_clock::time_point since;
        // Previewed already, once per rest, so the arrow keys can move on from it
        bool shown{false};
    };
    HoverRest g_hoverRest;

    // The row the keyboard moved to last, it's previewed when that changes
    std::string g_lastFocused;

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

    // One per sex, whether blacklisted presets are shown, and body (UBE or not)
    std::array<PresetMenuCache, 8> g_presetMenuCache;

    std::vector<PresetMenuEntry> BuildSortedPresetEntries(const PresetManager::PresetSet& presets, const bool ube) {
        std::vector<PresetMenuEntry> entries;
        entries.reserve(presets.size());

        for (const auto& preset : presets) {
            // Make sure "-Refit" presets don't appear
            if (PresetManager::IsRefitPreset(preset.name)) continue;

            // Only the presets made for the actor's body, UBE or not
            if (preset.ube != ube) continue;

            const auto trimmed = UI::Util::TrimWhitespace(preset.name);
            const std::wstring wide = UI::Util::Utf8ToWide(trimmed);

            PresetMenuEntry entry;
            entry.name = preset.name;
            entry.displayName = trimmed.empty() ? preset.name : std::string{trimmed};
            entry.searchText = UI::Util::ToLowerWide(wide);

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
            if (const int cmp = UI::Util::CompareLinguistic(a.sortKey, b.sortKey); cmp != 0) {
                return cmp < 0;
            }
            return a.name < b.name;
        });

        return entries;
    }

    bool ShowBlacklistedPresets() {
        const auto& presetDistributionConfig{Parser::JSONParser::GetInstance().presetDistributionConfig};
        const auto showBlacklistedPresetsItr{presetDistributionConfig.FindMember("blacklistedPresetsShowInOBodyMenu")};

        if (showBlacklistedPresetsItr != presetDistributionConfig.MemberEnd() &&
            showBlacklistedPresetsItr->value.IsBool()) {
            return showBlacklistedPresetsItr->value.GetBool();
        }

        logger::info(
            "Failed to read blacklistedPresetsShowInOBodyMenu key. Defaulting to showing the blacklisted presets in "
            "OBody menu.");
        return false;
    }

    // The presets made for the actor's sex and body, in the order they're listed
    const std::vector<PresetMenuEntry>& GetPresetEntries(RE::Actor* a_actor) {
        const bool showBlacklistedPresets = ShowBlacklistedPresets();
        const bool isFemale = Body::OBody::IsFemale(a_actor);
        const bool isUBE = Body::OBody::IsUBE(a_actor);
        auto& menuCache = g_presetMenuCache[(isUBE ? 4 : 0) + (isFemale ? 2 : 0) + (showBlacklistedPresets ? 1 : 0)];

        if (!menuCache.valid) {
            const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
            const auto& basePresets =
                isFemale ? (showBlacklistedPresets ? presetContainer.allFemalePresets : presetContainer.femalePresets)
                         : (showBlacklistedPresets ? presetContainer.allMalePresets : presetContainer.malePresets);
            menuCache.entries = BuildSortedPresetEntries(basePresets, isUBE);
            menuCache.valid = true;
        }

        return menuCache.entries;
    }

    // The actor's name, their current preset, and the preset previewed on them
    void RenderHeader(RE::Actor* a_actor, const std::optional<std::string>& a_current, const std::string& a_shown) {
        const std::string presetLine = UI::Translations::Get("obody_current_preset") + ": " +
                                       a_current.value_or(UI::Translations::Get("obody_none"));

        CenteredText(a_actor->GetDisplayFullName(), 1.4f);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.7f, 0.7f, 0.75f, 1.0f});
        CenteredText(presetLine.c_str(), 1.0f);
        ImGuiMCP::PopStyleColor();

        if (!a_shown.empty()) {
            const std::string previewLine{UI::Translations::Get("obody_previewing") + ": " + a_shown};
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.96f, 0.84f, 0.58f, 1.0f});
            CenteredText(previewLine.c_str(), 1.0f);
            ImGuiMCP::PopStyleColor();
        } else if (UI::PresetList::IsPreviewEnabled()) {
            ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, ImGuiMCP::GetTextLineHeight()});
        }
    }

    // Previews the row the mouse rests on, or the keyboard moved to
    void PreviewHovered(RE::Actor* a_actor, const PresetMenuEntry* a_hovered, const PresetMenuEntry* a_focused,
                        const std::optional<std::string>& a_current) {
        if (a_focused && a_focused->name != g_lastFocused) {
            g_lastFocused = a_focused->name;
            UI::PreviewPanel::Hover(a_actor, a_focused->name, a_current);
        }

        // Moving off the list keeps the last preset shown
        if (!a_hovered) {
            g_hoverRest.name.clear();
            return;
        }

        const auto now{std::chrono::steady_clock::now()};
        const auto mouse{ImGuiMCP::GetIO()->MousePos};
        const float movedX{mouse.x - g_hoverRest.anchor.x};
        const float movedY{mouse.y - g_hoverRest.anchor.y};

        if (a_hovered->name != g_hoverRest.name ||
            movedX * movedX + movedY * movedY > kHoverStillDistance * kHoverStillDistance) {
            g_hoverRest = {a_hovered->name, mouse, now, false};
        } else if (!g_hoverRest.shown && now - g_hoverRest.since >= kHoverRest) {
            g_hoverRest.shown = true;
            UI::PreviewPanel::Hover(a_actor, a_hovered->name, a_current);
        }
    }

    bool RenderPresetList(RE::Actor* a_actor, const std::optional<std::string>& a_current, const std::string& a_shown) {
        const auto& obody{Body::OBody::GetInstance()};
        const std::wstring query = UI::Util::ToLowerWide(UI::Util::Utf8ToWide(g_search));

        int idx = 0;
        const PresetMenuEntry* hovered{nullptr};
        const PresetMenuEntry* focused{nullptr};

        // this is here to later test glyphs sets
        // ImGuiMCP::Text("你好, 안녕하세요, こんにちは, Привет");

        for (const auto& entry : GetPresetEntries(a_actor)) {
            if (!query.empty() && entry.searchText.find(query) == std::wstring::npos) {
                continue;
            }

            ImGuiMCP::PushID(idx);
            const bool previewed{!a_shown.empty() && entry.name == a_shown};

            if (previewed) {
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, ImGuiMCP::ImVec4{0.96f, 0.84f, 0.58f, 0.35f});
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderHovered, ImGuiMCP::ImVec4{0.96f, 0.84f, 0.58f, 0.5f});
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderActive, ImGuiMCP::ImVec4{0.96f, 0.84f, 0.58f, 0.6f});
            }
            const bool picked{ImGuiMCP::Selectable(entry.displayName.c_str(), previewed || entry.name == a_current)};

            if (previewed) {
                ImGuiMCP::PopStyleColor(3);
            }

            if (picked) {
                UI::PreviewPanel::Apply(
                    a_actor,
                    [&] {
                        // An NPC that isn't loaded gets the morphs as their 3D is built
                        obody.GenerateBodyByName(a_actor, entry.name, &obody.specialPapyrusPluginInterface,
                                                 !a_actor->Is3DLoaded());
                    },
                    [handle{a_actor->GetHandle()}, name{entry.name}] {
                        if (const auto actor{handle.get()}) {
                            Body::OnActorPresetChangedWithoutGeneration.SendEvent(actor.get(), name);
                        }
                    });
                // The row clicked has the keyboard now, it isn't previewed for that
                g_lastFocused = entry.name;
                ImGuiMCP::PopID();
                return true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                hovered = &entry;
            }
            if (ImGuiMCP::IsItemFocused()) {
                focused = &entry;
            }
            ImGuiMCP::PopID();
            ++idx;
        }

        PreviewHovered(a_actor, hovered, focused, a_current);

        return false;
    }
}  // namespace

void UI::PresetsTab::Reset() {
    g_search[0] = '\0';
    g_hoverRest = {};
    g_lastFocused.clear();
    for (auto& cache : g_presetMenuCache) {
        cache.valid = false;
    }
}

bool UI::PresetsTab::Render(RE::Actor* a_actor) {
    const auto current{
        ActorTracker::Registry::GetInstance().GetPresetNameForActor(a_actor, Body::OBody::IsFemale(a_actor))};
    const auto shown{UI::PreviewPanel::GetShownPreset(a_actor)};

    RenderHeader(a_actor, current, shown);

    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();

    if (CenteredButton(UI::Translations::Get("obody_reset_morphs_button").c_str())) {
        UI::PreviewPanel::Apply(
            a_actor, [&] { Body::OBody::GetInstance().AssignPresetToActor(a_actor, "", true, false); },
            [handle{a_actor->GetHandle()}] {
                if (const auto actor{handle.get()}) {
                    Body::OnActorPresetChangedWithoutGeneration.SendEvent(actor.get(), "");
                }
            });
        return true;
    }

    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    UI::PreviewPanel::RenderControls(a_actor);

    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    UI::Widgets::SearchField(UI::Translations::Get("obody_search_presets").c_str(), "##Search", g_search,
                             sizeof(g_search), ImGuiMCP::IsWindowAppearing());

    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    // Only the list scrolls, so the header and the controls stay in view
    const auto hints{UI::PreviewPanel::GetHints(a_actor)};
    float hintHeight{0.0f};

    if (!hints.empty()) {
        hintHeight = UI::Widgets::ControlHintsHeight(hints) + ImGuiMCP::GetStyle()->ItemSpacing.y * 3.0f + 2.0f;
    }

    bool picked{false};

    if (ImGuiMCP::BeginChild("##PresetListScroll", ImGuiMCP::ImVec2{0.0f, -hintHeight}, ImGuiMCP::ImGuiChildFlags_None,
                             ImGuiMCP::ImGuiWindowFlags_NavFlattened)) {
        picked = RenderPresetList(a_actor, current, shown);
    }
    ImGuiMCP::EndChild();

    if (!hints.empty()) {
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();
        UI::Widgets::ControlHints(hints);
    }

    return picked;
}
