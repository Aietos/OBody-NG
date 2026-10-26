#include "Body/Event.h"

#include "Body/Body.h"
#include "JSONParser/JSONParser.h"
#include "Preview/Preview.h"
#include "UI/Translations.h"

#include <boost/unordered/concurrent_flat_set.hpp>

constinit Event::OBodyEventHandler Event::OBodyEventHandler::singleton;

namespace {
    // NPCs that loaded before OBody could distribute to them, see GenerateWaitingBodies
    boost::concurrent_flat_set<RE::FormID> g_waitingForDistribution;

    // Whether OBody distributes presets to this actor by itself
    bool IsDistributionTarget(RE::Actor* a_actor) {
        if (!a_actor->HasKeywordString("ActorTypeNPC") || a_actor->IsChild()) {
            return false;
        }

        const auto& parser{Parser::JSONParser::GetInstance()};
        return Body::OBody::IsFemale(a_actor) ? !parser.distributionDisabledForFemale
                                              : !parser.distributionDisabledForMale;
    }

    // Until the save has finished loading, the presets have the indexes of the previous one. And until the save's
    // distribution key is known, distributionKey may be another save's, so NPCs that already have their body would
    // look like they don't and get another one
    bool CanDistribute(const Body::OBody& a_obody) {
        return a_obody.readyForPluginAPIUsage && a_obody.distributionKeyIsSet;
    }

    // Character::Load3D builds the actor's 3D, from the threads that load NPCs
    // An actor whose body is generated before that gets each armor addon morphed by Racemenu as it's attached
    struct Load3D {
        static RE::NiAVObject* thunk(RE::Character* a_this, const bool a_backgroundLoading) {
            // The preview's copy gets the body of the actor it copies. It's hidden until it's dressed like them
            if (Preview::Session::GetInstance().IsCopy(a_this)) {
                auto* const result{func(a_this, a_backgroundLoading)};
                if (result) {
                    result->SetAppCulled(true);
                }
                return result;
            }

            if (const auto& obody{Body::OBody::GetInstance()};
                CanDistribute(obody) && obody.morphInterface && IsDistributionTarget(a_this)) {
                // A 3D that is already there doesn't get attached again, so it needs ApplyBodyMorphs
                obody.GenerateActorBody(a_this, nullptr, !a_this->Is3DLoaded());
            }

            return func(a_this, a_backgroundLoading);
        }

        static inline REL::Relocation<decltype(thunk)> func;
    };
}  // namespace

void Event::InstallHooks() {
    REL::Relocation<std::uintptr_t> characterVtable{RE::VTABLE_Character[0]};
    Load3D::func = characterVtable.write_vfunc(0x6A, Load3D::thunk);
    logger::info("Hooked Character::Load3D");
}

void Event::GenerateWaitingBodies() {
    if (g_waitingForDistribution.empty() || !CanDistribute(Body::OBody::GetInstance())) {
        return;
    }

    SKSE::GetTaskInterface()->AddTask([] {
        // Another save may have started loading since
        const auto& obody{Body::OBody::GetInstance()};
        if (!CanDistribute(obody)) {
            return;
        }

        std::vector<RE::FormID> formIDs;
        g_waitingForDistribution.erase_if([&](const RE::FormID a_formID) {
            formIDs.push_back(a_formID);
            return true;
        });

        std::size_t alreadyProcessed{0};
        for (const auto formID : formIDs) {
            // The ones whose 3D isn't loaded anymore get their body when it's loaded again
            // The form ID may be of another actor by now if another save was loaded
            auto* const actor{RE::TESForm::LookupByID<RE::Actor>(formID)};
            if (!actor || !actor->Is3DLoaded() || !IsDistributionTarget(actor)) continue;

            if (obody.IsProcessed(actor)) {
                ++alreadyProcessed;
                continue;
            }

            obody.GenerateActorBody(actor, nullptr);
        }

        logger::info("{} NPCs loaded before OBody could distribute to them, {} of them already had their body",
                     formIDs.size(), alreadyProcessed);
    });
}

