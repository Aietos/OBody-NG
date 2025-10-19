#include "SaveFileState/SaveFileState.h"

#pragma push_macro("max")
#pragma push_macro("min")
#undef max
#undef min
namespace SaveFileState {
    // Refer to https://github.com/Ryan-rsm-McKenzie/CommonLibSSE/wiki/Serialization

    void SaveState(SKSE::SerializationInterface* save) {
        Buffer buffer;

        if (save->OpenRecord(ActorRegistryTypeID, 0)) {
            if (!WriteRecordDataForActorRegistryV0(save, buffer, ActorTracker::Registry::GetInstance())) {
                logger::critical("Failed to save the actor-registry!");
            }
        } else {
            logger::critical("Failed to open a record for the actor-registry!");
        }

        if (save->OpenRecord(PresetNameIndexMapTypeID, 0)) {
            if (!WriteRecordDataForPresetNameIndexMapV0(save, buffer, PresetManager::PresetContainer::GetInstance())) {
                logger::critical("Failed to save the preset-name-index-map!");
            }
        } else {
            logger::critical("Failed to open a record for the preset-name-index-map!");
        }
    }

    void LoadState(SKSE::SerializationInterface* load) {
        Buffer buffer;

        size_t actorRegistryCount = 0;
        size_t presetNameMapCount = 0;

        uint32_t type;
        uint32_t version;
        uint32_t length;

        while (load->GetNextRecordInfo(type, version, length)) {
            switch (type) {
                case ActorRegistryTypeID: {
                    switch (version) {
                        case 0: {
                            if (++actorRegistryCount == 1) {
                                if (!ReadRecordDataForActorRegistryV0(load, buffer,
                                                                      ActorTracker::Registry::GetInstance())) {
                                    logger::critical("Failed to load the actor-registry!");
                                }
                            }
                            break;
                        }
                        default: {
                            logger::error(
                                "An actor-registry record of an unknown version '{}' was found in the cosave.",
                                version);
                            break;
                        }
                    }
                    break;
                }
                case PresetNameIndexMapTypeID: {
                    switch (version) {
                        case 0: {
                            if (++presetNameMapCount == 1) {
                                if (!ReadRecordDataForPresetNameIndexMapV0(
                                        load, buffer, PresetManager::PresetContainer::GetInstance())) {
                                    logger::critical("Failed to load the preset-name-index-map!");
                                }
                            }
                            break;
                        }
                        default: {
                            logger::error(
                                "A preset-name-index-map record of an unknown version '{}' was found in the cosave.",
                                version);
                            break;
                        }
                    }
                    break;
                }
                default: {
                    logger::error("A record of unknown type {:#010x} was found in the cosave.", type);
                    break;
                }
            }
        }

        if (presetNameMapCount > 1) {
            logger::error(
                "Multiple preset-name-index-map records were found in the cosave. Only the first one was read.");
        }
    }

    // In this context, revert means to undo the effects of a call to LoadState.
    void RevertState([[maybe_unused]] SKSE::SerializationInterface* revert) {
        auto& registry = ActorTracker::Registry::GetInstance();
        registry.stateForActor.clear();

        auto& presetContainer = PresetManager::PresetContainer::GetInstance();
        presetContainer.femalePresetIndexByName.clear();
        presetContainer.malePresetIndexByName.clear();
        presetContainer.nextFemalePresetIndex.value = 0;
        presetContainer.nextMalePresetIndex.value = 0;
    }

    bool WriteRecordDataForActorRegistryV0(SKSE::SerializationInterface* save, Buffer buffer,
                                           const ActorTracker::Registry& registry) {
        // The format for this is joyously simple:
        // We store 4 bytes for the form-ID, and then we store 4 bytes for the actor-state.
        // And we do that for each entry in the actor-registry.
        static_assert(IsAlignedTo(BufferSize, sizeof(RE::FormID) + sizeof(ActorTracker::ActorState)));

        size_t offset = 0;

        bool succeeded = registry.stateForActor.cvisit_while([&](const auto& entry) {
            RE::FormID formID = entry.first;
            std::memcpy(&buffer[offset], &formID, sizeof(decltype(formID)));
            offset += sizeof(decltype(formID));

            ActorTracker::ActorState actorState = entry.second;
            actorState.value = actorState.value & ActorTracker::ActorState::PersistedInCosaveMask;

            std::memcpy(&buffer[offset], &actorState, sizeof(decltype(actorState)));
            offset += sizeof(decltype(actorState));

            assert(offset <= BufferSize);
            if (offset == BufferSize) {
                if (!save->WriteRecordData(buffer, offset)) return false;
                offset = 0;
            }

            return true;
        });

        // Flush any remaining data.
        if (offset != 0) {
            if (!save->WriteRecordData(buffer, offset)) return false;
        }

        return succeeded;
    }

