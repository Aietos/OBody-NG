#include "UI/Lookups.h"

#include "ActorTracker/ActorTracker.h"
#include "Body/Body.h"
#include "PresetManager/PresetManager.h"
#include "STL.h"
#include "UI/Util.h"

namespace {
    using UI::Lookups::Item;
    using UI::Lookups::Kind;

    std::array<std::vector<Item>, static_cast<std::size_t>(Kind::kTotal)> g_lists;
    bool g_built{false};

    std::vector<Item>& List(const Kind a_kind) { return g_lists[static_cast<std::size_t>(a_kind)]; }

    // The game keeps the editor IDs of races and keywords, the others come from powerofthree's Tweaks
    std::string EditorID(const RE::TESForm* a_form) {
        switch (a_form->GetFormType()) {
            case RE::FormType::Race:
            case RE::FormType::Keyword:
                return a_form->GetFormEditorID();
            default: {
                const char* const editorID{stl::func ? stl::func(a_form->GetFormID()) : nullptr};

                return editorID ? editorID : "";
            }
        }
    }

    std::string FormIDText(const RE::TESForm* a_form) { return std::format("{:08X}", a_form->GetLocalFormID()); }

    std::string PluginOf(const RE::TESForm* a_form) {
        const auto* const file{a_form->GetFile(0)};

        return file ? std::string{file->GetFilename()} : std::string{};
    }

    void Add(const Kind a_kind, std::string a_label, std::string a_value, std::string a_plugin = {},
             const bool a_female = false) {
        auto search{UI::Util::ToLowerWide(UI::Util::Utf8ToWide(a_label))};
        List(a_kind).push_back(
            {std::move(a_label), std::move(a_value), std::move(a_plugin), std::move(search), a_female});
    }

    std::string EditorIDLabel(const std::string& a_editorID, const char* a_name) {
        const std::string_view name{a_name ? a_name : ""};

        return name.empty() || name == a_editorID ? a_editorID : std::format("{} ({})", a_editorID, name);
    }

    void SortByLabel(std::vector<Item>& a_items) {
        std::ranges::stable_sort(
            a_items, [](const Item& a, const Item& b) { return UI::Util::CompareLinguistic(a.search, b.search) < 0; });
    }

    void BuildPlugins(RE::TESDataHandler* a_dataHandler) {
        std::vector<const RE::TESFile*> files;

        for (const auto* const file : a_dataHandler->files) {
            if (file && file->compileIndex != 0xFF) {
                files.push_back(file);
            }
        }

        // In the load order, light plugins by their own index
        std::ranges::sort(files, {}, [](const RE::TESFile* a_file) {
            return (static_cast<std::uint32_t>(a_file->compileIndex) << 16) | a_file->smallFileCompileIndex;
        });

        for (const auto* const file : files) {
            Add(Kind::kPlugin, std::string{file->GetFilename()}, std::string{file->GetFilename()});
        }
    }

    void BuildNpcs(RE::TESDataHandler* a_dataHandler) {
        std::set<std::string> names;

        for (const auto* const npc : a_dataHandler->GetFormArray<RE::TESNPC>()) {
            if (!npc || npc->IsPlayer() || npc->IsDynamicForm()) continue;

            const std::string name{npc->GetName() ? npc->GetName() : ""};
            const auto plugin{PluginOf(npc)};

            if (plugin.empty()) continue;

            const auto formID{FormIDText(npc)};
            const auto shownName{name.empty() ? EditorID(npc) : name};
            const bool female{npc->GetSex() == RE::SEX::kFemale};

            Add(Kind::kNpcRecord, std::format("{} ({}, {})", shownName.empty() ? "?" : shownName, plugin, formID),
                formID, plugin, female);

            if (!name.empty()) {
                names.insert(name);
            }
        }

        for (const auto& name : names) {
            Add(Kind::kNpcName, name, name);
        }

        SortByLabel(List(Kind::kNpcRecord));
        SortByLabel(List(Kind::kNpcName));
    }