void Event::OBodyEventHandler::Register() {
    if (auto* const events{RE::ScriptEventSourceHolder::GetSingleton()}) {
        events->AddEventSink<RE::TESInitScriptEvent>(&singleton);
        events->AddEventSink<RE::TESLoadGameEvent>(&singleton);
        events->AddEventSink<RE::TESEquipEvent>(&singleton);
        events->AddEventSink<RE::TESObjectLoadedEvent>(&singleton);
    }

    if (auto* const skseCrosshairEvents{SKSE::GetCrosshairRefEventSource()}) {
        skseCrosshairEvents->AddEventSink<SKSE::CrosshairRefEvent>(&singleton);
    }
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESInitScriptEvent* a_event,
                                                                RE::BSTEventSource<RE::TESInitScriptEvent>*) {
    if (!a_event || !a_event->objectInitialized->Is3DLoaded() ||
        Preview::Session::GetInstance().IsCopy(a_event->objectInitialized.get())) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Most NPCs already got their body before their 3D was built (see Load3D), this is for those that didn't
    if (RE::Actor* actor{a_event->objectInitialized->As<RE::Actor>()}; actor && IsDistributionTarget(actor)) {
        if (const auto& obody{Body::OBody::GetInstance()}; CanDistribute(obody)) {
            obody.GenerateActorBody(actor, nullptr);
        } else {
            g_waitingForDistribution.insert(actor->GetFormID());
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event,
                                                                RE::BSTEventSource<RE::TESObjectLoadedEvent>*) {
    if (a_event && a_event->loaded && !Preview::Session::GetInstance().IsCopy(a_event->formID)) {
        Body::OBody::GetInstance().ProcessActorLoaded3D(a_event->formID);
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESLoadGameEvent* a_event,
                                                                RE::BSTEventSource<RE::TESLoadGameEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    const auto& parser{Parser::JSONParser::GetInstance()};
    if (!parser.bodyslidePresetsParsingValid) {
        RE::DebugMessageBox(UI::Translations::Get("obody_preset_list_failure").c_str());
    }

    if (parser.invalid_presets != 0) {
        char message[256];
        sprintf_s(message, std::size(message),
                  "There was(were) %zu invalid preset(s) with parsing error(s), they won't be loaded in but are "
                  "logged in OBody.log. Look for \"load failed: {filename} [{error description}]\" in the log.",
                  parser.invalid_presets);  // max length possible: 187
        RE::DebugMessageBox(message);
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESEquipEvent* a_event,
                                                                RE::BSTEventSource<RE::TESEquipEvent>*) {
    if (!a_event || !a_event->actor || !a_event->actor->As<RE::Actor>() || a_event->baseObject == 0) {
        return RE::BSEventNotifyControl::kContinue;
    }
    const auto actor = a_event->actor->As<RE::Actor>();
    const auto form = RE::TESForm::LookupByID(a_event->baseObject);

    if (!actor || !form) return RE::BSEventNotifyControl::kContinue;

    if (form->Is(RE::FormType::Armor) || form->Is(RE::FormType::Armature)) {
        // The preset menu's preview undresses and redresses the actor and their copy itself, and sets their body for it
        if (const auto& preview{Preview::Session::GetInstance()};
            preview.IsPreviewing(actor) || preview.IsCopy(actor)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (actor->HasKeywordString("ActorTypeNPC") && !actor->IsChild()) {
            Body::OBody::GetInstance().ProcessActorEquipEvent(actor, !a_event->equipped, form);
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
                                                                RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    RE::TESObjectREFR* ref = a_event->crosshairRef.get();

    if (ref) {
        _cachedActor = a_event->crosshairRef->As<RE::Actor>();
    } else {
        _cachedActor = nullptr;
    }
    return RE::BSEventNotifyControl::kContinue;
}

RE::NiPointer<RE::Actor> Event::OBodyEventHandler::GetCurrentCrosshairActor() {
    std::shared_lock<std::shared_mutex> lock(_mutex);

    return RE::NiPointer<RE::Actor>(_cachedActor);
}
