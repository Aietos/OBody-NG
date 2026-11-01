#pragma once

// The lists the Config Editor's pickers show: what the game has loaded, as the config names it
namespace UI::Lookups {
    enum class Kind {
        kPlugin,       // the plugins in the load order
        kNpcRecord,    // NPC records, by plugin and FormID
        kNpcName,      // NPC names
        kFaction,      // factions, by editor ID
        kRace,         // races, by editor ID
        kClass,        // classes, by editor ID
        kArmorRecord,  // armors, by plugin and FormID
        kArmorName,    // armor names
        kKeyword,      // the keywords armors have, by editor ID
        kPreset,       // the presets that aren't "-Refit" ones, of both sexes
        kPresetFemale,
        kPresetMale,
        kRefitFemale,  // the "-Refit" presets
        kRefitMale,
        kTotal
    };

    struct Item {
        // What the list shows
        std::string label;
        // What the config holds: a name, an editor ID, a plugin, a preset, or for records their FormID
        std::string value;
        // Records: their plugin
        std::string plugin;
        // The label in lowercase, for the search
        std::wstring search;
        // NPC records: their sex
        bool female{false};
    };

    // All of them are built the first time one is needed, the log tells how long it took
    const std::vector<Item>& Get(Kind a_kind);

    // The items of a_kind that a_actor has: their record, name, factions, race, class, plugin, worn armors and the
    // armors' keywords, or their preset
    std::vector<const Item*> FromActor(Kind a_kind, RE::Actor* a_actor);
}  // namespace UI::Lookups
