#pragma once

#include "PresetManager/PresetManager.h"
#include "SKEE.h"

namespace Body {
    inline SKSE::RegistrationSet<RE::Actor*, std::string> OnActorGenerated("OnActorGenerated"sv);
    inline SKSE::RegistrationSet<RE::Actor*> OnActorNaked("OnActorNaked"sv);
    inline SKSE::RegistrationSet<RE::Actor*> OnActorRemovingClothes("OnActorRemovingClothes"sv);

    class OBody {
    public:
        OBody(OBody&&) = delete;
        OBody(const OBody&) = delete;

        OBody& operator=(OBody&&) = delete;
        OBody& operator=(const OBody&) = delete;

        static OBody& GetInstance();

        bool SetMorphInterface(SKEE::IBodyMorphInterface* a_morphInterface);

        void SetMorph(RE::Actor* a_actor, const char* a_morphName, const char* a_key, float a_value) const;
        float GetMorph(RE::Actor* a_actor, const char* a_morphName) const;
        void ApplyMorphs(RE::Actor* a_actor, bool updateMorphsWithoutTimer, bool applyProcessedMorph = true) const;

        void ProcessActorEquipEvent(RE::Actor* a_actor, bool a_removingArmor, const RE::TESForm* a_equippedArmor) const;

        void GenerateActorBody(RE::Actor* a_actor) const;
        void GenerateBodyByName(RE::Actor* a_actor, const std::string& a_name) const;
        void GenerateBodyByPreset(RE::Actor* a_actor, PresetManager::Preset& a_preset, bool updateMorphsWithoutTimer) const;

        void ApplySlider(RE::Actor* a_actor, const PresetManager::Slider& a_slider, const char* a_key, float a_weight) const;
        void ApplySliderSet(RE::Actor* a_actor, PresetManager::SliderSet& a_sliders, const char* a_key) const;
        void ApplyClothePreset(RE::Actor* a_actor) const;
        void RemoveClothePreset(RE::Actor* a_actor) const;
        void ClearActorMorphs(RE::Actor* a_actor) const;

        static float GetWeight(RE::Actor* a_actor);

        bool IsClotheActive(RE::Actor* a_actor) const;
        static bool IsNaked(RE::Actor* a_actor, bool a_removingArmor, const RE::TESForm* a_equippedArmor);
        static bool IsRemovingClothes(RE::Actor* a_actor, bool a_removingArmor, const RE::TESForm* a_equippedArmor);
        static bool IsFemale(RE::Actor* a_actor);
        bool IsProcessed(RE::Actor* a_actor) const;
        bool IsBlacklisted(RE::Actor* a_actor) const;

        static PresetManager::SliderSet GenerateRandomNippleSliders();
        static PresetManager::SliderSet GenerateRandomGenitalSliders();
        PresetManager::SliderSet GenerateClotheSliders(RE::Actor* a_actor) const;

        PresetManager::Slider DeriveSlider(RE::Actor* a_actor, const char* a_morph, float a_target) const;

        bool synthesisInstalled = false;

        bool setRefit = true;
        bool setNippleSlidersRefitEnabled = true;
        bool setNippleRand = true;
        bool setGenitalRand = true;
        bool setPerformanceMode = true;

        std::string distributionKey;

        SKEE::IBodyMorphInterface* morphInterface{};

    private:
        static OBody instance_;

        OBody() = default;
        ~OBody() = default;
    };
} // namespace Body
