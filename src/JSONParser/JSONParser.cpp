#include "Body/Body.h"
#include "JSONParser/JSONParser.h"

#include "STL.h"

Parser::JSONParser Parser::JSONParser::instance;

namespace Parser {
    JSONParser& JSONParser::GetInstance() { return instance; }

    bool GetHasSourceFileArray(const RE::TESForm* form) {
        return form->sourceFiles.array; // Check if the source files array exists
    }

    std::string GetNthFormLocationName(const RE::TESForm* form, const uint32_t n) {
        std::string formName;

        if (GetHasSourceFileArray(form) && form->sourceFiles.array->size() > n) {
            RE::TESFile** sourceFiles = form->sourceFiles.array->data();
            formName = sourceFiles[n]->fileName;
        }

        // fix for weird bug where refs first defined in Skyrim.Esm aren't always detected properly
        if (((form->formID & 0xFF000000) ==
             0) // Refs from Skyrim.ESM will have 00 for the first two hexadecimal digits
            && formName != "Skyrim.esm") // And refs from all other mods will have a non-zero value, so a bitwise && of
        // those two digits with FF will be nonzero for all non Skyrim.ESM mods
        {
            return "Skyrim.esm";
        }

        return formName;
    }

    bool IsActorInForm(const RE::TESNPC* form, const std::string_view target) {
        if (GetHasSourceFileArray(form) && !form->sourceFiles.array->empty()) {
            RE::TESFile** sourceFiles = form->sourceFiles.array->data();

            for (int i = 0; i < form->sourceFiles.array->size(); i++) {
                if (sourceFiles[i]->fileName == target) {
                    return true;
                }
            }
        }

        return false;
    }

    bool JSONParser::IsActorInBlacklistedCharacterCategorySet(const uint32_t formID) const {
        for (const auto a_formID : blacklistedCharacterCategorySet | std::views::transform(&categorizedList::formID)) {
            if (a_formID == formID) {
                return true;
            }
        }

        return false;
    }

    bool JSONParser::IsOutfitInBlacklistedOutfitCategorySet(const uint32_t formID) {
        for (const auto a_formID : blacklistedOutfitCategorySet | std::views::transform(&categorizedList::formID)) {
            if (a_formID == formID) {
                return true;
            }
        }

        return false;
    }

    bool JSONParser::IsOutfitInForceRefitCategorySet(const uint32_t formID) const {
        for (const auto a_formID : forceRefitOutfitCategorySet | std::views::transform(&categorizedList::formID)) {
            if (a_formID == formID) {
                return true;
            }
        }

        return false;
    }

    categorizedList JSONParser::GetNPCFromCategorySet(const uint32_t formID) const {
        for (const categorizedList& character : characterCategorySet) {
            if (character.formID == formID) {
                return character;
            }
        }

        return {};
    }

    std::string DiscardFormDigits(const std::string_view formID) {
        std::string newFormID{formID};

        if (formID.size() == 8) {
            newFormID.erase(0, 2);
        }

        return newFormID;
    }

