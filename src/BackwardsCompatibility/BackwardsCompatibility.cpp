#pragma once

#include "BackwardsCompatibility/BackwardsCompatibility.h"
#include "Body/Body.h"

BackwardsCompatibility::State BackwardsCompatibility::State::instance;

namespace BackwardsCompatibility {
    State& State::GetInstance() { return instance; }

    void State::SetStateUponStartOfNewGame() { haveMigratedStorageUtilPresetAssignments = true; }

    void State::FixupAfterLoadingGame() {
        if (!haveMigratedStorageUtilPresetAssignments) {
            StorageUtilFunctions storageUtil;
            if (FindStorageUtilFunctions(storageUtil)) {
                if (MigrateStorageUtilPresetAssignmentsOverToSKSECosave(storageUtil)) {
                    haveMigratedStorageUtilPresetAssignments = true;
                }
            }
        }
    }

    bool State::MigrateStorageUtilPresetAssignmentsOverToSKSECosave(const StorageUtilFunctions& storageUtil) {
        logger::info("Migrating legacy StorageUtil preset assignments over to the SKSE cosave.");

        uint32_t stringKeyCount = storageUtil.getStringKeyCount(nullptr, nullptr);
        logger::info("StorageUtil.stringKeyCount: {}", stringKeyCount);

        if (stringKeyCount == 0) {
            logger::info("There were no legacy StorageUtil preset assignments to migrate!");
            return true;
        }

        auto& presets{PresetManager::PresetContainer::GetInstance()};
        auto& registry{ActorTracker::Registry::GetInstance()};

        for (uint32_t index = 0; index < stringKeyCount; ++index) {
            RE::BSFixedString fixedKey = storageUtil.getNthStringKey(nullptr, nullptr, index);

            // We're looking for keys in this format: `obody_<int32 as decimal>_preset`.

            auto keyLength = fixedKey.length();
            if (keyLength < 14) continue;
            const char* key = fixedKey.data();
            if ("obody_"sv.compare({key, 6}) != 0) continue;
            const char* endOfFormID = key + keyLength - 7;
            if ("_preset"sv.compare({endOfFormID, 7}) != 0) continue;

            int32_t formID;
            auto parse = std::from_chars(key + 6, endOfFormID, formID);
            if ((parse.ec != std::errc()) | (parse.ptr != endOfFormID)) continue;

            RE::BSFixedString fixedValue = storageUtil.getStringValue(nullptr, nullptr, key, nullptr);

            const char* value = fixedValue.data();
            auto valueLength = fixedValue.length();

            if (value == nullptr) {
                logger::info("StorageUtil preset assignment: {:#010x} = <null>", (uint32_t)formID);
                continue;
            }

            logger::info("StorageUtil preset assignment: {:#010x} = {}", (uint32_t)formID,
                         std::string_view{value, valueLength});

            RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(formID);

            if (actor == nullptr) {
                logger::info("\tNo actor could be found with a form-ID of {:#010x}", (uint32_t)formID);
                continue;
            }

            bool isFemale = Body::OBody::IsFemale(actor);

            std::string presetName{value, valueLength};

            auto& presetIndexMap = isFemale ? presets.femalePresetIndexByName : presets.malePresetIndexByName;
            auto& nextPresetIndex = isFemale ? presets.nextFemalePresetIndex : presets.nextMalePresetIndex;

            auto indexAssignment = presetIndexMap.emplace(presetName, nextPresetIndex.value);

            if (indexAssignment.second) {
                // This is a preset name we haven't seen before.
                ++nextPresetIndex.value;
                // The sparse-index is mapped to -1 because the preset is absent.
                (isFemale ? presets.allFemalePresetsByIndex : presets.allMalePresetsByIndex)
                    .resize(nextPresetIndex.value, (PresetManager::SparsePresetIndex)-1);

                // Maybe the actor's sex has changed since the preset was assigned?
                auto& presetIndexMapOtherSex =
                    isFemale ? presets.malePresetIndexByName : presets.femalePresetIndexByName;
                auto& nextPresetIndexOtherSex = isFemale ? presets.nextMalePresetIndex : presets.nextFemalePresetIndex;
                auto indexAssignmentOtherSex =
                    presetIndexMapOtherSex.emplace(presetName, nextPresetIndexOtherSex.value);

                if (indexAssignmentOtherSex.second) {
                    // This is also a preset name we haven't seen before.
                    ++nextPresetIndexOtherSex.value;
                    // The sparse-index is mapped to -1 because the preset is absent.
                    (isFemale ? presets.allMalePresetsByIndex : presets.allFemalePresetsByIndex)
                        .resize(nextPresetIndexOtherSex.value, (PresetManager::SparsePresetIndex)-1);
                }
            }

            auto assignedIndex = indexAssignment.first->second;

            ActorTracker::ActorState actorState{};
            // Plus one because an index of zero on the actor signifies the absence of a preset.
            actorState.presetIndex = assignedIndex.value + 1;

            // If the actor already has an entry in the actor-tracker we'll not overwrite it.
            // The SKSE cosave assignments take precedence over the StorageUtil assignments.
            if (registry.stateForActor.emplace(formID, actorState)) {
                logger::info("\tAssigned a preset-index of {}", assignedIndex.value);
            } else {
                logger::info("\tDid not assign a preset-index of {}, for the actor already has a preset-assignment.",
                             assignedIndex.value);
            }
        }

        logger::info("Migrated legacy StorageUtil preset assignments over to the SKSE cosave!");
        return true;
    }

