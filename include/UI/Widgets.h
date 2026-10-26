#pragma once

namespace UI::Widgets {
    bool SearchField(const char* a_label, const char* a_id, char* a_buffer, std::size_t a_size, bool a_focus = false);

    struct ControlHint {
        std::vector<std::string> keys;
        const char* joiner{""};
        std::string action;
    };

    void ControlHints(const std::vector<ControlHint>& a_hints);
    float ControlHintsHeight(const std::vector<ControlHint>& a_hints);
}  // namespace UI::Widgets
