#include "API/PluginInterface.h"
#include "Body/Body.h"

#pragma push_macro("max")
#pragma push_macro("min")
#undef max
#undef min

namespace OBody::API {
    PluginInterface::PluginInterface(const char* owner, void* context) {
        this->owner = owner;
        this->context = context;
    }

    PluginAPIVersion PluginInterface::PluginAPIVersion() { return PluginAPIVersion::v1; }

    const char* PluginInterface::SetOwner(const char* a_owner) { return this->owner = a_owner; }

    bool PluginInterface::ActorIsNaked(RE::Actor* a_actor) {
        return Body::OBody::GetInstance().IsNaked(a_actor, false, nullptr);
    }

    bool PluginInterface::ActorIsNaked(RE::Actor* a_actor, bool a_equippingArmor, const TESForm* a_equippedArmor) {
        return Body::OBody::GetInstance().IsNaked(a_actor, !a_equippingArmor, a_equippedArmor);
    }

    bool PluginInterface::ActorHasORefitApplied(RE::Actor* a_actor) {
        return Body::OBody::GetInstance().IsClotheActive(a_actor);
    };

    bool PluginInterface::ActorIsProcessed(RE::Actor* a_actor) {
        return Body::OBody::GetInstance().IsProcessed(a_actor);
    };

    bool PluginInterface::ActorIsBlacklisted(RE::Actor* a_actor) {
        return Body::OBody::GetInstance().IsBlacklisted(a_actor);
    };

    bool PluginInterface::IsORefitEnabled() { return Body::OBody::GetInstance().setRefit; }

    bool PluginInterface::RegisterEventListener(IActorChangeEventListener& eventListener) {
        return Body::OBody::GetInstance().AttachEventListener(eventListener);
    }

    bool PluginInterface::DeregisterEventListener(IActorChangeEventListener& eventListener) {
        return Body::OBody::GetInstance().DetachEventListener(eventListener);
    }

    bool PluginInterface::HasRegisteredEventListener(IActorChangeEventListener& eventListener) {
        return Body::OBody::GetInstance().IsEventListenerAttached(eventListener);
    }