    template <typename T>
    void BuildByEditorID(RE::TESDataHandler* a_dataHandler, const Kind a_kind) {
        for (const auto* const form : a_dataHandler->GetFormArray<T>()) {
            if (!form) continue;

            const auto editorID{EditorID(form)};

            // The config names them by editor ID, one without it can't be named
            if (editorID.empty()) continue;

            Add(a_kind, EditorIDLabel(editorID, form->GetName()), editorID);
        }

        SortByLabel(List(a_kind));
    }

    void BuildArmors(RE::TESDataHandler* a_dataHandler) {
        std::set<std::string> names;
        std::set<std::string> keywords{"ArmorHeavy", "ArmorLight", "ArmorClothing"};

        for (const auto* const armor : a_dataHandler->GetFormArray<RE::TESObjectARMO>()) {
            if (!armor) continue;

            const std::string name{armor->GetName() ? armor->GetName() : ""};
            const auto plugin{PluginOf(armor)};

            // The armors without a name are skins and the like, nobody wears them as an outfit
            if (name.empty() || plugin.empty()) continue;

            const auto formID{FormIDText(armor)};

            Add(Kind::kArmorRecord, std::format("{} ({}, {})", name, plugin, formID), formID, plugin);
            names.insert(name);

            for (const auto* const keyword : armor->GetKeywords()) {
                if (keyword && keyword->GetFormEditorID() && *keyword->GetFormEditorID()) {
                    keywords.insert(keyword->GetFormEditorID());
                }
            }
        }

        for (const auto& name : names) {
            Add(Kind::kArmorName, name, name);
        }

        for (const auto& keyword : keywords) {
            Add(Kind::kKeyword, keyword, keyword);
        }

        SortByLabel(List(Kind::kArmorRecord));
        SortByLabel(List(Kind::kArmorName));
        SortByLabel(List(Kind::kKeyword));
    }

    void AddPresets(const Kind a_kind, std::vector<std::pair<UI::Util::PresetOrder, bool>>& a_presets) {
        std::ranges::stable_sort(a_presets, UI::Util::IsBeforeInPresetOrder,
                                 &std::pair<UI::Util::PresetOrder, bool>::first);

        for (auto& [order, ube] : a_presets) {
            Add(a_kind, ube ? order.displayName + " (UBE)" : order.displayName, order.name);
        }
    }

    void BuildPresets() {
        const auto& container{PresetManager::PresetContainer::GetInstance()};
        std::set<std::string> anySexNames;
        std::vector<std::pair<UI::Util::PresetOrder, bool>> anySex;

        const auto add = [&](const PresetManager::PresetSet& a_presets, const Kind a_normal, const Kind a_refit) {
            std::vector<std::pair<UI::Util::PresetOrder, bool>> normal;
            std::vector<std::pair<UI::Util::PresetOrder, bool>> refit;

            for (const auto& preset : a_presets) {
                const bool isRefit{PresetManager::IsRefitPreset(preset.name)};

                (isRefit ? refit : normal).emplace_back(UI::Util::MakePresetOrder(preset.name), preset.ube);

                if (!isRefit && anySexNames.insert(preset.name).second) {
                    anySex.emplace_back(UI::Util::MakePresetOrder(preset.name), false);
                }
            }

            AddPresets(a_normal, normal);
            AddPresets(a_refit, refit);
        };

        add(container.allFemalePresets, Kind::kPresetFemale, Kind::kRefitFemale);
        add(container.allMalePresets, Kind::kPresetMale, Kind::kRefitMale);
        AddPresets(Kind::kPreset, anySex);
    }

    void Build() {
        auto* const dataHandler{RE::TESDataHandler::GetSingleton()};

        g_built = true;

        BuildPlugins(dataHandler);
        BuildNpcs(dataHandler);
        BuildByEditorID<RE::TESFaction>(dataHandler, Kind::kFaction);
        BuildByEditorID<RE::TESRace>(dataHandler, Kind::kRace);
        BuildByEditorID<RE::TESClass>(dataHandler, Kind::kClass);
        BuildArmors(dataHandler);
        BuildPresets();

        logger::info(
            "Config Editor lists: {} plugins, {} NPC records, {} NPC names, {} factions, {} races, {} classes, {} "
            "armor "
            "records, {} armor names, {} keywords, {} presets",
            List(Kind::kPlugin).size(), List(Kind::kNpcRecord).size(), List(Kind::kNpcName).size(),
            List(Kind::kFaction).size(), List(Kind::kRace).size(), List(Kind::kClass).size(),
            List(Kind::kArmorRecord).size(), List(Kind::kArmorName).size(), List(Kind::kKeyword).size(),
            List(Kind::kPreset).size());

        if (!stl::func) {
            logger::warn("powerofthree's Tweaks isn't there, so the factions and classes can't be listed");
        }
    }

