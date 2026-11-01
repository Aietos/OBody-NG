#include "UI/ConfigTabs.h"
#include "Body/Body.h"
#include "Body/Event.h"
#include "JSONParser/JSONParser.h"
#include "Preview/Preview.h"
#include "SKSEMenuFramework.h"
#include "UI/JsonView.h"
#include "UI/PreviewPanel.h"
#include "UI/Translations.h"
#include "UI/Widgets.h"

namespace {
    constexpr ImGuiMCP::ImVec4 kSuccessText{0.55f, 0.85f, 0.55f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kErrorText{1.0f, 0.45f, 0.4f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kWarningText{0.95f, 0.7f, 0.35f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kHintText{0.7f, 0.7f, 0.75f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kGoldText{0.96f, 0.84f, 0.58f, 1.0f};

    constexpr auto ShowBlacklistedKey{"blacklistedPresetsShowInOBodyMenu"};
    constexpr auto DisableDistributionKey{"disablePresetDistribution"};

    // The values disablePresetDistribution can have
    constexpr std::array<std::pair<std::string_view, const char*>, 4> DistributionValues{{
        {"none"sv, "obody_settings_distribution_to_all"},
        {"male"sv, "obody_settings_distribution_to_female"},
        {"female"sv, "obody_settings_distribution_to_male"},
        {"all"sv, "obody_settings_distribution_to_none"},
    }};

    // The tabs a reload can be started from, the result is shown in the one it was started from
    enum class Tab { kNone, kSettings, kIncludeFiles, kViewer };

    // The result of the last reload or setting change, shown until the menu is opened again
    std::string g_result;
    bool g_failed{false};
    Tab g_resultTab{Tab::kNone};

    void TextColored(const ImGuiMCP::ImVec4& a_color, const std::string& a_text) {
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, a_color);
        ImGuiMCP::TextWrapped("%s", a_text.c_str());
        ImGuiMCP::PopStyleColor();
    }

    void VerticalSpace(const float a_lines) {
        ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, ImGuiMCP::GetTextLineHeight() * a_lines});
    }

    void Reload(const Tab a_tab, const std::string& a_prefix) {
        const auto result{UI::ConfigTabs::Reload()};

        g_resultTab = a_tab;
        g_failed = result.failed;
        g_result = a_prefix + result.message;
    }

    void OnSettingSaved(const std::optional<std::string>& a_error) {
        if (a_error) {
            g_resultTab = Tab::kSettings;
            g_failed = true;
            g_result = UI::Translations::Get("obody_settings_save_failed") + " " + *a_error;
            return;
        }

        Reload(Tab::kSettings, UI::Translations::Get("obody_settings_saved") + " ");
    }

    void RenderResult(const Tab a_tab) {
        if (g_result.empty() || g_resultTab != a_tab) return;

        VerticalSpace(0.3f);
        TextColored(g_failed ? kErrorText : kSuccessText, g_result);
    }

    void RenderReloadButton(const Tab a_tab) {
        if (ImGuiMCP::Button(UI::Translations::Get("obody_reload_config").c_str())) {
            Reload(a_tab, "");
        }

        ImGuiMCP::SameLine();
        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kHintText);
        ImGuiMCP::TextUnformatted(UI::Translations::Get("obody_reload_config_hint").c_str());
        ImGuiMCP::PopStyleColor();

        RenderResult(a_tab);
    }

    void RenderDistributionSetting(const rapidjson::Document& a_document) {
        const auto distributionItr{a_document.FindMember(DisableDistributionKey)};
        const std::string_view distribution{
            distributionItr != a_document.MemberEnd() && distributionItr->value.IsString()
                ? std::string_view{distributionItr->value.GetString(), distributionItr->value.GetStringLength()}
                : "none"sv};
        const auto current{
            std::ranges::find(DistributionValues, distribution, &std::pair<std::string_view, const char*>::first)};
        // A value the config doesn't know is shown as it's written
        const std::string currentName{current != DistributionValues.end() ? UI::Translations::Get(current->second)
                                                                          : std::string{distribution}};

        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted(UI::Translations::Get("obody_settings_distribution").c_str());
        ImGuiMCP::SameLine();
        ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 11.0f);

