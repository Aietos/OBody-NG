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

    constexpr ImGuiMCP::ImVec4 kRuleName{0.90f, 0.89f, 0.86f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kRulePresets{0.80f, 0.74f, 0.62f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kRuleLine{0.30f, 0.28f, 0.25f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kGroupHeader{0.62f, 0.75f, 0.88f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kRuleButton{0.16f, 0.15f, 0.14f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kRuleButtonHovered{0.27f, 0.24f, 0.20f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kRuleButtonActive{0.33f, 0.29f, 0.23f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kRuleButtonBorder{0.55f, 0.46f, 0.30f, 0.55f};
    constexpr ImGuiMCP::ImVec4 kRemoveButtonHovered{0.42f, 0.17f, 0.15f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kRemoveButtonActive{0.52f, 0.20f, 0.17f, 1.0f};

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
        // The label in two parts, as the rules list shows it, see EntryParts
        std::string name;
        std::string detail;
        // The name, for the alphabetical order
        std::wstring sortName;
        // An NPC record's sex, see NpcSexShown. Unknown when the record isn't in the load order
        std::optional<bool> female;
        std::vector<std::string> presets;
        // The file it comes from
        std::string source;
        // The in-game file's rule replaces the one this file has
        std::string replaces;
        // It comes from the in-game file, so it can be edited
        bool inGame{false};
        // Another file has it, the in-game file removes it
        bool switchedOff{false};
        // What the rules search looks in: the label, the presets and the file, in lower case
        std::wstring search;
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

        // The rule being added or edited, one rule per target, all with the same presets, in the order they were
        // picked
        std::vector<Item> targets;
        std::vector<std::string> presets;
        // The target is an existing rule's, it can't be changed
        bool editing{false};

        // What the last change or save did, shown until the menu is opened again
        std::string message;
        bool failed{false};
        // The rule (EntrySourceId) or plugin group (PluginGroupId) of the rules list it's about, shown under it there.
        // Empty for the form above the list
        std::string messageAt;
    };
    State g_state;

    // The picker lists' search, there's one list open at a time
    char g_search[128]{};
    // The rules table's search, cleared when another category is picked
    char g_ruleSearch[128]{};

    // A rule of an ordered category being dragged, nothing is saved until it's let go of
    struct RuleDrag {
        std::size_t from;
        std::string key;
        std::vector<std::size_t> shownOrder;
        // Where the mouse holds the rule, from its top, and how tall the rule is
        float grab{0.0f};
        float height{0.0f};
        // How far up and down it can be drawn, from the top of the list: its plugin's group, or the whole list
        float minTop{-FLT_MAX};
        float maxBottom{FLT_MAX};
    };
    std::optional<RuleDrag> g_ruleDrag;

    // A plugin's group of outfits being dragged by its header, nothing is saved until it's let go of
    struct GroupDrag {
        std::string key;
        std::string plugin;
        // All the plugins, in the order shown
        std::vector<std::string> shownOrder;
        // Like RuleDrag
        float grab{0.0f};
        float height{0.0f};
        float minTop{-FLT_MAX};
        float maxBottom{FLT_MAX};
    };
    std::optional<GroupDrag> g_groupDrag;

    // The groups of rules collapsed to their header
    // This makes it easier to reorder long lists of plugins
    std::unordered_set<std::string> g_collapsed;

    // Changes when groups are folded or unfolded, the rules below them jump to their new places instead of sliding
    std::uint32_t g_collapsedVersion{0};

    // The rule the last move put somewhere
    struct MovedFlash {
        std::string id;
        std::chrono::steady_clock::time_point at;
    };
    std::optional<MovedFlash> g_movedFlash;
    constexpr float kMovedFlashSeconds{1.0f};

    // Edit / Change of a rule puts it in the form: the menu scrolls up to it, and its title lights up for a moment
    bool g_scrollToForm{false};
    std::optional<std::chrono::steady_clock::time_point> g_formFlashAt;

    // The items matching the search, made again when it or the sex shown changes
    struct Filtered {
        Kind kind{Kind::kTotal};
        std::string search;
        std::optional<bool> female;
        std::uint32_t generation{0};
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

    std::string Translate(const char* a_key, const std::string_view a_first, const std::string_view a_second) {
        auto text{Translate(a_key, a_first)};
        boost::algorithm::replace_first(text, "{}", a_second);

        return text;
    }

    std::string PickedPreview(const std::vector<std::string>& a_labels, const char* a_nothingKey) {
        if (a_labels.empty()) return Translate(a_nothingKey);

        if (a_labels.size() == 1) return a_labels.front();

        return Translate("obody_editor_picked_count", std::to_string(a_labels.size()));
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
        UI::Widgets::SetTextScale(1.15f);
        ImGuiMCP::TextUnformatted(a_text.c_str());
        UI::Widgets::SetTextScale();
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

    // The rules whose order is their priority, the first one that fits is used: the refit ones, and the presets for
    // factions and plugins (an NPC can be in several factions, and added or changed by several plugins). They're
    // listed in the merged config's order instead of alphabetically, numbered, and can be moved
    bool IsOrdered(const Category& a_category) {
        if (a_category.gives == Gives::kRefitPresets || a_category.target == Target::kKeyword) return true;

        return a_category.gives == Gives::kPresets &&
               (a_category.target == Target::kFaction || a_category.target == Target::kPlugin);
    }

    // "Specific NPCs" that get presets have one key for both sexes, but an NPC's presets are the ones of their sex: the
    // Female / Male choice shows the NPCs of that sex, in the picker and in the rules. Nullopt for the other categories
    std::optional<bool> NpcSexShown(const Category& a_category) {
        if (a_category.target != Target::kNpcRecord || a_category.gives == Gives::kNothing) return std::nullopt;

        return g_state.female;
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

    // Explains what a rule is for
    std::pair<std::string, std::string> EntryParts(const Target a_target, const Parser::EntryRef& a_ref) {
        if (IsRecord(a_target)) {
            const auto* const form{UI::Lookups::FindRecord(a_ref.plugin, a_ref.entry)};
            const std::string_view name{form && form->GetName() ? form->GetName() : ""};
            const auto where{std::format("{}, {}", a_ref.plugin, a_ref.entry)};

            if (!form) return {where, Translate("obody_editor_not_found")};

            return {std::string{name.empty() ? "?" : name}, where};
        }

        if (a_target == Target::kFaction || a_target == Target::kRace || a_target == Target::kClass) {
            if (const auto* const form{RE::TESForm::LookupByEditorID(a_ref.entry)};
                form && form->GetName() && *form->GetName() && a_ref.entry != form->GetName()) {
                return {a_ref.entry, form->GetName()};
            }
        }

        return {a_ref.entry, {}};
    }

    std::string EntryLabel(const Target a_target, const Parser::EntryRef& a_ref) {
        const auto [name, detail]{EntryParts(a_target, a_ref)};

        if (detail.empty()) return name;

        // "(not in your load order)" has its own parentheses
        return detail.starts_with('(') ? std::format("{} {}", name, detail) : std::format("{} ({})", name, detail);
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
                    std::tie(row.name, row.detail) = EntryParts(category.target, row.ref);
                    row.sortName = UI::Util::Utf8ToWide(row.name);

                    if (category.target == Target::kNpcRecord) {
                        const auto* const form{UI::Lookups::FindRecord(row.ref.plugin, row.ref.entry)};

                        if (const auto* const npc{form ? form->As<RE::TESNPC>() : nullptr}) {
                            row.female = npc->GetSex() == RE::SEX::kFemale;
                        }
                    }

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

                // Alphabetical, the switched off ones among the others. Not the refit rules, see IsOrdered
                if (!IsOrdered(category)) {
                    std::ranges::stable_sort(rows, [](const Row& a_left, const Row& a_right) {
                        if (const int order{UI::Util::CompareLinguistic(a_left.sortName, a_right.sortName)};
                            order != 0) {
                            return order < 0;
                        }

                        return a_left.detail < a_right.detail;
                    });
                }

                for (auto& row : rows) {
                    std::string text{row.label + "\n" + row.source};

                    for (const auto& preset : row.presets) {
                        text += "\n" + preset;
                    }

                    row.search = UI::Util::ToLowerWide(UI::Util::Utf8ToWide(text));
                }
            }
        }
    }

    void ResetForm() {
        g_state.targets.clear();
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

    // a_female: only the NPC records of that sex, the Female / Male choice of "Specific NPCs"
    bool IsOfSex(const Item& a_item, const std::optional<bool> a_female) {
        return !a_female || a_item.female == *a_female;
    }

    const std::vector<const Item*>& Filter(const Kind a_kind, const std::optional<bool> a_female) {
        if (g_filtered.kind == a_kind && g_filtered.search == g_search && g_filtered.female == a_female &&
            g_filtered.generation == UI::Lookups::Generation()) {
            return g_filtered.items;
        }

        const auto query{UI::Util::ToLowerWide(UI::Util::Utf8ToWide(g_search))};

        g_filtered = {a_kind, g_search, a_female, UI::Lookups::Generation(), {}};

        for (const auto& item : UI::Lookups::Get(a_kind)) {
            if (IsOfSex(item, a_female) && (query.empty() || item.search.find(query) != std::wstring::npos)) {
                g_filtered.items.push_back(&item);
            }
        }

        return g_filtered.items;
    }

    constexpr float kPickedLines{4.0f};

    std::size_t g_pickedShown{0};

    void SetPickerPopupSize() {
        const float height{ImGuiMCP::GetMainViewport()->Size.y * 0.6f};

        ImGuiMCP::SetNextWindowSizeConstraints(ImGuiMCP::ImVec2{ImGuiMCP::CalcItemWidth(), height},
                                               ImGuiMCP::ImVec2{FLT_MAX, height});
    }

    // The searchable list inside a picker's popup
    const Item* RenderPickerList(const Kind a_kind, RE::Actor* a_actor,
                                 const std::function<bool(const Item&)>& a_ticked, const bool a_keepOpen,
                                 const std::vector<const Item*>& a_picked = {},
                                 const std::optional<bool> a_female = std::nullopt) {
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
        }

        const auto separate = [] {
            VerticalSpace(0.4f);
            ImGuiMCP::Separator();
            VerticalSpace(0.3f);
        };

        const float lineHeight{ImGuiMCP::GetTextLineHeightWithSpacing()};

        TextColored(kGoldText, Translate("obody_editor_picked", std::to_string(a_picked.size())));
        ImGuiMCP::Indent();

        if (ImGuiMCP::BeginChild("##OBodyPickerPicked", ImGuiMCP::ImVec2{0.0f, lineHeight * kPickedLines},
                                 ImGuiMCP::ImGuiChildFlags_None, ImGuiMCP::ImGuiWindowFlags_NavFlattened)) {
            if (a_picked.empty()) {
                TextColored(kDimText, Translate("obody_editor_nothing_picked"));
            }

            for (std::size_t i{0}; i < a_picked.size(); ++i) {
                ImGuiMCP::PushID(static_cast<int>(200000 + i));
                pickerItem(*a_picked[i]);
                ImGuiMCP::PopID();
            }

            // A new pick is added last, scrolled into view, so the click visibly did something
            if (appearing || a_picked.size() > g_pickedShown) {
                ImGuiMCP::SetScrollHereY(1.0f);
            }
        }
        ImGuiMCP::EndChild();

        g_pickedShown = a_picked.size();

        ImGuiMCP::Unindent();
        separate();

        UI::Widgets::SearchInput("##OBodyPickerSearch", g_search, sizeof(g_search), appearing);

        bool headed{false};

        if (g_search[0] == '\0') {
            auto fromActor{UI::Lookups::FromActor(a_kind, a_actor)};
            std::erase_if(fromActor, [&](const Item* a_item) { return !IsOfSex(*a_item, a_female); });

            if (!fromActor.empty()) {
                VerticalSpace(0.2f);
                TextColored(kGoldText, Translate("obody_editor_from_npc", a_actor->GetDisplayFullName()));
                ImGuiMCP::Indent();

                const float height{lineHeight * static_cast<float>((std::min)(fromActor.size(), std::size_t{5}))};

                if (ImGuiMCP::BeginChild("##OBodyPickerFromNpc", ImGuiMCP::ImVec2{0.0f, height},
                                         ImGuiMCP::ImGuiChildFlags_None, ImGuiMCP::ImGuiWindowFlags_NavFlattened)) {
                    for (std::size_t i{0}; i < fromActor.size(); ++i) {
                        ImGuiMCP::PushID(static_cast<int>(100000 + i));
                        pickerItem(*fromActor[i]);
                        ImGuiMCP::PopID();
                    }
                }
                ImGuiMCP::EndChild();

                ImGuiMCP::Unindent();
                headed = true;
            }

            if (headed) {
                separate();
                TextColored(kGoldText, Translate("obody_editor_all"));
            }
        }

        const bool searchChanged{g_filtered.kind != a_kind || g_filtered.search != g_search ||
                                 g_filtered.female != a_female};
        const auto& items{Filter(a_kind, a_female)};

        if (headed) {
            ImGuiMCP::Indent();
        }

        if (items.empty()) {
            TextColored(kHintText, Translate("obody_editor_nothing_found"));
        }

        // Only the visible rows are drawn, there can be thousands
        const float listHeight{(std::max)(ImGuiMCP::GetContentRegionAvail().y, lineHeight * 4.0f)};

        if (ImGuiMCP::BeginChild("##OBodyPickerItems", ImGuiMCP::ImVec2{0.0f, listHeight},
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

    bool IsSameItem(const Item& a_left, const Item& a_right) {
        return a_left.value == a_right.value && a_left.plugin == a_right.plugin;
    }

    void PickTargets(const char* a_id, const Kind a_kind, std::vector<Item>& a_targets, RE::Actor* a_actor,
                     const std::optional<bool> a_female) {
        std::optional<Item> clicked;
        std::vector<std::string> labels;

        for (const auto& target : a_targets) {
            labels.push_back(target.label);
        }

        const auto preview{PickedPreview(labels, "obody_editor_pick")};

        ImGuiMCP::SetNextItemWidth(-FLT_MIN);
        SetPickerPopupSize();

        if (UI::Widgets::BeginCombo(a_id, preview.c_str(), ImGuiMCP::ImGuiComboFlags_HeightLarge)) {
            const auto ticked = [&](const Item& a_item) {
                return std::ranges::any_of(a_targets,
                                           [&](const Item& a_target) { return IsSameItem(a_target, a_item); });
            };

            std::vector<const Item*> picked;

            for (const auto& target : a_targets) {
                picked.push_back(&target);
            }

            // A copy: the item clicked may be one of the targets, which is taken away below
            if (const auto* const item{RenderPickerList(a_kind, a_actor, ticked, true, picked, a_female)}) {
                clicked = *item;
            }

            if (clicked) {
                const auto same = [&](const Item& a_target) { return IsSameItem(a_target, *clicked); };

                if (const auto found{std::ranges::find_if(a_targets, same)}; found != a_targets.end()) {
                    a_targets.erase(found);
                } else {
                    a_targets.push_back(*clicked);
                }
            }

            ImGuiMCP::EndCombo();
        }
    }

    // Ticks presets, the list stays open
    void PickPresets(const char* a_id, const Kind a_kind, std::vector<std::string>& a_presets, RE::Actor* a_actor) {
        std::vector<std::string> labels;

        for (const auto& preset : a_presets) {
            labels.push_back(UI::Util::MakePresetOrder(preset).displayName);
        }

        const auto preview{PickedPreview(labels, "obody_editor_pick_presets")};

        ImGuiMCP::SetNextItemWidth(-FLT_MIN);
        SetPickerPopupSize();

        if (UI::Widgets::BeginCombo(a_id, preview.c_str(), ImGuiMCP::ImGuiComboFlags_HeightLarge)) {
            const auto ticked = [&](const Item& a_item) { return std::ranges::contains(a_presets, a_item.value); };

            // In the order they were picked
            std::vector<const Item*> picked;
            const auto& items{UI::Lookups::Get(a_kind)};

            for (const auto& preset : a_presets) {
                if (const auto item{std::ranges::find(items, preset, &Item::value)}; item != items.end()) {
                    picked.push_back(&*item);
                }
            }

            if (const auto* const clicked{RenderPickerList(a_kind, a_actor, ticked, true, picked)}) {
                const std::string value{clicked->value};

                if (const auto found{std::ranges::find(a_presets, value)}; found != a_presets.end()) {
                    a_presets.erase(found);
                } else {
                    a_presets.push_back(value);
                }
            }

            ImGuiMCP::EndCombo();
        }
    }

    // What was done in a list of picks
    struct PickedChange {
        enum class Kind { kRemove, kUp, kDown, kClear };

        Kind kind;
        std::size_t index{0};
    };

    template <typename T>
    void ApplyPickedChange(std::vector<T>& a_picks, const PickedChange& a_change) {
        const auto at{a_picks.begin() + static_cast<std::ptrdiff_t>(a_change.index)};

        switch (a_change.kind) {
            case PickedChange::Kind::kRemove:
                a_picks.erase(at);
                break;
            case PickedChange::Kind::kUp:
                std::iter_swap(at, at - 1);
                break;
            case PickedChange::Kind::kDown:
                std::iter_swap(at, at + 1);
                break;
            case PickedChange::Kind::kClear:
                a_picks.clear();
                break;
        }
    }

    // What was picked
    std::optional<PickedChange> RenderPickedList(const char* a_id, const std::vector<std::string>& a_labels,
                                                 const bool a_reorder) {
        static std::unordered_map<std::string, std::size_t> shownCounts;
        static std::unordered_map<std::string, std::optional<std::size_t>> movedTo;

        static std::unordered_map<std::string, float> slides;

        std::optional<PickedChange> change;
        auto& shown{shownCounts[a_id]};
        auto& moved{movedTo[a_id]};
        const float step{ImGuiMCP::GetTextLineHeightWithSpacing()};
        const float height{step * kPickedLines + ImGuiMCP::GetStyle()->WindowPadding.y * 2.0f};

        const auto slideKey = [&](const std::size_t a_index) { return std::format("{}|{}", a_id, a_labels[a_index]); };

        if (ImGuiMCP::BeginChild(a_id, ImGuiMCP::ImVec2{0.0f, height}, ImGuiMCP::ImGuiChildFlags_Border,
                                 ImGuiMCP::ImGuiWindowFlags_NavFlattened)) {
            for (std::size_t i{0}; i < a_labels.size(); ++i) {
                // Its place in the list, and where it's drawn while it slides there
                const auto slot{ImGuiMCP::GetCursorScreenPos()};
                float slide{0.0f};

                if (const auto found{slides.find(slideKey(i))}; found != slides.end()) {
                    found->second *= std::exp(-ImGuiMCP::GetIO()->DeltaTime * 18.0f);
                    slide = found->second;

                    if (std::abs(slide) < 0.5f) {
                        slides.erase(found);
                        slide = 0.0f;
                    }
                }

                if (slide != 0.0f) {
                    ImGuiMCP::SetCursorScreenPos(ImGuiMCP::ImVec2{slot.x, slot.y + slide});
                }

                ImGuiMCP::PushID(static_cast<int>(i));

                if (a_reorder) {
                    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImGuiMCP::ImVec2{0.0f, 0.0f});

                    ImGuiMCP::BeginDisabled(i == 0);
                    if (ImGuiMCP::ArrowButton("##up", ImGuiMCP::ImGuiDir_Up)) {
                        change = PickedChange{PickedChange::Kind::kUp, i};
                    }
                    ImGuiMCP::EndDisabled();

                    ImGuiMCP::SameLine();
                    ImGuiMCP::BeginDisabled(i + 1 == a_labels.size());
                    if (ImGuiMCP::ArrowButton("##down", ImGuiMCP::ImGuiDir_Down)) {
                        change = PickedChange{PickedChange::Kind::kDown, i};
                    }
                    ImGuiMCP::EndDisabled();

                    ImGuiMCP::PopStyleVar();
                    ImGuiMCP::SameLine();
                }

                if (ImGuiMCP::SmallButton("x")) {
                    change = PickedChange{PickedChange::Kind::kRemove, i};
                }

                ImGuiMCP::SameLine();
                ImGuiMCP::PopID();

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kGoldText);

                if (a_reorder) {
                    ImGuiMCP::Selectable(std::format("{}##pick", a_labels[i]).c_str());
                } else {
                    ImGuiMCP::TextUnformatted(a_labels[i].c_str());
                }

                ImGuiMCP::PopStyleColor();

                const bool active{ImGuiMCP::IsItemActive()};
                const bool hovered{ImGuiMCP::IsItemHovered()};
                const auto itemMin{ImGuiMCP::GetItemRectMin()};
                const auto itemMax{ImGuiMCP::GetItemRectMax()};

                if (moved == i && !ImGuiMCP::IsItemVisible()) {
                    ImGuiMCP::SetScrollHereY(0.5f);
                }

                if (a_reorder && active) {
                    ImGuiMCP::ImDrawListManager::AddRectFilled(
                        ImGuiMCP::GetWindowDrawList(), itemMin, itemMax,
                        ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{kGoldText.x, kGoldText.y, kGoldText.z, 0.18f}), 2.0f, 0);
                }

                const float mouseY{ImGuiMCP::GetMousePos().y};

                if (a_reorder && active && (mouseY < slot.y || mouseY >= slot.y + step)) {
                    const bool up{mouseY < slot.y};

                    if (up ? i > 0 : i + 1 < a_labels.size()) {
                        change = PickedChange{up ? PickedChange::Kind::kUp : PickedChange::Kind::kDown, i};
                    }
                } else if (a_reorder && hovered && !active) {
                    ImGuiMCP::SetTooltip("%s", Translate("obody_editor_drag_hint").c_str());
                }

                if (slide != 0.0f) {
                    ImGuiMCP::SetCursorScreenPos(slot);
                    ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, step - ImGuiMCP::GetStyle()->ItemSpacing.y});
                }
            }

            if (a_labels.size() > shown) {
                ImGuiMCP::SetScrollHereY(1.0f);
            }
        }
        ImGuiMCP::EndChild();

        shown = a_labels.size();
        moved.reset();

        if (change && (change->kind == PickedChange::Kind::kUp || change->kind == PickedChange::Kind::kDown)) {
            const bool up{change->kind == PickedChange::Kind::kUp};
            const auto other{up ? change->index - 1 : change->index + 1};

            moved = other;
            slides[slideKey(change->index)] += up ? step : -step;
            slides[slideKey(other)] += up ? -step : step;
        }

        return change;
    }

    void RenderCategoryPicker() {
        const auto& current{Categories[g_state.category]};
        const auto preview{std::format("{}: {}", Translate(current.group), Translate(current.name))};

        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted(Translate("obody_editor_rules_for").c_str());
        ImGuiMCP::SameLine();
        ImGuiMCP::SetNextItemWidth(-FLT_MIN);

        if (!UI::Widgets::BeginCombo("##OBodyEditorCategory", preview.c_str(), ImGuiMCP::ImGuiComboFlags_HeightLarge)) {
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
                g_ruleSearch[0] = '\0';
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

                if ((a_category.maleKey || NpcSexShown(a_category)) && g_state.editing) {
                    ResetForm();
                }

                if (a_category.target == Target::kNpcRecord && !g_state.editing) {
                    g_state.targets.clear();
                }
            }
        };

        choose("obody_editor_female", true);
        ImGuiMCP::SameLine(0.0f, ImGuiMCP::GetStyle()->ItemSpacing.x * 4.0f);
        choose("obody_editor_male", false);
    }

    // The plugins of the rules, in their order. Outfits listed by record are grouped by plugin
    std::vector<std::string> PluginOrder(const std::vector<const Row*>& a_order) {
        std::vector<std::string> plugins;

        for (const auto* const row : a_order) {
            if (plugins.empty() || plugins.back() != row->ref.plugin) {
                plugins.push_back(row->ref.plugin);
            }
        }

        return plugins;
    }

    // Puts the rules of an ordered category (IsOrdered) in a_desired's order. The rules from the first one that changes
    // place are taken out and added again to the in-game file, which puts them after the ones above. For outfits
    // listed by record, only the plugins whose rules or place change are written again, with the plugins after them
    bool SaveOrder(const Category& a_category, const std::vector<const Row*>& a_order,
                   const std::vector<const Row*>& a_desired, const bool a_plugins) {
        const auto* const key{KeyFor(a_category, g_state.female)};
        const bool gives{a_category.gives != Gives::kNothing};
        std::size_t first{0};

        while (first < a_order.size() && a_order[first] == a_desired[first]) {
            ++first;
        }

        if (first == a_order.size()) return false;

        // Outfits listed by record
        std::unordered_set<std::string> movedPlugins;
        std::unordered_set<std::string> changedPlugins;

        if (IsRecord(a_category.target)) {
            const auto before{PluginOrder(a_order)};
            const auto after{PluginOrder(a_desired)};

            // A plugin added again goes last
            movedPlugins.insert(std::ranges::mismatch(before, after).in2, after.end());

            for (std::size_t i{first}; i < a_order.size(); ++i) {
                if (a_order[i] != a_desired[i]) {
                    changedPlugins.insert(a_desired[i]->ref.plugin);
                }
            }
        }

        rapidjson::Document candidate;
        candidate.CopyFrom(g_state.working, candidate.GetAllocator());

        const auto takeOut = [&](const Row& a_row) {
            if (!a_row.inGame || !a_row.replaces.empty()) {
                Parser::InGameEdit::AddRemoval(candidate, a_row.ref);
            }

            Parser::InGameEdit::DeleteEntry(candidate, a_row.ref);
        };

        // A plugin that moves has all its rules taken out first
        for (std::size_t i{first}; i < a_desired.size(); ++i) {
            if (movedPlugins.contains(a_desired[i]->ref.plugin)) {
                takeOut(*a_desired[i]);
            }
        }

        // The others are taken out and added again one by one, so their plugin is never left empty
        for (std::size_t i{first}; i < a_desired.size(); ++i) {
            const auto& row{*a_desired[i]};
            const bool moved{movedPlugins.contains(row.ref.plugin)};

            if (IsRecord(a_category.target) && !moved && !changedPlugins.contains(row.ref.plugin)) continue;

            if (!moved) {
                takeOut(row);
            }

            Parser::InGameEdit::SetEntry(candidate, row.ref, gives ? &row.presets : nullptr);
        }

        const auto fail = [&](const std::string& a_message) {
            g_state.message = a_message;
            g_state.failed = true;

            return false;
        };

        rapidjson::Document merged;
        Parser::EntrySources sources;
        std::vector<std::string> order;
        std::unordered_set<std::string> seen;

        auto error{Parser::ValidateInGameConfig(candidate)};

        if (!error) {
            error = Parser::MergeConfigFiles(merged, sources, &candidate);
        }

        if (error) {
            logger::info("Config Editor: reordering {} failed: {}", key, *error);

            return fail(Translate(a_plugins ? "obody_editor_plugin_move_error" : "obody_editor_move_error", *error));
        }

        ForEachEntry(merged, key, a_category, [&](auto a_plugin, auto a_entry, const auto*) {
            if (auto id{Parser::EntrySourceId({key, std::string{a_plugin}, std::string{a_entry}})};
                seen.insert(id).second) {
                order.push_back(std::move(id));
            }
        });

        std::vector<std::string> expected;

        for (const auto* const row : a_desired) {
            expected.push_back(Parser::EntrySourceId(row->ref));
        }

        if (order != expected) {
            logger::info(
                "Config Editor: reordering {} would give another order than asked, nothing was saved. Asked: "
                "{}. Would be: {}",
                key, boost::algorithm::join(expected, ", "), boost::algorithm::join(order, ", "));

            return fail(Translate(a_plugins ? "obody_editor_plugin_move_failed" : "obody_editor_move_failed"));
        }

        return Change([&](rapidjson::Document& a_config) { a_config.CopyFrom(candidate, a_config.GetAllocator()); });
    }

    // Moves a rule of an ordered category (IsOrdered)
    void MoveRule(const Category& a_category, const std::vector<const Row*>& a_order, const std::size_t a_from,
                  const std::size_t a_to) {
        if (a_from == a_to || a_to >= a_order.size()) return;

        auto desired{a_order};
        const auto* const moved{desired[a_from]};
        desired.erase(desired.begin() + static_cast<std::ptrdiff_t>(a_from));
        desired.insert(desired.begin() + static_cast<std::ptrdiff_t>(a_to), moved);

        if (SaveOrder(a_category, a_order, desired, false)) {
            logger::info("Config Editor: rule '{}' of {} moved from place {} to {}", moved->name,
                         KeyFor(a_category, g_state.female), a_from + 1, a_to + 1);
            g_movedFlash = MovedFlash{Parser::EntrySourceId(moved->ref), std::chrono::steady_clock::now()};
        }
    }

    // The id a plugin's group header slides and flashes by
    std::string PluginGroupId(const std::string& a_plugin) { return "#plugin|" + a_plugin; }

    // Puts the outfits of the plugins in a_plugins' order, a_moved is the plugin that was moved
    void SavePluginOrder(const Category& a_category, const std::vector<const Row*>& a_order,
                         const std::vector<std::string>& a_plugins, const std::string& a_moved) {
        std::vector<const Row*> desired;

        for (const auto& plugin : a_plugins) {
            for (const auto* const row : a_order) {
                if (row->ref.plugin == plugin) {
                    desired.push_back(row);
                }
            }
        }

        if (desired.size() != a_order.size()) return;

        if (SaveOrder(a_category, a_order, desired, true)) {
            const auto place{std::ranges::find(a_plugins, a_moved) - a_plugins.begin() + 1};

            logger::info("Config Editor: plugin '{}' of {} moved to place {}", a_moved,
                         KeyFor(a_category, g_state.female), place);
            g_movedFlash = MovedFlash{PluginGroupId(a_moved), std::chrono::steady_clock::now()};
        }
    }

    // Moves the outfits of a plugin above or below the ones of the plugin next to it
    void MovePluginGroup(const Category& a_category, const std::vector<const Row*>& a_order,
                         const std::string& a_plugin, const bool a_up) {
        auto plugins{PluginOrder(a_order)};
        const auto at{static_cast<std::size_t>(std::ranges::find(plugins, a_plugin) - plugins.begin())};

        if (at >= plugins.size() || (a_up ? at == 0 : at + 1 >= plugins.size())) return;

        std::swap(plugins[at], plugins[a_up ? at - 1 : at + 1]);
        SavePluginOrder(a_category, a_order, plugins, a_plugin);
    }

    struct RuleRect {
        std::size_t position;
        const Row* row;
        float top;
        float bottom;
    };

    bool IsDragOf(const char* a_key, const std::vector<const Row*>& a_order) {
        return g_ruleDrag && g_ruleDrag->key == a_key && g_ruleDrag->shownOrder.size() == a_order.size() &&
               g_ruleDrag->from < a_order.size();
    }

    // Where the dragged rule (RuleDrag) or plugin group (GroupDrag) is drawn, following the mouse inside its bounds
    template <typename Drag>
    float DraggedTop(const Drag& a_drag, const float a_listTop) {
        const float top{a_listTop + a_drag.minTop};

        return std::clamp(ImGuiMCP::GetMousePos().y - a_drag.grab, top,
                          (std::max)(top, a_listTop + a_drag.maxBottom - a_drag.height));
    }

    std::function<void()> UpdateRuleDrag(const Category& a_category, const std::vector<const Row*>& a_order,
                                         const std::vector<RuleRect>& a_rects, const float a_listTop) {
        std::function<void()> move;
        const auto* const key{KeyFor(a_category, g_state.female)};

        if (!g_ruleDrag) return move;

        if (!IsDragOf(key, a_order)) {
            g_ruleDrag.reset();
            return move;
        }

        auto& drag{*g_ruleDrag};
        const auto from{drag.from};
        const auto& dragged{*a_order[from]};

        if (!ImGuiMCP::IsMouseDown(0)) {
            const auto to{static_cast<std::size_t>(std::ranges::find(drag.shownOrder, from) - drag.shownOrder.begin())};

            g_ruleDrag.reset();

            if (to != from) {
                logger::info("Config Editor: rule '{}' of {} dropped, from place {} to {}", dragged.name, key, from + 1,
                             to + 1);
                move = [&a_category, a_order, from, to] { MoveRule(a_category, a_order, from, to); };
            }

            return move;
        }

        const auto here{std::ranges::find(a_rects, from, &RuleRect::position)};

        if (here == a_rects.end()) return move;

        const auto canPass = [&](const RuleRect& a_other) {
            return !IsRecord(a_category.target) || a_other.row->ref.plugin == dragged.ref.plugin;
        };

        drag.height = here->bottom - here->top;
        drag.minTop = FLT_MAX;
        drag.maxBottom = -FLT_MAX;

        for (const auto& rect : a_rects) {
            if (canPass(rect)) {
                drag.minTop = (std::min)(drag.minTop, rect.top - a_listTop);
                drag.maxBottom = (std::max)(drag.maxBottom, rect.bottom - a_listTop);
            }
        }

        const float top{DraggedTop(drag, a_listTop)};
        const float bottom{top + drag.height};

        const auto moveNextTo = [&](const RuleRect& a_other, const bool a_after) {
            std::erase(drag.shownOrder, from);
            auto at{std::ranges::find(drag.shownOrder, a_other.position)};

            if (a_after) {
                ++at;
            }

            drag.shownOrder.insert(at, from);
        };

        if (here != a_rects.begin()) {
            if (const auto& previous{*(here - 1)}; canPass(previous) && top < (previous.top + previous.bottom) * 0.5f) {
                moveNextTo(previous, false);
                return move;
            }
        }

        if (here + 1 != a_rects.end()) {
            if (const auto& next{*(here + 1)}; canPass(next) && bottom > (next.top + next.bottom) * 0.5f) {
                moveNextTo(next, true);
            }
        }

        return move;
    }

    // Where a plugin's group (its header and its rules) has its place in the list, for dragging the groups
    struct GroupRect {
        std::string plugin;
        float top;
        float bottom;
    };

    bool IsGroupDragOf(const char* a_key, std::vector<std::string> a_plugins) {
        if (!g_groupDrag || g_groupDrag->key != a_key) return false;

        auto dragged{g_groupDrag->shownOrder};
        std::ranges::sort(dragged);
        std::ranges::sort(a_plugins);

        return dragged == a_plugins;
    }

    std::function<void()> UpdateGroupDrag(const Category& a_category, const std::vector<const Row*>& a_order,
                                          const std::vector<GroupRect>& a_rects, const float a_listTop) {
        std::function<void()> move;
        const auto* const key{KeyFor(a_category, g_state.female)};

        if (!g_groupDrag) return move;

        if (!IsGroupDragOf(key, PluginOrder(a_order))) {
            g_groupDrag.reset();
            return move;
        }

        auto& drag{*g_groupDrag};

        if (!ImGuiMCP::IsMouseDown(0)) {
            const auto plugins{drag.shownOrder};
            const auto plugin{drag.plugin};

            g_groupDrag.reset();

            if (plugins != PluginOrder(a_order)) {
                logger::info("Config Editor: plugin '{}' of {} dropped", plugin, key);
                move = [&a_category, a_order, plugins, plugin] {
                    SavePluginOrder(a_category, a_order, plugins, plugin);
                };
            }

            return move;
        }

        const auto here{std::ranges::find(a_rects, drag.plugin, &GroupRect::plugin)};

        if (here == a_rects.end()) return move;

        drag.height = here->bottom - here->top;
        drag.minTop = a_rects.front().top - a_listTop;
        drag.maxBottom = a_rects.back().bottom - a_listTop;

        const float top{DraggedTop(drag, a_listTop)};
        const float bottom{top + drag.height};

        const auto moveNextTo = [&](const GroupRect& a_other, const bool a_after) {
            std::erase(drag.shownOrder, drag.plugin);
            auto at{std::ranges::find(drag.shownOrder, a_other.plugin)};

            if (a_after) {
                ++at;
            }

            drag.shownOrder.insert(at, drag.plugin);
        };

        if (here != a_rects.begin()) {
            if (const auto& previous{*(here - 1)}; top < (previous.top + previous.bottom) * 0.5f) {
                moveNextTo(previous, false);
                return move;
            }
        }

        if (here + 1 != a_rects.end()) {
            if (const auto& next{*(here + 1)}; bottom > (next.top + next.bottom) * 0.5f) {
                moveNextTo(next, true);
            }
        }

        return move;
    }

    // While a rule or a plugin's group is dragged near the top or the bottom of the menu, the menu scrolls, so it can
    // be dropped anywhere
    void ScrollWhileDraggingRule() {
        if (!g_ruleDrag && !g_groupDrag) return;

        const float line{ImGuiMCP::GetTextLineHeightWithSpacing()};
        const float top{ImGuiMCP::GetWindowPos().y + ImGuiMCP::GetFrameHeight()};
        const float bottom{ImGuiMCP::GetWindowPos().y + ImGuiMCP::GetWindowHeight()};
        const float mouse{ImGuiMCP::GetMousePos().y};
        const float step{line * 15.0f * ImGuiMCP::GetIO()->DeltaTime};

        if (mouse < top + line * 1.5f) {
            ImGuiMCP::SetScrollY((std::max)(0.0f, ImGuiMCP::GetScrollY() - step));
        } else if (mouse > bottom - line * 1.5f) {
            ImGuiMCP::SetScrollY((std::min)(ImGuiMCP::GetScrollMaxY(), ImGuiMCP::GetScrollY() + step));
        }
    }

    void RenderRules(const Category& a_category) {
        const auto* const key{KeyFor(a_category, g_state.female)};
        const auto& rows{g_state.rows[key]};
        const bool gives{a_category.gives != Gives::kNothing};

        const auto sex{NpcSexShown(a_category)};
        std::vector<std::size_t> ofSex;

        for (std::size_t i{0}; i < rows.size(); ++i) {
            if (!sex || !rows[i].female || *rows[i].female == *sex) {
                ofSex.push_back(i);
            }
        }

        const bool anchored{!g_state.message.empty() && !g_state.messageAt.empty()};

        const auto anchoredMessage = [] { TextColored(g_state.failed ? kErrorText : kSuccessText, g_state.message); };

        if (ofSex.empty()) {
            if (anchored) {
                anchoredMessage();
            }

            TextColored(kHintText, Translate("obody_editor_no_rules"));
            return;
        }

        // Where the search is, to bring it back into view
        const float searchTop{ImGuiMCP::GetCursorPosY()};

        const bool searchChanged{UI::Widgets::SearchField(Translate("obody_editor_search_rules").c_str(),
                                                          "##OBodyEditorRuleSearch", g_ruleSearch,
                                                          sizeof(g_ruleSearch))};

        if (searchChanged && !ImGuiMCP::IsItemVisible()) {
            ImGuiMCP::SetScrollFromPosY(searchTop - ImGuiMCP::GetScrollY(), 0.0f);
        }

        // The rules the search finds, in the list's order
        const auto query{UI::Util::ToLowerWide(UI::Util::Utf8ToWide(g_ruleSearch))};
        std::vector<std::size_t> shown;

        for (const auto i : ofSex) {
            if (query.empty() || rows[i].search.find(query) != std::wstring::npos) {
                shown.push_back(i);
            }
        }

        // Outfits listed by record are grouped by plugin, the groups can be folded while nothing is searched
        const bool grouped{IsOrdered(a_category) && IsRecord(a_category.target)};
        const bool foldable{grouped && query.empty()};
        // 1 = unfold all the groups, -1 = fold them all
        int foldAll{0};

        // Always there, so the table doesn't move when a search starts
        VerticalSpace(0.1f);
        TextColored(kDimText,
                    Translate("obody_editor_rules_shown", std::to_string(shown.size()), std::to_string(ofSex.size())));

        if (foldable && !shown.empty()) {
            const auto expandLabel{Translate("obody_expand_all")};
            const auto collapseLabel{Translate("obody_collapse_all")};
            const auto& padding{ImGuiMCP::GetStyle()->FramePadding};
            const float width{ImGuiMCP::CalcTextSize(expandLabel.c_str()).x +
                              ImGuiMCP::CalcTextSize(collapseLabel.c_str()).x + padding.x * 4.0f +
                              ImGuiMCP::GetStyle()->ItemSpacing.x};

            ImGuiMCP::SameLine(ImGuiMCP::GetWindowContentRegionMax().x - width);

            if (ImGuiMCP::SmallButton(expandLabel.c_str())) {
                foldAll = 1;
            }

            ImGuiMCP::SameLine();

            if (ImGuiMCP::SmallButton(collapseLabel.c_str())) {
                foldAll = -1;
            }
        }

        VerticalSpace(0.2f);

        const auto keepHeight = [&] {
            if (query.empty()) return;

            const float viewBottom{ImGuiMCP::GetWindowPos().y + ImGuiMCP::GetWindowHeight() -
                                   ImGuiMCP::GetStyle()->WindowPadding.y};

            if (const float room{viewBottom - ImGuiMCP::GetCursorScreenPos().y}; room > 0.0f) {
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, room});
            }
        };

        if (shown.empty()) {
            if (anchored) {
                anchoredMessage();
            }

            TextColored(kHintText, Translate("obody_editor_no_rules_match"));
            keepHeight();
            return;
        }

        std::function<void()> action;
        std::string actionAt;

        // A rule's buttons, at the right of its name
        struct RowButton {
            const char* label;
            const char* tooltip;
            bool removes;
            std::function<void()> run;
        };

        const auto& style{*ImGuiMCP::GetStyle()};

        // Ordered categories (IsOrdered)
        std::vector<const Row*> order;
        std::unordered_map<const Row*, std::size_t> positions;

        if (IsOrdered(a_category)) {
            for (const auto& row : rows) {
                if (!row.switchedOff) {
                    positions.emplace(&row, order.size());
                    order.push_back(&row);
                }
            }
        }

        ScrollWhileDraggingRule();

        const bool dragging{IsDragOf(key, order)};
        std::vector<std::size_t> shownPlace;

        if (dragging) {
            shownPlace.resize(order.size());

            for (std::size_t k{0}; k < order.size(); ++k) {
                shownPlace[g_ruleDrag->shownOrder[k]] = k;
            }

            // The rules in force found by the search keep their slots in the list, in the dragged order
            std::vector<std::size_t> slots;
            std::vector<std::size_t> inForce;

            for (std::size_t s{0}; s < shown.size(); ++s) {
                if (positions.contains(&rows[shown[s]])) {
                    slots.push_back(s);
                    inForce.push_back(shown[s]);
                }
            }

            std::ranges::sort(inForce, {},
                              [&](const std::size_t a_row) { return shownPlace[positions.at(&rows[a_row])]; });

            for (std::size_t k{0}; k < slots.size(); ++k) {
                shown[slots[k]] = inForce[k];
            }
        }

        const auto plugins{grouped ? PluginOrder(order) : std::vector<std::string>{}};
        std::unordered_map<std::string, std::size_t> groupStart;

        for (std::size_t k{order.size()}; k-- > 0;) {
            groupStart[order[k]->ref.plugin] = k;
        }

        const bool groupDragging{grouped && IsGroupDragOf(key, plugins)};

        // While a plugin's group is dragged, the groups are shown in the dragged order
        if (groupDragging) {
            const auto& dragged{g_groupDrag->shownOrder};

            std::ranges::stable_sort(shown, {}, [&](const std::size_t a_row) {
                const auto& row{rows[a_row]};

                return row.switchedOff
                           ? dragged.size()
                           : static_cast<std::size_t>(std::ranges::find(dragged, row.ref.plugin) - dragged.begin());
            });
        }

        const auto& pluginsShown{groupDragging ? g_groupDrag->shownOrder : plugins};

        const auto groupId = [](const std::string& a_header) {
            return a_header.empty() ? std::string{"#switched off"} : PluginGroupId(a_header);
        };
        const auto foldId = [&](const std::string& a_header) { return std::format("{}|{}", key, groupId(a_header)); };

        if (foldAll != 0) {
            const auto prefix{std::format("{}|", key)};

            std::erase_if(g_collapsed, [&](const std::string& a_id) { return a_id.starts_with(prefix); });

            if (foldAll < 0) {
                for (const auto& plugin : plugins) {
                    g_collapsed.insert(foldId(plugin));
                }

                g_collapsed.insert(foldId({}));
            }

            ++g_collapsedVersion;
        }

        std::vector<std::optional<std::string>> headers(shown.size());
        std::vector<bool> folded(shown.size(), false);
        std::unordered_map<std::string, std::size_t> groupSizes;

        if (grouped) {
            const auto groupOf = [&](const std::size_t a_shown) {
                const auto& row{rows[shown[a_shown]]};

                return row.switchedOff ? std::string{} : row.ref.plugin;
            };

            bool groupFolded{false};

            for (std::size_t s{0}; s < shown.size(); ++s) {
                const auto group{groupOf(s)};

                if (s == 0 || groupOf(s - 1) != group) {
                    headers[s] = group;
                    groupFolded = foldable && g_collapsed.contains(foldId(group));
                }

                folded[s] = groupFolded;
                ++groupSizes[group];
            }
        }

        const auto messageFor = [&](const std::string& a_id) { return anchored && g_state.messageAt == a_id; };
        bool messageShown{false};

        for (std::size_t s{0}; s < shown.size(); ++s) {
            if ((!folded[s] && messageFor(Parser::EntrySourceId(rows[shown[s]].ref))) ||
                (headers[s] && !headers[s]->empty() && messageFor(PluginGroupId(*headers[s])))) {
                messageShown = true;
            }
        }

        if (anchored && !messageShown) {
            anchoredMessage();
            VerticalSpace(0.3f);
        }

        const auto drawMessage = [&] {
            VerticalSpace(0.15f);
            ImGuiMCP::Indent();
            anchoredMessage();
            ImGuiMCP::Unindent();
        };

        // Where the rules in force have their places, for the dragging, once they're all drawn
        std::vector<RuleRect> rects;
        // The same for the plugins' groups
        std::vector<GroupRect> groupRects;

        static std::unordered_map<std::size_t, float> rowHeights;
        static std::unordered_map<std::string, float> groupHeights;
        static std::string rowHeightsKey;

        if (rowHeightsKey != key) {
            rowHeights.clear();
            groupHeights.clear();
            rowHeightsKey = key;
        }

        ImGuiMCP::ImVec2 groupSlot{};
        float groupWidth{0.0f};
        bool groupDragged{false};
        float groupOffset{0.0f};
        // A fold toggle clicked, applied once the list is drawn
        std::optional<std::string> toggled;

        struct Slide {
            float place{-1.0f};
            float offset{0.0f};
        };

        static std::unordered_map<std::string, Slide> slides;
        static std::string slidesFor;

        if (const auto context{std::format("{}|{}|{:.0f}|{}", key, g_ruleSearch, ImGuiMCP::GetContentRegionAvail().x,
                                           g_collapsedVersion)};
            context != slidesFor) {
            slides.clear();
            slidesFor = context;
        }

        const float listTop{ImGuiMCP::GetCursorPosY()};
        const float listScreenTop{ImGuiMCP::GetCursorScreenPos().y};
        const float slideDecay{std::exp(-ImGuiMCP::GetIO()->DeltaTime * 18.0f)};

        const auto slideAt = [&](const std::string& a_id) -> Slide& {
            auto& state{slides[a_id]};
            const float place{ImGuiMCP::GetCursorPosY() - listTop};

            if (state.place >= 0.0f) {
                state.offset += state.place - place;
            }

            state.place = place;
            state.offset = std::abs(state.offset) < 0.5f ? 0.0f : state.offset * slideDecay;

            return state;
        };

        auto* const drawList{ImGuiMCP::GetWindowDrawList()};

        const auto drawFlash = [&](const std::string& a_id, const ImGuiMCP::ImVec2 a_min,
                                   const ImGuiMCP::ImVec2 a_max) {
            if (!g_movedFlash || g_movedFlash->id != a_id) return;

            const float elapsed{
                std::chrono::duration<float>(std::chrono::steady_clock::now() - g_movedFlash->at).count()};
            const float fade{1.0f - elapsed / kMovedFlashSeconds};

            if (fade <= 0.0f) {
                g_movedFlash.reset();
                return;
            }

            const float pad{style.ItemSpacing.y * 0.5f};

            ImGuiMCP::ImDrawListManager::AddRectFilled(
                drawList, ImGuiMCP::ImVec2{a_min.x - pad, a_min.y - pad},
                ImGuiMCP::ImVec2{a_max.x + pad, a_max.y + pad},
                ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{kGoldText.x, kGoldText.y, kGoldText.z, 0.22f * fade}), 3.0f, 0);
        };

        const auto endGroup = [&](const Row& a_last) {
            const float groupBottom{ImGuiMCP::GetCursorScreenPos().y - style.ItemSpacing.y};
            const float groupHeight{groupBottom - groupSlot.y - (groupDragged ? groupOffset : 0.0f)};

            if (!a_last.switchedOff) {
                groupHeights[a_last.ref.plugin] = groupHeight;
                groupRects.push_back(GroupRect{a_last.ref.plugin, groupSlot.y, groupSlot.y + groupHeight});
            }

            if (!groupDragged) return;

            const float pad{style.ItemSpacing.y * 0.5f};
            const ImGuiMCP::ImVec2 min{groupSlot.x - pad, groupSlot.y + groupOffset - pad};
            const ImGuiMCP::ImVec2 max{groupSlot.x + groupWidth + pad, groupBottom + pad};

            ImGuiMCP::ImDrawListManager::AddRectFilled(
                drawList, min, max,
                ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{kGoldText.x, kGoldText.y, kGoldText.z, 0.08f}), 3.0f, 0);
            ImGuiMCP::ImDrawListManager::AddRect(
                drawList, min, max,
                ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{kGoldText.x, kGoldText.y, kGoldText.z, 0.5f}), 3.0f, 0, 1.0f);
            ImGuiMCP::ImDrawListManager::ChannelsSetCurrent(drawList, 0);

            ImGuiMCP::SetCursorScreenPos(groupSlot);
            ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, groupHeight});
            groupDragged = false;
        };

        // The dragged rule or group is drawn over the others
        if (dragging || groupDragging) {
            ImGuiMCP::ImDrawListManager::ChannelsSplit(drawList, 2);
        }

        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, kRuleButton);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, kRuleButtonHovered);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, kRuleButtonActive);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Border, kRuleButtonBorder);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Separator, kRuleLine);

        for (std::size_t n{0}; n < shown.size(); ++n) {
            const auto i{shown[n]};
            const auto& row{rows[i]};

            if (const auto& header{headers[n]}) {
                const bool off{header->empty()};
                const auto id{groupId(*header)};
                const bool groupFolded{folded[n]};
                const float left{ImGuiMCP::GetCursorPosX()};
                const float right{left + ImGuiMCP::GetContentRegionAvail().x};
                const auto slot{ImGuiMCP::GetCursorScreenPos()};
                const float pad{style.ItemSpacing.y * 0.5f};
                auto& headerSlide{slideAt(id)};

                groupSlot = slot;
                groupWidth = right - left;
                groupDragged = groupDragging && !off && *header == g_groupDrag->plugin;

                // The dragged group follows the mouse, its header and its rules together
                if (groupDragged) {
                    groupOffset = DraggedTop(*g_groupDrag, listScreenTop) - slot.y;
                    headerSlide.place += groupOffset;
                    headerSlide.offset = 0.0f;
                }

                const float slide{groupDragged ? groupOffset : headerSlide.offset};
                const auto start{ImGuiMCP::ImVec2{slot.x, slot.y + slide}};

                if (slide != 0.0f) {
                    ImGuiMCP::SetCursorScreenPos(start);
                }

                if (groupDragged) {
                    const float height{groupHeights.contains(*header) ? groupHeights[*header]
                                                                      : ImGuiMCP::GetTextLineHeight()};
                    auto background{style.Colors[ImGuiMCP::ImGuiCol_WindowBg]};
                    background.w = 1.0f;

                    ImGuiMCP::ImDrawListManager::ChannelsSetCurrent(drawList, 1);
                    ImGuiMCP::ImDrawListManager::AddRectFilled(
                        drawList, ImGuiMCP::ImVec2{start.x - pad, start.y - pad},
                        ImGuiMCP::ImVec2{start.x + (right - left) + pad, start.y + height + pad},
                        ImGuiMCP::GetColorU32(background), 3.0f, 0);
                }

                ImGuiMCP::PushID(id.c_str());

                const auto foldMark = [&](const float a_lineHeight) {
                    if (!foldable) return;

                    const float size{ImGuiMCP::GetFontSize()};
                    const auto at{ImGuiMCP::GetCursorScreenPos()};

                    ImGuiMCP::RenderArrow(drawList, ImGuiMCP::ImVec2{at.x, at.y + (a_lineHeight - size * 0.7f) * 0.5f},
                                          ImGuiMCP::GetColorU32(kDimText),
                                          groupFolded ? ImGuiMCP::ImGuiDir_Right : ImGuiMCP::ImGuiDir_Down, 0.7f);
                    ImGuiMCP::Dummy(ImGuiMCP::ImVec2{size, a_lineHeight});
                    ImGuiMCP::SameLine(0.0f, 0.0f);
                };
                const auto foldHint{Translate(groupFolded ? "obody_editor_group_unfold" : "obody_editor_group_fold")};
                const auto folding{groupFolded ? std::format(" ({})", groupSizes[*header]) : std::string{}};

                if (off) {
                    const float lineHeight{ImGuiMCP::GetTextLineHeight()};

                    if (foldable) {
                        const bool clicked{ImGuiMCP::InvisibleButton(
                            "##fold", ImGuiMCP::ImVec2{(std::max)(right - left, 1.0f), lineHeight})};

                        if (clicked && !dragging && !groupDragging) {
                            toggled = foldId(*header);
                        }

                        if (ImGuiMCP::IsItemHovered() && !g_ruleDrag && !g_groupDrag) {
                            ImGuiMCP::ImDrawListManager::AddRectFilled(
                                drawList, ImGuiMCP::ImVec2{start.x - pad, start.y - pad},
                                ImGuiMCP::ImVec2{start.x + (right - left) + pad, start.y + lineHeight + pad},
                                ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{1.0f, 1.0f, 1.0f, 0.04f}), 3.0f, 0);
                        }

                        if (ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_DelayNormal) && !g_ruleDrag &&
                            !g_groupDrag) {
                            ImGuiMCP::SetTooltip("%s", foldHint.c_str());
                        }

                        ImGuiMCP::SetCursorScreenPos(start);
                    }

                    foldMark(lineHeight);
                    TextColored(kDimText, Translate("obody_editor_switched_off_group") + folding);
                } else {
                    const auto place{
                        static_cast<std::size_t>(std::ranges::find(pluginsShown, *header) - pluginsShown.begin())};
                    const float arrowsWidth{(ImGuiMCP::GetFontSize() + style.ItemSpacing.x) * 2.0f};
                    const float lineHeight{ImGuiMCP::GetTextLineHeight() * 1.2f};

                    ImGuiMCP::SetNextItemAllowOverlap();

                    const bool grabClicked{ImGuiMCP::InvisibleButton(
                        "##grab", ImGuiMCP::ImVec2{(std::max)(right - left - arrowsWidth, 1.0f), lineHeight})};

                    const bool grabHovered{ImGuiMCP::IsItemHovered()};
                    const bool grabHint{ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_DelayNormal)};
                    const bool grabHeld{ImGuiMCP::IsItemActive()};

                    if (grabClicked && foldable && !g_groupDrag && !g_ruleDrag) {
                        toggled = foldId(*header);
                    }

                    if (grabHeld && ImGuiMCP::IsMouseDragging(0) && !g_groupDrag && !g_ruleDrag) {
                        // Where it was clicked, the drag started there
                        const float clicked{ImGuiMCP::GetMousePos().y - ImGuiMCP::GetMouseDragDelta(0).y};
                        const float height{groupHeights.contains(*header) ? groupHeights[*header] : lineHeight};

                        g_groupDrag =
                            GroupDrag{key, *header, plugins, std::clamp(clicked - start.y, 0.0f, height), height};
                    }

                    if (grabHovered && !g_ruleDrag && !g_groupDrag) {
                        ImGuiMCP::ImDrawListManager::AddRectFilled(
                            drawList, ImGuiMCP::ImVec2{start.x - pad, start.y - pad},
                            ImGuiMCP::ImVec2{start.x + (right - left) + pad, start.y + lineHeight + pad},
                            ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{1.0f, 1.0f, 1.0f, 0.04f}), 3.0f, 0);
                    }

                    if (grabHint && !g_ruleDrag && !g_groupDrag && !grabHeld) {
                        const auto dragHint{Translate("obody_editor_plugin_drag_hint")};

                        ImGuiMCP::SetTooltip("%s", (foldable ? foldHint + "\n" + dragHint : dragHint).c_str());
                    }

                    ImGuiMCP::SetCursorScreenPos(start);

                    const auto groupArrow = [&](const char* a_id, const ImGuiMCP::ImGuiDir a_direction,
                                                const bool a_up) {
                        const bool can{!dragging && !groupDragging && (a_up ? place > 0 : place + 1 < plugins.size())};

                        ImGuiMCP::BeginDisabled(!can);

                        if (ImGuiMCP::ArrowButton(a_id, a_direction)) {
                            action = [&a_category, order, plugin = *header, a_up] {
                                MovePluginGroup(a_category, order, plugin, a_up);
                            };
                            actionAt = id;
                        }

                        ImGuiMCP::EndDisabled();

                        if (can && ImGuiMCP::IsItemHovered()) {
                            ImGuiMCP::SetTooltip(
                                "%s", Translate(a_up ? "obody_editor_plugin_up" : "obody_editor_plugin_down").c_str());
                        }
                    };

                    ImGuiMCP::SetCursorPosX(right - arrowsWidth + style.ItemSpacing.x);
                    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImGuiMCP::ImVec2{0.0f, 0.0f});
                    groupArrow("##up", ImGuiMCP::ImGuiDir_Up, true);
                    ImGuiMCP::SameLine();
                    groupArrow("##down", ImGuiMCP::ImGuiDir_Down, false);
                    ImGuiMCP::PopStyleVar();

                    ImGuiMCP::SameLine();
                    ImGuiMCP::SetCursorPosX(left);
                    foldMark(lineHeight);
                    UI::Widgets::SetTextScale(1.2f);
                    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kDimText);
                    ImGuiMCP::TextUnformatted(std::format("{}.", place + 1).c_str());
                    ImGuiMCP::PopStyleColor();
                    ImGuiMCP::SameLine();
                    ImGuiMCP::PushTextWrapPos(right - arrowsWidth);
                    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kGroupHeader);
                    ImGuiMCP::TextWrapped("%s", (*header + folding).c_str());
                    ImGuiMCP::PopStyleColor();
                    ImGuiMCP::PopTextWrapPos();
                    UI::Widgets::SetTextScale();
                }

                ImGuiMCP::PopID();

                VerticalSpace(0.1f);
                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Separator, kRuleButtonBorder);
                ImGuiMCP::Separator();
                ImGuiMCP::PopStyleColor();

                const float headerBottom{ImGuiMCP::GetCursorScreenPos().y - style.ItemSpacing.y};

                drawFlash(id, start, ImGuiMCP::ImVec2{start.x + (right - left), headerBottom});

                if (slide != 0.0f && !groupDragged) {
                    ImGuiMCP::SetCursorScreenPos(slot);
                    ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, headerBottom - start.y});
                }

                VerticalSpace(0.25f);

                if (!off && messageFor(id)) {
                    drawMessage();
                    VerticalSpace(0.25f);
                }
            }

            const bool groupEnds{grouped && (n + 1 == shown.size() || headers[n + 1])};

            // Only the header of a folded group is drawn
            if (folded[n]) {
                if (groupEnds) {
                    endGroup(row);

                    if (n + 1 < shown.size()) {
                        VerticalSpace(0.9f);
                    }
                }

                continue;
            }

            ImGuiMCP::PushID(static_cast<int>(i));

            const auto edit = [&row] {
                g_state.targets = {Item{row.label, row.ref.entry, row.ref.plugin, {}, g_state.female}};
                g_state.presets = row.presets;
                g_state.editing = true;
                g_state.message.clear();
                g_scrollToForm = true;
                g_formFlashAt = std::chrono::steady_clock::now();
            };

            std::vector<RowButton> buttons;

            if (row.inGame) {
                if (gives) {
                    buttons.push_back({"obody_editor_edit", nullptr, false, edit});
                }

                buttons.push_back(
                    {"obody_editor_delete", row.replaces.empty() ? nullptr : "obody_editor_delete_hint", true, [&row] {
                         Change([&](rapidjson::Document& a_config) {
                             Parser::InGameEdit::DeleteEntry(a_config, row.ref);
                             Parser::InGameEdit::DeleteRemoval(a_config, row.ref);
                         });
                     }});
            } else if (row.switchedOff) {
                buttons.push_back({"obody_editor_switch_on", nullptr, false, [&row] {
                                       Change([&](rapidjson::Document& a_config) {
                                           Parser::InGameEdit::DeleteRemoval(a_config, row.ref);
                                       });
                                   }});
            } else {
                if (gives) {
                    buttons.push_back({"obody_editor_change", "obody_editor_change_hint", false, edit});
                }

                buttons.push_back({"obody_editor_switch_off", "obody_editor_switch_off_hint", true, [&row] {
                                       Change([&](rapidjson::Document& a_config) {
                                           Parser::InGameEdit::AddRemoval(a_config, row.ref);
                                       });
                                   }});
            }

            float buttonsWidth{0.0f};

            for (const auto& button : buttons) {
                buttonsWidth += ImGuiMCP::CalcTextSize(Translate(button.label).c_str()).x +
                                style.FramePadding.x * 2.0f + style.ItemSpacing.x;
            }

            const auto position{positions.find(&row)};
            const bool numbered{position != positions.end()};
            const float arrowSize{ImGuiMCP::GetFontSize()};

            if (numbered) {
                buttonsWidth += (arrowSize + style.ItemSpacing.x) * 2.0f;
            }

            const float left{ImGuiMCP::GetCursorPosX()};
            const float right{left + ImGuiMCP::GetContentRegionAvail().x};

            const auto slot{ImGuiMCP::GetCursorScreenPos()};
            const bool dragged{dragging && numbered && g_ruleDrag->from == position->second};
            auto& slideState{slideAt(Parser::EntrySourceId(row.ref))};

            if (dragged) {
                slideState.offset = DraggedTop(*g_ruleDrag, listScreenTop) - slot.y;
            } else if (groupDragged) {
                slideState.offset = 0.0f;
            }

            const float slide{slideState.offset};
            const auto rowStart{ImGuiMCP::ImVec2{slot.x, slot.y + slide}};

            if (slide != 0.0f) {
                ImGuiMCP::SetCursorScreenPos(rowStart);
            }

            // Over the others, on a solid background, as tall as it was last frame
            if (dragged) {
                const float pad{style.ItemSpacing.y * 0.5f};
                const float height{rowHeights.contains(i) ? rowHeights[i] : ImGuiMCP::GetTextLineHeight()};
                auto background{style.Colors[ImGuiMCP::ImGuiCol_WindowBg]};
                background.w = 1.0f;

                ImGuiMCP::ImDrawListManager::ChannelsSetCurrent(drawList, 1);
                ImGuiMCP::ImDrawListManager::AddRectFilled(
                    drawList, ImGuiMCP::ImVec2{rowStart.x - pad, rowStart.y - pad},
                    ImGuiMCP::ImVec2{rowStart.x + (right - left) + pad, rowStart.y + height + pad},
                    ImGuiMCP::GetColorU32(background), 3.0f, 0);
            }

            bool grabHovered{false};
            bool grabHeld{false};
            bool grabHint{false};

            if (numbered) {
                const float height{rowHeights.contains(i) ? rowHeights[i] : ImGuiMCP::GetTextLineHeight()};

                ImGuiMCP::SetNextItemAllowOverlap();
                ImGuiMCP::InvisibleButton("##grab", ImGuiMCP::ImVec2{right - left, (std::max)(height, 1.0f)});

                grabHovered = ImGuiMCP::IsItemHovered();
                grabHeld = ImGuiMCP::IsItemActive();
                grabHint = ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_DelayNormal);

                if (grabHeld && ImGuiMCP::IsMouseDragging(0) && !g_ruleDrag && !g_groupDrag) {
                    std::vector<std::size_t> inOrder(order.size());
                    std::iota(inOrder.begin(), inOrder.end(), std::size_t{0});

                    // Where it was clicked, the drag started there
                    const float clicked{ImGuiMCP::GetMousePos().y - ImGuiMCP::GetMouseDragDelta(0).y};

                    g_ruleDrag = RuleDrag{position->second, key, std::move(inOrder),
                                          std::clamp(clicked - rowStart.y, 0.0f, height), height};
                }

                ImGuiMCP::SetCursorScreenPos(rowStart);
            }

            ImGuiMCP::SetCursorPosX(right - buttonsWidth + style.ItemSpacing.x);

            if (numbered) {
                const auto here{position->second};

                // Outfits listed by record are grouped by plugin, they only move inside their plugin's group
                const auto canSwapWith = [&](const std::size_t a_other) {
                    return a_other < order.size() &&
                           (!IsRecord(a_category.target) || order[a_other]->ref.plugin == row.ref.plugin);
                };

                const auto arrow = [&](const char* a_id, const ImGuiMCP::ImGuiDir a_direction, const bool a_can,
                                       const std::size_t a_other, const char* a_tooltip) {
                    ImGuiMCP::BeginDisabled(!a_can);

                    if (ImGuiMCP::ArrowButton(a_id, a_direction)) {
                        action = [&a_category, order, here, a_other] { MoveRule(a_category, order, here, a_other); };
                        actionAt = Parser::EntrySourceId(row.ref);
                    }

                    ImGuiMCP::EndDisabled();

                    if (a_can && ImGuiMCP::IsItemHovered()) {
                        ImGuiMCP::SetTooltip("%s", Translate(a_tooltip).c_str());
                    }
                };

                ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImGuiMCP::ImVec2{0.0f, 0.0f});

                arrow("##up", ImGuiMCP::ImGuiDir_Up, !dragging && !groupDragging && here > 0 && canSwapWith(here - 1),
                      here - 1, "obody_editor_move_up");
                ImGuiMCP::SameLine();
                arrow("##down", ImGuiMCP::ImGuiDir_Down, !dragging && !groupDragging && canSwapWith(here + 1), here + 1,
                      "obody_editor_move_down");
                ImGuiMCP::PopStyleVar();
            }

            for (std::size_t b{0}; b < buttons.size(); ++b) {
                const auto& button{buttons[b]};

                if (b > 0 || numbered) {
                    ImGuiMCP::SameLine();
                }

                if (button.removes) {
                    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, kRemoveButtonHovered);
                    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, kRemoveButtonActive);
                }

                if (ImGuiMCP::SmallButton(Translate(button.label).c_str())) {
                    action = button.run;
                    actionAt = Parser::EntrySourceId(row.ref);
                }

                if (button.removes) {
                    ImGuiMCP::PopStyleColor(2);
                }

                if (button.tooltip && ImGuiMCP::IsItemHovered()) {
                    ImGuiMCP::SetTooltip("%s", Translate(button.tooltip).c_str());
                }
            }

            ImGuiMCP::SameLine();
            ImGuiMCP::SetCursorPosX(left);

            if (numbered) {
                const auto place{dragging ? shownPlace[position->second] : position->second};

                ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, kDimText);
                ImGuiMCP::TextUnformatted(
                    std::format("{}.", place - (grouped ? groupStart.at(row.ref.plugin) : 0) + 1).c_str());
                ImGuiMCP::PopStyleColor();
                ImGuiMCP::SameLine();
            }

            ImGuiMCP::PushTextWrapPos(right - buttonsWidth);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, row.switchedOff ? kDimText : kRuleName);
            ImGuiMCP::TextWrapped("%s", row.name.c_str());
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::PopTextWrapPos();

            std::string details{grouped && !row.switchedOff &&
                                        row.detail == std::format("{}, {}", row.ref.plugin, row.ref.entry)
                                    ? row.ref.entry
                                    : row.detail};

            const auto addDetail = [&](const std::string& a_text) {
                details += (details.empty() ? "" : "    ") + a_text;
            };

            if (!row.inGame) {
                addDetail(Translate("obody_editor_from_file", row.source));
            }

            if (row.switchedOff) {
                addDetail(Translate("obody_editor_switched_off"));
            }

            if (!row.replaces.empty()) {
                addDetail(Translate("obody_editor_replaces", row.replaces));
            }

            ImGuiMCP::Indent();

            if (!details.empty()) {
                TextColored(kDimText, details);
            }

            if (!row.presets.empty()) {
                TextColored(row.switchedOff ? kDimText : kRulePresets, JoinPresets(row.presets));
            }

            ImGuiMCP::Unindent();

            // Where the rule was drawn down to
            const float rowBottom{ImGuiMCP::GetCursorScreenPos().y - style.ItemSpacing.y};
            const float rowHeight{rowBottom - rowStart.y};

            if (numbered) {
                rowHeights[i] = rowHeight;
                rects.push_back(RuleRect{position->second, &row, slot.y, slot.y + rowHeight});

                const bool held{g_ruleDrag && g_ruleDrag->from == position->second};

                if (held || (grabHovered && !g_ruleDrag && !g_groupDrag)) {
                    const float pad{style.ItemSpacing.y * 0.5f};
                    const ImGuiMCP::ImVec2 min{rowStart.x - pad, rowStart.y - pad};
                    const ImGuiMCP::ImVec2 max{rowStart.x + (right - left) + pad, rowBottom + pad};
                    const auto color{held ? ImGuiMCP::ImVec4{kGoldText.x, kGoldText.y, kGoldText.z, 0.16f}
                                          : ImGuiMCP::ImVec4{1.0f, 1.0f, 1.0f, 0.04f}};

                    ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, min, max, ImGuiMCP::GetColorU32(color), 3.0f,
                                                               0);

                    if (dragged) {
                        ImGuiMCP::ImDrawListManager::AddRect(
                            drawList, min, max,
                            ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{kGoldText.x, kGoldText.y, kGoldText.z, 0.5f}), 3.0f,
                            0, 1.0f);
                    }
                }

                if (grabHint && !g_ruleDrag && !g_groupDrag && !grabHeld) {
                    ImGuiMCP::SetTooltip("%s", Translate("obody_editor_drag_hint").c_str());
                }
            }

            drawFlash(Parser::EntrySourceId(row.ref), rowStart,
                      ImGuiMCP::ImVec2{rowStart.x + (right - left), rowBottom});

            if (dragged) {
                ImGuiMCP::ImDrawListManager::ChannelsSetCurrent(drawList, 0);
            }

            if (slide != 0.0f) {
                ImGuiMCP::SetCursorScreenPos(slot);
                ImGuiMCP::Dummy(ImGuiMCP::ImVec2{0.0f, rowHeight});
            }

            if (messageFor(Parser::EntrySourceId(row.ref))) {
                drawMessage();
            }

            if (groupEnds) {
                endGroup(row);
            }

            // A group's header has its own line
            if (n + 1 < shown.size() && headers[n + 1]) {
                VerticalSpace(0.9f);
            } else if (n + 1 < shown.size()) {
                VerticalSpace(0.25f);
                ImGuiMCP::Separator();
                VerticalSpace(0.25f);
            }

            ImGuiMCP::PopID();
        }

        if (dragging || groupDragging) {
            ImGuiMCP::ImDrawListManager::ChannelsMerge(drawList);
        }

        ImGuiMCP::PopStyleColor(5);
        ImGuiMCP::PopStyleVar();

        if (toggled) {
            if (!g_collapsed.erase(*toggled)) {
                g_collapsed.insert(*toggled);
            }

            ++g_collapsedVersion;
        }

        const auto ruleDropAt{IsDragOf(key, order) ? Parser::EntrySourceId(order[g_ruleDrag->from]->ref)
                                                   : std::string{}};
        const auto groupDropAt{g_groupDrag ? PluginGroupId(g_groupDrag->plugin) : std::string{}};

        if (auto move{UpdateRuleDrag(a_category, order, rects, listScreenTop)}) {
            action = std::move(move);
            actionAt = ruleDropAt;
        }

        if (auto move{UpdateGroupDrag(a_category, order, groupRects, listScreenTop)}) {
            action = std::move(move);
            actionAt = groupDropAt;
        }

        keepHeight();

        if (action) {
            g_state.message.clear();
            g_state.messageAt = actionAt;
            action();
        }
    }

    void AddRule(const Category& a_category) {
        const bool gives{a_category.gives != Gives::kNothing};

        // Its messages are shown under the form
        g_state.messageAt.clear();

        if (g_state.targets.empty()) {
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
        const auto untrimmed = [](const Item& a_target) {
            return UI::Util::TrimWhitespace(a_target.value).size() != a_target.value.size();
        };

        if (std::ranges::any_of(g_state.targets, untrimmed)) {
            g_state.message = Translate("obody_editor_untrimmed");
            g_state.failed = true;
            return;
        }

        const auto* const key{KeyFor(a_category, g_state.female)};
        const auto presets{g_state.presets};

        // All the rules in one change, so they're saved together or not at all
        const bool changed{Change([&](rapidjson::Document& a_config) {
            for (const auto& target : g_state.targets) {
                const Parser::EntryRef ref{key, target.plugin, target.value};
                Parser::InGameEdit::SetEntry(a_config, ref, gives ? &presets : nullptr);

                // A rule that was switched off is on again
                Parser::InGameEdit::DeleteRemoval(a_config, ref);
            }
        })};

        if (changed) {
            ResetForm();
        }
    }

    void RenderRuleForm(const Category& a_category, RE::Actor* a_actor) {
        const bool gives{a_category.gives != Gives::kNothing};
        const auto targetLabel{Translate("obody_editor_target")};
        const auto presetsLabel{Translate("obody_editor_presets")};
        const auto clearLabel{Translate("obody_editor_clear")};
        const float labelWidth{
            (std::max)({ImGuiMCP::CalcTextSize(targetLabel.c_str()).x,
                        gives ? ImGuiMCP::CalcTextSize(presetsLabel.c_str()).x : 0.0f,
                        ImGuiMCP::CalcTextSize(clearLabel.c_str()).x + ImGuiMCP::GetStyle()->FramePadding.x * 2.0f}) +
            ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f};

        const auto pickedList = [&](const char* a_id, const char* a_clearId, const std::vector<std::string>& a_labels,
                                    const bool a_reorder) -> std::optional<PickedChange> {
            if (a_labels.empty()) return std::nullopt;

            VerticalSpace(0.1f);
            ImGuiMCP::PushID(a_clearId);
            const bool clear{ImGuiMCP::SmallButton(clearLabel.c_str())};
            ImGuiMCP::PopID();
            ImGuiMCP::SameLine(labelWidth);

            const auto change{RenderPickedList(a_id, a_labels, a_reorder)};

            return clear ? std::optional{PickedChange{PickedChange::Kind::kClear}} : change;
        };

        if (g_scrollToForm) {
            g_scrollToForm = false;
            ImGuiMCP::SetScrollFromPosY(
                ImGuiMCP::GetCursorPosY() - ImGuiMCP::GetTextLineHeightWithSpacing() - ImGuiMCP::GetScrollY(), 0.0f);
        }

        const auto titleStart{ImGuiMCP::GetCursorScreenPos()};

        SectionHeader(Translate(g_state.editing ? "obody_editor_edit_title" : "obody_editor_add_title"));

        if (g_formFlashAt) {
            const float elapsed{
                std::chrono::duration<float>(std::chrono::steady_clock::now() - *g_formFlashAt).count()};
            const float fade{1.0f - elapsed / kMovedFlashSeconds};
            const float pad{ImGuiMCP::GetStyle()->ItemSpacing.y * 0.5f};

            if (fade > 0.0f) {
                ImGuiMCP::ImDrawListManager::AddRectFilled(
                    ImGuiMCP::GetWindowDrawList(), ImGuiMCP::ImVec2{titleStart.x - pad, titleStart.y - pad},
                    ImGuiMCP::ImVec2{titleStart.x + ImGuiMCP::GetContentRegionAvail().x + pad,
                                     ImGuiMCP::GetCursorScreenPos().y - ImGuiMCP::GetStyle()->ItemSpacing.y},
                    ImGuiMCP::GetColorU32(ImGuiMCP::ImVec4{kGoldText.x, kGoldText.y, kGoldText.z, 0.22f * fade}), 3.0f,
                    0);
            } else {
                g_formFlashAt.reset();
            }
        }

        ImGuiMCP::AlignTextToFramePadding();
        ImGuiMCP::TextUnformatted(targetLabel.c_str());
        ImGuiMCP::SameLine(labelWidth);

        if (g_state.editing) {
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(g_state.targets.front().label.c_str());
        } else {
            PickTargets("##OBodyEditorTarget", TargetKind(a_category.target), g_state.targets, a_actor,
                        NpcSexShown(a_category));

            std::vector<std::string> labels;

            for (const auto& target : g_state.targets) {
                labels.push_back(target.label);
            }

            // Where order matters, the picks are added in the order they're listed in, so they can be moved first
            if (const auto change{
                    pickedList("##OBodyEditorTargets", "OBodyEditorClearTargets", labels, IsOrdered(a_category))}) {
                ApplyPickedChange(g_state.targets, *change);
            }
        }

        if (gives) {
            VerticalSpace(0.2f);
            ImGuiMCP::AlignTextToFramePadding();
            ImGuiMCP::TextUnformatted(presetsLabel.c_str());
            ImGuiMCP::SameLine(labelWidth);
            PickPresets("##OBodyEditorPresets", PresetKind(a_category.gives, g_state.female), g_state.presets, a_actor);

            std::vector<std::string> labels;

            for (const auto& preset : g_state.presets) {
                labels.push_back(UI::Util::MakePresetOrder(preset).displayName);
            }

            // A rule's presets have no order, one of them is picked at random
            if (const auto change{pickedList("##OBodyEditorPresetList", "OBodyEditorClearPresets", labels, false)}) {
                ApplyPickedChange(g_state.presets, *change);
            }
        }

        VerticalSpace(0.4f);

        if (ImGuiMCP::Button(Translate(g_state.editing ? "obody_editor_save_rule" : "obody_editor_add").c_str())) {
            AddRule(a_category);
        }

        if (g_state.editing || !g_state.targets.empty() || !g_state.presets.empty()) {
            ImGuiMCP::SameLine(0.0f, ImGuiMCP::GetStyle()->ItemSpacing.x * 3.0f);

            if (ImGuiMCP::Button(Translate("obody_editor_cancel").c_str())) {
                ResetForm();
            }
        }
    }

    void RenderMessage() {
        if (!g_state.message.empty() && g_state.messageAt.empty()) {
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

    RenderRuleForm(category, a_actor);

    VerticalSpace(0.6f);
    RenderMessage();

    VerticalSpace(0.6f);
    ImGuiMCP::Separator();
    VerticalSpace(0.6f);

    SectionHeader(Translate("obody_editor_rules_header"));
    RenderRules(category);
}

void UI::ConfigEditor::Reset() {
    g_state.message.clear();
    g_state.messageAt.clear();
    g_search[0] = '\0';
    g_ruleSearch[0] = '\0';

    g_ruleDrag.reset();
    g_groupDrag.reset();
    g_scrollToForm = false;
    g_formFlashAt.reset();
    g_movedFlash.reset();
}
