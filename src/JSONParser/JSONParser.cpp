#include "Body/Body.h"
#include "JSONParser/JSONParser.h"

#include "STL.h"

Parser::JSONParser Parser::JSONParser::instance;

namespace Parser {
    JSONParser& JSONParser::GetInstance() { return instance; }

    bool GetHasSourceFileArray(const RE::TESForm* form) {
        return form->sourceFiles.array;  // Check if the source files array exists
    }

    std::string GetNthFormLocationName(const RE::TESForm* form, const uint32_t n) {
        std::string formName;

        if (GetHasSourceFileArray(form) && form->sourceFiles.array->size() > n) {
            RE::TESFile** sourceFiles = form->sourceFiles.array->data();
            formName = sourceFiles[n]->fileName;
        }

        // fix for weird bug where refs first defined in Skyrim.Esm aren't always detected properly
        // Refs from Skyrim.ESM will have 00 for the first two hexadecimal digits
        // And refs from all other mods will have a non-zero value, so a bitwise && of
        // those two digits with FF will be nonzero for all non Skyrim.ESM mods
        if (((form->formID & 0xFF000000) == 0) && formName != "Skyrim.esm") {
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
        if (presetDistributionConfig.HasMember("npcFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& npcFormID = presetDistributionConfig["npcFormID"];
            for (auto itr = npcFormID.MemberBegin(); itr != npcFormID.MemberEnd(); ++itr) {
                auto& [owningMod, value] = *itr;
                if (!data_handler->LookupModByName(owningMod.GetString())) {
                    logger::info("removed '{}' from NPC FormID(Plugin file Not Loaded)", owningMod.GetString());
                    continue;
                }
                for (auto valueItr = value.MemberBegin(); valueItr != value.MemberEnd(); ++valueItr) {
                    auto& [formKey, formValue] = *valueItr;
                    stl::RemoveDuplicatesInJsonArray(formValue, presetDistributionConfig.GetAllocator());
                    std::string formID = DiscardFormDigits(formKey.GetString());

                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto actorform = data_handler->LookupForm(hexnumber, owningMod.GetString());

                    if (!actorform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = actorform->GetFormID();
                    std::vector<std::string> bodyslidePresets(formValue.GetArray().Size());
                    for (const auto& item : formValue.GetArray()) {
                        bodyslidePresets.emplace_back(item.GetString());
                    }

                    characterCategorySet.emplace_back(owningMod.GetString(), ID, std::move(bodyslidePresets));
                }
            }
        }
    }

    void JSONParser::ProcessNPCsFormIDBlacklist() {
        if (presetDistributionConfig.HasMember("blacklistedNpcsFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& blacklistedNpcsFormID = presetDistributionConfig["blacklistedNpcsFormID"];
            for (auto itr = blacklistedNpcsFormID.MemberBegin(); itr != blacklistedNpcsFormID.MemberEnd(); ++itr) {
                auto& [plugin, val] = *itr;
                if (!data_handler->LookupModByName(plugin.GetString())) {
                    logger::info("removed '{}' from NPC Blacklist(Plugin file Not Loaded)", plugin.GetString());
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val, presetDistributionConfig.GetAllocator());
                for (const auto& formIDRaw : val.GetArray()) {
                    std::string formID{DiscardFormDigits(formIDRaw.GetString())};
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto actorform = data_handler->LookupForm(hexnumber, plugin.GetString());

                    if (!actorform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = actorform->GetFormID();

                    blacklistedCharacterCategorySet.emplace_back(plugin.GetString(), ID);
                }
            }
        }
    }

    void JSONParser::ProcessOutfitsFormIDBlacklist() {
        if (presetDistributionConfig.HasMember("blacklistedOutfitsFromORefitFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& blacklistedOutfitsFromORefitFormID = presetDistributionConfig["blacklistedOutfitsFromORefitFormID"];
            for (auto itr = blacklistedOutfitsFromORefitFormID.MemberBegin();
                 itr != blacklistedOutfitsFromORefitFormID.MemberEnd(); ++itr) {
                auto& [plugin, val] = *itr;
                if (!data_handler->LookupModByName(plugin.GetString())) {
                    logger::info("removed '{}' from Outfit FormID Blacklist(Plugin file Not Loaded)",
                                 plugin.GetString());
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val, presetDistributionConfig.GetAllocator());
                for (const auto& formIDRaw : val.GetArray()) {
                    std::string formID = DiscardFormDigits(formIDRaw.GetString());
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto outfitform = data_handler->LookupForm(hexnumber, plugin.GetString());

                    if (!outfitform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = outfitform->GetFormID();

                    blacklistedOutfitCategorySet.emplace_back(plugin.GetString(), ID);
                }
            }
        }
    }

    void JSONParser::ProcessOutfitsForceRefitFormIDBlacklist() {
        if (presetDistributionConfig.HasMember("outfitsForceRefitFormID")) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& outfitsForceRefitFormID = presetDistributionConfig["outfitsForceRefitFormID"];
            for (auto itr = outfitsForceRefitFormID.MemberBegin(); itr != outfitsForceRefitFormID.MemberEnd(); ++itr) {
                auto& [plugin, val] = *itr;
                if (!data_handler->LookupModByName(plugin.GetString())) {
                    logger::info("removed '{}' from Outfits Force Refit FormID Blacklist(Plugin file Not Loaded)",
                                 plugin.GetString());
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val, presetDistributionConfig.GetAllocator());
                for (const auto& formIDRaw : val.GetArray()) {
                    std::string formID = DiscardFormDigits(formIDRaw.GetString());
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    const auto outfitform = data_handler->LookupForm(hexnumber, plugin.GetString());

                    if (!outfitform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = outfitform->GetFormID();

                    forceRefitOutfitCategorySet.emplace_back(plugin.GetString(), ID);
                }
            }
        }
    }

    inline bool ValidateActor(const RE::Actor* const actor) {
        if (actor == nullptr || (actor->formFlags & RE::TESForm::RecordFlags::kDeleted) ||
            (actor->inGameFormFlags & RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted) ||
            (actor->inGameFormFlags & RE::TESForm::InGameFormFlag::kWantsDelete) || actor->GetFormID() == 0 ||
            (actor->formFlags & RE::TESForm::RecordFlags::kDisabled))
            return false;

        return true;
    }

    void JSONParser::FilterOutNonLoaded() {
        logger::info("{:-^47}", "Starting: Removing Not-Loaded Items");
        auto* const data_handler{RE::TESDataHandler::GetSingleton()};
        if (presetDistributionConfig.HasMember("npc") || presetDistributionConfig.HasMember("blacklistedNpcs")) {
            std::set<std::string> npc_names;
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
            if (presetDistributionConfig.HasMember("npc")) {
                logger::info("{:-^47}", "npc");
                auto& original = presetDistributionConfig["npc"];
                for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                    if (!npc_names.contains(it->name.GetString())) {
                        logger::info("removed '{}'", it->name.GetString());
                        it = original.EraseMember(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.HasMember("blacklistedNpcs")) {
                logger::info("{:-^47}", "blacklistedNpcs");
                auto& original = presetDistributionConfig["blacklistedNpcs"];
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!npc_names.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        if (presetDistributionConfig.HasMember("factionFemale")) {
            logger::info("{:-^47}", "factionFemale");
            auto& original = presetDistributionConfig["factionFemale"];
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!RE::TESForm::LookupByEditorID(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.HasMember("factionMale")) {
            logger::info("{:-^47}", "factionMale");
            auto& original = presetDistributionConfig["factionMale"];
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!RE::TESForm::LookupByEditorID(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.HasMember("npcPluginFemale")) {
            logger::info("{:-^47}", "npcPluginFemale");
            auto& original = presetDistributionConfig["npcPluginFemale"];
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!data_handler->LookupModByName(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.HasMember("npcPluginMale")) {
            logger::info("{:-^47}", "npcPluginMale");
            auto& original = presetDistributionConfig["npcPluginMale"];
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!data_handler->LookupModByName(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.HasMember("raceFemale") || presetDistributionConfig.HasMember("raceMale") ||
            presetDistributionConfig.HasMember("blacklistedRacesFemale") ||
            presetDistributionConfig.HasMember("blacklistedRacesMale")) {
            auto d = data_handler->GetFormArray<RE::TESRace>() | std::views::transform([&](const RE::TESRace* race) {
                         return stl::get_editorID(race->As<RE::TESForm>());
                     });
            const std::set<std::string> d_set(d.begin(), d.end());
            if (presetDistributionConfig.HasMember("raceFemale")) {
                logger::info("{:-^47}", "raceFemale");
                auto& original = presetDistributionConfig["raceFemale"];
                for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                    if (!d_set.contains(it->name.GetString())) {
                        logger::info("removed '{}'", it->name.GetString());
                        it = original.EraseMember(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.HasMember("raceMale")) {
                logger::info("{:-^47}", "raceMale");
                auto& original = presetDistributionConfig["raceMale"];
                for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                    if (!d_set.contains(it->name.GetString())) {
                        logger::info("removed '{}'", it->name.GetString());
                        it = original.EraseMember(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.HasMember("blacklistedRacesFemale")) {
                logger::info("{:-^47}", "blacklistedRacesFemale");
                auto& original = presetDistributionConfig["blacklistedRacesFemale"];
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.HasMember("blacklistedRacesMale")) {
                logger::info("{:-^47}", "blacklistedRacesMale");
                auto& original = presetDistributionConfig["blacklistedRacesMale"];
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        if (presetDistributionConfig.HasMember("blacklistedNpcsPluginFemale")) {
            logger::info("{:-^47}", "blacklistedNpcsPluginFemale");
            auto& original = presetDistributionConfig["blacklistedNpcsPluginFemale"];
            stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
            for (auto it = original.Begin(); it != original.End();) {
                if (!data_handler->LookupModByName(it->GetString())) {
                    logger::info("removed '{}'", it->GetString());
                    it = original.Erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.HasMember("blacklistedNpcsPluginMale")) {
            logger::info("{:-^47}", "blacklistedNpcsPluginMale");
            auto& original = presetDistributionConfig["blacklistedNpcsPluginMale"];
            stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
            for (auto it = original.Begin(); it != original.End();) {
                if (!data_handler->LookupModByName(it->GetString())) {
                    logger::info("removed '{}'", it->GetString());
                    it = original.Erase(it);
                } else {
                    ++it;
                }
            }
        }
        if (presetDistributionConfig.HasMember("blacklistedOutfitsFromORefit") ||
            presetDistributionConfig.HasMember("outfitsForceRefit")) {
            auto d = data_handler->GetFormArray<RE::TESObjectARMO>() |
                     std::views::transform([](const RE::TESObjectARMO* outfit) { return outfit->GetName(); });
            const std::set<std::string> d_set(d.begin(), d.end());
            if (presetDistributionConfig.HasMember("blacklistedOutfitsFromORefit")) {
                logger::info("{:-^47}", "blacklistedOutfitsFromORefit");
                auto& original = presetDistributionConfig["blacklistedOutfitsFromORefit"];
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            if (presetDistributionConfig.HasMember("outfitsForceRefit")) {
                logger::info("{:-^47}", "outfitsForceRefit");
                auto& original = presetDistributionConfig["outfitsForceRefit"];
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        if (presetDistributionConfig.HasMember("blacklistedOutfitsFromORefitPlugin")) {
            logger::info("{:-^47}", "blacklistedOutfitsFromORefitPlugin");
            auto& original = presetDistributionConfig["blacklistedOutfitsFromORefitPlugin"];
            stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
            for (auto it = original.Begin(); it != original.End();) {
                if (!data_handler->LookupModByName(it->GetString())) {
                    logger::info("removed '{}'", it->GetString());
                    it = original.Erase(it);
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
            explicit timeit(const std::source_location& a_curr = std::source_location::current())
                : curr(std::move(a_curr)) {}

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
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter writer(buffer);
        presetDistributionConfig.Accept(writer);

        logger::info("After Filtering: \n{}", buffer.GetString());
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsStringInJsonConfigKey(std::string a_value, const char* key) {
        boost::trim(a_value);

        return presetDistributionConfig.HasMember(key) &&
               std::find(presetDistributionConfig[key].Begin(), presetDistributionConfig[key].End(), a_value.c_str()) !=
                   presetDistributionConfig[key].End();
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsSubKeyInJsonConfigKey(const char* key, std::string subKey) {
        boost::trim(subKey);

        return presetDistributionConfig.HasMember(key) && presetDistributionConfig[key].HasMember(subKey.c_str());
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
                                 pair.first, pair.second.second);  // Return the unique_ptr directly
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
        const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                        : presetContainer.allMalePresets};

        auto& a_faction = presetDistributionConfig[female ? "factionFemale" : "factionMale"];

        for (auto itr = a_faction.MemberBegin(); itr != a_faction.MemberEnd(); ++itr) {
            auto& [key, value] = *itr;
            if (std::ranges::find(actorFactions, RE::TESFaction::LookupByEditorID(key.GetString())) !=
                actorFactions.end()) {
                std::vector<std::string_view> copy_of_value{value.Size()};
                for (const auto& item : value.GetArray()) {
                    copy_of_value.emplace_back(item.GetString());
                }

                return PresetManager::GetRandomPresetByName(presetSet, std::move(copy_of_value), female);
            }
        }

        return {};
    }

    PresetManager::Preset JSONParser::GetNPCPreset(const char* actorName, const uint32_t formID, const bool female) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                        : presetContainer.allMalePresets};

        const auto character{GetNPCFromCategorySet(formID)};

        std::vector<std::string_view> characterBodyslidePresets;

        if (!character.bodyslidePresets.empty()) {
            characterBodyslidePresets =
                std::vector<std::string_view>(character.bodyslidePresets.begin(), character.bodyslidePresets.end());
        } else if (presetDistributionConfig.HasMember("npc") && presetDistributionConfig["npc"].HasMember(actorName)) {
            characterBodyslidePresets.reserve(presetDistributionConfig["npc"][actorName].Size());
            for (const auto& item : presetDistributionConfig["npc"][actorName].GetArray()) {
                characterBodyslidePresets.emplace_back(item.GetString());
            }

        }  // else {
        // }

        return PresetManager::GetRandomPresetByName(presetSet, characterBodyslidePresets, female);
    }

    PresetManager::Preset JSONParser::GetNPCPluginPreset(const RE::TESNPC* a_actor, const char* actorName,
                                                         const bool female) {
        const char* keyForPreset{female ? "npcPluginFemale" : "npcPluginMale"};

        if (presetDistributionConfig.HasMember(keyForPreset)) {
            auto& presets = presetDistributionConfig[keyForPreset];
            for (auto itr = presets.MemberBegin(); itr != presets.MemberEnd(); ++itr) {
                auto& [mod, presetList] = *itr;
                logger::info("Checking if actor {} is in mod {}", actorName, mod.GetString());

                if (IsActorInForm(a_actor, mod.GetString())) {
                    const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

                    const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                                    : presetContainer.allMalePresets};

                    std::vector<std::string_view> presets_copy{presets[mod].Size()};
                    for (const auto& item : presets[mod].GetArray()) {
                        presets_copy.emplace_back(item.GetString());
                    }

                    return GetRandomPresetByName(presetSet, std::move(presets_copy), female);
                }
            }
        }

        return {};
    }

    PresetManager::Preset JSONParser::GetNPCRacePreset(const char* actorRace, const bool female) {
        const char* key{female ? "raceFemale" : "raceMale"};

        if (IsSubKeyInJsonConfigKey(key, actorRace)) {
            const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

            const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                            : presetContainer.allMalePresets};
            std::vector<std::string_view> presets_copy{presetDistributionConfig[key][actorRace].Size()};
            for (const auto& item : presetDistributionConfig[key][actorRace].GetArray()) {
                presets_copy.emplace_back(item.GetString());
            }
            return PresetManager::GetRandomPresetByName(presetSet, std::move(presets_copy), female);
        }

        return {};
    }
}  // namespace Parser