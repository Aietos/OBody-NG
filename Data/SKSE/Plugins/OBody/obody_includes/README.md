# OBody Include Files

Place `.json` files in `Data/SKSE/Plugins/OBody/obody_includes/` to assign BodySlide presets to individual NPCs without editing the main `OBody_presetDistributionConfig.json`.

## Rules

- Files must be valid JSON
- All keys are supported EXCEPT `blacklistedPresetsShowInOBodyMenu` and `disablePresetDistribution`. These are a boolean and string key, respectively, so it makes no sense to have them in includes/overrides as there is nothing to merge
- If you included non-supported or non-existent keys, they are logged as warnings and ignored
- Files are loaded in **alphabetical order** by filename, ignoring upper/lower case
- Non-overlapping assignments are additive (merged together)
- Only `.json` files are processed; other files are ignored
- If this directory does not exist, OBody loads normally with just the main config
- Parse errors will prevent the game from loading

## Examples of some of the supported keys

### `npcFormID` — Assign presets by FormID

```json
{
  "npcFormID": {
    "Skyrim.esm": {
      "0001A696": ["Aela the Huntress"]
    }
  }
}
```

### `npc` — Assign presets by NPC name

```json
{
  "npc": {
    "Lydia": ["PresetName1", "PresetName2"]
  }
}
```

### `npcPluginFemale` — Assign presets to all female NPCs from a plugin

```json
{
  "npcPluginFemale": {
    "MyMod.esp": ["PresetA", "PresetB"]
  }
}
```

### `npcPluginMale` — Assign presets to all male NPCs from a plugin

```json
{
  "npcPluginMale": {
    "MyMod.esp": ["PresetC", "PresetD"]
  }
}
```

### `blacklistedOutfitsFromORefit` - Prevent outfits from triggering ORefit
```json
{
  "blacklistedOutfitsFromORefit": ["Demon Hunter's Cuirass Light", "Demon Hunter's Cuirass Heavy"]
}
```

### `refitOutfitPresetsFemale` - Use specific -Refit presets for an outfit
```json
{
  "refitOutfitPresetsFemale": {
    "Blades Armor": ["Warrior Woman-Refit Compress Chest", "Female-Refit Compress Chest"]
  }
}
```

## Changing the config in game

The OBody menu can change the config without editing any file. These changes are saved to an include file of their own, `ZZZOBody_Ingame.json` in this folder, which is created the first time you save a change. They apply right away, with no reload needed.

- **Config Editor tab**: shows the rules of each category (presets for NPCs, factions, classes, plugins and races, blacklists, outfits and ORefit) from all the files, with the file each rule comes from, and lets you add rules. A rule from another file can be switched off or changed: the in-game file then removes it or replaces it (with a `"remove"` section, see below), and the other file stays as it is. Each category shows the JSON key it writes to
- **Preset Selection tab**: **Save preset to config** writes the NPC's current preset to the in-game file (`npcFormID`), so the config always gives it to them. **Remove from config** takes it out again. It isn't available for the player, or for NPCs spawned from leveled lists, which the config can't name
- **Settings tab**: `blacklistedPresetsShowInOBodyMenu` and `disablePresetDistribution` are saved to the main `OBody_presetDistributionConfig.json`, since include files can't have them. **Re-roll all NPCs** gives every NPC a new preset by the config's rules, except the presets that were picked manually
- **Include Files tab**: the include files in load order, the folder each one is in, and the files that load after the in-game file

Important notes:

- The in-game file is an include file like the others. Its `ZZZ` name makes it load after most files, but a file that loads after it still wins over the changes made in game. The Include Files tab lists those files
- You can edit the file by hand too. If it changed on disk since the menu last read it, the menu doesn't overwrite it: it reads the file again, and you can make the change again
- With Mod Organizer 2, the game creates the file in Overwrite

## Reloading

Changes to the config files apply without restarting the game. Open the OBody menu, go to the Include Files or JSON Config Viewer tab and press **Reload config**. If a file has an error, the reload fails and the config that was in use stays in use, the error is shown in the menu and written to `OBody.log`.

- NPCs that already got a body keep it, the new config only applies to the NPCs that get their body after the reload (the same as after restarting the game). To give NPCs a preset by the new config, use **Re-roll preset** (Preset Selection tab) for one NPC, or **Re-roll all NPCs** (Settings tab)
- With Mod Organizer 2, include files added while the game runs are found too, in Overwrite or in a mod's `obody_includes` folder that already had a file in it. `OBody.log` shows which folder each include file really comes from