    void JSONParser::ProcessNPCsFormID() {
        if (presetDistributionConfig.contains("npcFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            for (auto& [owningMod, value] : presetDistributionConfig["npcFormID"].items()) {
                if (!data_handler->LookupModByName(owningMod)) {
                    logger::info("removed '{}' from NPC FormID(Plugin file Not Loaded)", owningMod);
                    continue;
                }
                for (auto& [formKey, formValue] : value.items()) {
                    stl::RemoveDuplicatesInJsonArray(formValue);
                    std::string formID = DiscardFormDigits(formKey);

                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto actorform = data_handler->LookupForm(hexnumber, owningMod);

                    if (!actorform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = actorform->GetFormID();

                    characterCategorySet.emplace_back(owningMod, ID, formValue);
                }
            }
        }
    }

    void JSONParser::ProcessNPCsFormIDBlacklist() {
        if (presetDistributionConfig.contains("blacklistedNpcsFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            for (auto& [plugin, val] : presetDistributionConfig["blacklistedNpcsFormID"].items()) {
                if (!data_handler->LookupModByName(plugin)) {
                    logger::info("removed '{}' from NPC Blacklist(Plugin file Not Loaded)", plugin);
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val);
                for (const std::string_view formIDRaw : val) {
                    std::string formID{DiscardFormDigits(formIDRaw)};
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto actorform = data_handler->LookupForm(hexnumber, plugin);

                    if (!actorform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = actorform->GetFormID();

                    blacklistedCharacterCategorySet.emplace_back(plugin, ID);
                }
            }
        }
    }

    void JSONParser::ProcessOutfitsFormIDBlacklist() {
        if (presetDistributionConfig.contains("blacklistedOutfitsFromORefitFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            for (auto& [plugin, val] : presetDistributionConfig["blacklistedOutfitsFromORefitFormID"].items()) {
                if (!data_handler->LookupModByName(plugin)) {
                    logger::info("removed '{}' from Outfit FormID Blacklist(Plugin file Not Loaded)", plugin);
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val);
                for (const std::string_view formIDRaw : val) {
                    std::string formID = DiscardFormDigits(formIDRaw);
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto outfitform = data_handler->LookupForm(hexnumber, plugin);

                    if (!outfitform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = outfitform->GetFormID();

                    blacklistedOutfitCategorySet.emplace_back(plugin, ID);
                }
            }
        }
    }

    void JSONParser::ProcessOutfitsForceRefitFormIDBlacklist() {
        if (presetDistributionConfig.contains("outfitsForceRefitFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            for (auto& [plugin, val] : presetDistributionConfig["outfitsForceRefitFormID"].items()) {
                if (!data_handler->LookupModByName(plugin)) {
                    logger::info("removed '{}' from Outfits Force Refit FormID Blacklist(Plugin file Not Loaded)",
                                 plugin);
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val);
                for (const std::string_view formIDRaw : val) {
                    std::string formID = DiscardFormDigits(formIDRaw);
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto outfitform = data_handler->LookupForm(hexnumber, plugin);

                    if (!outfitform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = outfitform->GetFormID();

                    forceRefitOutfitCategorySet.emplace_back(plugin, ID);
                }
            }
        }
    }

    inline bool ValidateActor(const RE::Actor* const actor) {
        if (actor == nullptr || (actor->formFlags & RE::TESForm::RecordFlags::kDeleted) || (
                actor->inGameFormFlags & RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted) || (
                actor->inGameFormFlags & RE::TESForm::InGameFormFlag::kWantsDelete) || actor->GetFormID() == 0 || (
                actor->formFlags & RE::TESForm::RecordFlags::kDisabled))
            return false;

        return true;
    }

    void JSONParser::FilterOutNonLoaded() {
        logger::info("{:-^47}", "Starting: Removing Not-Loaded Items");
        auto* const data_handler{RE::TESDataHandler::GetSingleton()};
        if (presetDistributionConfig.contains("npc") || presetDistributionConfig.contains("blacklistedNpcs")) {
            std::set<std::string_view> npc_names;
            {
                const auto& [hashtable, lock]{RE::TESForm::GetAllForms()};
                const RE::BSReadLockGuard locker{lock};
                if (hashtable) {
                    for (auto& [_, form] : *hashtable) {
                        if (form) {
                            if (const auto* const actor{form->As<RE::Actor>()}; ValidateActor(actor)) {
                                npc_names.emplace(actor->GetBaseObject()->GetName());
                            }
                        }
                    }
                }
            }
            // for (auto [it,i]{std::make_pair(npc_names.begin(), 1llu)}; it != npc_names.end();) {
            //     logger::info("{}: {}", i++, *(it++));
            // }
            if (presetDistributionConfig.contains("npc")) {
                logger::info("{:-^47}", "npc");
                auto& original = presetDistributionConfig["npc"];
                for (auto it = original.begin(); it != original.end();) {
                    if (!npc_names.contains(it.key())) {
                        logger::info("removed '{}'", it.key());
                        it = original.erase(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it.value());
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.contains("blacklistedNpcs")) {
                logger::info("{:-^47}", "blacklistedNpcs");
                auto& original = presetDistributionConfig["blacklistedNpcs"];
                stl::RemoveDuplicatesInJsonArray(original);
                for (auto it = original.begin(); it != original.end();) {
                    if (!npc_names.contains(*it)) {
                        logger::info("removed '{}'", it->get<std::string>());
                        it = original.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        if (presetDistributionConfig.contains("factionFemale")) {
            logger::info("{:-^47}", "factionFemale");
            auto& original = presetDistributionConfig["factionFemale"];
            for (auto it = original.begin(); it != original.end();) {
                if (!RE::TESForm::LookupByEditorID(it.key())) {
                    logger::info("removed '{}'", it.key());
                    it = original.erase(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it.value());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.contains("factionMale")) {
            logger::info("{:-^47}", "factionMale");
            auto& original = presetDistributionConfig["factionMale"];
            for (auto it = original.begin(); it != original.end();) {
                if (!RE::TESForm::LookupByEditorID(it.key())) {
                    logger::info("removed '{}'", it.key());
                    it = original.erase(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it.value());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.contains("npcPluginFemale")) {
            logger::info("{:-^47}", "npcPluginFemale");
            auto& original = presetDistributionConfig["npcPluginFemale"];
            for (auto it = original.begin(); it != original.end();) {
                if (!data_handler->LookupModByName(it.key())) {
                    logger::info("removed '{}'", it.key());
                    it = original.erase(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it.value());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.contains("npcPluginMale")) {
            logger::info("{:-^47}", "npcPluginMale");
            auto& original = presetDistributionConfig["npcPluginMale"];
            for (auto it = original.begin(); it != original.end();) {
                if (!data_handler->LookupModByName(it.key())) {
                    logger::info("removed '{}'", it.key());
                    it = original.erase(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it.value());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.contains("raceFemale")
            || presetDistributionConfig.contains("raceMale")
            || presetDistributionConfig.contains("blacklistedRacesFemale")
            || presetDistributionConfig.contains("blacklistedRacesMale")) {
            auto d = data_handler->GetFormArray<RE::TESRace>() | std::views::transform([&](const RE::TESRace* race) {
                return stl::get_editorID(race->As<RE::TESForm>());
            });
            const std::set<std::string> d_set(d.begin(), d.end());
            if (presetDistributionConfig.contains("raceFemale")) {
                logger::info("{:-^47}", "raceFemale");
                auto& original = presetDistributionConfig["raceFemale"];
                for (auto it = original.begin(); it != original.end();) {
                    if (!d_set.contains(it.key())) {
                        logger::info("removed '{}'", it.key());
                        it = original.erase(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it.value());
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.contains("raceMale")) {
                logger::info("{:-^47}", "raceMale");
                auto& original = presetDistributionConfig["raceMale"];
                for (auto it = original.begin(); it != original.end();) {
                    if (!d_set.contains(it.key())) {
                        logger::info("removed '{}'", it.key());
                        it = original.erase(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it.value());
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.contains("blacklistedRacesFemale")) {
                logger::info("{:-^47}", "blacklistedRacesFemale");
                auto& original = presetDistributionConfig["blacklistedRacesFemale"];
                stl::RemoveDuplicatesInJsonArray(original);
                for (auto it = original.begin(); it != original.end();) {
                    if (!d_set.contains(*it)) {
                        logger::info("removed '{}'", it->get<std::string>());
                        it = original.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.contains("blacklistedRacesMale")) {
                logger::info("{:-^47}", "blacklistedRacesMale");
                auto& original = presetDistributionConfig["blacklistedRacesMale"];
                stl::RemoveDuplicatesInJsonArray(original);
                for (auto it = original.begin(); it != original.end();) {
                    if (!d_set.contains(*it)) {
                        logger::info("removed '{}'", it->get<std::string>());
                        it = original.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        if (presetDistributionConfig.contains("blacklistedNpcsPluginFemale")) {
            logger::info("{:-^47}", "blacklistedNpcsPluginFemale");
            auto& original = presetDistributionConfig["blacklistedNpcsPluginFemale"];
            stl::RemoveDuplicatesInJsonArray(original);
            for (auto it = original.begin(); it != original.end();) {
                if (!data_handler->LookupModByName(*it)) {
                    logger::info("removed '{}'", it->get<std::string>());
                    it = original.erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.contains("blacklistedNpcsPluginMale")) {
            logger::info("{:-^47}", "blacklistedNpcsPluginMale");
            auto& original = presetDistributionConfig["blacklistedNpcsPluginMale"];
            stl::RemoveDuplicatesInJsonArray(original);
            for (auto it = original.begin(); it != original.end();) {
                if (!data_handler->LookupModByName(*it)) {
                    logger::info("removed '{}'", it->get<std::string>());
                    it = original.erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.contains("blacklistedOutfitsFromORefit")
            || presetDistributionConfig.contains("outfitsForceRefit")) {
            auto d = data_handler->GetFormArray<RE::TESObjectARMO>() |
                     std::views::transform([](const RE::TESObjectARMO* outfit) { return outfit->GetName(); });
            const std::set<std::string> d_set(d.begin(), d.end());
            if (presetDistributionConfig.contains("blacklistedOutfitsFromORefit")) {
                logger::info("{:-^47}", "blacklistedOutfitsFromORefit");
                auto& original = presetDistributionConfig["blacklistedOutfitsFromORefit"];
                stl::RemoveDuplicatesInJsonArray(original);
                for (auto it = original.begin(); it != original.end();) {
                    if (!d_set.contains(*it)) {
                        logger::info("removed '{}'", it->get<std::string>());
                        it = original.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.contains("outfitsForceRefit")) {
                logger::info("{:-^47}", "outfitsForceRefit");
                auto& original = presetDistributionConfig["outfitsForceRefit"];
                stl::RemoveDuplicatesInJsonArray(original);
                for (auto it = original.begin(); it != original.end();) {
                    if (!d_set.contains(*it)) {
                        logger::info("removed '{}'", it->get<std::string>());
                        it = original.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        if (presetDistributionConfig.contains("blacklistedOutfitsFromORefitPlugin")) {
            logger::info("{:-^47}", "blacklistedOutfitsFromORefitPlugin");
            auto& original = presetDistributionConfig["blacklistedOutfitsFromORefitPlugin"];
            stl::RemoveDuplicatesInJsonArray(original);
            for (auto it = original.begin(); it != original.end();) {
                if (!data_handler->LookupModByName(*it)) {
                    logger::info("removed '{}'", it->get<std::string>());
                    it = original.erase(it);
                } else {
                    ++it;
                }
            }
        }
        logger::info("{:-^47}", "Finished: Removing Not-Loaded Items");
    }

    void JSONParser::ProcessJSONCategories() {
        class timeit {
        public:
            explicit timeit(const std::source_location& a_curr = std::source_location::current()): curr(
                std::move(a_curr)) {
            }

            ~timeit() {
                const auto stop = std::chrono::steady_clock::now() - start;
                logger::info(
                    "Time Taken in '{}' is {} nanoseconds or {} microseconds or {} milliseconds or {} seconds or "
                    "{} minutes",
                    curr.function_name(), stop.count(),
                    std::chrono::duration_cast<std::chrono::microseconds>(stop).count(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(stop).count(),
                    std::chrono::duration_cast<std::chrono::seconds>(stop).count(),
                    std::chrono::duration_cast<std::chrono::minutes>(stop).count());
            }

        private:
            std::source_location curr;
            std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
        };
        timeit const t;
        ProcessNPCsFormIDBlacklist();
        ProcessNPCsFormID();
        ProcessOutfitsFormIDBlacklist();
        ProcessOutfitsForceRefitFormIDBlacklist();
        FilterOutNonLoaded();
        logger::info("After Filtering: \n{}", presetDistributionConfig.dump(4));
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsStringInJsonConfigKey(std::string a_value, const char* key) {
        boost::trim(a_value);

        return presetDistributionConfig.contains(key) &&
               std::find(presetDistributionConfig[key].begin(), presetDistributionConfig[key].end(), a_value) !=
               presetDistributionConfig[key].end();
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsSubKeyInJsonConfigKey(const char* key, std::string subKey) {
        boost::trim(subKey);

        return presetDistributionConfig.contains(key) && presetDistributionConfig[key].contains(subKey);
    }

    bool JSONParser::IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit) {
        return IsStringInJsonConfigKey(a_outfit.GetName(), "blacklistedOutfitsFromORefit") ||
               IsOutfitInBlacklistedOutfitCategorySet(a_outfit.GetFormID()) ||
               IsStringInJsonConfigKey(GetNthFormLocationName(a_outfit.As<RE::TESForm>(), 0),
                                       "blacklistedOutfitsFromORefitPlugin");
    }

    bool JSONParser::IsAnyForceRefitItemEquipped(RE::Actor* a_actor, const bool a_removingArmor,
                                                 const RE::TESForm* a_equippedArmor) {
        auto inventory = a_actor->GetInventory() | std::views::transform([](const auto& pair) {
            return std::pair<RE::TESBoundObject*, const std::unique_ptr<RE::InventoryEntryData>&>(
                pair.first, pair.second.second); // Return the unique_ptr directly
        });

        std::vector<std::string> wornItems;

        for (const auto& [bound_obj, inventory_entry_data] : inventory) {
            if (inventory_entry_data->IsWorn()) {
                // Check if the item is being unequipped or not first
                if (a_removingArmor && bound_obj->GetFormID() == a_equippedArmor->GetFormID()) {
                    continue;
                }

                if (const RE::FormType itemFormType = bound_obj->GetFormType();
                    (itemFormType == RE::FormType::Armor || itemFormType == RE::FormType::Armature) &&
                    IsStringInJsonConfigKey(inventory_entry_data->GetDisplayName(), "outfitsForceRefit") ||
                    IsOutfitInForceRefitCategorySet(bound_obj->GetFormID())) {
                    logger::info("Outfit {} is in force refit list", inventory_entry_data->GetDisplayName());

                    return true;
                }
            }
        }

        return false;
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsNPCBlacklisted(const std::string actorName, const uint32_t actorID) {
        if (IsStringInJsonConfigKey(actorName, "blacklistedNpcs")) {
            logger::info("{} is Blacklisted by blacklistedNpcs", actorName);
            return true;
        }

        if (IsActorInBlacklistedCharacterCategorySet(actorID)) {
            logger::info("{} is Blacklisted by character category set", actorName);
            return true;
        }

        return false;
    }

    bool JSONParser::IsNPCBlacklistedGlobally(const RE::Actor* a_actor, const char* actorRace, const bool female) {
        auto actorOwningMod = GetNthFormLocationName(a_actor, 0);

        if (female) {
            return IsStringInJsonConfigKey(actorOwningMod, "blacklistedNpcsPluginFemale") ||
                   IsStringInJsonConfigKey(actorRace, "blacklistedRacesFemale");
        }
        return IsStringInJsonConfigKey(actorOwningMod, "blacklistedNpcsPluginMale") ||
               IsStringInJsonConfigKey(actorRace, "blacklistedRacesMale");
    }

    PresetManager::Preset JSONParser::GetNPCFactionPreset(const RE::TESNPC* a_actor, const bool female) {
        auto actorRanks{a_actor->factions | std::views::transform(&RE::FACTION_RANK::faction)};

        const std::vector<RE::TESFaction*> actorFactions{actorRanks.begin(), actorRanks.end()};

        if (actorFactions.empty()) {
            return {};
        }

        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        const PresetManager::PresetSet presetSet{female
                                                     ? presetContainer.allFemalePresets
                                                     : presetContainer.allMalePresets};

        const char* factionKey{female ? "factionFemale" : "factionMale"};

        for (auto& [key, value] : presetDistributionConfig[factionKey].items()) {
            if (std::ranges::find(actorFactions, RE::TESFaction::LookupByEditorID(key)) != actorFactions.end()) {
                return PresetManager::GetRandomPresetByName(presetSet, value, female);
            }
        }

        return {};
    }

    PresetManager::Preset JSONParser::GetNPCPreset(const char* actorName, const uint32_t formID, const bool female) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        const PresetManager::PresetSet presetSet{female
                                                     ? presetContainer.allFemalePresets
                                                     : presetContainer.allMalePresets};

        const auto character{GetNPCFromCategorySet(formID)};

        const std::vector characterBodyslidePresets{
            ((!character.bodyslidePresets.empty())
                 ? std::vector<std::string_view>(character.bodyslidePresets.begin(), character.bodyslidePresets.end())
                 : (presetDistributionConfig.contains("npc") && presetDistributionConfig["npc"].contains(actorName)
                        ? presetDistributionConfig["npc"][actorName].get<std::vector<std::string_view>>()
                        : std::vector<std::string_view>()))};
        return PresetManager::GetRandomPresetByName(presetSet, characterBodyslidePresets, female);
    }

    PresetManager::Preset JSONParser::GetNPCPluginPreset(const RE::TESNPC* a_actor, const char* actorName,
                                                         const bool female) {
        const char* keyForPreset{female ? "npcPluginFemale" : "npcPluginMale"};

        if (presetDistributionConfig.contains(keyForPreset)) {
            for (auto& [mod, presetList] : presetDistributionConfig[keyForPreset].items()) {
                logger::info("Checking if actor {} is in mod {}", actorName, mod);

                if (IsActorInForm(a_actor, mod)) {
                    const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

                    const PresetManager::PresetSet presetSet{female
                                                                 ? presetContainer.allFemalePresets
                                                                 : presetContainer.allMalePresets};

                    return GetRandomPresetByName(presetSet, presetDistributionConfig[keyForPreset][mod], female);
                }
            }
        }

        return {};
    }

    PresetManager::Preset JSONParser::GetNPCRacePreset(const char* actorRace, const bool female) {
        const char* key{female ? "raceFemale" : "raceMale"};

        if (IsSubKeyInJsonConfigKey(key, actorRace)) {
            const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

            const PresetManager::PresetSet presetSet{female
                                                         ? presetContainer.allFemalePresets
                                                         : presetContainer.allMalePresets};

            return PresetManager::GetRandomPresetByName(presetSet, presetDistributionConfig[key][actorRace], female);
        }

        return {};
    }
} // namespace Parser