#pragma once

#include "ActorTracker/ActorTracker.h"

namespace BackwardsCompatibility {
    struct StorageUtilFunctions {
        RE::BSFixedString (*getStringValue)(RE::StaticFunctionTag* base, RE::TESForm* object, const char* key,
                                            const char* fallback);
        uint32_t (*getStringKeyCount)(RE::StaticFunctionTag* base, RE::TESForm* object);
        RE::BSFixedString (*getNthStringKey)(RE::StaticFunctionTag* base, RE::TESForm* object, uint32_t index);
    };

    bool FindStorageUtilFunctions(StorageUtilFunctions& functions);

    class State {
    public:
        static State& GetInstance();
        void SetStateUponStartOfNewGame();
        void FixupAfterLoadingGame();
        bool MigrateStorageUtilPresetAssignmentsOverToSKSECosave(const StorageUtilFunctions& storageUtil);

        union {
            uint32_t flags = 0;

            struct {
                uint32_t haveMigratedStorageUtilPresetAssignments : 1;
            };
        };

    private:
        static State instance;
    };
}  // namespace BackwardsCompatibility