## Merge Behavior

- **Additive**: Assignments in include files are added to the main config. Lists (blacklists, `outfitsForceRefit`, keywords, and the FormID lists under each plugin) get the entries they don't have yet
- **Last-loaded-wins**: If two include files target the same NPC or plugin, the last file alphabetically wins
- **FormIDs are compared by value**: `"0001A696"` and `"1A696"` are the same NPC, so a later file replaces the entry instead of adding a second one
- **Removing**: An include file can remove what the main config or the files loaded before it set, with a `"remove"` section (see below)

## Removing entries

Add a `"remove"` object to an include file, with the keys to remove entries from:

```json
{
  "remove": {
    "blacklistedRacesFemale": ["ElderRace"],
    "npc": ["Lydia"],
    "raceFemale": ["NordRace"],
    "npcFormID": {
      "Skyrim.esm": ["0001A696"]
    },
    "blacklistedNpcsFormID": {
      "Skyrim.esm": ["00013BA3"]
    }
  }
}
```

- **List keys** (`blacklistedRacesFemale`, `outfitsForceRefit`, `refitOutfitKeywordsFemale`, ...) take the values to remove
- **Name keys** (`npc`, `raceFemale`, `factionMale`, `npcPluginFemale`, `refitOutfitPresetsFemale`, ...) take the names of the entries to remove
- **FormID keys** (`npcFormID`, `blacklistedNpcsFormID`, `refitOutfitPresetsFemaleFormID`, ...) take the FormIDs to remove, under their plugin
- The removals only affect the main config and the files loaded **before** this one. A file loaded after it can add the entry again
- A file's removals are done before its own entries are added, so a file can remove an entry and add it again. For ORefit, that moves it to the end of the order
- `blacklistedPresetsShowInOBodyMenu` and `disablePresetDistribution` can't be removed, they are only set in the main config
- What was removed, and what had nothing to remove, is written to `OBody.log`

## ORefit priority in include files

For `refitOutfitPresetsFemale`, `refitOutfitPresetsMale`, their `FormID` versions, `refitOutfitKeywordsFemale` and `refitOutfitKeywordsMale`, the order of the entries decides which -Refit preset wins. Include files change that order like this:

- **New outfits go last**: An outfit that isn't in the main config yet is added after all the outfits already there, so it has lower priority than the main config and the include files loaded before it
- **Existing outfits keep their place**: If an include file has an outfit that is already defined, its list of presets replaces the old one completely (it is not combined with it), but the outfit keeps its old position in the order
- **FormID outfits are grouped by plugin**: A new FormID under a plugin that is already defined goes after the other FormIDs of that plugin. A new plugin goes after all the plugins already there
- **New keywords go last**: Keywords from include files are added to the end of the keyword array, so they have lower priority than the ones already there. A keyword that is already in the array keeps its position, so to move it, remove it in the `"remove"` section and add it again
- **Moving an outfit**: The same works for outfits: remove it and add it again to put it last

## ORefit and UBE bodies

A -Refit preset must be made for the body it refits, UBE or not. Otherwise it's skipped, and `OBody.log` says why.

- **`<Preset>-Refit...` presets** are compared with their base preset, so they must be made for the same body as that preset. A UBE NPC with a non-UBE preset (picked manually, with "Show presets for other bodies too") uses the non-UBE `<Preset>-Refit` presets
- **Additive refits** are added to the NPC's own body, so UBE NPCs use `UBE Female-Refit...` / `UBE Male-Refit...` presets instead of `Female-Refit...` / `Male-Refit...`. Without one, ORefit leaves a UBE body as it is, because its default values are made for CBBE and UNP sliders
- A preset made for another body only changes the sliders whose names that body also has. Its -Refit presets have the same limit

## Use Cases

- Mod authors can ship an include file alongside their mod to assign presets to their custom NPCs or blacklist them entirely
- Mod authors can ship an include file alongside their outfits mod to blacklist them from ORefit or force ORefit on them
- End-users can create personal NPC/race/class preset assignments, blacklists and ORefit configuration without touching the main config
- Multiple mods can coexist without config file conflicts (as long as they don't target the same NPCs)
