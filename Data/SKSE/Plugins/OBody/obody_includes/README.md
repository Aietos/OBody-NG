# OBody Include Files

Place `.json` files in `Data/SKSE/Plugins/OBody/obody_includes/` to assign BodySlide presets to individual NPCs without editing the main `OBody_presetDistributionConfig.json`.

## Rules

- Files must be valid JSON
- All keys are supported EXCEPT `blacklistedPresetsShowInOBodyMenu` and `disablePresetDistribution`. These are a boolean and string key, respectively, so it makes no sense to have them in includes/overrides as there is nothing to merge
- If you included non-supported or non-existent keys, they are logged as warnings and ignored
- Files are loaded in **alphabetical order** by filename — later files override earlier ones for the same NPC/plugin
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

## Merge Behavior

- **Additive**: Assignments in include files are added to the main config
- **Last-loaded-wins**: If two include files target the same NPC or plugin, the last file alphabetically wins
- **Non-destructive**: Include files cannot remove any other main config settings

## ORefit priority in include files

For `refitOutfitPresetsFemale`, `refitOutfitPresetsMale`, their `FormID` versions, `refitOutfitKeywordsFemale` and `refitOutfitKeywordsMale`, the order of the entries decides which -Refit preset wins. Include files change that order like this:

- **New outfits go last**: An outfit that isn't in the main config yet is added after all the outfits already there, so it has lower priority than the main config and the include files loaded before it
- **Existing outfits keep their place**: If an include file has an outfit that is already defined, its list of presets replaces the old one completely (it is not combined with it), but the outfit keeps its old position in the order
- **FormID outfits are grouped by plugin**: A new FormID under a plugin that is already defined goes after the other FormIDs of that plugin. A new plugin goes after all the plugins already there
- **Write FormIDs the same way**: `"0004B28B"` and `"4B28B"` are the same outfit but count as different entries, so the include file adds a second entry instead of replacing the first one, and only the first one is used
- **New keywords go last**: Keywords from include files are added to the end of the keyword array, so they have lower priority than the ones already there. A keyword that is already in the array keeps its position, so an include file cannot move it up

## Use Cases

- Mod authors can ship an include file alongside their mod to assign presets to their custom NPCs or blacklist them entirely
- Mod authors can ship an include file alongside their outfits mod to blacklist them from ORefit or force ORefit on them
- End-users can create personal NPC/race/class preset assignments, blacklists and ORefit configuration without touching the main config
- Multiple mods can coexist without config file conflicts (as long as they don't target the same NPCs)
