#pragma once

namespace UI::NpcList {
    // The NPC Selection tab. Nearby NPCs are scanned again when a_appearing
    // Returns the actor picked in current frame, nullptr if no actor was picked
    RE::Actor* Render(const RE::Actor* a_target, bool a_appearing);
}  // namespace UI::NpcList
