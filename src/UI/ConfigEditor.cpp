#include "UI/ConfigEditor.h"

#include "JSONParser/JSONParser.h"
#include "SKSEMenuFramework.h"
#include "UI/ConfigTabs.h"
#include "UI/Lookups.h"
#include "UI/Translations.h"
#include "UI/Util.h"
#include "UI/Widgets.h"

namespace {
    using UI::Lookups::Item;
    using UI::Lookups::Kind;

    constexpr ImGuiMCP::ImVec4 kSuccessText{0.55f, 0.85f, 0.55f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kErrorText{1.0f, 0.45f, 0.4f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kWarningText{0.95f, 0.7f, 0.35f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kHintText{0.7f, 0.7f, 0.75f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kDimText{0.55f, 0.55f, 0.6f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kGoldText{0.96f, 0.84f, 0.58f, 1.0f};

    // What a rule is about, which decides its picker and how the config writes it
    enum class Target {
        kNpcRecord,
        kNpcName,
        kFaction,
        kPlugin,
        kRace,
        kClass,
        kArmorRecord,
        kArmorName,
        kKeyword,
        kPreset
    };

    // What a rule gives what it's about
    enum class Gives { kNothing, kPresets, kRefitPresets };

    // A kind of rule, as the editor shows it. Its hint is the translation of name + "_hint"
    struct Category {
        const char* group;
        const char* name;
        // The key, or the female one of a pair
        const char* femaleKey;
        // Null when one key is for both sexes
        const char* maleKey;
        Target target;
        Gives gives;
    };

    constexpr auto kPresetsFor{"obody_editor_group_presets"};
    constexpr auto kBlacklists{"obody_editor_group_blacklists"};
    constexpr auto kOutfits{"obody_editor_group_outfits"};

    // In the order the distribution checks them
    constexpr std::array<Category, 20> Categories{{
        {kPresetsFor, "obody_editor_npc_record", "npcFormID", nullptr, Target::kNpcRecord, Gives::kPresets},
        {kPresetsFor, "obody_editor_npc_name", "npc", nullptr, Target::kNpcName, Gives::kPresets},
        {kPresetsFor, "obody_editor_faction", "factionFemale", "factionMale", Target::kFaction, Gives::kPresets},
        {kPresetsFor, "obody_editor_class", "classFemale", "classMale", Target::kClass, Gives::kPresets},
        {kPresetsFor, "obody_editor_plugin", "npcPluginFemale", "npcPluginMale", Target::kPlugin, Gives::kPresets},
        {kPresetsFor, "obody_editor_race", "raceFemale", "raceMale", Target::kRace, Gives::kPresets},
        {kBlacklists, "obody_editor_bl_npc_record", "blacklistedNpcsFormID", nullptr, Target::kNpcRecord,
         Gives::kNothing},
        {kBlacklists, "obody_editor_bl_npc_name", "blacklistedNpcs", nullptr, Target::kNpcName, Gives::kNothing},
        {kBlacklists, "obody_editor_bl_plugin", "blacklistedNpcsPluginFemale", "blacklistedNpcsPluginMale",
         Target::kPlugin, Gives::kNothing},
        {kBlacklists, "obody_editor_bl_race", "blacklistedRacesFemale", "blacklistedRacesMale", Target::kRace,
         Gives::kNothing},
        {kBlacklists, "obody_editor_bl_class", "blacklistedClassesFemale", "blacklistedClassesMale", Target::kClass,
         Gives::kNothing},
        {kBlacklists, "obody_editor_bl_preset", "blacklistedPresetsFromRandomDistribution", nullptr, Target::kPreset,
         Gives::kNothing},
        {kOutfits, "obody_editor_orefit_off_record", "blacklistedOutfitsFromORefitFormID", nullptr,
         Target::kArmorRecord, Gives::kNothing},
        {kOutfits, "obody_editor_orefit_off_name", "blacklistedOutfitsFromORefit", nullptr, Target::kArmorName,
         Gives::kNothing},
        {kOutfits, "obody_editor_orefit_off_plugin", "blacklistedOutfitsFromORefitPlugin", nullptr, Target::kPlugin,
         Gives::kNothing},
        {kOutfits, "obody_editor_orefit_on_record", "outfitsForceRefitFormID", nullptr, Target::kArmorRecord,
         Gives::kNothing},
        {kOutfits, "obody_editor_orefit_on_name", "outfitsForceRefit", nullptr, Target::kArmorName, Gives::kNothing},
        {kOutfits, "obody_editor_refit_record", "refitOutfitPresetsFemaleFormID", "refitOutfitPresetsMaleFormID",
         Target::kArmorRecord, Gives::kRefitPresets},
        {kOutfits, "obody_editor_refit_name", "refitOutfitPresetsFemale", "refitOutfitPresetsMale", Target::kArmorName,
         Gives::kRefitPresets},
        {kOutfits, "obody_editor_refit_keyword", "refitOutfitKeywordsFemale", "refitOutfitKeywordsMale",
         Target::kKeyword, Gives::kNothing},
    }};

    // A rule of the config, as the editor lists it
    struct Row {
        Parser::EntryRef ref;
        std::string label;
        std::vector<std::string> presets;
        // The file it comes from
        std::string source;
        // The in-game file's rule replaces the one this file has
        std::string replaces;
        // It comes from the in-game file, so it can be edited
        bool inGame{false};
        // Another file has it, the in-game file removes it
        bool switchedOff{false};
    };

    struct State {
        // The in-game file was read, the copy being edited is below
        bool loaded{false};
        rapidjson::Document working;
        // The file as it was read, WriteInGameConfig refuses to write over another change
        std::string readText;
        // The in-game file couldn't be read, or the files couldn't be merged
        std::string error;
        // The config the rows were made with
        std::uint32_t generation{0};
        // By key
        std::unordered_map<std::string, std::vector<Row>> rows;

        std::size_t category{0};
        bool female{true};

        // The rule being added or edited
        std::optional<Item> target;
        std::vector<std::string> presets;
        // The target is an existing rule's, it can't be changed
        bool editing{false};

        // What the last change or save did, shown until the menu is opened again
        std::string message;
        bool failed{false};
    };
    State g_state;

    // The picker lists' search, there's one list open at a time
    char g_search[128]{};

    // The items matching the search, made again when it changes
    struct Filtered {
        Kind kind{Kind::kTotal};
        std::string search;
        std::vector<const Item*> items;
    };
    Filtered g_filtered;

    std::string Translate(const char* a_key) { return UI::Translations::Get(a_key); }

    // A translated text with a_value in place of its "{}"
    std::string Translate(const char* a_key, const std::string_view a_value) {
        auto text{Translate(a_key)};
        boost::algorithm::replace_first(text, "{}", a_value);

        return text;
    }

    void TextColored(const ImGuiMCP::ImVec4& a_color, const std::string& a_text) {
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, a_color);
        ImGuiMCP::TextWrapped("%s", a_text.c_str());
        ImGuiMCP::PopStyleColor();
    }

    void VerticalSpace(const float a_lines) {
        ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, ImGuiMCP::GetTextLineHeight() * a_lines});
    }

    void SectionHeader(const std::string& a_text) {
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kGoldText);
        ImGuiMCP::SetWindowFontScale(1.15f);
        ImGuiMCP::TextUnformatted(a_text.c_str());
        ImGuiMCP::SetWindowFontScale(1.0f);
        ImGuiMCP::PopStyleColor();
        VerticalSpace(0.3f);
    }

    // Presets as the preset menu shows them, without leading and trailing whitespace
    std::string JoinPresets(const std::vector<std::string>& a_presets) {
        std::string text;

        for (const auto& preset : a_presets) {
            text += (text.empty() ? "" : ", ") + UI::Util::MakePresetOrder(preset).displayName;
        }

        return text;
    }

    bool IsRecord(const Target a_target) { return a_target == Target::kNpcRecord || a_target == Target::kArmorRecord; }

    const char* KeyFor(const Category& a_category, const bool a_female) {
        return a_category.maleKey && !a_female ? a_category.maleKey : a_category.femaleKey;
    }

    std::string JsonKeys(const Category& a_category) {
        return a_category.maleKey ? std::format("{}, {}", a_category.femaleKey, a_category.maleKey)
                                  : std::string{a_category.femaleKey};
    }

    Kind TargetKind(const Target a_target) {
        switch (a_target) {
            case Target::kNpcRecord:
                return Kind::kNpcRecord;
            case Target::kNpcName:
                return Kind::kNpcName;
            case Target::kFaction:
                return Kind::kFaction;
            case Target::kPlugin:
                return Kind::kPlugin;
            case Target::kRace:
                return Kind::kRace;
            case Target::kClass:
                return Kind::kClass;
            case Target::kArmorRecord:
                return Kind::kArmorRecord;
            case Target::kArmorName:
                return Kind::kArmorName;
            case Target::kKeyword:
                return Kind::kKeyword;
            case Target::kPreset:
                return Kind::kPreset;
        }

        return Kind::kPreset;
    }

    Kind PresetKind(const Gives a_gives, const bool a_female) {
        if (a_gives == Gives::kRefitPresets) return a_female ? Kind::kRefitFemale : Kind::kRefitMale;

        return a_female ? Kind::kPresetFemale : Kind::kPresetMale;
    }

    std::vector<std::string> PresetNames(const rapidjson::Value& a_value) {
        std::vector<std::string> names;

        if (a_value.IsString()) {
            names.emplace_back(a_value.GetString(), a_value.GetStringLength());
        } else if (a_value.IsArray()) {
            for (const auto& preset : a_value.GetArray()) {
                if (preset.IsString()) {
                    names.emplace_back(preset.GetString(), preset.GetStringLength());
                }
            }
        }

        return names;
    }

    std::string EntryLabel(const Target a_target, const Parser::EntryRef& a_ref) {
        if (IsRecord(a_target)) {
            auto* const dataHandler{RE::TESDataHandler::GetSingleton()};
            const auto* const file{dataHandler->LookupModByName(a_ref.plugin)};
            RE::FormID formID{};
            const auto* const end{a_ref.entry.data() + a_ref.entry.size()};
            const auto [last, error]{std::from_chars(a_ref.entry.data(), end, formID, 16)};
            const auto* const form{
                file && error == std::errc{} && last == end
                    ? dataHandler->LookupForm(formID & (file->IsLight() ? 0xFFFu : 0xFFFFFFu), a_ref.plugin)
                    : nullptr};

            if (!form) {
                return std::format("{}, {} {}", a_ref.plugin, a_ref.entry, Translate("obody_editor_not_found"));
            }

            const std::string_view name{form->GetName() ? form->GetName() : ""};

            return std::format("{} ({}, {})", name.empty() ? "?" : name, a_ref.plugin, a_ref.entry);
        }

        if (a_target == Target::kFaction || a_target == Target::kRace || a_target == Target::kClass) {
            if (const auto* const form{RE::TESForm::LookupByEditorID(a_ref.entry)};
                form && form->GetName() && *form->GetName() && a_ref.entry != form->GetName()) {
                return std::format("{} ({})", a_ref.entry, form->GetName());
            }
        }

        return a_ref.entry;
    }

    template <typename Visit>
    void ForEachEntry(const rapidjson::Value& a_config, const char* a_key, const Category& a_category,
                      const Visit& a_visit) {
        const auto member{a_config.FindMember(a_key)};

        if (member == a_config.MemberEnd()) return;

        const auto& value{member->value};
        constexpr const rapidjson::Value* kNoPresets{nullptr};
        const bool gives{a_category.gives != Gives::kNothing};
        const auto view = [](const rapidjson::Value& a_text) {
            return std::string_view{a_text.GetString(), a_text.GetStringLength()};
        };

        if (IsRecord(a_category.target)) {
            if (!value.IsObject()) return;

            for (auto plugin{value.MemberBegin()}; plugin != value.MemberEnd(); ++plugin) {
                if (gives && plugin->value.IsObject()) {
                    for (auto entry{plugin->value.MemberBegin()}; entry != plugin->value.MemberEnd(); ++entry) {
                        a_visit(view(plugin->name), view(entry->name), &entry->value);
                    }
                } else if (!gives && plugin->value.IsArray()) {
                    for (const auto& entry : plugin->value.GetArray()) {
                        if (entry.IsString()) a_visit(view(plugin->name), view(entry), kNoPresets);
                    }
                }
            }
        } else if (gives) {
            if (!value.IsObject()) return;

            for (auto entry{value.MemberBegin()}; entry != value.MemberEnd(); ++entry) {
                a_visit(std::string_view{}, view(entry->name), &entry->value);
            }
        } else if (value.IsArray()) {
            for (const auto& entry : value.GetArray()) {
                if (entry.IsString()) a_visit(std::string_view{}, view(entry), kNoPresets);
            }
        }
    }

    void Recompute() {
        rapidjson::Document others;
        rapidjson::Document full;
        Parser::EntrySources othersSources;
        Parser::EntrySources fullSources;

        g_state.rows.clear();
        g_state.generation = Parser::JSONParser::GetInstance().GetConfig()->generation;

        if (auto error{Parser::MergeConfigFiles(others, othersSources, nullptr)};
            error || (error = Parser::MergeConfigFiles(full, fullSources, &g_state.working))) {
            g_state.error = Translate("obody_editor_merge_failed") + " " + *error;
            return;
        }

        for (const auto& category : Categories) {
            for (const char* key : {category.femaleKey, category.maleKey}) {
                if (!key) continue;

                auto& rows{g_state.rows[key]};
                std::vector<std::pair<std::string, Row>> otherRows;
                std::unordered_set<std::string> shown;

                const auto makeRow = [&](const std::string_view a_plugin, const std::string_view a_entry,
                                         const rapidjson::Value* a_presets, const Parser::EntrySources& a_sources) {
                    Row row;
                    row.ref = {key, std::string{a_plugin}, std::string{a_entry}};
                    row.label = EntryLabel(category.target, row.ref);

                    if (a_presets) {
                        row.presets = PresetNames(*a_presets);
                    }

                    const auto source{a_sources.find(Parser::EntrySourceId(row.ref))};
                    row.source = source != a_sources.end() ? source->second : "?";

                    return row;
                };

                ForEachEntry(others, key, category, [&](auto a_plugin, auto a_entry, const auto* a_presets) {
                    auto row{makeRow(a_plugin, a_entry, a_presets, othersSources)};
                    otherRows.emplace_back(Parser::EntrySourceId(row.ref), std::move(row));
                });

                ForEachEntry(full, key, category, [&](auto a_plugin, auto a_entry, const auto* a_presets) {
                    auto row{makeRow(a_plugin, a_entry, a_presets, fullSources)};
                    const auto id{Parser::EntrySourceId(row.ref)};

                    // Two ways of writing the same FormID are the same entry
                    if (!shown.insert(id).second) return;

                    row.inGame = row.source == Parser::InGameConfigFileName;

                    if (const auto other{std::ranges::find(otherRows, id, &decltype(otherRows)::value_type::first)};
                        row.inGame && other != otherRows.end()) {
                        row.replaces = other->second.source;
                    }

                    rows.push_back(std::move(row));
                });

                for (auto& [id, row] : otherRows) {
                    if (shown.insert(id).second) {
                        row.switchedOff = true;
                        rows.push_back(std::move(row));
                    }
                }
            }
        }
    }

    void ResetForm() {
        g_state.target.reset();
        g_state.presets.clear();
        g_state.editing = false;
    }

    // Reads the in-game file again
    void LoadWorkingCopy() {
        g_state.loaded = true;
        g_state.error.clear();

        if (auto error{Parser::ReadInGameConfig(g_state.working, g_state.readText)}) {
            g_state.error = Translate("obody_editor_read_failed") + " " + *error;
            g_state.rows.clear();
            g_state.generation = Parser::JSONParser::GetInstance().GetConfig()->generation;
            return;
        }

        Recompute();
    }

    // Makes a change to the in-game file and saves it right away, then the config is reloaded to use it. Nothing
    // changes when the file wouldn't be valid, or when something else wrote it since it was read
    bool Change(const std::function<void(rapidjson::Document&)>& a_change) {
        rapidjson::Document before;
        before.CopyFrom(g_state.working, before.GetAllocator());

        a_change(g_state.working);

        if (auto error{Parser::ValidateInGameConfig(g_state.working)}) {
            g_state.working.Swap(before);
            g_state.message = Translate("obody_editor_invalid") + " " + *error;
            g_state.failed = true;
            return false;
        }

        if (auto error{Parser::WriteInGameConfig(g_state.working, g_state.readText)}) {
            g_state.working.Swap(before);
            g_state.message = Translate("obody_editor_save_failed") + " " + *error;
            g_state.failed = true;

            // What's on the disk now is shown, for the next change
            g_state.loaded = false;
            return false;
        }

        // The rules list shows the change, the reload is only worth telling about when it went wrong
        const auto reload{UI::ConfigTabs::Reload()};
        g_state.failed = reload.failed;
        g_state.message = reload.failed || reload.filesSkipped ? reload.message : std::string{};

        // Read again, as it was written
        g_state.loaded = false;

        return true;
    }

    const std::vector<const Item*>& Filter(const Kind a_kind) {
        if (g_filtered.kind == a_kind && g_filtered.search == g_search) return g_filtered.items;

        const auto query{UI::Util::ToLowerWide(UI::Util::Utf8ToWide(g_search))};

        g_filtered = {a_kind, g_search, {}};

        for (const auto& item : UI::Lookups::Get(a_kind)) {
            if (query.empty() || item.search.find(query) != std::wstring::npos) {
                g_filtered.items.push_back(&item);
            }
        }

        return g_filtered.items;
    }

    // The searchable list inside a picker's popup: the NPC picked in the menu's items first, then all of them.
    // Returns the item clicked
    const Item* RenderPickerList(const Kind a_kind, RE::Actor* a_actor,
                                 const std::function<bool(const Item&)>& a_ticked, const bool a_keepOpen) {
        const Item* clicked{nullptr};
        const auto flags{a_keepOpen ? ImGuiMCP::ImGuiSelectableFlags_DontClosePopups : 0};

        const auto pickerItem = [&](const Item& a_item) {
            const bool ticked{a_ticked(a_item)};

            if (ticked) {
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kGoldText);
            }

            if (ImGuiMCP::Selectable(a_item.label.c_str(), ticked, flags)) {
                clicked = &a_item;
            }

            if (ticked) {
                ImGuiMCP::PopStyleColor();
            }
        };

        const bool appearing{ImGuiMCP::IsWindowAppearing()};

        if (appearing) {
            g_search[0] = '\0';
            g_filtered.kind = Kind::kTotal;
            ImGuiMCP::SetKeyboardFocusHere();
        }

        ImGuiMCP::SetNextItemWidth(-FLT_MIN);
        ImGuiMCP::InputTextWithHint("##OBodyPickerSearch", Translate("obody_search_hint").c_str(), g_search,
                                    sizeof(g_search));

        bool headed{false};

        if (g_search[0] == '\0') {
            if (const auto fromActor{UI::Lookups::FromActor(a_kind, a_actor)}; !fromActor.empty()) {
                VerticalSpace(0.2f);
                TextColored(kGoldText, Translate("obody_editor_from_npc", a_actor->GetDisplayFullName()));
                ImGuiMCP::Indent();

                for (std::size_t i{0}; i < fromActor.size(); ++i) {
                    ImGuiMCP::PushID(static_cast<int>(100000 + i));
                    pickerItem(*fromActor[i]);
                    ImGuiMCP::PopID();
                }

                ImGuiMCP::Unindent();
                VerticalSpace(0.4f);
                ImGuiMCP::Separator();
                VerticalSpace(0.3f);
                TextColored(kGoldText, Translate("obody_editor_all"));
                headed = true;
            }
        }

        const bool searchChanged{g_filtered.kind != a_kind || g_filtered.search != g_search};
        const auto& items{Filter(a_kind)};

        if (headed) {
            ImGuiMCP::Indent();
        }

        if (items.empty()) {
            TextColored(kHintText, Translate("obody_editor_nothing_found"));
        }

        // Only the visible rows are drawn, there can be thousands
        if (ImGuiMCP::BeginChild("##OBodyPickerItems",
                                 ImGuiMCP::ImVec2{0.0f, ImGuiMCP::GetTextLineHeightWithSpacing() * 12.0f},
                                 ImGuiMCP::ImGuiChildFlags_None, ImGuiMCP::ImGuiWindowFlags_NavFlattened)) {
            if (appearing || searchChanged) {
                ImGuiMCP::SetScrollY(0.0f);
            }

            auto* const clipper{ImGuiMCP::ImGuiListClipperManager::Create()};
            ImGuiMCP::ImGuiListClipperManager::Begin(clipper, static_cast<int>(items.size()), -1.0f);

            while (ImGuiMCP::ImGuiListClipperManager::Step(clipper)) {
                for (int i = clipper->DisplayStart; i < clipper->DisplayEnd; ++i) {
                    ImGuiMCP::PushID(i);
                    pickerItem(*items[static_cast<std::size_t>(i)]);
                    ImGuiMCP::PopID();
                }
            }

            ImGuiMCP::ImGuiListClipperManager::End(clipper);
            ImGuiMCP::ImGuiListClipperManager::Destroy(clipper);
        }
        ImGuiMCP::EndChild();

        if (headed) {
            ImGuiMCP::Unindent();
        }

        // A click in the child list doesn't close the popup by itself
        if (clicked && !a_keepOpen) {
            ImGuiMCP::CloseCurrentPopup();
        }

        return clicked;
    }