        if (ImGuiMCP::BeginCombo("##OBodyDistribution", currentName.c_str())) {
            for (const auto& [value, nameKey] : DistributionValues) {
                const bool selected{value == distribution};

                if (ImGuiMCP::Selectable(UI::Translations::Get(nameKey).c_str(), selected) && !selected) {
                    OnSettingSaved(Parser::SetMasterConfigValue(DisableDistributionKey, value));
                }

                if (selected) {
                    ImGuiMCP::SetItemDefaultFocus();
                }
            }
            ImGuiMCP::EndCombo();
        }

        TextColored(kHintText, UI::Translations::Get("obody_settings_distribution_hint"));
    }

    bool g_confirmReroll{false};
    std::string g_rerollResult;

    std::string WithCount(const char* a_key, const std::size_t a_count) {
        auto text{UI::Translations::Get(a_key)};
        boost::algorithm::replace_first(text, "{}", std::to_string(a_count));

        return text;
    }

    // The NPCs around the player whose presets the distribution gives out, except the ones picked by hand
    std::vector<RE::Actor*> GetLoadedRerollTargets() {
        std::vector<RE::Actor*> actors;
        const auto& registry{ActorTracker::Registry::GetInstance()};
        auto& preview{Preview::Session::GetInstance()};

        if (auto* const processLists{RE::ProcessLists::GetSingleton()}) {
            for (const auto& handle : processLists->highActorHandles) {
                const auto actor{handle.get()};

                if (!actor || !actor->Is3DLoaded() || actor->IsPlayerRef() || preview.IsCopy(actor.get()) ||
                    !Event::IsDistributionTarget(actor.get()) || registry.IsPresetPickedByHand(actor.get())) {
                    continue;
                }

                actors.push_back(actor.get());
            }
        }

        return actors;
    }

    // The other NPCs the game keeps in memory, like the persistent NPCs of cells that aren't loaded
    std::vector<RE::Actor*> GetOtherRerollTargets(const std::vector<RE::Actor*>& a_loaded) {
        std::vector<RE::Actor*> actors;
        auto& preview{Preview::Session::GetInstance()};
        const auto [map, lock]{RE::TESForm::GetAllForms()};
        RE::BSReadLockGuard guard{lock.get()};

        if (!map) return actors;

        for (const auto& [formID, form] : *map) {
            if (!form || form->GetFormType() != RE::FormType::ActorCharacter) continue;

            auto* const actor{form->As<RE::Actor>()};

            if (!actor || actor->IsDeleted() || actor->IsPlayerRef() || preview.IsCopy(actor) ||
                std::ranges::contains(a_loaded, actor) || !Event::IsDistributionTarget(actor)) {
                continue;
            }

            actors.push_back(actor);
        }

        return actors;
    }

    void SectionHeader(const std::string& a_text) {
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kGoldText);
        ImGuiMCP::SetWindowFontScale(1.15f);
        ImGuiMCP::TextUnformatted(a_text.c_str());
        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::PopStyleColor();
        VerticalSpace(0.3f);
    }

    bool AccentButton(const std::string& a_label, const float a_width) {
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, ImGuiMCP::ImVec4{0.27f, 0.23f, 0.16f, 1.0f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, ImGuiMCP::ImVec4{0.35f, 0.29f, 0.19f, 1.0f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, ImGuiMCP::ImVec4{0.42f, 0.35f, 0.23f, 1.0f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kGoldText);
        ImGuiMCP::PushStyleVar(
            ImGuiMCP::ImGuiStyleVar_FramePadding,
            ImGuiMCP::ImVec2{ImGuiMCP::GetStyle()->FramePadding.x * 2.0f, ImGuiMCP::GetTextLineHeight() * 0.45f});

        const bool clicked{ImGuiMCP::Button(a_label.c_str(), ImGuiMCP::ImVec2{a_width, 0.0f})};

        ImGuiMCP::PopStyleVar();
        ImGuiMCP::PopStyleColor(4);

        return clicked;
    }

    void RenderRerollAll() {
        const float width{ImGuiMCP::GetContentRegionAvail().x * 0.6f};
        const float centered{(ImGuiMCP::GetWindowSize().x - width) * 0.5f};

        SectionHeader(UI::Translations::Get("obody_settings_section_reroll"));
        TextColored(kHintText, UI::Translations::Get("obody_reroll_all_hint"));
        VerticalSpace(0.6f);

        if (!g_confirmReroll) {
            ImGuiMCP::SetCursorPosX(centered);

            if (AccentButton(UI::Translations::Get("obody_reroll_all_button"), width)) {
                g_confirmReroll = true;
                g_rerollResult.clear();
            }

            if (!g_rerollResult.empty()) {
                VerticalSpace(0.5f);
                TextColored(kSuccessText, g_rerollResult);
            }

            return;
        }

        const auto targets{GetLoadedRerollTargets()};

        TextColored(kWarningText, WithCount("obody_reroll_all_confirm", targets.size()));
        VerticalSpace(0.5f);

        const float buttonWidth{(width - ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f) * 0.5f};
        ImGuiMCP::SetCursorPosX(centered);

        if (AccentButton(UI::Translations::Get("obody_reroll_all_yes"), buttonWidth)) {
            g_confirmReroll = false;

            // The preview would put the NPC's old body back as it ends
            UI::PreviewPanel::End();

            const auto rerolled{Body::OBody::GetInstance().RerollAllBodies(targets, GetOtherRerollTargets(targets))};
            g_rerollResult = WithCount("obody_reroll_all_done", rerolled);
        }

        ImGuiMCP::SameLine(0.0f, ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f);
        ImGuiMCP::PushStyleVar(
            ImGuiMCP::ImGuiStyleVar_FramePadding,
            ImGuiMCP::ImVec2{ImGuiMCP::GetStyle()->FramePadding.x * 2.0f, ImGuiMCP::GetTextLineHeight() * 0.45f});

        if (ImGuiMCP::Button(UI::Translations::Get("obody_reroll_all_cancel").c_str(),
                             ImGuiMCP::ImVec2{buttonWidth, 0.0f})) {
            g_confirmReroll = false;
        }

        ImGuiMCP::PopStyleVar();
    }

    void RenderIncludeFileTable(const std::vector<Parser::IncludeFileInfo>& a_files, const std::size_t a_inGame) {
        constexpr auto flags{ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_BordersInnerV |
                             ImGuiMCP::ImGuiTableFlags_BordersOuterH | ImGuiMCP::ImGuiTableFlags_SizingStretchProp};

        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_CellPadding, ImGuiMCP::ImVec2{8.0f, 5.0f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_TableHeaderBg, UI::Widgets::TableHeaderBackground);

        if (!ImGuiMCP::BeginTable("##OBodyIncludeFiles", 3, flags)) {
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::PopStyleVar();
            return;
        }

        ImGuiMCP::TableSetupColumn("#", 0, 0.3f);
        ImGuiMCP::TableSetupColumn(UI::Translations::Get("obody_include_files_column_file").c_str(), 0, 3.0f);
        ImGuiMCP::TableSetupColumn(UI::Translations::Get("obody_include_files_column_folder").c_str(), 0, 5.0f);

        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, UI::Widgets::TableHeaderText);
        ImGuiMCP::TableHeadersRow();
        ImGuiMCP::PopStyleColor();

        for (std::size_t i{0}; i < a_files.size(); ++i) {
            const auto& file{a_files[i]};
            const auto separator{file.from.find_last_of("\\/")};
            const std::string folder{separator == std::string::npos ? file.from : file.from.substr(0, separator)};

            ImGuiMCP::TableNextRow();

            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text("%zu", i + 1);

            ImGuiMCP::TableNextColumn();

            if (file.skipped) {
                TextColored(kErrorText, file.name + " " + UI::Translations::Get("obody_include_files_skipped"));
            } else if (i == a_inGame) {
                TextColored(kGoldText, file.name);
            } else {
                ImGuiMCP::TextUnformatted(file.name.c_str());
            }

            ImGuiMCP::TableNextColumn();
            ImGuiMCP::TextUnformatted(folder.c_str());

            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("%s", file.from.c_str());
            }
        }

        ImGuiMCP::EndTable();
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::PopStyleVar();
    }
}  // namespace