    void PluginInterface::GetPresetCounts(PresetCounts& payload) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        payload.female = static_cast<decltype(payload.female)>(presetContainer.femalePresets.size());
        payload.male = static_cast<decltype(payload.male)>(presetContainer.malePresets.size());
        payload.femaleBlacklisted =
            static_cast<decltype(payload.femaleBlacklisted)>(presetContainer.blacklistedFemalePresets.size());
        payload.maleBlacklisted =
            static_cast<decltype(payload.maleBlacklisted)>(presetContainer.blacklistedMalePresets.size());
    }

    size_t PluginInterface::GetPresetNames(PresetCategory category, std::string_view* buffer, size_t bufferLength,
                                           size_t offset, size_t limit) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        const PresetManager::PresetSet* presets;

        switch (category) {
            case PresetCategory::PresetCategoryFemale:
                presets = &presetContainer.femalePresets;
                break;
            case PresetCategory::PresetCategoryFemaleBlacklisted:
                presets = &presetContainer.blacklistedFemalePresets;
                break;
            case PresetCategory::PresetCategoryMale:
                presets = &presetContainer.malePresets;
                break;
            case PresetCategory::PresetCategoryMaleBlacklisted:
                presets = &presetContainer.blacklistedMalePresets;
                break;
            default:
                presets = nullptr;
        }

        if (presets == nullptr) {
            return 0;
        }

        size_t presetCount = presets->size();
        limit = std::min(bufferLength, limit);

        size_t index = 0;
        for (size_t presetIndex = offset; (presetIndex < presetCount) & (index < limit); ++presetIndex, ++index) {
            const auto& preset = (*presets)[presetIndex];
            buffer[index] = {preset.name.data(), preset.name.size()};
        }

        return index;
    }

    void PluginInterface::EnsureActorIsProcessed(Actor* a_actor) {
        Body::OBody::GetInstance().GenerateActorBody(a_actor, this);
    }

    void PluginInterface::ApplyOBodyMorphsToActor(Actor* a_actor) {
        Body::OBody::GetInstance().ReapplyActorMorphs(a_actor, this);
    }

    void PluginInterface::RemoveOBodyMorphsFromActor(Actor* a_actor) {
        Body::OBody::GetInstance().ClearActorMorphs(a_actor, true, this);
    }

    void PluginInterface::ForcefullyChangeORefitForActor(Actor* a_actor, bool orefitShouldBeApplied) {
        Body::OBody::GetInstance().ForcefullyChangeORefit(a_actor, orefitShouldBeApplied, this);
    }

    void PluginInterface::GetPresetAssignedToActor(Actor* a_actor, PresetAssignmentInformation& payload) {
        bool isFemale = Body::OBody::GetInstance().IsFemale(a_actor);

        static_assert(PresetAssignmentInformation::Flags::IsFemale == 1);
        auto flags = static_cast<PresetAssignmentInformation::Flags>(isFemale);

        payload.flags = flags;

        uint32_t actorPresetIndex = ActorTracker::Registry::GetInstance().GetPresetIndexForActor(a_actor);

        if (actorPresetIndex != 0) {
            // Minus one because an index of zero assigned to the actor signifies the absence of a preset.
            auto preset = PresetManager::AssignedPresetIndex{actorPresetIndex - 1}.GetPreset(isFemale);

            if (preset != nullptr) {
                payload.presetName = {preset->name.data(), preset->name.size()};
                return;
            }
        }

        payload.presetName = ""sv;
    }

    bool PluginInterface::AssignPresetToActor(Actor* a_actor, AssignPresetPayload& payload) {
        const auto& obody{Body::OBody::GetInstance()};
        auto& registry{ActorTracker::Registry::GetInstance()};
        auto formID = a_actor->formID;

        if ((payload.presetName.size() == 0) | (payload.presetName.data() == nullptr)) {
            // Clear their preset assignment, if they have one.
            uint32_t previousPresetIndex = 0;
            registry.stateForActor.visit(formID, [&](auto& entry) {
                previousPresetIndex = entry.second.presetIndex;
                entry.second.presetIndex = 0;
            });

            if ((payload.flags & AssignPresetPayload::Flags::DoNotApplyMorphs) == 0) {
                bool immediate = (payload.flags & AssignPresetPayload::Flags::ForceImmediateApplicationOfMorphs) != 0;
                obody.ClearActorMorphs(a_actor, immediate, this);
            }

            if (previousPresetIndex != 0) {
                obody.SendActorChangeEvent(
                    a_actor,
                    [&] {
                        using Event = ::OBody::API::IActorChangeEventListener;

                        Event::OnActorPresetChangedWithoutGeneration::Payload payload{
                            this,
                            // Note that the plugin-API mandates that this be a null-terminated string.
                            // Minus one because an index of zero assigned to the actor signifies the absence of a
                            // preset.
                            PresetManager::AssignedPresetIndex{previousPresetIndex - 1}.GetPresetNameView(
                                obody.IsFemale(a_actor))};

                        auto flags = Event::OnActorPresetChangedWithoutGeneration::Flags::PresetWasUnassigned;

                        return std::make_pair(flags, payload);
                    },
                    [](auto listener, auto actor, auto&& args) {
                        listener->OnActorPresetChangedWithoutGeneration(actor, args.first, args.second);
                    });
            }

            return true;
        }

        bool isFemale = obody.IsFemale(a_actor);

        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        auto preset = GetPresetByNameForRandom(
            isFemale ? presetContainer.allFemalePresets : presetContainer.allMalePresets, payload.presetName);

        if (!preset) {
            return false;
        }

        // Like OBody::GenerateBodyByName, we set this morph to prevent a crash with SynthEBD/Synthesis.
        if (obody.synthesisInstalled) {
            obody.SetMorph(a_actor, "obody_synthebd", "OBody", 1.0F);
        }

        if ((payload.flags & AssignPresetPayload::Flags::DoNotApplyMorphs) == 0) {
            bool immediate = (payload.flags & AssignPresetPayload::Flags::ForceImmediateApplicationOfMorphs) != 0;
            obody.GenerateBodyByPreset(a_actor, *preset, immediate, this);
        } else {
            // Assign the preset to the actor.
            auto assignedPresetIndex = preset->assignedIndex;
            // Plus one because an index of zero on the actor signifies the absence of a preset.
            uint32_t actorPresetIndex = assignedPresetIndex.value + 1;
            ActorTracker::ActorState fallbackActorState{};
            fallbackActorState.presetIndex = actorPresetIndex;

            registry.stateForActor.emplace_or_visit(formID, fallbackActorState,
                                                    [&](auto& entry) { entry.second.presetIndex = actorPresetIndex; });

            obody.SendActorChangeEvent(
                a_actor,
                [&] {
                    using Event = ::OBody::API::IActorChangeEventListener;

                    Event::OnActorPresetChangedWithoutGeneration::Payload payload{
                        this,
                        // Note that the plugin-API mandates that this be a null-terminated string.
                        assignedPresetIndex.GetPresetNameView(isFemale)};

                    Event::OnActorPresetChangedWithoutGeneration::Flags flags{};

                    return std::make_pair(flags, payload);
                },
                [](auto listener, auto actor, auto&& args) {
                    listener->OnActorPresetChangedWithoutGeneration(actor, args.first, args.second);
                });
        }

        return true;
    }
}  // namespace OBody::API

#pragma pop_macro("max")
#pragma pop_macro("min")