    bool FindStorageUtilFunctions(StorageUtilFunctions& functions) {
        auto papyrus = RE::BSScript::Internal::VirtualMachine::GetSingleton();

        if (papyrus == nullptr) {
            return false;
        }

        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> storageUtilTypeInfo{};
        if (!papyrus->GetScriptObjectType1("StorageUtil", storageUtilTypeInfo)) {
            return false;
        }

        uint32_t globalFunctionCount = storageUtilTypeInfo->GetNumGlobalFuncs();

        if (globalFunctionCount < 3) {
            return false;
        }

        const RE::BSScript::ObjectTypeInfo::GlobalFuncInfo* globalFunctions = storageUtilTypeInfo->GetGlobalFuncIter();
        const RE::BSScript::ObjectTypeInfo::GlobalFuncInfo* globalFunctionsEnd = globalFunctions + globalFunctionCount;

        uint32_t found = 0;

        for (auto globalFunction = globalFunctions; globalFunction < globalFunctionsEnd; ++globalFunction) {
            if (found == 0b111) {
                return true;
            }

            const RE::BSTSmartPointer<RE::BSScript::IFunction>& function = globalFunction->func;

            if (!function->GetIsNative()) {
                continue;
            }

            const RE::BSFixedString& fixedName = function->GetName();
            auto nameLength = fixedName.length();

            if (nameLength < 14) {
                continue;
            }

            const std::string_view name = {fixedName.data(), nameLength};

            static_assert(sizeof(RE::BSScript::NF_util::NativeFunctionBase) == 0x50);

            // This member is protected, so we can't access it nicely,
            // but its offset is the same across every versions of Skyrim SE>
            uintptr_t callback = reinterpret_cast<uintptr_t>(function.get()) + 0x50;

            if ("GetStringValue"sv.compare(name) == 0) {
                functions.getStringValue =
                    *reinterpret_cast<const decltype(StorageUtilFunctions::getStringValue)*>(callback);
                logger::info("Found StorageUtil::GetStringValue: {:#018x}", callback);
                found |= 1 << 0;
            } else if ("debug_GetStringKeysCount"sv.compare(name) == 0) {
                functions.getStringKeyCount =
                    *reinterpret_cast<const decltype(StorageUtilFunctions::getStringKeyCount)*>(callback);
                logger::info("Found StorageUtil::debug_GetStringKeysCount: {:#018x}", callback);
                found |= 1 << 1;
            } else if ("debug_GetStringKey"sv.compare(name) == 0) {
                functions.getNthStringKey =
                    *reinterpret_cast<const decltype(StorageUtilFunctions::getNthStringKey)*>(callback);
                logger::info("Found StorageUtil::debug_GetStringKey: {:#018x}", callback);
                found |= 1 << 2;
            }
        }

        return found == 0b111;
    }
}  // namespace BackwardsCompatibility
