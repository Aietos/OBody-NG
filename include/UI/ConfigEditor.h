#pragma once

namespace UI::ConfigEditor {
    // The Config Editor tab: the config's rules in plain words, changed in the in-game include file. a_actor is the
    // NPC picked in the menu, the pickers offer what they have first (their factions, race, worn outfits)
    void Render(RE::Actor* a_actor);

    // When the menu is opened. Changes that weren't saved yet are kept
    void Reset();
}  // namespace UI::ConfigEditor
