#pragma once

using Actor = RE::Actor;
using TESForm = RE::TESForm;
#include "API/API.h"

namespace OBody {
    namespace API {
        class PluginInterface : public IPluginInterface {
        public:
            PluginInterface(const char* owner, void* context = nullptr);

            virtual ::OBody::API::PluginAPIVersion PluginAPIVersion() override;
            virtual const char* SetOwner(const char* owner) override;

            virtual bool ActorIsNaked(RE::Actor* a_actor) override;
            virtual bool ActorIsNaked(RE::Actor* a_actor, bool a_equippingArmor,
                                      const RE::TESForm* a_equippedArmor) override;
            virtual bool ActorHasORefitApplied(RE::Actor* a_actor) override;
            virtual bool ActorIsProcessed(RE::Actor* a_actor) override;
            virtual bool ActorIsBlacklisted(RE::Actor* a_actor) override;
            virtual bool IsORefitEnabled() override;

            virtual bool RegisterEventListener(IActorChangeEventListener& eventListener) override;
            virtual bool DeregisterEventListener(IActorChangeEventListener& eventListener) override;
            virtual bool HasRegisteredEventListener(IActorChangeEventListener& eventListener) override;

            virtual void GetPresetCounts(PresetCounts& payload) override;
            virtual size_t GetPresetNames(PresetCategory category, std::string_view* buffer, size_t bufferLength,
                                          size_t offset, size_t limit) override;

            virtual void EnsureActorIsProcessed(Actor* actor) override;

            virtual void ApplyOBodyMorphsToActor(Actor* actor) override;
            virtual void RemoveOBodyMorphsFromActor(Actor* actor) override;

            virtual void ForcefullyChangeORefitForActor(Actor* actor, bool orefitShouldBeApplied) override;

            virtual void GetPresetAssignedToActor(Actor* actor, PresetAssignmentInformation& payload) override;
            virtual bool AssignPresetToActor(Actor* actor, AssignPresetPayload& payload) override;
        };
    }  // namespace API
}  // namespace OBody