void UI::ConfigTabs::RenderSettings() {
    const auto config{Parser::JSONParser::GetInstance().GetConfig()};
    const auto& document{config->presetDistributionConfig};
    const auto showItr{document.FindMember(ShowBlacklistedKey)};
    bool show{showItr != document.MemberEnd() && showItr->value.IsBool() && showItr->value.GetBool()};

    VerticalSpace(0.5f);
    SectionHeader(UI::Translations::Get("obody_settings_section_config"));

    if (ImGuiMCP::Checkbox(UI::Translations::Get("obody_settings_show_blacklisted").c_str(), &show)) {
        OnSettingSaved(Parser::SetMasterConfigValue(ShowBlacklistedKey, show));
    }

    VerticalSpace(0.8f);
    RenderDistributionSetting(document);
    VerticalSpace(0.8f);
    TextColored(kHintText, UI::Translations::Get("obody_settings_master_note"));
    RenderResult(Tab::kSettings);

    VerticalSpace(1.2f);
    ImGuiMCP::Separator();
    VerticalSpace(1.0f);
    RenderRerollAll();
}

void UI::ConfigTabs::RenderIncludeFiles() {
    const auto config{Parser::JSONParser::GetInstance().GetConfig()};
    const auto& files{config->includeFiles};
    const auto inGame{static_cast<std::size_t>(
        std::ranges::find_if(
            files, [](const auto& a_file) { return boost::iequals(a_file.name, Parser::InGameConfigFileName); }) -
        files.begin())};

    VerticalSpace(0.3f);
    RenderReloadButton(Tab::kIncludeFiles);
    VerticalSpace(0.8f);

    TextColored(kHintText, UI::Translations::Get("obody_include_files_rule"));
    VerticalSpace(0.6f);
    TextColored(kGoldText, UI::Translations::Get("obody_include_files_ingame") + " " + Parser::InGameConfigFileName);

    if (inGame == files.size()) {
        TextColored(kHintText, UI::Translations::Get("obody_include_files_ingame_missing"));
    } else if (inGame + 1 < files.size()) {
        std::string after{UI::Translations::Get("obody_include_files_after_ingame")};

        for (std::size_t i{inGame + 1}; i < files.size(); ++i) {
            after += "\n" + files[i].name;
        }

        VerticalSpace(0.3f);
        TextColored(kWarningText, after);
    }

    VerticalSpace(0.8f);

    if (files.empty()) {
        TextColored(kHintText, UI::Translations::Get("obody_include_files_none"));
    } else {
        RenderIncludeFileTable(files, inGame);
    }
}

