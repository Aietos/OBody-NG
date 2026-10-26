#include "Preview/Physics.h"

void Preview::Physics::Reset() {
    if (!REX::W32::GetModuleHandleA("hdtSMP64")) return;

    auto* const factory{RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()};
    auto* const script{factory ? factory->Create() : nullptr};
    if (!script) return;

    script->SetCommand("smp reset");
    script->CompileAndRun(nullptr);
    delete script;

    logger::info("Preview: asked HDT-SMP to reset its physics");
}