    // The armors a_actor wears
    std::vector<const RE::TESObjectARMO*> WornArmors(RE::Actor* a_actor) {
        std::vector<const RE::TESObjectARMO*> armors;

        for (const auto* const entry : stl::GetWornEntries(a_actor)) {
            if (const auto* const armor{entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr}) {
                armors.push_back(armor);
            }
        }

        return armors;
    }

    // What a_actor has, as {plugin, value} the items of a_kind hold
    std::vector<std::pair<std::string, std::string>> ActorValues(const Kind a_kind, RE::Actor* a_actor) {
        std::vector<std::pair<std::string, std::string>> values;
        const auto* const base{a_actor->GetActorBase()};

        if (!base) return values;

        switch (a_kind) {
            case Kind::kPlugin:
                values.emplace_back("", PluginOf(base));
                values.emplace_back("", PluginOf(a_actor));
                break;
            case Kind::kNpcRecord:
                // A leveled NPC's record is made as they spawn, the config can't name it
                if (!base->IsDynamicForm()) {
                    values.emplace_back(PluginOf(base), FormIDText(base));
                }
                break;
            case Kind::kNpcName:
                values.emplace_back("", base->GetName() ? base->GetName() : "");
                break;
            case Kind::kFaction:
                for (const auto& rank : base->factions) {
                    if (rank.faction) {
                        values.emplace_back("", EditorID(rank.faction));
                    }
                }
                break;
            case Kind::kRace:
                if (const auto* const race{a_actor->GetRace()}) {
                    values.emplace_back("", EditorID(race));
                }
                break;
            case Kind::kClass:
                if (base->npcClass) {
                    values.emplace_back("", EditorID(base->npcClass));
                }
                break;
            case Kind::kArmorRecord:
            case Kind::kArmorName:
            case Kind::kKeyword:
                for (const auto* const armor : WornArmors(a_actor)) {
                    if (a_kind == Kind::kArmorRecord) {
                        values.emplace_back(PluginOf(armor), FormIDText(armor));
                    } else if (a_kind == Kind::kArmorName) {
                        values.emplace_back("", armor->GetName() ? armor->GetName() : "");
                    } else {
                        for (const auto* const keyword : armor->GetKeywords()) {
                            if (keyword && keyword->GetFormEditorID()) {
                                values.emplace_back("", keyword->GetFormEditorID());
                            }
                        }
                    }
                }
                break;
            case Kind::kPreset:
            case Kind::kPresetFemale:
            case Kind::kPresetMale:
                if (const auto preset{ActorTracker::Registry::GetInstance().GetPresetNameForActor(
                        a_actor, Body::OBody::IsFemale(a_actor))}) {
                    values.emplace_back("", *preset);
                }
                break;
            case Kind::kRefitFemale:
            case Kind::kRefitMale:
            case Kind::kTotal:
                break;
        }

        return values;
    }
}  // namespace

const std::vector<Item>& UI::Lookups::Get(const Kind a_kind) {
    if (!g_built) {
        Build();
    }

    return List(a_kind);
}

std::vector<const Item*> UI::Lookups::FromActor(const Kind a_kind, RE::Actor* a_actor) {
    std::vector<const Item*> items;

    if (!a_actor) return items;

    const auto values{ActorValues(a_kind, a_actor)};

    if (values.empty()) return items;

    for (const auto& item : Get(a_kind)) {
        const bool has{std::ranges::any_of(
            values, [&](const auto& a_value) { return a_value.first == item.plugin && a_value.second == item.value; })};

        if (has && std::ranges::find(items, &item) == items.end()) {
            items.push_back(&item);
        }
    }

    return items;
}
