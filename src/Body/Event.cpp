#include "Body/Event.h"

#include "Body/Body.h"
#include "JSONParser/JSONParser.h"
#include "UI/Translations.h"

constinit Event::OBodyEventHandler Event::OBodyEventHandler::singleton;

void Event::OBodyEventHandler::Register() {
    if (auto* const events{RE::ScriptEventSourceHolder::GetSingleton()}) {
        events->AddEventSink<RE::TESInitScriptEvent>(&singleton);
        events->AddEventSink<RE::TESLoadGameEvent>(&singleton);
        events->AddEventSink<RE::TESEquipEvent>(&singleton);
    }

    if (auto* const skseCrosshairEvents{SKSE::GetCrosshairRefEventSource()}) {
        skseCrosshairEvents->AddEventSink<SKSE::CrosshairRefEvent>(&singleton);
    }
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESInitScriptEvent* a_event,
                                                                RE::BSTEventSource<RE::TESInitScriptEvent>*) {
    if (!a_event || !a_event->objectInitialized->Is3DLoaded()) return RE::BSEventNotifyControl::kContinue;

    if (RE::Actor * actor{a_event->objectInitialized->As<RE::Actor>()};
        (actor != nullptr) && actor->HasKeywordString("ActorTypeNPC") && !actor->IsChild()) {
        const bool actorIsFemale = Body::OBody::IsFemale(actor);

        const auto& parser{Parser::JSONParser::GetInstance()};

        if ((actorIsFemale && !parser.distributionDisabledForFemale) || (!actorIsFemale && !parser.distributionDisabledForMale)) {
            Body::OBody::GetInstance().GenerateActorBody(actor, nullptr);   
        }
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
