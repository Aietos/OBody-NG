#pragma once

#include "ActorTracker/ActorTracker.h"

ActorTracker::Registry ActorTracker::Registry::instance;

namespace ActorTracker {
    Registry& Registry::GetInstance() { return instance; }

    uint32_t Registry::GetPresetIndexForActor(RE::Actor* a_actor) const {
        uint32_t actorPresetIndex = 0;
        stateForActor.cvisit(a_actor->formID, [&](auto& entry) { actorPresetIndex = entry.second.presetIndex; });
        return actorPresetIndex;
    }

    std::optional<PresetManager::Preset> Registry::GetPresetForActor(RE::Actor* a_actor, const bool isFemale) const {
        uint32_t actorPresetIndex = GetPresetIndexForActor(a_actor);

        if (actorPresetIndex != 0) {
            // Minus one because an index of zero assigned to the actor signifies the absence of a preset.
            auto preset = PresetManager::AssignedPresetIndex{actorPresetIndex - 1}.GetPreset(isFemale);

            if (preset) {
                return *preset;
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> Registry::GetPresetNameForActor(RE::Actor* a_actor, const bool isFemale) const {
        const auto preset = GetPresetForActor(a_actor, isFemale);
        if (preset) {
            return preset->name;
        }
        return std::nullopt;
    }

}  // namespace ActorTracker
