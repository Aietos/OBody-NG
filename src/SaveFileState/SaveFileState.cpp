#include "SaveFileState/SaveFileState.h"

#include "Body/Body.h"
#include "UI/PreviewPanel.h"
#include "UI/UI.h"

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

        if (save->OpenRecord(BackwardsCompatibilityStateTypeID, 0)) {
            if (!WriteRecordDataForBackwardsCompatibilityStateV0(save, buffer,
                                                                 BackwardsCompatibility::State::GetInstance())) {
                logger::critical("Failed to save the backwards-compatibility-state!");
            }
        } else {
            logger::critical("Failed to open a record for the backwards-compatibility-state!");
        }

        // To make sure it's known while the save loads, before Papyrus sets it again
        if (const auto& obody{Body::OBody::GetInstance()}; obody.distributionKeyIsSet) {
            if (save->OpenRecord(DistributionKeyTypeID, 0)) {
                if (!WriteRecordDataForDistributionKeyV0(save, obody)) {
                    logger::critical("Failed to save the distribution key!");
                }
            } else {
                logger::critical("Failed to open a record for the distribution key!");
            }
        }

        if (save->OpenRecord(PreviewSettingsTypeID, 0)) {
            if (!WriteRecordDataForPreviewSettingsV0(save)) {
                logger::critical("Failed to save the preview settings!");
            }
        } else {
            logger::critical("Failed to open a record for the preview settings!");
        }
    }

    void LoadState(SKSE::SerializationInterface* load) {
        Buffer buffer;

        size_t actorRegistryCount = 0;
        size_t presetNameMapCount = 0;
        size_t backwardsCompatibilityStateCount = 0;
        size_t distributionKeyCount = 0;
        size_t previewSettingsCount = 0;

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
                case BackwardsCompatibilityStateTypeID: {
                    switch (version) {
                        case 0: {
                            if (++backwardsCompatibilityStateCount == 1) {
                                if (!ReadRecordDataForBackwardsCompatibilityStateV0(
                                        load, buffer, BackwardsCompatibility::State::GetInstance())) {
                                    logger::critical("Failed to load the backwards-compatibility-state!");
                                }
                            }
                            break;
                        }
                        default: {
                            logger::error(
                                "A backwards-compatibility-state record of an unknown version '{}' was found in the "
                                "cosave.",
                                version);
                            break;
                        }
                    }
                    break;
                }
                case DistributionKeyTypeID: {
                    switch (version) {
                        case 0: {
                            if (++distributionKeyCount == 1) {
                                if (!ReadRecordDataForDistributionKeyV0(load, length, Body::OBody::GetInstance())) {
                                    logger::critical("Failed to load the distribution key!");
                                }
                            }
                            break;
                        }
                        default: {
                            logger::error(
                                "A distribution-key record of an unknown version '{}' was found in the cosave.",
                                version);
                            break;
                        }
                    }
                    break;
                }
                case PreviewSettingsTypeID: {
                    switch (version) {
                        case 0: {
                            if (++previewSettingsCount == 1) {
                                if (!ReadRecordDataForPreviewSettingsV0(load, length)) {
                                    logger::critical("Failed to load the preview settings!");
                                }
                            }
                            break;
                        }
                        default: {
                            logger::error(
                                "A preview-settings record of an unknown version '{}' was found in the cosave.",
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

        if (actorRegistryCount > 1) {
            logger::error("Multiple actor-registry records were found in the cosave. Only the first one was read.");
        }
        if (presetNameMapCount > 1) {
            logger::error(
                "Multiple preset-name-index-map records were found in the cosave. Only the first one was read.");
        }
        if (backwardsCompatibilityStateCount > 1) {
            logger::error(
                "Multiple backwards-compatibility-state records were found in the cosave. Only the first one was "
                "read.");
        }
        if (distributionKeyCount > 1) {
            logger::error("Multiple distribution-key records were found in the cosave. Only the first one was read.");
        }
        if (previewSettingsCount > 1) {
            logger::error("Multiple preview-settings records were found in the cosave. Only the first one was read.");
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

        auto& backwardsCompatibility = BackwardsCompatibility::State::GetInstance();
        backwardsCompatibility.flags = 0;

        // Saves from before the cosave kept the distribution key only get it once Papyrus sets it
        Body::OBody::GetInstance().distributionKeyIsSet = false;

        // New games, and saves from before the cosave kept them, have the preview checkboxes as they start out
        UI::PresetList::SetPreviewEnabled(true);
        UI::PreviewPanel::SetKeepMenuOpen(false);
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

            if (load->ResolveFormID(formID, formID)) {
                actorState.value = actorState.value & ActorTracker::ActorState::PersistedInCosaveMask;
                registry.stateForActor.insert_or_assign(formID, actorState);
            }
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

    bool WriteRecordDataForBackwardsCompatibilityStateV0(SKSE::SerializationInterface* save, Buffer buffer,
                                                         const BackwardsCompatibility::State& backwardsCompatibility) {
        static_assert(sizeof(decltype(backwardsCompatibility)) <= BufferSize);

        std::memcpy(&buffer[0], &backwardsCompatibility, sizeof(decltype(backwardsCompatibility)));

        if (!save->WriteRecordData(buffer, sizeof(decltype(backwardsCompatibility)))) return false;

        return true;
    }

    bool ReadRecordDataForBackwardsCompatibilityStateV0(SKSE::SerializationInterface* load, Buffer buffer,
                                                        BackwardsCompatibility::State& backwardsCompatibility) {
        static_assert(sizeof(decltype(backwardsCompatibility)) <= BufferSize);

        size_t remainingBytes = load->ReadRecordData(buffer, BufferSize);

        // Presently, we assume that the backwards-compatibility-state can never be larger than 4-bytes.
        // If the `BackwardsCompatibility::State` structure is made larger in the future,
        // this code for reading the backwards-compatibility-state will need to be amended
        // to handle reading the first 4-bytes and then reading more if those 4-bytes indicate that
        // the structure is larger than that.
        // Alternatively, the version of this record in the SKSE cosave can be incremented
        // and functions for reading and writing the V1 structures can be implemented.
        static_assert(sizeof(decltype(backwardsCompatibility)) == 4);

        if (remainingBytes < 4) {
            logger::critical("This save file's backwards-compatibility-state is invalid! {{remainingBytes: {}}}",
                             remainingBytes);
            return false;
        }

        std::memcpy(&backwardsCompatibility, &buffer[0], sizeof(decltype(backwardsCompatibility)));

        return true;
    }

    bool WriteRecordDataForDistributionKeyV0(SKSE::SerializationInterface* save, const Body::OBody& obody) {
        // The format is just the key's characters, the record's length is the key's
        const auto& distributionKey{obody.distributionKey};
        return save->WriteRecordData(distributionKey.data(), static_cast<uint32_t>(distributionKey.size()));
    }

    bool ReadRecordDataForDistributionKeyV0(SKSE::SerializationInterface* load, const uint32_t length,
                                            Body::OBody& obody) {
        std::string distributionKey(length, '\0');
        if (length == 0 || load->ReadRecordData(distributionKey.data(), length) != length) {
            logger::critical("This save file's distribution key is invalid! {{length: {}}}", length);
            return false;
        }

        obody.distributionKey = std::move(distributionKey);
        obody.distributionKeyIsSet = true;
        logger::info("Distribution key from the cosave: {}", obody.distributionKey);

        return true;
    }

    bool WriteRecordDataForPreviewSettingsV0(SKSE::SerializationInterface* save) {
        PreviewSettingsV0 settings{0};

        if (UI::PresetList::IsPreviewEnabled()) settings.flags |= PreviewSettingsV0::kLivePreview;

        if (UI::PreviewPanel::IsKeepMenuOpenSet()) settings.flags |= PreviewSettingsV0::kKeepMenuOpen;

        return save->WriteRecordData(&settings, sizeof(settings));
    }

    bool ReadRecordDataForPreviewSettingsV0(SKSE::SerializationInterface* load, const uint32_t length) {
        PreviewSettingsV0 settings{0};

        if (length < sizeof(settings) || load->ReadRecordData(&settings, sizeof(settings)) != sizeof(settings)) {
            logger::critical("This save file's preview settings are invalid! {{length: {}}}", length);
            return false;
        }

        UI::PresetList::SetPreviewEnabled((settings.flags & PreviewSettingsV0::kLivePreview) != 0);
        UI::PreviewPanel::SetKeepMenuOpen((settings.flags & PreviewSettingsV0::kKeepMenuOpen) != 0);

        return true;
    }
}  // namespace SaveFileState
#pragma pop_macro("max")
#pragma pop_macro("min")
