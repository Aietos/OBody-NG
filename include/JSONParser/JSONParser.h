#pragma once

#include "../PresetManager/PresetManager.h"
#include "ConfigLoader.h"

namespace Parser {
    struct categorizedList {
        std::string owningMod;
        uint32_t formID = 0;
        std::vector<std::string> bodyslidePresets;
    };

    // The preset distribution config: the JSON files merged together (see ConfigLoader.h), and what is looked up from
    // them. Once processed it never changes, a reload builds a new one (see JSONParser)
    class Config {
    public:
        Config() { presetDistributionConfig.SetObject(); }

        Config(Config&&) = delete;
        Config(const Config&) = delete;

        Config& operator=(Config&&) = delete;
        Config& operator=(const Config&) = delete;

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
        [[nodiscard]] bool IsOutfitInBlacklistedOutfitCategorySet(uint32_t formID) const;
        [[nodiscard]] bool IsOutfitInForceRefitCategorySet(uint32_t formID) const;

        [[nodiscard]] std::optional<categorizedList> GetNPCFromCategorySet(uint32_t formID) const;
        [[nodiscard]] bool IsStringInJsonConfigKey(std::string_view a_value, const char* key) const;
        [[nodiscard]] bool IsSubKeyInJsonConfigKey(const char* key, std::string_view subKey) const;

        void ProcessDisablePresetDistribution();
        [[nodiscard]] bool IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit) const;
        bool IsAnyForceRefitItemEquipped(RE::Actor* a_actor, bool a_removingArmor,
                                         const RE::TESForm* a_equippedArmor) const;
        bool IsNPCBlacklisted(std::string_view actorName, const std::vector<RE::FormID>& actorIDs) const;
        bool IsNPCBlacklistedGlobally(const RE::Actor* a_actor, const std::vector<const RE::TESNPC*>& a_lookupNPCs,
                                      const char* actorRace, const char* actorClass, bool female) const;

        std::optional<PresetManager::Preset> GetNPCFactionPreset(const RE::TESNPC* a_actor, bool female,
                                                                 bool a_ube) const;

        std::optional<PresetManager::Preset> GetNPCPreset(const char* actorName, const std::vector<RE::FormID>& formIDs,
                                                          bool female, bool a_ube) const;
        std::optional<PresetManager::Preset> GetNPCPluginPreset(const RE::TESNPC* a_actor, const char* actorName,
                                                                bool female, bool a_ube) const;
        std::optional<PresetManager::Preset> GetNPCRacePreset(const char* actorRace, bool female, bool a_ube) const;
        std::optional<PresetManager::Preset> GetNPCClassPreset(const char* actorClass, bool female, bool a_ube) const;

        rapidjson::Document presetDistributionConfig;

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
        // a_ube: the actor's preset is made for UBE, so only refit presets made for UBE are used. It's the preset's
        // body, not the actor's: these refits are compared with that preset
        std::optional<PresetManager::Preset> GetRefitPresetFromEquippedItems(
            RE::Actor* a_actor, bool female, bool a_ube, std::string_view a_actorPresetName,
            const RE::TESForm* a_changedArmor = nullptr, bool a_removingArmor = false) const;

        // Additive default refit presets ("Female-Refit<anything>" / "Male-Refit<anything>", and
        // "UBE Female-Refit<anything>" / "UBE Male-Refit<anything>" for UBE bodies). Priority is:
        // 1. "Female-Refit<anything>" entries in the outfit lists
        // 2. "Female-Refit <Keyword>" for each keyword of CollectRefitKeywords
        std::optional<PresetManager::Preset> GetRefitDefaultFromEquippedItems(
            RE::Actor* a_actor, bool female, bool a_ube, const RE::TESForm* a_changedArmor = nullptr,
            bool a_removingArmor = false) const;

        void ValidateRefitPresetNames() const;

        // Splits PresetContainer's presets into the allowed and blacklisted ones, for this config
        void SplitPresets();

        [[nodiscard]] const PresetManager::PresetLists& GetPresetLists(bool female) const;
        // The allowed presets, then the blacklisted ones too if a_withBlacklisted
        [[nodiscard]] PresetManager::PresetPointers GetPresets(bool female, bool a_withBlacklisted) const;
        // Whether there are presets that can be given out to actors of this sex and body
        [[nodiscard]] bool HasPresetsFor(bool a_female, bool a_ube) const;

        PresetManager::PresetLists femalePresets;
        PresetManager::PresetLists malePresets;

        // Counts the configs loaded, so what is built from one (like the preset menu's lists) knows when it's outdated
        std::uint32_t generation{};

        // The include files, in the order they were loaded
        std::vector<IncludeFileInfo> includeFiles;

        // The armors the refit lookups consider: the ones worn in the body slots (in slot order), then any other worn
        // armor that has ORefit forced on (outfitsForceRefit*), because forcing ORefit on an outfit also means that its
        // own -Refit presets must be found, wherever it is worn
        [[nodiscard]] std::vector<const RE::TESObjectARMO*> GetRefitArmors(RE::Actor* a_actor,
                                                                           const RE::TESForm* a_changedArmor,
                                                                           bool a_removingArmor) const;

        // Refit preset lists written for the worn outfits
        [[nodiscard]] std::vector<std::vector<std::string>> CollectOutfitRefitLists(
            const std::vector<const RE::TESObjectARMO*>& a_armors, bool female) const;

        // The keywords to look refit presets up for, highest priority first, limited to the ones the worn outfits have:
        // ArmorHeavy, ArmorLight and ArmorClothing are always checked by default
        [[nodiscard]] std::vector<std::string> CollectRefitKeywords(
            const std::vector<const RE::TESObjectARMO*>& a_armors, bool female) const;
    };

    // Owns the config in use. The NPCs' bodies are generated from the threads that load them, so a reload never
    // changes the config they may be reading. It builds a new one and swaps it in, and the old one is freed when its
    // last reader is done with it
    class JSONParser {
    public:
        JSONParser(JSONParser&&) = delete;
        JSONParser(const JSONParser&) = delete;

        JSONParser& operator=(JSONParser&&) = delete;
        JSONParser& operator=(const JSONParser&) = delete;

        static JSONParser& GetInstance();

        // Take it once and use the same one for the whole lookup, a reload can replace it at any time
        [[nodiscard]] std::shared_ptr<const Config> GetConfig() const { return config_.load(); }

        struct LoadResult {
            std::optional<std::string> error;
            std::vector<std::string> skippedFiles;
        };

        // Reads and processes the config files, then makes them the config in use
        LoadResult Load();

        bool bodyslidePresetsParsingValid{};
        std::size_t invalid_presets{};

    private:
        JSONParser() = default;
        static JSONParser instance;

        std::atomic<std::shared_ptr<const Config>> config_{std::make_shared<const Config>()};
        std::uint32_t loadedConfigs_{0};
    };
}  // namespace Parser