    // Picks one item, returns it when it's picked
    const Item* PickOne(const char* a_id, const Kind a_kind, const std::string& a_shown, RE::Actor* a_actor) {
        const Item* picked{nullptr};
        const auto preview{a_shown.empty() ? Translate("obody_editor_pick") : a_shown};

        ImGuiMCP::SetNextItemWidth(-FLT_MIN);

        if (ImGuiMCP::BeginCombo(a_id, preview.c_str(), ImGuiMCP::ImGuiComboFlags_HeightLarge)) {
            picked =
                RenderPickerList(a_kind, a_actor, [&](const Item& a_item) { return a_item.label == a_shown; }, false);
            ImGuiMCP::EndCombo();
        }

        return picked;
    }

    // Ticks presets, the list stays open
    void PickPresets(const char* a_id, const Kind a_kind, std::vector<std::string>& a_presets, RE::Actor* a_actor) {
        const auto preview{a_presets.empty() ? Translate("obody_editor_pick_presets") : JoinPresets(a_presets)};

        ImGuiMCP::SetNextItemWidth(-FLT_MIN);

        if (ImGuiMCP::BeginCombo(a_id, preview.c_str(), ImGuiMCP::ImGuiComboFlags_HeightLarge)) {
            const auto ticked = [&](const Item& a_item) { return std::ranges::contains(a_presets, a_item.value); };

            if (const auto* const clicked{RenderPickerList(a_kind, a_actor, ticked, true)}) {
                if (const auto found{std::ranges::find(a_presets, clicked->value)}; found != a_presets.end()) {
                    a_presets.erase(found);
                } else {
                    a_presets.push_back(clicked->value);
                }
            }

            ImGuiMCP::EndCombo();
        }
    }

