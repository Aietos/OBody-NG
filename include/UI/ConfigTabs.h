#pragma once

namespace UI::ConfigTabs {
    // The Settings tab: the config's bool and string keys, saved to the master config
    void RenderSettings();

    // The Include Files tab: the include files in the order they're loaded
    void RenderIncludeFiles();

    // The JSON Viewer tab
    void RenderViewer();

    // When the menu is opened: the result of the last reload isn't shown anymore
    void Reset();

    struct ReloadResult {
        std::string message;
        bool failed{false};
        bool filesSkipped{false};
    };

    // Reloads the config, after a change written to its files. a_presets: the BodySlide presets are read again first
    ReloadResult Reload(bool a_presets = false);
}  // namespace UI::ConfigTabs
