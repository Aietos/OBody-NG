#pragma once

#include "PresetManager/PresetManager.h"

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

        [[nodiscard]] bool IsActorInBlacklistedCharacterCategorySet(uint32_t formID) const;
        bool IsOutfitInBlacklistedOutfitCategorySet(uint32_t formID);
        [[nodiscard]] bool IsOutfitInForceRefitCategorySet(uint32_t formID) const;

        [[nodiscard]] std::optional<categorizedList> GetNPCFromCategorySet(uint32_t formID) const;
        bool IsStringInJsonConfigKey(std::string_view a_value, const char* key);
        bool IsSubKeyInJsonConfigKey(const char* key, std::string_view subKey);

        bool IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit);
        bool IsAnyForceRefitItemEquipped(RE::Actor* a_actor, bool a_removingArmor, const RE::TESForm* a_equippedArmor);
        bool IsNPCBlacklisted(std::string_view actorName, uint32_t actorID);
        bool IsNPCBlacklistedGlobally(const RE::Actor* a_actor, const char* actorRace, bool female);

        std::optional<PresetManager::Preset> GetNPCFactionPreset(const RE::TESNPC* a_actor, bool female);

        std::optional<PresetManager::Preset> GetNPCPreset(const char* actorName, uint32_t formID, bool female);
        std::optional<PresetManager::Preset> GetNPCPluginPreset(const RE::TESNPC* a_actor, const char* actorName,
                                                                bool female);
        std::optional<PresetManager::Preset> GetNPCRacePreset(const char* actorRace, bool female);

        rapidjson::Document presetDistributionConfig;
        bool bodyslidePresetsParsingValid{};
        std::size_t invalid_presets{};

        std::vector<categorizedList> blacklistedCharacterCategorySet;
        std::vector<categorizedList> characterCategorySet;

        std::vector<categorizedList> blacklistedOutfitCategorySet;
        std::vector<categorizedList> forceRefitOutfitCategorySet;

    private:
        JSONParser() = default;
        static JSONParser instance;
    };
}  // namespace Parser