    void RenderCategoryPicker() {
        const auto& current{Categories[g_state.category]};
        const auto preview{std::format("{}: {}", Translate(current.group), Translate(current.name))};

        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted(Translate("obody_editor_rules_for").c_str());
        ImGuiMCP::SameLine();
        ImGuiMCP::SetNextItemWidth(-FLT_MIN);

        if (!ImGuiMCP::BeginCombo("##OBodyEditorCategory", preview.c_str(), ImGuiMCP::ImGuiComboFlags_HeightLarge)) {
            return;
        }

        const char* group{nullptr};

        for (std::size_t i{0}; i < Categories.size(); ++i) {
            const auto& category{Categories[i]};
            const bool selected{i == g_state.category};

            if (group != category.group) {
                group = category.group;

                if (i != 0) {
                    VerticalSpace(0.3f);
                }

                TextColored(kGoldText, Translate(group));
            }

            ImGuiMCP::PushID(static_cast<int>(i));
            ImGuiMCP::Indent();

            if (ImGuiMCP::Selectable(Translate(category.name).c_str(), selected) && !selected) {
                g_state.category = i;
                g_state.message.clear();
                ResetForm();
            }

            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("%s %s", Translate("obody_editor_json_key").c_str(), JsonKeys(category).c_str());
            }

            if (selected) {
                ImGuiMCP::SetItemDefaultFocus();
            }

            ImGuiMCP::Unindent();
            ImGuiMCP::PopID();
        }