    bool ReadRecordDataForActorRegistryV0(SKSE::SerializationInterface* load, Buffer buffer,
                                          ActorTracker::Registry& registry) {
        // See `WriteRecordDataForActorRegistryV0` for a description of this format.
        size_t offset{};
        size_t remainingBytes = 0;

        for (;;) {
            if (remainingBytes == 0) {
                remainingBytes = load->ReadRecordData(buffer, BufferSize);

                if (remainingBytes == 0) {
                    // We've reached the end of the data--we're done.
                    return true;
                }

                if (!IsAlignedTo(remainingBytes, sizeof(RE::FormID) + sizeof(ActorTracker::ActorState))) {
                    logger::critical("This save file's actor-registry is misaligned! {{remainingBytes: {}}}",
                                     remainingBytes);
                    return false;
                }

                offset = 0;
            }

            RE::FormID formID;
            std::memcpy(&formID, &buffer[offset], sizeof(decltype(formID)));
            offset += sizeof(decltype(formID));
            remainingBytes -= sizeof(decltype(formID));

            ActorTracker::ActorState actorState;
            std::memcpy(&actorState, &buffer[offset], sizeof(decltype(actorState)));
            offset += sizeof(decltype(actorState));
            remainingBytes -= sizeof(decltype(actorState));

            actorState.value = actorState.value & ActorTracker::ActorState::PersistedInCosaveMask;

            registry.stateForActor.insert_or_assign(formID, actorState);
        }

        assert(false);
    }

    bool WriteRecordDataForPresetNameIndexMapV0(SKSE::SerializationInterface* save, Buffer buffer,
                                                const PresetManager::PresetContainer& presetContainer) {
        size_t offset = 0;

        auto write = [&](const auto& indexByNameMap, auto nextPresetIndex) -> bool {
            // The format for this is dead simple:
            // For all female presets, and then all male presets,
            // we begin with a fixed-sized header,
            // and then for each preset we know, we store the length of its name as a 4-byte integer,
            // followed by a struct detailing its index and other pertinent information,
            // and then the contents of its name.
            // We terminate the preset names with a length of 0, which is not followed by anything.
            static_assert(alignof(PresetNameIndexMapHeaderV0) >= alignof(PresetNameLength));
            static_assert(alignof(PresetNameLength) >= alignof(StateForPresetNameV0));

            PresetNameIndexMapHeaderV0 header;
            PresetNameLength terminatingLength;

            assert(IsAlignedTo(offset, alignof(PresetNameIndexMapHeaderV0)));

            if (offset >= BufferSize - sizeof(PresetNameIndexMapHeaderV0)) {
                if (!save->WriteRecordData(buffer, offset)) goto failedToWriteRecordData;
                offset = 0;
            }

            header = {nextPresetIndex.value};
            std::memcpy(&buffer[offset], &header, sizeof(decltype(header)));
            offset += sizeof(decltype(header));

            for (const auto& [name, index] : indexByNameMap) {
                assert(IsAlignedTo(offset, alignof(PresetNameLength)));

                assert(name.size() <= std::numeric_limits<PresetNameLength>::max());

                PresetNameLength remainingNameBytes = static_cast<PresetNameLength>(name.size());
                PresetNameLength nameOffset = 0;

                if (offset >= BufferSize - sizeof(PresetNameLength) - sizeof(StateForPresetNameV0)) {
                    if (!save->WriteRecordData(buffer, offset)) goto failedToWriteRecordData;
                    offset = 0;
                }

                assert(offset <= BufferSize - sizeof(PresetNameLength) - sizeof(StateForPresetNameV0));
                std::memcpy(&buffer[offset], &remainingNameBytes, sizeof(decltype(remainingNameBytes)));
                offset += sizeof(decltype(remainingNameBytes));

                StateForPresetNameV0 presetNameState{};
                presetNameState.presetIndex = index.value;

                std::memcpy(&buffer[offset], &presetNameState, sizeof(decltype(presetNameState)));
                offset += sizeof(decltype(presetNameState));

                for (;;) {
                    size_t bytesToWrite =
                        std::min(remainingNameBytes, static_cast<PresetNameLength>(BufferSize - offset));
                    remainingNameBytes -= bytesToWrite;

                    std::memcpy(&buffer[offset], name.data() + nameOffset, bytesToWrite);
                    offset += bytesToWrite;

                    assert(offset <= BufferSize);
                    if (offset == BufferSize) {
                        if (!save->WriteRecordData(buffer, BufferSize)) goto failedToWriteRecordData;
                        nameOffset += bytesToWrite;
                        offset = 0;
                        continue;
                    }

                    assert(remainingNameBytes == 0);

                    break;
                }

                offset = AlignUpTo(offset, alignof(PresetNameLength));
            }

            assert(offset <= BufferSize);
            if (offset >= BufferSize - sizeof(PresetNameLength)) {
                if (!save->WriteRecordData(buffer, offset)) goto failedToWriteRecordData;
                offset = 0;
            }

            terminatingLength = 0;
            std::memcpy(&buffer[offset], &terminatingLength, sizeof(decltype(terminatingLength)));
            offset += sizeof(decltype(terminatingLength));

            return true;
        failedToWriteRecordData:
            logger::critical("Failed to write to the open record's data.");
            return false;
        };

        if (!write(presetContainer.femalePresetIndexByName, presetContainer.nextFemalePresetIndex)) return false;
        if (!write(presetContainer.malePresetIndexByName, presetContainer.nextMalePresetIndex)) return false;

        // Flush any remaining data.
        if (!save->WriteRecordData(buffer, offset)) return false;

        return true;
    }

