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
    }  // namespace API
}  // namespace OBody
