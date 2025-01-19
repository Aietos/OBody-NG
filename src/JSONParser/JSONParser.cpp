#include "Body/Body.h"
#include "JSONParser/JSONParser.h"

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
        if (((form->formID & 0xFF000000) ==
             0)                           // Refs from Skyrim.ESM will have 00 for the first two hexadecimal digits
            && formName != "Skyrim.esm")  // And refs from all other mods will have a non-zero value, so a bitwise && of
                                          // those two digits with FF will be nonzero for all non Skyrim.ESM mods
        {
            formName = "Skyrim.esm";
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
        categorizedList npc;

        for (const categorizedList& character : characterCategorySet) {
            if (character.formID == formID) {
                npc = character;
                break;
            }
        }

        return npc;
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
            for (auto& plugin : presetDistributionConfig["npcFormID"].items()) {
                std::string owningMod = plugin.key();

                for (auto& form : plugin.value().items()) {
                    std::string formID = DiscardFormDigits(form.key());

                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    auto datahandler{RE::TESDataHandler::GetSingleton()};

                    auto actorform = datahandler->LookupForm(hexnumber, owningMod);

                    if (!actorform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = actorform->GetFormID();

                    characterCategorySet.emplace_back(owningMod, ID, form.value());
                }
            }
        }
    }

    void JSONParser::ProcessNPCsFormIDBlacklist() {
        if (presetDistributionConfig.contains("blacklistedNpcsFormID")) {
            for (auto& [plugin, val] : presetDistributionConfig["blacklistedNpcsFormID"].items()) {
                for (const std::string_view formIDRaw : val) {
                    std::string formID = DiscardFormDigits(formIDRaw);
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    auto datahandler = RE::TESDataHandler::GetSingleton();

                    auto actorform = datahandler->LookupForm(hexnumber, plugin);

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
            for (auto& [plugin, val] : presetDistributionConfig["blacklistedOutfitsFromORefitFormID"].items()) {
                for (const std::string_view formIDRaw : val) {
                    std::string formID = DiscardFormDigits(formIDRaw);
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    auto datahandler = RE::TESDataHandler::GetSingleton();

                    auto outfitform = datahandler->LookupForm(hexnumber, plugin);

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
            for (auto& [plugin, val] : presetDistributionConfig["outfitsForceRefitFormID"].items()) {
                for (const std::string_view formIDRaw : val) {
                    std::string formID = DiscardFormDigits(formIDRaw);
                    uint32_t hexnumber;
                    sscanf_s(formID.c_str(), "%x", &hexnumber);

                    auto datahandler = RE::TESDataHandler::GetSingleton();

                    auto outfitform = datahandler->LookupForm(hexnumber, plugin);

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

    void JSONParser::ProcessJSONCategories() {
        ProcessNPCsFormIDBlacklist();
        ProcessNPCsFormID();
        ProcessOutfitsFormIDBlacklist();
        ProcessOutfitsForceRefitFormIDBlacklist();
    }

    bool JSONParser::IsStringInJsonConfigKey(std::string a_value, const char* key) {
        boost::trim(a_value);

        return presetDistributionConfig.contains(key) &&
               std::find(presetDistributionConfig[key].begin(), presetDistributionConfig[key].end(), a_value) !=
                   presetDistributionConfig[key].end();
    }

    bool JSONParser::IsSubKeyInJsonConfigKey(const char* key, std::string subKey) {
        boost::trim(subKey);

        return presetDistributionConfig.contains(key) && presetDistributionConfig[key].contains(subKey);
    }

    bool JSONParser::IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit) {
        return IsStringInJsonConfigKey(a_outfit.GetName(), "blacklistedOutfitsFromORefit") ||
               IsOutfitInBlacklistedOutfitCategorySet(a_outfit.GetFormID()) ||
               IsStringInJsonConfigKey(GetNthFormLocationName(RE::TESForm::LookupByID(a_outfit.GetFormID()), 0),
                                       "blacklistedOutfitsFromORefitPlugin");
    }

    bool JSONParser::IsAnyForceRefitItemEquipped(RE::Actor* a_actor, const bool a_removingArmor,
                                                 const RE::TESForm* a_equippedArmor) {
        auto inventory = a_actor->GetInventory();

        std::vector<std::string> wornItems;

        for (auto const& item : inventory) {
            if (item.second.second->IsWorn()) {
                // Check if the item is being unequipped or not first
                if (a_removingArmor && item.first->GetFormID() == a_equippedArmor->GetFormID()) {
                    continue;
                }

                if (const RE::FormType itemFormType = item.first->GetFormType();
                    (itemFormType == RE::FormType::Armor || itemFormType == RE::FormType::Armature) &&
                        IsStringInJsonConfigKey(item.second.second->GetDisplayName(), "outfitsForceRefit") ||
                    IsOutfitInForceRefitCategorySet(item.first->GetFormID())) {
                    logger::info("Outfit {} is in force refit list", item.second.second->GetDisplayName());

                    return true;
                }
            }
        }

        return false;
    }

    bool JSONParser::IsNPCBlacklisted(std::string actorName, uint32_t actorID) {
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
        } else {
            return IsStringInJsonConfigKey(actorOwningMod, "blacklistedNpcsPluginMale") ||
                   IsStringInJsonConfigKey(actorRace, "blacklistedRacesMale");
        }
    }

    PresetManager::Preset JSONParser::GetNPCFactionPreset(const RE::TESNPC* a_actor, const bool female) {
        PresetManager::Preset preset;
        const auto& presetContainer = PresetManager::PresetContainer::GetInstance();

        auto actorRanks = a_actor->factions | std::views::transform(&RE::FACTION_RANK::faction);

        std::vector<RE::TESFaction*> actorFactions(actorRanks.begin(), actorRanks.end());

        if (actorFactions.empty()) {
            return preset;
        }

        PresetManager::PresetSet presetSet = female ? presetContainer.allFemalePresets : presetContainer.allMalePresets;

        const char* factionKey = female ? "factionFemale" : "factionMale";

        for (auto& faction : presetDistributionConfig[factionKey].items()) {
            if (std::ranges::find(actorFactions, RE::TESFaction::LookupByEditorID(faction.key())) !=
                actorFactions.end()) {
                preset = PresetManager::GetRandomPresetByName(presetSet, faction.value(), female);
                break;
            }
        }

        return preset;
    }

    PresetManager::Preset JSONParser::GetNPCPreset(const char* actorName, uint32_t formID, bool female) {
        const auto& presetContainer = PresetManager::PresetContainer::GetInstance();

        PresetManager::Preset preset;

        PresetManager::PresetSet presetSet = female ? presetContainer.allFemalePresets : presetContainer.allMalePresets;

        if (const auto character = GetNPCFromCategorySet(formID); !character.bodyslidePresets.empty()) {
            preset = PresetManager::GetRandomPresetByName(presetSet, character.bodyslidePresets, female);
        } else if (presetDistributionConfig.contains("npc") && presetDistributionConfig["npc"].contains(actorName)) {
            preset =
                PresetManager::GetRandomPresetByName(presetSet, presetDistributionConfig["npc"][actorName], female);
        }

        return preset;
    }

    PresetManager::Preset JSONParser::GetNPCPluginPreset(const RE::TESNPC* a_actor, std::string actorName,
                                                         const bool female) {
        const auto& presetContainer = PresetManager::PresetContainer::GetInstance();

        // auto actorOwningMod = GetNthFormLocationName(a_actor, 0);

        PresetManager::Preset preset;

        const PresetManager::PresetSet presetSet =
            female ? presetContainer.allFemalePresets : presetContainer.allMalePresets;

        if (female) {
            if (presetDistributionConfig.contains("npcPluginFemale")) {
                for (auto& [mod, presetList] : presetDistributionConfig["npcPluginFemale"].items()) {
                    logger::info("Checking if actor {} is in mod {}", actorName, mod);

                    if (IsActorInForm(a_actor, mod)) {
                        preset = PresetManager::GetRandomPresetByName(
                            presetSet, presetDistributionConfig["npcPluginFemale"][mod], female);
                        break;
                    }
                }
            }
        } else {
            if (presetDistributionConfig.contains("npcPluginMale")) {
                for (auto& [mod, presetList] : presetDistributionConfig["npcPluginMale"].items()) {
                    if (IsActorInForm(a_actor, mod)) {
                        preset = PresetManager::GetRandomPresetByName(
                            presetSet, presetDistributionConfig["npcPluginMale"][mod], female);
                        break;
                    }
                }
            }
        }

        return preset;
    }

    PresetManager::Preset JSONParser::GetNPCRacePreset(const char* actorRace, const bool female) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        PresetManager::Preset preset;

        const PresetManager::PresetSet presetSet =
            female ? presetContainer.allFemalePresets : presetContainer.allMalePresets;

        if (female) {
            if (IsSubKeyInJsonConfigKey("raceFemale", actorRace)) {
                preset = PresetManager::GetRandomPresetByName(
                    presetSet, presetDistributionConfig["raceFemale"][actorRace], female);
            }
        } else {
            if (IsSubKeyInJsonConfigKey("raceMale", actorRace)) {
                preset = PresetManager::GetRandomPresetByName(presetSet,
                                                              presetDistributionConfig["raceMale"][actorRace], female);
            }
        }

        return preset;
    }
}  // namespace Parser
