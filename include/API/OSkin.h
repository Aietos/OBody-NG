#pragma once

#include "API/PluginInterface.h"

// OBody's link to OSkin's API, to switch to its menu and to leave its preview copy alone
namespace API::OSkin {
    // Asks OSkin for its interface, at kPostPostLoad
    void Request();

    [[nodiscard]] bool IsAvailable();

    // Opens OSkin's menu on the actor
    bool OpenMenu(RE::Actor* a_actor, const OBody::API::MenuState& a_state);

    // The NPC whose copy OSkin's preview spawned, when a_actor is that copy
    [[nodiscard]] RE::Actor* GetPreviewCopyTarget(RE::Actor* a_actor);
}  // namespace API::OSkin
