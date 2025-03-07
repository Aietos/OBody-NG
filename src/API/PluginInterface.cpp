#include "API/PluginInterface.h"
#include "Body/Body.h"

namespace OBody {
    namespace API {
        PluginInterface::PluginInterface(const char* owner, void* context) {
            this->owner = owner;
            this->context = context;
        }

        PluginAPIVersion PluginInterface::PluginAPIVersion() { return PluginAPIVersion::v1; }

        const char* PluginInterface::SetOwner(const char* owner) { return this->owner = owner; }

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
            payload.female = presetContainer.femalePresets.size();
            payload.femaleBlacklisted = presetContainer.blacklistedFemalePresets.size();
            payload.male = presetContainer.malePresets.size();
            payload.maleBlacklisted = presetContainer.blacklistedMalePresets.size();
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
    }  // namespace API
}  // namespace OBody
