#pragma once

namespace PapyrusBody {
    using VM = RE::BSScript::IVirtualMachine;

    void GenActor(RE::StaticFunctionTag*, RE::Actor* a_actor);

    void SetORefit(RE::StaticFunctionTag*, bool a_enabled);

    void SetNippleSlidersORefitEnabled(RE::StaticFunctionTag*, bool a_enabled);

    void SetNippleRand(RE::StaticFunctionTag*, bool a_enabled);

    void SetGenitalRand(RE::StaticFunctionTag*, bool a_enabled);

    void SetPerformanceMode(RE::StaticFunctionTag*, bool a_enabled);

    void SetDistributionKey(RE::StaticFunctionTag*, std::string a_distributionKey);

    int GetFemaleDatabaseSize(RE::StaticFunctionTag*);

    int GetMaleDatabaseSize(RE::StaticFunctionTag*);

    void RegisterForOBodyEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest);

    void RegisterForOBodyNakedEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest);

    void RegisterForOBodyRemovingClothesEvent(RE::StaticFunctionTag*, const RE::TESQuest* a_quest);

    void ApplyPresetByName(RE::StaticFunctionTag*, RE::Actor* a_actor, std::string a_name);

    void AddClothesOverlay(RE::StaticFunctionTag*, RE::Actor* a_actor);

    void ResetActorOBodyMorphs(RE::StaticFunctionTag*, RE::Actor* a_actor);

    std::vector<std::string> GetAllPossiblePresets(RE::StaticFunctionTag*, RE::Actor* a_actor);

    bool Bind(VM* a_vm);
}  // namespace PapyrusBody