    bool ReadRecordDataForPresetNameIndexMapV0(SKSE::SerializationInterface* load, Buffer buffer,
                                               PresetManager::PresetContainer& presetContainer) {
        size_t offset = 0;
        size_t remainingBytes = 0;

        auto getMoreBytes = [&]() -> bool {
            if (remainingBytes == 0) {
                remainingBytes = load->ReadRecordData(buffer, BufferSize);

                if (remainingBytes == 0) return false;

                if (!IsAlignedTo(remainingBytes, alignof(PresetNameIndexMapHeaderV0))) {
                    logger::critical("This save file's preset-name-index-map is misaligned! {{remainingBytes: {}}}",
                                     remainingBytes);
                    return false;
                }

                offset = 0;
            }

            return true;
        };

        auto read = [&](auto& indexByNameMap, auto& nextPresetIndex) -> bool {
            // See `WriteRecordDataForPresetNameIndexMapV0` for a description of this format.
            if (!getMoreBytes()) goto prematurelyTerminated;
            if (remainingBytes < sizeof(PresetNameIndexMapHeaderV0)) goto prematurelyTerminated;

            PresetNameIndexMapHeaderV0 header;
            std::memcpy(&header, &buffer[offset], sizeof(decltype(header)));
            offset += sizeof(decltype(header));
            remainingBytes -= sizeof(decltype(header));

            nextPresetIndex.value = header.nextPresetIndex;

            indexByNameMap.reserve(nextPresetIndex.value);

            for (;;) {
                if (!getMoreBytes()) goto prematurelyTerminated;
                if (remainingBytes < sizeof(PresetNameLength)) goto prematurelyTerminated;

                PresetNameLength length;
                std::memcpy(&length, &buffer[offset], sizeof(decltype(length)));
                offset += sizeof(decltype(length));
                remainingBytes -= sizeof(decltype(length));

                if (length == 0) {
                    break;
                }

                if (!getMoreBytes()) goto prematurelyTerminated;
                if (remainingBytes < sizeof(StateForPresetNameV0)) goto prematurelyTerminated;

                StateForPresetNameV0 presetNameState;
                std::memcpy(&presetNameState, &buffer[offset], sizeof(decltype(presetNameState)));
                offset += sizeof(decltype(presetNameState));
                remainingBytes -= sizeof(decltype(presetNameState));

                std::string presetName;
                presetName.reserve(length);

                PresetNameLength remainingNameBytes = length;

                for (;;) {
                    size_t bytesToRead = std::min(remainingNameBytes, static_cast<PresetNameLength>(remainingBytes));
                    remainingNameBytes -= bytesToRead;

                    presetName.append(reinterpret_cast<const char*>(&buffer[offset]), bytesToRead);
                    offset += bytesToRead;
                    remainingBytes -= bytesToRead;

                    if (remainingNameBytes == 0) {
                        break;
                    }

                    if (!getMoreBytes()) goto prematurelyTerminated;
                }

                assert(presetName.size() == length);

                size_t unalignedOffset = offset;
                offset = AlignUpTo(offset, alignof(PresetNameLength));
                remainingBytes -= offset - unalignedOffset;

                indexByNameMap[presetName] = PresetManager::AssignedPresetIndex{presetNameState.presetIndex};
            }

            return true;
        prematurelyTerminated:
            logger::critical("This save file's preset-name-index-map was prematurely terminated!");
            return false;
        };

        if (!read(presetContainer.femalePresetIndexByName, presetContainer.nextFemalePresetIndex)) return false;
        if (!read(presetContainer.malePresetIndexByName, presetContainer.nextMalePresetIndex)) return false;

        if (remainingBytes != 0) {
            logger::error(
                "This save file's preset-name-index-map contains unknown trailing data which has been ignored. "
                "{{remainingBytes: {}}}",
                remainingBytes);
        }

        return true;
    }
}  // namespace SaveFileState
#pragma pop_macro("max")
#pragma pop_macro("min")