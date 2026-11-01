#include "UI/PresetsTab.h"
#include "SKSEMenuFramework.h"
#include "UI/ConfigTabs.h"
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

    void VerticalSpace(const float a_lines) {
        ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, ImGuiMCP::GetTextLineHeight() * a_lines});
    }

    void CenteredWrappedText(const ImGuiMCP::ImVec4& a_color, const std::string& a_text) {
        const float windowWidth{ImGuiMCP::GetWindowSize().x};
        const float maxWidth{windowWidth - ImGuiMCP::GetStyle()->WindowPadding.x * 2.0f};
        const auto width = [](const std::string& a_line) { return ImGuiMCP::CalcTextSize(a_line.c_str()).x; };
        std::vector<std::string> lines;
        std::string line;
        bool fits{true};

        for (const auto word : std::views::split(a_text, ' ')) {
            const std::string_view wordView{word.begin(), word.end()};
            std::string candidate{line.empty() ? std::string{wordView} : line + " " + std::string{wordView}};

            if (width(candidate) <= maxWidth) {
                line = std::move(candidate);
                continue;
            }

            if (line.empty()) {
                fits = false;
                break;
            }

            lines.push_back(std::move(line));
            line = std::string{wordView};
            fits = width(line) <= maxWidth;

            if (!fits) break;
        }

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, a_color);

        if (!fits) {
            ImGuiMCP::TextWrapped("%s", a_text.c_str());
            ImGuiMCP::PopStyleColor();
            return;
        }

        lines.push_back(std::move(line));

        for (const auto& text : lines) {
            ImGuiMCP::SetCursorPosX((windowWidth - width(text)) * 0.5f);
            ImGuiMCP::TextUnformatted(text.c_str());
        }

        ImGuiMCP::PopStyleColor();
    }

    constexpr ImGuiMCP::ImVec4 kSuccessText{0.55f, 0.85f, 0.55f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kErrorText{1.0f, 0.45f, 0.4f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kWarningText{0.95f, 0.7f, 0.35f, 1.0f};

    struct ConfigResult {
        RE::FormID actor{0};
        std::string message;
        bool failed{false};
    };
    ConfigResult g_configResult;

    // Whether the config can name the actor by their own record. The player isn't given presets by the config, and a
    // leveled NPC's record is made as they spawn (0xFF ID format)
    bool IsNamedByConfig(RE::Actor* a_actor) {
        const auto* const base{a_actor->GetActorBase()};

        return !a_actor->IsPlayerRef() && base && !base->IsDynamicForm() &&
               !a_actor->extraList.HasType<RE::ExtraLeveledCreature>() && base->GetFile(0);
    }

    // After the NPC's entry was written to the in-game config file: the config is reloaded to use it
    void OnConfigEntryChanged(RE::Actor* a_actor, const std::optional<std::string>& a_error, const char* a_doneKey) {
        g_configResult.actor = a_actor->GetFormID();

        if (a_error) {
            g_configResult.failed = true;
            g_configResult.message = UI::Translations::Get("obody_config_entry_failed") + " " + *a_error;
            return;
        }

        const auto reload{UI::ConfigTabs::Reload()};
        const std::string done{a_doneKey ? UI::Translations::Get(a_doneKey) : std::string{}};
        g_configResult.failed = reload.failed;

        // The status line already shows what changed, the reload is only worth telling about when it went wrong
        if (!reload.failed && !reload.filesSkipped) {
            g_configResult.message = done;
            return;
        }

        g_configResult.message = done.empty() ? reload.message : done + " " + reload.message;
    }

    // Writes the NPC's preset, or takes their entry out, as the config names them
    void ChangeConfigEntry(RE::Actor* a_actor, const std::optional<std::string>& a_preset) {
        const auto* const base{a_actor->GetActorBase()};
        const std::string plugin{base->GetFile(0)->GetFilename()};
        const auto localFormID{base->GetLocalFormID()};
        const auto formID{std::format("{:08X}", localFormID)};

        // What the config finds with that name, it has to be the NPC
        if (RE::TESDataHandler::GetSingleton()->LookupForm(localFormID, plugin) != base) {
            logger::error("{} {} doesn't find {} ({:08X})", plugin, formID, base->GetName(), base->GetFormID());
            OnConfigEntryChanged(a_actor, std::format("{} {} doesn't find this NPC", plugin, formID), nullptr);
            return;
        }

        if (a_preset) {
            OnConfigEntryChanged(a_actor, Parser::SaveNpcPreset(plugin, formID, *a_preset), nullptr);
        } else {
            OnConfigEntryChanged(a_actor, Parser::RemoveNpcPreset(plugin, formID), "obody_config_entry_removed");
        }
    }

    float ButtonWidth(const std::string& a_label) {
        return ImGuiMCP::CalcTextSize(a_label.c_str()).x + ImGuiMCP::GetStyle()->FramePadding.x * 2.0f;
    }

    bool ButtonWithTooltip(const char* a_label, const bool a_enabled, const std::string& a_tooltip) {
        ImGuiMCP::BeginDisabled(!a_enabled);
        const bool clicked{ImGuiMCP::Button(a_label)};
        ImGuiMCP::EndDisabled();

        if (!a_tooltip.empty() && ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGuiMCP::SetTooltip("%s", a_tooltip.c_str());
        }

        return clicked;
    }

    // "Save preset to config", "Remove from config", and what the config gives the NPC
    void RenderConfigEntry(RE::Actor* a_actor, const std::optional<std::string>& a_current) {
        const auto config{Parser::JSONParser::GetInstance().GetConfig()};
        const bool named{IsNamedByConfig(a_actor)};
        const auto* const base{a_actor->GetActorBase()};
        const auto entry{named ? config->GetNPCFromCategorySet(base->GetFormID()) : std::nullopt};
        const bool same{entry && a_current && entry->bodyslidePresets == std::vector{*a_current}};

        const auto saveLabel{UI::Translations::Get("obody_config_save_button")};
        const auto removeLabel{UI::Translations::Get("obody_config_remove_button")};
        float width{ButtonWidth(saveLabel)};

        if (entry) {
            width += ImGuiMCP::GetStyle()->ItemSpacing.x + ButtonWidth(removeLabel);
        }

        std::string saveTooltip{UI::Translations::Get("obody_config_save_hint")};

        if (a_actor->IsPlayerRef()) {
            saveTooltip = UI::Translations::Get("obody_config_player");
        } else if (!named) {
            saveTooltip = UI::Translations::Get("obody_config_leveled");
        } else if (!a_current) {
            saveTooltip = UI::Translations::Get("obody_config_no_preset");
        } else if (same) {
            saveTooltip.clear();
        }

        ImGuiMCP::SetCursorPosX((ImGuiMCP::GetWindowSize().x - width) * 0.5f);

        if (ButtonWithTooltip(saveLabel.c_str(), named && a_current && !same, saveTooltip)) {
            ChangeConfigEntry(a_actor, a_current);
        }

        if (entry) {
            ImGuiMCP::SameLine();

            if (ButtonWithTooltip(removeLabel.c_str(), true, UI::Translations::Get("obody_config_remove_hint"))) {
                ChangeConfigEntry(a_actor, std::nullopt);
            }
        }

        // Taken again, a change above reloads the config
        const auto shownConfig{Parser::JSONParser::GetInstance().GetConfig()};
        const auto shownEntry{named ? shownConfig->GetNPCFromCategorySet(base->GetFormID()) : std::nullopt};

        std::vector<std::pair<ImGuiMCP::ImVec4, std::string>> lines;

        if (shownEntry && a_current && shownEntry->bodyslidePresets == std::vector{*a_current}) {
            lines.emplace_back(kSuccessText, UI::Translations::Get("obody_config_entry_same"));
        } else if (shownEntry) {
            std::string presets;

            for (const auto& preset : shownEntry->bodyslidePresets) {
                presets += (presets.empty() ? "" : ", ") + preset;
            }

            lines.emplace_back(kWarningText, UI::Translations::Get("obody_config_entry_differs") + " " + presets);
        }

        // The blacklists are checked before npcFormID
        if (named && (shownConfig->IsStringInJsonConfigKey(base->GetName(), "blacklistedNpcs") ||
                      shownConfig->IsActorInBlacklistedCharacterCategorySet(base->GetFormID()))) {
            lines.emplace_back(kWarningText, UI::Translations::Get("obody_config_entry_blacklisted"));
        }

        if (g_configResult.actor == a_actor->GetFormID() && !g_configResult.message.empty()) {
            lines.emplace_back(g_configResult.failed ? kErrorText : kSuccessText, g_configResult.message);
        }

        for (const auto& [color, text] : lines) {
            VerticalSpace(0.3f);
            CenteredWrappedText(color, text);
        }
    }

    // PresetContainer keeps non-blacklisted presets first and blacklisted ones last, and
    // PresetContainer::AssignPresetIndexes relies on that order, so we must order the presets alphabetically ourselves
    using PresetMenuEntry = UI::Util::PresetOrder;

    struct PresetMenuCache {
        std::vector<PresetMenuEntry> entries;
        bool valid{false};
        // The config the entries were built from: a reload can change which presets are blacklisted
        std::uint32_t configGeneration{0};
    };

    // One per sex, whether blacklisted presets are shown, and body (UBE, not UBE, or UBE with the other bodies' presets)
    std::array<PresetMenuCache, 12> g_presetMenuCache;

    // UBE actors also list the presets made for the other bodies, to pick one by hand. Kept while the game runs
    bool g_showOtherBodies{false};

    // a_otherBodies: the presets made for the other bodies are listed too, marked as such
    std::vector<PresetMenuEntry> BuildSortedPresetEntries(const PresetManager::PresetPointers& presets,
                                                          const bool ube, const bool a_otherBodies) {
        std::vector<PresetMenuEntry> entries;
        entries.reserve(presets.size());

        for (const auto* const presetPointer : presets) {
            const auto& preset{*presetPointer};

            // Make sure "-Refit" presets don't appear
            if (PresetManager::IsRefitPreset(preset.name)) continue;

            // Only the presets made for the actor's body, UBE or not
            if (preset.ube != ube && !a_otherBodies) continue;

            auto entry{UI::Util::MakePresetOrder(preset.name)};

            if (preset.ube != ube) {
                entry.displayName += " " + UI::Translations::Get("obody_preset_other_body");
            }

            entries.push_back(std::move(entry));
        }

        std::ranges::stable_sort(entries, UI::Util::IsBeforeInPresetOrder);

        return entries;
    }

    bool ShowBlacklistedPresets(const Parser::Config& a_config) {
        const auto& presetDistributionConfig{a_config.presetDistributionConfig};
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
        const auto config{Parser::JSONParser::GetInstance().GetConfig()};
        const bool showBlacklistedPresets = ShowBlacklistedPresets(*config);
        const bool isFemale = Body::OBody::IsFemale(a_actor);
        const bool isUBE = Body::OBody::IsUBE(a_actor);
        const bool otherBodies = isUBE && g_showOtherBodies;
        auto& menuCache = g_presetMenuCache[(otherBodies ? 8 : isUBE ? 4 : 0) + (isFemale ? 2 : 0) +
                                            (showBlacklistedPresets ? 1 : 0)];

        if (!menuCache.valid || menuCache.configGeneration != config->generation) {
            menuCache.entries = BuildSortedPresetEntries(config->GetPresets(isFemale, showBlacklistedPresets), isUBE,
                                                          otherBodies);
            menuCache.valid = true;
            menuCache.configGeneration = config->generation;
        }

        return menuCache.entries;
    }

    // The actor's name, their current preset, and the preset previewed on them
    void RenderHeader(RE::Actor* a_actor, const std::optional<std::string>& a_current, const std::string& a_shown) {
        std::string presetLine = UI::Translations::Get("obody_current_preset") + ": " +
                                 a_current.value_or(UI::Translations::Get("obody_none"));

        // Re-rolling all presets keeps it
        if (a_current && ActorTracker::Registry::GetInstance().IsPresetPickedByHand(a_actor)) {
            presetLine += " " + UI::Translations::Get("obody_picked_by_hand");
        }

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
    g_configResult = {};
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

    const auto resetLabel{UI::Translations::Get("obody_reset_morphs_button")};
    const auto rerollLabel{UI::Translations::Get("obody_reroll_button")};
    const bool player{a_actor->IsPlayerRef()};

    ImGuiMCP::SetCursorPosX(
        (ImGuiMCP::GetWindowSize().x - ButtonWidth(resetLabel) - ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f -
         ButtonWidth(rerollLabel)) *
        0.5f);

    if (ImGuiMCP::Button(resetLabel.c_str())) {
        UI::PreviewPanel::Apply(
            a_actor, [&] { Body::OBody::GetInstance().AssignPresetToActor(a_actor, "", false); },
            [handle{a_actor->GetHandle()}] {
                if (const auto actor{handle.get()}) {
                    Body::OnActorPresetChangedWithoutGeneration.SendEvent(actor.get(), "");
                }
            });
        return true;
    }

    ImGuiMCP::SameLine(0.0f, ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f);

    if (ButtonWithTooltip(rerollLabel.c_str(), !player,
                          UI::Translations::Get(player ? "obody_reroll_player" : "obody_reroll_hint"))) {
        UI::PreviewPanel::Apply(
            a_actor,
            [&] {
                const auto& obody{Body::OBody::GetInstance()};
                obody.RerollActorBody(a_actor, &obody.specialPapyrusPluginInterface);
            },
            [handle{a_actor->GetHandle()}] {
                if (const auto actor{handle.get()}) {
                    const auto preset{ActorTracker::Registry::GetInstance().GetPresetNameForActor(
                        actor.get(), Body::OBody::IsFemale(actor.get()))};
                    Body::OnActorPresetChangedWithoutGeneration.SendEvent(actor.get(), preset.value_or(""));
                }
            });
        return true;
    }

    VerticalSpace(0.4f);
    RenderConfigEntry(a_actor, current);

    VerticalSpace(0.4f);
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    UI::PreviewPanel::RenderControls(a_actor);

    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();

    UI::Widgets::SearchField(UI::Translations::Get("obody_search_presets").c_str(), "##Search", g_search,
                             sizeof(g_search), ImGuiMCP::IsWindowAppearing());

    // Presets made for other bodies may not fit a UBE body, they're only listed when asked for
    if (Body::OBody::IsUBE(a_actor)) {
        ImGuiMCP::Spacing();
        ImGuiMCP::Checkbox(UI::Translations::Get("obody_show_other_bodies").c_str(), &g_showOtherBodies);

        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("%s", UI::Translations::Get("obody_show_other_bodies_hint").c_str());
        }
    }

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
