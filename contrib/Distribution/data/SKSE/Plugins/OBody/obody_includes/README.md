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

## Merge Behavior

- **Additive**: Assignments in include files are added to the main config
- **Last-loaded-wins**: If two include files target the same NPC or plugin, the last file alphabetically wins
- **Non-destructive**: Include files cannot remove any other main config settings

## Use Cases

- Mod authors can ship an include file alongside their mod to assign presets to their custom NPCs or blacklist them entirely
- Mod authors can ship an include file alongside their outfits mod to blacklist them from ORefit or force ORefit on them
- End-users can create personal NPC/race/class preset assignments, blacklists and ORefit configuration without touching the main config
- Multiple mods can coexist without config file conflicts (as long as they don't target the same NPCs)
