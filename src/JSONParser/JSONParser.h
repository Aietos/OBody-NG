#pragma once

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

        void ProcessNPCsFormID();
        void ProcessNPCsFormIDBlacklist();
        void ProcessOutfitsFormIDBlacklist();
        void ProcessOutfitsForceRefitFormIDBlacklist();
        void FilterOutNonLoaded();

        void ProcessJSONCategories();

        bool IsActorInBlacklistedCharacterCategorySet(uint32_t formID) const;
        bool IsOutfitInBlacklistedOutfitCategorySet(uint32_t formID);
        bool IsOutfitInForceRefitCategorySet(uint32_t formID) const;

        categorizedList GetNPCFromCategorySet(uint32_t formID) const;
        bool IsStringInJsonConfigKey(std::string a_value, const char* key);
        bool IsSubKeyInJsonConfigKey(const char* key, std::string subKey);

        bool IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit);
        bool IsAnyForceRefitItemEquipped(RE::Actor* a_actor, bool a_removingArmor, const RE::TESForm* a_equippedArmor);
        bool IsNPCBlacklisted(std::string actorName, uint32_t actorID);
        bool IsNPCBlacklistedGlobally(const RE::Actor* a_actor, const char* actorRace, bool female);

        PresetManager::Preset GetNPCFactionPreset(const RE::TESNPC* a_actor, bool female);

        PresetManager::Preset GetNPCPreset(const char* actorName, uint32_t formID, bool female);
        PresetManager::Preset GetNPCPluginPreset(const RE::TESNPC* a_actor, const char* actorName, bool female);
        PresetManager::Preset GetNPCRacePreset(const char* actorRace, bool female);

        nlohmann::ordered_json presetDistributionConfig;
        bool presetDistributionConfigValid{};
        bool bodyslidePresetsParsingValid{};

        std::vector<categorizedList> blacklistedCharacterCategorySet;
        std::vector<categorizedList> characterCategorySet;

        std::vector<categorizedList> blacklistedOutfitCategorySet;
        std::vector<categorizedList> forceRefitOutfitCategorySet;

    private:
        JSONParser() = default;
        static JSONParser instance;
    };
} // namespace Parser
