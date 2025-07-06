#pragma once

#include "PresetManager/PresetManager.h"
#include <boost/unordered/concurrent_flat_map.hpp>

namespace ActorTracker {
    struct ActorState {
        union {
            uint32_t value = 0;

            struct {
                // This is set when events are being sent to `IActorChangeEventListener` instances for this actor.
                // We use this bit to prevent the sending of recursive events for an actor--should an event-listener
                // do something that would trigger a recursive event.
                // This is particularly important for the `OnActorClothingUpdate` event, as if an event-listener equips
                // or unequips armour in response to it: it can easily cause an infinite loop of `TESEquipEvent`s,
                // which would freeze the game until it crashes from a stack overflow.
                uint32_t actorChangeEventsAreBeingSent : 1;
                uint32_t unusedSpareBitsForLaterUse : 11;
                static_assert(PresetManager::AssignedPresetIndex::BitWidth == 20);
                uint32_t presetIndex : 20;
            };
        };

        static constexpr uint32_t PersistedInCosaveMask = 0b11111111111111111111000000000000;
    };

    // We want this structure to be the same size as a FormID so that
    // storing it in a hashtable with form-IDs is as compact as can be.
    static_assert(sizeof(ActorState) == 4);

    // This keeps track of the state which must remain unaffected by the clearing of RaceMenu body-morphs
    // associated with each actor.
    // We persist some of this state via our SKSE cosave.
    class Registry {
    public:
        static Registry& GetInstance();

        boost::concurrent_flat_map<RE::FormID, ActorState> stateForActor;

    private:
        static Registry instance;
    };
}  // namespace ActorTracker
