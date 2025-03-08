#pragma once

#include "ActorTracker/ActorTracker.h"
#include "API/PluginInterface.h"
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

        void GenerateActorBody(RE::Actor* a_actor, ::OBody::API::IPluginInterface* responsibleInterface) const;
        void GenerateBodyByName(RE::Actor* a_actor, const std::string& a_name,
                                ::OBody::API::IPluginInterface* responsibleInterface) const;
        void GenerateBodyByPreset(RE::Actor* a_actor, PresetManager::Preset& a_preset, bool updateMorphsWithoutTimer,
                                  ::OBody::API::IPluginInterface* responsibleInterface) const;

        void ApplySlider(RE::Actor* a_actor, const PresetManager::Slider& a_slider, const char* a_key,
                         float a_weight) const;
        void ApplySliderSet(RE::Actor* a_actor, PresetManager::SliderSet& a_sliders, const char* a_key) const;
        void ApplyClothePreset(RE::Actor* a_actor) const;
        void RemoveClothePreset(RE::Actor* a_actor) const;
        void ClearActorMorphs(RE::Actor* a_actor, ::OBody::API::IPluginInterface* responsibleInterface) const;

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

        bool BecomingReadyForPluginAPIUsage();
        void ReadyForPluginAPIUsage();
        bool BecomingUnreadyForPluginAPIUsage();
        void NoLongerReadyForPluginAPIUsage();

        bool AttachEventListener(::OBody::API::IOBodyReadinessEventListener& eventListener);
        bool DetachEventListener(::OBody::API::IOBodyReadinessEventListener& eventListener);

        bool AttachEventListener(::OBody::API::IActorChangeEventListener& eventListener);
        bool DetachEventListener(::OBody::API::IActorChangeEventListener& eventListener);
        bool IsEventListenerAttached(::OBody::API::IActorChangeEventListener& eventListener);

        template <typename PrepareArguments, typename EventMethod>
        __forceinline void SendActorChangeEvent(RE::Actor* a_actor, PrepareArguments&& prepareArguments,
                                                EventMethod&& eventMethod) const {
            std::lock_guard<std::recursive_mutex> lock(actorChangeListenerLock);

            if (actorChangeEventListeners.size() == 0) {
                return;
            }

            auto& registry{ActorTracker::Registry::GetInstance()};
            auto formID = a_actor->formID;

            ActorTracker::ActorState fallbackActorState{};
            fallbackActorState.actorChangeEventsAreBeingSent = true;

            bool isRecursiveActorEvent = false;

            registry.stateForActor.emplace_or_visit(formID, fallbackActorState, [&](auto& entry) {
                isRecursiveActorEvent = entry.second.actorChangeEventsAreBeingSent;
                entry.second.actorChangeEventsAreBeingSent = true;
            });

            if (isRecursiveActorEvent) {
                return;
            }

            auto arguments = prepareArguments();

            for (auto eventListener : actorChangeEventListeners) {
                eventMethod(eventListener, a_actor, arguments);
            }

            registry.stateForActor.visit(formID,
                                         [&](auto& entry) { entry.second.actorChangeEventsAreBeingSent = false; });
        }

        bool readyForPluginAPIUsage = false;

        bool synthesisInstalled = false;

        bool setRefit = true;
        bool setNippleSlidersRefitEnabled = true;
        bool setNippleRand = true;
        bool setGenitalRand = true;
        bool setPerformanceMode = true;

        std::string distributionKey;

        SKEE::IBodyMorphInterface* morphInterface{};

        mutable std::recursive_mutex actorChangeListenerLock;
        std::vector<::OBody::API::IActorChangeEventListener*> actorChangeEventListeners;

        mutable std::recursive_mutex readinessListenerLock;
        std::vector<::OBody::API::IOBodyReadinessEventListener*> readinessEventListeners;

        // This `IPluginInterface` instance is a special one used to signal to plugin-API event-listeners
        // that a change was effected by OBody's Papyrus functions (the OBodyNative script).
        mutable ::OBody::API::PluginInterface specialPapyrusPluginInterface{"_Papyrus"};

    private:
        static OBody instance_;

        OBody() = default;
        ~OBody() = default;
    };
}  // namespace Body
