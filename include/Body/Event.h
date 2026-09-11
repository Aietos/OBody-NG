#pragma once

#include <mutex>
#include <shared_mutex> 

namespace Event {
    class OBodyEventHandler final : public RE::BSTEventSink<RE::TESInitScriptEvent>,
                                    public RE::BSTEventSink<RE::TESLoadGameEvent>,
                                    public RE::BSTEventSink<RE::TESEquipEvent>,
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

        RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event,
                                        RE::BSTEventSource<SKSE::CrosshairRefEvent>*)  override;

        mutable std::shared_mutex _mutex;
        RE::Actor* _cachedActor{ nullptr };

        OBodyEventHandler() = default;
    };
}  // namespace Event
