// ReSharper disable CppPassValueParameterByConstReference
#pragma once

#include "Body/Body.h"
#include "JSONParser/JSONParser.h"
#include "PresetManager/PresetManager.h"

namespace PapyrusBody {
    using VM = RE::BSScript::IVirtualMachine;
    inline auto& obody{Body::OBody::GetInstance()};

    static void GenActor(RE::StaticFunctionTag*, RE::Actor* a_actor) { obody.GenerateActorBody(a_actor); }

    static void SetORefit(RE::StaticFunctionTag*, const bool a_enabled) { obody.setRefit = a_enabled; }

    static void SetNippleSlidersORefitEnabled(RE::StaticFunctionTag*, const bool a_enabled) {
        obody.setNippleSlidersRefitEnabled = a_enabled;
    }

    static void SetNippleRand(RE::StaticFunctionTag*, const bool a_enabled) { obody.setNippleRand = a_enabled; }

    static void SetGenitalRand(RE::StaticFunctionTag*, const bool a_enabled) { obody.setGenitalRand = a_enabled; }

    static void SetPerformanceMode(RE::StaticFunctionTag*, const bool a_enabled) {
        obody.setPerformanceMode = a_enabled;
    }

    static void SetDistributionKey(RE::StaticFunctionTag*, const std::string a_distributionKey) {
        obody.distributionKey = a_distributionKey;
    }

    static int GetFemaleDatabaseSize(RE::StaticFunctionTag*) {
        const auto& presetContainer = PresetManager::PresetContainer::GetInstance();
        return static_cast<int>(presetContainer.femalePresets.size());
    }

    static int GetMaleDatabaseSize(RE::StaticFunctionTag*) {
        const auto& presetContainer = PresetManager::PresetContainer::GetInstance();
        return static_cast<int>(presetContainer.malePresets.size());
    }

    static void RegisterForOBodyEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest) {
        Body::OnActorGenerated.Register(a_quest);
    }

    static void RegisterForOBodyNakedEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest) {
        Body::OnActorNaked.Register(a_quest);
    }

    static void RegisterForOBodyRemovingClothesEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest) {
        Body::OnActorRemovingClothes.Register(a_quest);
    }

    static void ApplyPresetByName(RE::StaticFunctionTag*, RE::Actor* a_actor, const std::string a_name) {
        obody.GenerateBodyByName(a_actor, a_name);
    }

    static void AddClothesOverlay(RE::StaticFunctionTag*, RE::Actor* a_actor) {
        obody.ApplyClothePreset(a_actor);
        obody.ApplyMorphs(a_actor, true);
    }

    static void ResetActorOBodyMorphs(RE::StaticFunctionTag*, RE::Actor* a_actor) { obody.ClearActorMorphs(a_actor); }

    static bool presetNameComparison(std::string a, std::string b) {
        boost::algorithm::to_lower(a);
        boost::algorithm::to_lower(b);
        return a < b;
    }

    static auto GetAllPossiblePresets(RE::StaticFunctionTag*, RE::Actor* a_actor) {
        const auto& presetContainer = PresetManager::PresetContainer::GetInstance();

        const auto& presetDistributionConfig = Parser::JSONParser::GetInstance().presetDistributionConfig;

        bool showBlacklistedPresets = false;

        // For some reason, some users get CTDs when reading this value, even when everything else seems correct...
        // Also happens when used with Immersive Equipment displays. I have no idea why.
        // Catch the error, default to True in case something fails
        try {
            showBlacklistedPresets = presetDistributionConfig.contains("blacklistedPresetsShowInOBodyMenu") &&
                                     presetDistributionConfig["blacklistedPresetsShowInOBodyMenu"];
        } catch (json::type_error) {
            logger::info(
                "Failed to read blacklistedPresetsShowInOBodyMenu key. Defaulting to showing the blacklisted presets "
                "in OBody menu.");
            showBlacklistedPresets = true;
        }
        auto presets_to_show =
            (obody.IsFemale(a_actor)
                 ? (showBlacklistedPresets ? presetContainer.allFemalePresets : presetContainer.femalePresets)
                 : (showBlacklistedPresets ? presetContainer.allMalePresets : presetContainer.malePresets)) |
            std::views::transform(&PresetManager::Preset::name);

        std::vector<std::string> ret(presets_to_show.begin(), presets_to_show.end());
        std::ranges::sort(ret, presetNameComparison);

        return ret;
    }

    static bool Bind(VM* a_vm) {
        constexpr auto obj = "OBodyNative"sv;

        BIND(GenActor);
        BIND(ApplyPresetByName);
        BIND(GetAllPossiblePresets);
        BIND(AddClothesOverlay);
        BIND(RegisterForOBodyEvent);
        BIND(RegisterForOBodyNakedEvent);
        BIND(RegisterForOBodyRemovingClothesEvent);
        BIND(GetFemaleDatabaseSize);
        BIND(GetMaleDatabaseSize);
        BIND(ResetActorOBodyMorphs);

        BIND(SetORefit);
        BIND(SetNippleSlidersORefitEnabled);
        BIND(SetNippleRand);
        BIND(SetGenitalRand);
        BIND(SetPerformanceMode);
        BIND(SetDistributionKey);

        return true;
    }
}  // namespace PapyrusBody
