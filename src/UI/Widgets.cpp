#include "UI/Widgets.h"
#include "SKSEMenuFramework.h"
#include "UI/Translations.h"

bool UI::Widgets::SearchField(const char* a_label, const char* a_id, char* a_buffer, const std::size_t a_size,
                              const bool a_focus) {
    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();

    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameRounding, 3.0f);
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBg, ImGuiMCP::ImVec4{0.19f, 0.18f, 0.17f, 1.0f});
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgHovered, ImGuiMCP::ImVec4{0.24f, 0.23f, 0.21f, 1.0f});
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgActive, ImGuiMCP::ImVec4{0.27f, 0.25f, 0.22f, 1.0f});
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Border, ImGuiMCP::ImVec4{0.55f, 0.46f, 0.30f, 1.0f});

    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_TextDisabled, ImGuiMCP::ImVec4{0.66f, 0.64f, 0.60f, 1.0f});

    ImGuiMCP::AlignTextToFramePadding();
    ImGuiMCP::TextUnformatted(a_label);
    ImGuiMCP::SameLine();

    if (a_focus) {
        ImGuiMCP::SetKeyboardFocusHere();
    }

    ImGuiMCP::SetNextItemWidth(-FLT_MIN);

    const bool changed{
        ImGuiMCP::InputTextWithHint(a_id, UI::Translations::Get("obody_search_hint").c_str(), a_buffer, a_size)};

    ImGuiMCP::PopStyleColor(5);
    ImGuiMCP::PopStyleVar(2);

    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();
    return changed;
}

namespace {
    constexpr ImGuiMCP::ImVec4 kKeyFill{0.19f, 0.18f, 0.17f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kKeyBorder{0.55f, 0.46f, 0.30f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kKeyText{0.96f, 0.84f, 0.58f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kActionText{0.85f, 0.85f, 0.88f, 1.0f};

    struct HintMetrics {
        float keyPadding;
        float keyHeight;
        float gap;
        float between;
        float rowGap;
    };

    HintMetrics Metrics() {
        const float line{ImGuiMCP::GetTextLineHeight()};

        return {line * 0.3f, line * 1.25f, line * 0.3f, line * 1.25f, ImGuiMCP::GetStyle()->ItemSpacing.y};
    }

    float TextWidth(const std::string& a_text) { return ImGuiMCP::CalcTextSize(a_text.c_str()).x; }

    float HintWidth(const UI::Widgets::ControlHint& a_hint, const HintMetrics& a_metrics) {
        float width{0.0f};

        for (std::size_t i{0}; i < a_hint.keys.size(); ++i) {
            if (i > 0) width += TextWidth(a_hint.joiner) + a_metrics.gap * 2.0f;
            width += TextWidth(a_hint.keys[i]) + a_metrics.keyPadding * 2.0f;
        }

        return width + a_metrics.gap * 2.0f + TextWidth(a_hint.action);
    }

    std::vector<std::pair<std::vector<const UI::Widgets::ControlHint*>, float>> Rows(
        const std::vector<UI::Widgets::ControlHint>& a_hints, const float a_width, const HintMetrics& a_metrics) {
        std::vector<std::pair<std::vector<const UI::Widgets::ControlHint*>, float>> rows;

        for (const auto& hint : a_hints) {
            const float width{HintWidth(hint, a_metrics)};

            if (rows.empty() ||
                (!rows.back().first.empty() && rows.back().second + a_metrics.between + width > a_width)) {
                rows.push_back({{&hint}, width});
            } else {
                rows.back().first.push_back(&hint);
                rows.back().second += a_metrics.between + width;
            }
        }

        return rows;
    }
}  // namespace

float UI::Widgets::ControlHintsHeight(const std::vector<ControlHint>& a_hints) {
    if (a_hints.empty()) return 0.0f;

    const auto metrics{Metrics()};
    const auto rows{Rows(a_hints, ImGuiMCP::GetContentRegionAvail().x, metrics)};
    const auto count{static_cast<float>(rows.size())};

    return count * metrics.keyHeight + (count - 1.0f) * metrics.rowGap;
}

void UI::Widgets::ControlHints(const std::vector<ControlHint>& a_hints) {
    if (a_hints.empty()) return;

    const auto metrics{Metrics()};
    const float available{ImGuiMCP::GetContentRegionAvail().x};
    const auto rows{Rows(a_hints, available, metrics)};
    const float line{ImGuiMCP::GetTextLineHeight()};
    const float textOffset{(metrics.keyHeight - line) * 0.5f};
    const float rounding{line * 0.2f};

    auto* const drawList{ImGuiMCP::GetWindowDrawList()};
    const auto keyFill{ImGuiMCP::GetColorU32(kKeyFill)};
    const auto keyBorder{ImGuiMCP::GetColorU32(kKeyBorder)};
    const auto keyText{ImGuiMCP::GetColorU32(kKeyText)};
    const auto actionText{ImGuiMCP::GetColorU32(kActionText)};

    const auto origin{ImGuiMCP::GetCursorScreenPos()};
    float y{origin.y};

    for (const auto& [hints, rowWidth] : rows) {
        float x{origin.x + (std::max)(0.0f, (available - rowWidth) * 0.5f)};
        bool first{true};

        for (const auto* const hint : hints) {
            if (!first) x += metrics.between;
            first = false;

            for (std::size_t i{0}; i < hint->keys.size(); ++i) {
                if (i > 0) {
                    x += metrics.gap;
                    ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::ImVec2{x, y + textOffset}, actionText,
                                                         hint->joiner);
                    x += TextWidth(hint->joiner) + metrics.gap;
                }
                const auto& key{hint->keys[i]};
                const float width{TextWidth(key) + metrics.keyPadding * 2.0f};
                const ImGuiMCP::ImVec2 min{x, y};
                const ImGuiMCP::ImVec2 max{x + width, y + metrics.keyHeight};
                ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, min, max, keyFill, rounding, 0);
                ImGuiMCP::ImDrawListManager::AddRect(drawList, min, max, keyBorder, rounding, 0, 1.0f);
                ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::ImVec2{x + metrics.keyPadding, y + textOffset},
                                                     keyText, key.c_str());
                x += width;
            }

            x += metrics.gap * 2.0f;
            ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::ImVec2{x, y + textOffset}, actionText,
                                                 hint->action.c_str());
            x += TextWidth(hint->action);
        }
        y += metrics.keyHeight + metrics.rowGap;
    }

    // The space they take, as an item would
    ImGuiMCP::Dummy(ImGuiMCP::ImVec2{available, y - metrics.rowGap - origin.y});
}