        ImGuiMCP::EndCombo();
    }

    void RenderSexChoice(const Category& a_category) {
        const auto choose = [&](const char* a_label, const bool a_female) {
            if (ImGuiMCP::RadioButton(Translate(a_label).c_str(), g_state.female == a_female) &&
                g_state.female != a_female) {
                g_state.female = a_female;
                g_state.presets.clear();

                // The rule being edited is the other key's
                if (a_category.maleKey && g_state.editing) {
                    ResetForm();
                }
            }
        };

        choose("obody_editor_female", true);
        ImGuiMCP::SameLine(0.0f, ImGuiMCP::GetStyle()->ItemSpacing.x * 4.0f);
        choose("obody_editor_male", false);
    }

    void RenderRules(const Category& a_category) {
        const auto& rows{g_state.rows[KeyFor(a_category, g_state.female)]};
        const bool gives{a_category.gives != Gives::kNothing};
        constexpr auto flags{ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_BordersInnerV |
                             ImGuiMCP::ImGuiTableFlags_BordersOuterH | ImGuiMCP::ImGuiTableFlags_SizingStretchProp |
                             ImGuiMCP::ImGuiTableFlags_ScrollY};

        if (rows.empty()) {
            TextColored(kHintText, Translate("obody_editor_no_rules"));
            return;
        }

        // Done once the table is drawn, a change makes the rows again
        std::function<void()> action;

        const float lineHeight{ImGuiMCP::GetTextLineHeightWithSpacing()};
        const float rowHeight{lineHeight * (gives ? 2.4f : 2.2f)};
        const float height{
            (std::min)(lineHeight * 1.5f + rowHeight * static_cast<float>(rows.size()), lineHeight * 15.0f)};

        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_CellPadding, ImGuiMCP::ImVec2{8.0f, 5.0f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_TableHeaderBg, UI::Widgets::TableHeaderBackground);

        if (ImGuiMCP::BeginTable("##OBodyEditorRules", 3, flags, ImGuiMCP::ImVec2{0.0f, height})) {
            ImGuiMCP::TableSetupColumn(Translate("obody_editor_column_rule").c_str(), 0, 4.0f);
            ImGuiMCP::TableSetupColumn(Translate("obody_editor_column_from").c_str(), 0, 2.2f);
            ImGuiMCP::TableSetupColumn("", 0, 1.6f);
            ImGuiMCP::TableSetupScrollFreeze(0, 1);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, UI::Widgets::TableHeaderText);
            ImGuiMCP::TableHeadersRow();
            ImGuiMCP::PopStyleColor();

            for (std::size_t i{0}; i < rows.size(); ++i) {
                const auto& row{rows[i]};

                ImGuiMCP::PushID(static_cast<int>(i));
                ImGuiMCP::TableNextRow();
                ImGuiMCP::TableNextColumn();

                if (row.switchedOff) {
                    TextColored(kDimText, row.label + " " + Translate("obody_editor_switched_off"));
                } else {
                    ImGuiMCP::TextWrapped("%s", row.label.c_str());
                }

                if (!row.presets.empty()) {
                    TextColored(row.switchedOff ? kDimText : kGoldText, JoinPresets(row.presets));
                }

                if (!row.replaces.empty()) {
                    TextColored(kDimText, Translate("obody_editor_replaces", row.replaces));
                }

                ImGuiMCP::TableNextColumn();
                TextColored(row.inGame ? kGoldText : kDimText, row.source);

                ImGuiMCP::TableNextColumn();

                const auto button = [&](const char* a_label, const char* a_tooltip) {
                    const bool clicked{ImGuiMCP::SmallButton(Translate(a_label).c_str())};

                    if (a_tooltip && ImGuiMCP::IsItemHovered()) {
                        ImGuiMCP::SetTooltip("%s", Translate(a_tooltip).c_str());
                    }

                    return clicked;
                };

                const auto edit = [row] {
                    g_state.target = Item{row.label, row.ref.entry, row.ref.plugin, {}, g_state.female};
                    g_state.presets = row.presets;
                    g_state.editing = true;
                    g_state.message.clear();
                };

                if (row.inGame) {
                    if (gives && button("obody_editor_edit", nullptr)) {
                        action = edit;
                    }

                    if (button("obody_editor_delete", row.replaces.empty() ? nullptr : "obody_editor_delete_hint")) {
                        action = [ref{row.ref}] {
                            Change(
                                [&](rapidjson::Document& a_config) { Parser::InGameEdit::DeleteEntry(a_config, ref); });
                        };
                    }
                } else if (row.switchedOff) {
                    if (button("obody_editor_switch_on", nullptr)) {
                        action = [ref{row.ref}] {
                            Change([&](rapidjson::Document& a_config) {
                                Parser::InGameEdit::DeleteRemoval(a_config, ref);
                            });
                        };
                    }
                } else {
                    if (gives && button("obody_editor_change", "obody_editor_change_hint")) {
                        action = edit;
                    }

                    if (button("obody_editor_switch_off", "obody_editor_switch_off_hint")) {
                        action = [ref{row.ref}] {
                            Change(
                                [&](rapidjson::Document& a_config) { Parser::InGameEdit::AddRemoval(a_config, ref); });
                        };
                    }
                }

                ImGuiMCP::PopID();
            }

            ImGuiMCP::EndTable();
        }

        ImGuiMCP::PopStyleColor();
        ImGuiMCP::PopStyleVar();

        if (action) {
            action();
        }
    }

    void AddRule(const Category& a_category) {
        const bool gives{a_category.gives != Gives::kNothing};

        if (!g_state.target) {
            g_state.message = Translate("obody_editor_need_target");
            g_state.failed = true;
            return;
        }

        if (gives && g_state.presets.empty()) {
            g_state.message = Translate("obody_editor_need_presets");
            g_state.failed = true;
            return;
        }

        // The schema doesn't take names with trailing or leading whitespace
        if (const auto& value{g_state.target->value}; UI::Util::TrimWhitespace(value).size() != value.size()) {
            g_state.message = Translate("obody_editor_untrimmed");
            g_state.failed = true;
            return;
        }

        const Parser::EntryRef ref{KeyFor(a_category, g_state.female), g_state.target->plugin, g_state.target->value};
        const auto presets{g_state.presets};

        const bool changed{Change([&](rapidjson::Document& a_config) {
            Parser::InGameEdit::SetEntry(a_config, ref, gives ? &presets : nullptr);

            // A rule that was switched off is on again
            Parser::InGameEdit::DeleteRemoval(a_config, ref);
        })};

        if (changed) {
            ResetForm();
        }
    }

    void RenderRuleForm(const Category& a_category, RE::Actor* a_actor) {
        const bool gives{a_category.gives != Gives::kNothing};
        const auto targetLabel{Translate("obody_editor_target")};
        const auto presetsLabel{Translate("obody_editor_presets")};
        const float labelWidth{(std::max)(ImGuiMCP::CalcTextSize(targetLabel.c_str()).x,
                                          gives ? ImGuiMCP::CalcTextSize(presetsLabel.c_str()).x : 0.0f) +
                               ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f};

        SectionHeader(Translate(g_state.editing ? "obody_editor_edit_title" : "obody_editor_add_title"));

        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted(targetLabel.c_str());
        ImGuiMCP::SameLine(labelWidth);

        if (g_state.editing) {
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(g_state.target->label.c_str());
        } else if (const auto* const picked{PickOne("##OBodyEditorTarget", TargetKind(a_category.target),
                                                    g_state.target ? g_state.target->label : "", a_actor)}) {
            g_state.target = *picked;

            // An NPC's presets are the ones of their sex
            if (a_category.target == Target::kNpcRecord && gives && g_state.female != picked->female) {
                g_state.female = picked->female;
                g_state.presets.clear();
            }
        }

        if (gives) {
            VerticalSpace(0.2f);
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(presetsLabel.c_str());
            ImGuiMCP::SameLine(labelWidth);
            PickPresets("##OBodyEditorPresets", PresetKind(a_category.gives, g_state.female), g_state.presets, a_actor);
        }

        VerticalSpace(0.4f);

        if (ImGuiMCP::Button(Translate(g_state.editing ? "obody_editor_save_rule" : "obody_editor_add").c_str())) {
            AddRule(a_category);
        }

        if (g_state.editing || g_state.target || !g_state.presets.empty()) {
            ImGuiMCP::SameLine(0.0f, ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f);

            if (ImGuiMCP::Button(Translate("obody_editor_cancel").c_str())) {
                ResetForm();
            }
        }
    }

    void RenderMessage() {
        if (!g_state.message.empty()) {
            TextColored(g_state.failed ? kErrorText : kSuccessText, g_state.message);
        }
    }
}  // namespace

