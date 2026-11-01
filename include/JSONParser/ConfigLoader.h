#pragma once

namespace Parser {
    // The include file the changes made in game are written to. Its name makes it load after the other include files
    constexpr auto InGameConfigFileName{"ZZZOBody_Ingame.json"};

    // An include file, as the config was loaded
    struct IncludeFileInfo {
        std::string name;
        // Where it really is: under Mod Organizer 2 it's in a mod's folder or in Overwrite. Empty when unknown
        std::string from;
        // It couldn't be opened, the config was loaded without it
        bool skipped{false};
    };

    // Reads OBody_presetDistributionConfig.json, validates it with its schema, then merges into it the include files
    // of obody_includes, in alphabetical order. It has to run once the game data is loaded.
    // Returns the error message when a file can't be used. An include file that can't be opened is skipped instead.
    // a_includeFiles gets the include files in the order they were loaded. Under Mod Organizer 2, the include files
    // added while the game runs are found in the real folders of obody_includes (see FindIncludeFiles)
    std::optional<std::string> LoadConfig(rapidjson::Document& a_config, std::vector<IncludeFileInfo>& a_includeFiles);

    // Writes a bool or string key to OBody_presetDistributionConfig.json. The file is
    // read again from the disk, so the rest of it stays as it is, and checked with its schema before it's written.
    // Returns the error message when it couldn't be written. The config has to be reloaded to use the new value
    std::optional<std::string> SetMasterConfigValue(const char* a_key, bool a_value);
    std::optional<std::string> SetMasterConfigValue(const char* a_key, std::string_view a_value);

    // Gives an NPC a preset in the in-game include file: npcFormID, plugin -> FormID -> [preset]. The file is read
    // again from the disk, so what was written to it by hand stays, and checked with the schema before it's written.
    // A removal of that NPC in its "remove" section is taken out. Returns the error message when it couldn't be
    // written. The config has to be reloaded to use it
    std::optional<std::string> SaveNpcPreset(std::string_view a_plugin, std::string_view a_formID,
                                             std::string_view a_preset);

    // Takes an NPC's npcFormID entry out of the config
    std::optional<std::string> RemoveNpcPreset(std::string_view a_plugin, std::string_view a_formID);

    // An entry of the config
    struct EntryRef {
        std::string key;
        std::string plugin;
        std::string entry;
    };

    // The file each entry comes from, by EntrySourceId
    using EntrySources = std::unordered_map<std::string, std::string>;

    // Identifies an entry in EntrySources. FormIDs are compared by their value inside the plugin, like the merge does
    [[nodiscard]] std::string EntrySourceId(const EntryRef& a_ref);

    // Merges the config files like LoadConfig, without processing them, and tells which file each entry comes from
    std::optional<std::string> MergeConfigFiles(rapidjson::Document& a_config, EntrySources& a_sources,
                                                const rapidjson::Document* a_inGame);

    // Reads the in-game include file and checks it
    std::optional<std::string> ReadInGameConfig(rapidjson::Document& a_config, std::string& a_text);

    // Checks the in-game file's content with the schema, which "remove" was added to
    std::optional<std::string> ValidateInGameConfig(rapidjson::Document& a_config);

    // Writes the in-game file
    std::optional<std::string> WriteInGameConfig(const rapidjson::Document& a_config, std::string_view a_readText);

    // Changes to the in-game file's content. The content has to be checked (ValidateInGameConfig) before they're
    // made, so each key has the shape they expect
    namespace InGameEdit {
        // Adds the entry, or replaces its presets. a_presets is null for the keys that hold a list
        void SetEntry(rapidjson::Document& a_config, const EntryRef& a_ref, const std::vector<std::string>* a_presets);
        // Returns whether the file had it
        bool DeleteEntry(rapidjson::Document& a_config, const EntryRef& a_ref);

        // The "remove" section, which takes away what the files loaded before set
        void AddRemoval(rapidjson::Document& a_config, const EntryRef& a_ref);
        bool DeleteRemoval(rapidjson::Document& a_config, const EntryRef& a_ref);
    }  // namespace InGameEdit
}  // namespace Parser
