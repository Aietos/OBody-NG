//
// Created by judah on 05-02-2025.
//

#include "Body/Body.h"
#include "PresetManager/PresetManager.h"
#include "JSONParser/JSONParser.h"
#include "Papyrus/PapyrusBody.h"

namespace PapyrusBody {
    void GenActor(RE::StaticFunctionTag*, RE::Actor* a_actor) { Body::OBody::GetInstance().GenerateActorBody(a_actor); }

    void SetORefit(RE::StaticFunctionTag*, const bool a_enabled) { Body::OBody::GetInstance().setRefit = a_enabled; }

    void SetNippleSlidersORefitEnabled(RE::StaticFunctionTag*, const bool a_enabled) {
        Body::OBody::GetInstance().setNippleSlidersRefitEnabled = a_enabled;
    }

    void SetNippleRand(RE::StaticFunctionTag*, const bool a_enabled) {
        Body::OBody::GetInstance().setNippleRand = a_enabled;
    }

    void SetGenitalRand(RE::StaticFunctionTag*, const bool a_enabled) {
        Body::OBody::GetInstance().setGenitalRand = a_enabled;
    }

    void SetPerformanceMode(RE::StaticFunctionTag*, const bool a_enabled) {
        Body::OBody::GetInstance().setPerformanceMode = a_enabled;
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    void SetDistributionKey(RE::StaticFunctionTag*,
                            const std::string a_distributionKey) {  // NOLINT(*-unnecessary-value-param)
        Body::OBody::GetInstance().distributionKey = a_distributionKey;
    }

    int GetFemaleDatabaseSize(RE::StaticFunctionTag*) {
        return static_cast<int>(PresetManager::PresetContainer::GetInstance().femalePresets.size());
    }

    int GetMaleDatabaseSize(RE::StaticFunctionTag*) {
        return static_cast<int>(PresetManager::PresetContainer::GetInstance().malePresets.size());
    }

    void RegisterForOBodyEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest) {
        Body::OnActorGenerated.Register(a_quest);
    }

    void RegisterForOBodyNakedEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest) {
        Body::OnActorNaked.Register(a_quest);
    }

    void RegisterForOBodyRemovingClothesEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest) {
        Body::OnActorRemovingClothes.Register(a_quest);
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    void ApplyPresetByName(RE::StaticFunctionTag*, RE::Actor* a_actor,
                           const std::string a_name) {  // NOLINT(*-unnecessary-value-param)
        Body::OBody::GetInstance().GenerateBodyByName(a_actor, a_name);
    }

    void AddClothesOverlay(RE::StaticFunctionTag*, RE::Actor* a_actor) {
        const auto& obody{Body::OBody::GetInstance()};
        obody.ApplyClothePreset(a_actor);
        obody.ApplyMorphs(a_actor, true);
    }

    void ResetActorOBodyMorphs(RE::StaticFunctionTag*, RE::Actor* a_actor) {
        Body::OBody::GetInstance().ClearActorMorphs(a_actor);
    }

    bool presetNameComparison(const std::string_view a, const std::string_view b) {
        return boost::algorithm::ilexicographical_compare(a, b);
    }

    std::vector<std::string> GetAllPossiblePresets(RE::StaticFunctionTag*, RE::Actor* a_actor) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        const auto& presetDistributionConfig{Parser::JSONParser::GetInstance().presetDistributionConfig};
        const auto showBlacklistedPresetsItr{presetDistributionConfig.FindMember("blacklistedPresetsShowInOBodyMenu")};
        const auto end{presetDistributionConfig.MemberEnd()};

        // Default to showing blacklisted presets if the key is missing or invalid
        bool showBlacklistedPresets{false};

        if (showBlacklistedPresetsItr != end && showBlacklistedPresetsItr->value.IsBool()) {
            showBlacklistedPresets = showBlacklistedPresetsItr->value.GetBool();
        } else {
            logger::info(
                "Failed to read blacklistedPresetsShowInOBodyMenu key. Defaulting to showing the blacklisted presets "
                "in OBody menu.");
        }

        auto presets_to_show =
            (Body::OBody::IsFemale(a_actor)
                 ? (showBlacklistedPresets ? presetContainer.allFemalePresets : presetContainer.femalePresets)
                 : (showBlacklistedPresets ? presetContainer.allMalePresets : presetContainer.malePresets)) |
            std::views::transform(&PresetManager::Preset::name);

        std::vector ret(presets_to_show.begin(), presets_to_show.end());
        std::ranges::sort(ret, presetNameComparison);

        return ret;
    }

    bool Bind(VM* a_vm) {
        constexpr auto obj = "OBodyNative"sv;

#define OBODY_PAPYRUS_BIND(a_method, ...) \
    a_vm->RegisterFunction(#a_method##sv, obj, a_method __VA_OPT__(, ) __VA_ARGS__)
        OBODY_PAPYRUS_BIND(GenActor);
        OBODY_PAPYRUS_BIND(ApplyPresetByName);
        OBODY_PAPYRUS_BIND(GetAllPossiblePresets);
        OBODY_PAPYRUS_BIND(AddClothesOverlay);
        OBODY_PAPYRUS_BIND(RegisterForOBodyEvent);
        OBODY_PAPYRUS_BIND(RegisterForOBodyNakedEvent);
        OBODY_PAPYRUS_BIND(RegisterForOBodyRemovingClothesEvent);
        OBODY_PAPYRUS_BIND(GetFemaleDatabaseSize);
        OBODY_PAPYRUS_BIND(GetMaleDatabaseSize);
        OBODY_PAPYRUS_BIND(ResetActorOBodyMorphs);

        OBODY_PAPYRUS_BIND(SetORefit);
        OBODY_PAPYRUS_BIND(SetNippleSlidersORefitEnabled);
        OBODY_PAPYRUS_BIND(SetNippleRand);
        OBODY_PAPYRUS_BIND(SetGenitalRand);
        OBODY_PAPYRUS_BIND(SetPerformanceMode);
        OBODY_PAPYRUS_BIND(SetDistributionKey);
#undef OBODY_PAPYRUS_BIND
        return true;
    }
}  // namespace PapyrusBody