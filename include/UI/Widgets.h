#pragma once

#include "SKSEMenuFramework.h"

namespace UI::Widgets {
    inline constexpr ImGuiMCP::ImVec4 TableHeaderBackground{0.26f, 0.21f, 0.14f, 1.0f};
    inline constexpr ImGuiMCP::ImVec4 TableHeaderText{0.96f, 0.84f, 0.58f, 1.0f};

    bool SearchField(const char* a_label, const char* a_id, char* a_buffer, std::size_t a_size, bool a_focus = false);

    struct ControlHint {
        std::vector<std::string> keys;
        const char* joiner{""};
        std::string action;
    };

    void ControlHints(const std::vector<ControlHint>& a_hints);
    float ControlHintsHeight(const std::vector<ControlHint>& a_hints);
}  // namespace UI::Widgets
