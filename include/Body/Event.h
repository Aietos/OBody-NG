#pragma once

#include <mutex>
#include <shared_mutex>

namespace Event {
    // Hooks Character::Load3D, to generate the bodies of NPCs before their 3D is built
    void InstallHooks();

    // Generates the bodies of the NPCs that loaded before OBody could distribute to them
    // During the loading screen or before Papyrus set the distribution key of a save whose cosave doesn't have it
    // Even if the cosave has it, this also makes sure we generate all needed bodies
    // in case NPCs load before OBody is done reading the cosave
    void GenerateWaitingBodies();

    class OBodyEventHandler final : public RE::BSTEventSink<RE::TESInitScriptEvent>,
                                    public RE::BSTEventSink<RE::TESLoadGameEvent>,
                                    public RE::BSTEventSink<RE::TESEquipEvent>,
                                    public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
                                    public RE::BSTEventSink<SKSE::CrosshairRefEvent> {
    public:
        static OBodyEventHandler* GetSingleton() { return &singleton; }
        static void Register();

        OBodyEventHandler(OBodyEventHandler&&) = delete;
        OBodyEventHandler(const OBodyEventHandler&) = delete;

        OBodyEventHandler& operator=(OBodyEventHandler&&) = delete;
        OBodyEventHandler& operator=(const OBodyEventHandler&) = delete;

        RE::NiPointer<RE::Actor> GetCurrentCrosshairActor();

    private:
        static OBodyEventHandler singleton;

        RE::BSEventNotifyControl ProcessEvent(const RE::TESInitScriptEvent* a_event,
                                              RE::BSTEventSource<RE::TESInitScriptEvent>*) override;

        RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* a_event,
                                              RE::BSTEventSource<RE::TESLoadGameEvent>*) override;

        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event,
                                              RE::BSTEventSource<RE::TESEquipEvent>*) override;

        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event,
                                              RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;

        RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
                                              RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override;

        mutable std::shared_mutex _mutex;
        RE::Actor* _cachedActor{nullptr};

        OBodyEventHandler() = default;
    };
}  // namespace Event
