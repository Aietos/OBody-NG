#pragma once

#include "ActorTracker/ActorTracker.h"
#include "PresetManager/PresetManager.h"

namespace SaveFileState {
    constexpr uint32_t CosaveUID = 0xa0B0D9ee;
    constexpr uint32_t ActorRegistryTypeID = 0xa0B0D9ea;
    constexpr uint32_t PresetNameIndexMapTypeID = 0xa0B0D9e0;

    // Making a call for each tiny piece of data and having SKSE copy it into its filestream
    // is the recipe for a mod that makes saves and loads take longer than they should.
    // We buffer our reads and writes.
    constexpr size_t BufferSize = 65536;
    using Buffer = uint8_t[BufferSize];

    void SaveState(SKSE::SerializationInterface* save);
    void LoadState(SKSE::SerializationInterface* load);
    void RevertState(SKSE::SerializationInterface* revert);

    bool WriteRecordDataForActorRegistryV0(SKSE::SerializationInterface* save, Buffer buffer,
                                           const ActorTracker::Registry& registry);
    bool ReadRecordDataForActorRegistryV0(SKSE::SerializationInterface* load, Buffer buffer,
                                          ActorTracker::Registry& registry);

    bool WriteRecordDataForPresetNameIndexMapV0(SKSE::SerializationInterface* save, Buffer buffer,
                                                const PresetManager::PresetContainer& presetContainer);
    bool ReadRecordDataForPresetNameIndexMapV0(SKSE::SerializationInterface* load, Buffer buffer,
                                               PresetManager::PresetContainer& presetContainer);

    struct PresetNameIndexMapHeaderV0 {
        PresetManager::SparsePresetIndex nextPresetIndex;
    };

    static_assert(sizeof(PresetNameIndexMapHeaderV0) == 4);

    struct StateForPresetNameV0 {
        uint32_t presetIndex;
    };

    static_assert(sizeof(StateForPresetNameV0) == 4);

    using PresetNameLength = uint32_t;

    // Used to round up an integer to a specified multiple-of-power-of-two alignment.
    template <typename I>
    inline __forceinline constexpr I AlignUpTo(I value, I alignment) {
        return static_cast<I>((value + alignment - 1) & ~(alignment - 1));
    }

    template <typename I>
    inline __forceinline constexpr bool IsAlignedTo(I value, I alignment) {
        return (value & (alignment - 1)) == 0;
    }
}  // namespace SaveFileState