void UI::ConfigEditor::Render(RE::Actor* a_actor) {
    const auto generation{Parser::JSONParser::GetInstance().GetConfig()->generation};

    // A reload of the config may come from a change of any config file, the in-game one included
    if (!g_state.loaded || generation != g_state.generation) {
        LoadWorkingCopy();
    }

    const auto& category{Categories[g_state.category]};

    VerticalSpace(0.5f);
    RenderCategoryPicker();
    VerticalSpace(0.3f);
    TextColored(kHintText, Translate((std::string{category.name} + "_hint").c_str()));
    VerticalSpace(0.4f);
    TextColored(kDimText, std::format("{} {}", Translate("obody_editor_json_key"), KeyFor(category, g_state.female)));
    VerticalSpace(0.4f);

    if (category.maleKey || category.gives != Gives::kNothing) {
        VerticalSpace(0.2f);
        RenderSexChoice(category);
    }

    VerticalSpace(0.6f);

    if (!g_state.error.empty()) {
        TextColored(kErrorText, g_state.error);
        return;
    }

    RenderRules(category);

    VerticalSpace(0.8f);
    ImGuiMCP::Separator();
    VerticalSpace(0.6f);

    RenderRuleForm(category, a_actor);

    VerticalSpace(0.8f);
    RenderMessage();
}

void UI::ConfigEditor::Reset() {
    g_state.message.clear();
    g_search[0] = '\0';
}
