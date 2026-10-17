#pragma once

#include "../PresetManager/PresetManager.h"

namespace Parser {
    struct categorizedList {
        std::string owningMod;
        uint32_t formID = 0;
        std::vector<std::string> bodyslidePresets;
    };

    class JSONParser {
    public:
        JSONParser(JSONParser&&) = delete;
        JSONParser(const JSONParser&) = delete;

        JSONParser& operator=(JSONParser&&) = delete;
        JSONParser& operator=(const JSONParser&) = delete;

        static JSONParser& GetInstance();

        bool distributionDisabledForMale{};
        bool distributionDisabledForFemale{};

        void ProcessNPCsFormID();
        void ProcessNPCsFormIDBlacklist();
        void ProcessOutfitsFormIDBlacklist();
        void ProcessOutfitsForceRefitFormIDBlacklist();
        void ProcessRefitOutfitPresetsFormID();
        void FilterOutNonLoaded();

        void ProcessJSONCategories();

        [[nodiscard]] bool IsActorInBlacklistedCharacterCategorySet(uint32_t formID) const;
        bool IsOutfitInBlacklistedOutfitCategorySet(uint32_t formID);
        [[nodiscard]] bool IsOutfitInForceRefitCategorySet(uint32_t formID) const;

        [[nodiscard]] std::optional<categorizedList> GetNPCFromCategorySet(uint32_t formID) const;
        bool IsStringInJsonConfigKey(std::string_view a_value, const char* key);
        bool IsSubKeyInJsonConfigKey(const char* key, std::string_view subKey);

        void ProcessDisablePresetDistribution();
        bool IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit);
        bool IsAnyForceRefitItemEquipped(RE::Actor* a_actor, bool a_removingArmor, const RE::TESForm* a_equippedArmor);
        bool IsNPCBlacklisted(std::string_view actorName, uint32_t actorID);
        bool IsNPCBlacklistedGlobally(const RE::Actor* a_actor, const char* actorRace, const char* actorClass,
                                      bool female);

        std::optional<PresetManager::Preset> GetNPCFactionPreset(const RE::TESNPC* a_actor, bool female);

        std::optional<PresetManager::Preset> GetNPCPreset(const char* actorName, uint32_t formID, bool female);
        std::optional<PresetManager::Preset> GetNPCPluginPreset(const RE::TESNPC* a_actor, const char* actorName,
                                                                bool female);
        std::optional<PresetManager::Preset> GetNPCRacePreset(const char* actorRace, bool female);
        std::optional<PresetManager::Preset> GetNPCClassPreset(const char* actorClass, bool female);

        rapidjson::Document presetDistributionConfig;
        bool bodyslidePresetsParsingValid{};
        std::size_t invalid_presets{};

        std::vector<categorizedList> blacklistedCharacterCategorySet;
        std::vector<categorizedList> characterCategorySet;

        std::vector<categorizedList> blacklistedOutfitCategorySet;
        std::vector<categorizedList> forceRefitOutfitCategorySet;

        // Outfit FormID -> list of candidate "-Refit" preset names
        // The order in which the outfits are written in the JSON matters, the first one wins in priority
        using RefitOutfitFormIDList = std::vector<std::pair<std::uint32_t, std::vector<std::string>>>;
        RefitOutfitFormIDList refitOutfitPresetFormIDListFemale;
        RefitOutfitFormIDList refitOutfitPresetFormIDListMale;

        // Differential refit presets ("<ActorPreset>-Refit<anything>"). Priority is:
        // 1. refitOutfitPresetsFormID
        // 2. refitOutfitPresets
        // 3. "<ActorPreset>-Refit <Keyword>" for each keyword of CollectRefitKeywords
        // a_changedArmor / a_removingArmor describe the armor of the equip event being processed: the inventory isn't
        // updated yet at that point, so an armor being removed must not be counted as worn
        std::optional<PresetManager::Preset> GetRefitPresetFromEquippedItems(
            RE::Actor* a_actor, bool female, std::string_view a_actorPresetName,
            const RE::TESForm* a_changedArmor = nullptr, bool a_removingArmor = false);

        // Additive default refit presets ("Female-Refit<anything>" / "Male-Refit<anything>"). Priority is:
        // 1. "Female-Refit<anything>" entries in the outfit lists
        // 2. "Female-Refit <Keyword>" for each keyword of CollectRefitKeywords
        std::optional<PresetManager::Preset> GetRefitDefaultFromEquippedItems(
            RE::Actor* a_actor, bool female, const RE::TESForm* a_changedArmor = nullptr, bool a_removingArmor = false);

        void ValidateRefitPresetNames() const;

        // The armors the refit lookups consider: the ones worn in the body slots (in slot order), then any other worn
        // armor that has ORefit forced on (outfitsForceRefit*), because forcing ORefit on an outfit also means that its
        // own -Refit presets must be found, wherever it is worn
        [[nodiscard]] std::vector<const RE::TESObjectARMO*> GetRefitArmors(RE::Actor* a_actor,
                                                                           const RE::TESForm* a_changedArmor,
                                                                           bool a_removingArmor);

        // Refit preset lists written for the worn outfits
        [[nodiscard]] std::vector<std::vector<std::string>> CollectOutfitRefitLists(
            const std::vector<const RE::TESObjectARMO*>& a_armors, bool female) const;

        // The keywords to look refit presets up for, highest priority first, limited to the ones the worn outfits have:
        // ArmorHeavy, ArmorLight and ArmorClothing are always checked by default
        [[nodiscard]] std::vector<std::string> CollectRefitKeywords(
            const std::vector<const RE::TESObjectARMO*>& a_armors, bool female) const;

    private:
        JSONParser() = default;
        static JSONParser instance;
    };
}  // namespace Parser
