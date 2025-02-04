#pragma once

namespace Event {
    class OBodyEventHandler final : public RE::BSTEventSink<RE::TESInitScriptEvent>,
                                    public RE::BSTEventSink<RE::TESLoadGameEvent>,
                                    public RE::BSTEventSink<RE::TESEquipEvent> {
    public:
        static OBodyEventHandler* GetSingleton() { return &singleton; }
        static void Register();

        OBodyEventHandler(OBodyEventHandler&&) = delete;
        OBodyEventHandler(const OBodyEventHandler&) = delete;

        OBodyEventHandler& operator=(OBodyEventHandler&&) = delete;
        OBodyEventHandler& operator=(const OBodyEventHandler&) = delete;

    private:
        static OBodyEventHandler singleton;

        RE::BSEventNotifyControl ProcessEvent(const RE::TESInitScriptEvent* a_event,
                                              RE::BSTEventSource<RE::TESInitScriptEvent>*) override;

        RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* a_event,
                                              RE::BSTEventSource<RE::TESLoadGameEvent>*) override;

        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event,
                                              RE::BSTEventSource<RE::TESEquipEvent>*) override;

        OBodyEventHandler() = default;
    };
}  // namespace Event
