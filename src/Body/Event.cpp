#include "Body/Event.h"

#include "Body/Body.h"
#include "JSONParser/JSONParser.h"

Event::OBodyEventHandler Event::OBodyEventHandler::singleton;

void Event::OBodyEventHandler::Register() {
    if (auto* const events{RE::ScriptEventSourceHolder::GetSingleton()}) {
        events->AddEventSink<RE::TESInitScriptEvent>(&singleton);
        events->AddEventSink<RE::TESLoadGameEvent>(&singleton);
        events->AddEventSink<RE::TESEquipEvent>(&singleton);
    }
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESInitScriptEvent* a_event,
                                                                RE::BSTEventSource<RE::TESInitScriptEvent>*) {
    if (!a_event || !a_event->objectInitialized->Is3DLoaded()) return RE::BSEventNotifyControl::kContinue;

    if (RE::Actor* actor{a_event->objectInitialized->As<RE::Actor>()};
        (actor != nullptr) && actor->HasKeywordString("ActorTypeNPC") && !actor->IsChild()) {
        Body::OBody::GetInstance().GenerateActorBody(actor);
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESLoadGameEvent* a_event,
                                                                RE::BSTEventSource<RE::TESLoadGameEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;

    const auto& parser{Parser::JSONParser::GetInstance()};

    if (!parser.presetDistributionConfigValid) {
        RE::DebugMessageBox(
            "The OBody NG JSON configuration file contains errors! OBody NG will not work properly. Please "
            "exit the game now and refer to the OBody NG JSON Configuration Guide to validate your "
            "configuration file.");
    }

    if (!parser.bodyslidePresetsParsingValid) {
        RE::DebugMessageBox(
            "A critical error has occurred while parsing the Bodyslide presets files. This most likely means "
            "you have a corrupt bodyslide preset, or a bodyslide preset where the name contains "
            "special/incompatible characters. As a result, the presets list in the OBody menu will be empty. "
            "Please exit the game now and refer to the OBody NG mod page for more information.");
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Event::OBodyEventHandler::ProcessEvent(const RE::TESEquipEvent* a_event,
                                                                RE::BSTEventSource<RE::TESEquipEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
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