void UI::ConfigTabs::RenderViewer() {
    VerticalSpace(0.3f);
    RenderReloadButton(Tab::kViewer);
    VerticalSpace(0.8f);
    TextColored(kHintText, UI::Translations::Get("obody_config_tree_header"));
    VerticalSpace(0.3f);

    // Taken after the reload button, which can replace the config in use
    const auto config{Parser::JSONParser::GetInstance().GetConfig()};
    UI::JsonView::Render(config->presetDistributionConfig);
}

void UI::ConfigTabs::Reset() {
    g_result.clear();
    g_resultTab = Tab::kNone;
    g_confirmReroll = false;
    g_rerollResult.clear();
}

UI::ConfigTabs::ReloadResult UI::ConfigTabs::Reload() {
    const auto loaded{Parser::JSONParser::GetInstance().Load()};
    ReloadResult result{.failed = loaded.error.has_value(),
                        .filesSkipped = !loaded.error && !loaded.skippedFiles.empty()};

    result.message = loaded.error ? UI::Translations::Get("obody_reload_config_failed") + " " + *loaded.error
                                  : UI::Translations::Get("obody_reload_config_done");

    if (!loaded.error && !loaded.skippedFiles.empty()) {
        result.message += "\n" + UI::Translations::Get("obody_reload_config_skipped");

        for (const auto& file : loaded.skippedFiles) {
            result.message += "\n" + file;
        }
    }

    return result;
}
