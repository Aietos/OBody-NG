#include "UI/Widgets.h"
#include "SKSEMenuFramework.h"
#include "UI/Translations.h"

bool UI::Widgets::SearchField(const char* a_label, const char* a_id, char* a_buffer, const std::size_t a_size,
                              const bool a_focus) {
    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();

    ImGuiMCP::AlignTextToFramePadding();
    ImGuiMCP::TextUnformatted(a_label);
    ImGuiMCP::SameLine();

    const bool changed{SearchInput(a_id, a_buffer, a_size, a_focus)};

    ImGuiMCP::Spacing();
    ImGuiMCP::Spacing();
    return changed;
}

bool UI::Widgets::SearchInput(const char* a_id, char* a_buffer, const std::size_t a_size, const bool a_focus) {
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameRounding, 3.0f);
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBg, ImGuiMCP::ImVec4{0.19f, 0.18f, 0.17f, 1.0f});
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgHovered, ImGuiMCP::ImVec4{0.24f, 0.23f, 0.21f, 1.0f});
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgActive, ImGuiMCP::ImVec4{0.27f, 0.25f, 0.22f, 1.0f});
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Border, ImGuiMCP::ImVec4{0.55f, 0.46f, 0.30f, 1.0f});

    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_TextDisabled, ImGuiMCP::ImVec4{0.66f, 0.64f, 0.60f, 1.0f});

    if (a_focus) {
        ImGuiMCP::SetKeyboardFocusHere();
    }

    ImGuiMCP::SetNextItemWidth(-FLT_MIN);

    const bool changed{
        ImGuiMCP::InputTextWithHint(a_id, UI::Translations::Get("obody_search_hint").c_str(), a_buffer, a_size)};

    ImGuiMCP::PopStyleColor(5);
    ImGuiMCP::PopStyleVar(2);

    return changed;
}

namespace {
    float CheckboxSquare() { return std::round(ImGuiMCP::GetFontSize() * 0.8f); }
}  // namespace

float UI::Widgets::CheckboxWidth(const char* a_label) {
    return CheckboxSquare() + ImGuiMCP::GetStyle()->ItemInnerSpacing.x + ImGuiMCP::CalcTextSize(a_label).x;
}

bool UI::Widgets::Checkbox(const char* a_label, bool* a_value) {
    const auto* style{ImGuiMCP::GetStyle()};
    const float height{ImGuiMCP::GetFrameHeight()};
    const float square{CheckboxSquare()};
    const auto pos{ImGuiMCP::GetCursorScreenPos()};

    const bool clicked{ImGuiMCP::InvisibleButton(a_label, ImGuiMCP::ImVec2{CheckboxWidth(a_label), height})};

    if (clicked) {
        *a_value = !*a_value;
    }

    const auto fill{ImGuiMCP::IsItemActive()    ? ImGuiMCP::ImGuiCol_FrameBgActive
                    : ImGuiMCP::IsItemHovered() ? ImGuiMCP::ImGuiCol_FrameBgHovered
                                                : ImGuiMCP::ImGuiCol_FrameBg};
    const ImGuiMCP::ImVec2 min{pos.x, pos.y + std::round((height - square) * 0.5f)};
    const ImGuiMCP::ImVec2 max{min.x + square, min.y + square};
    auto* const drawList{ImGuiMCP::GetWindowDrawList()};

    ImGuiMCP::ImDrawListManager::AddRectFilled(drawList, min, max, ImGuiMCP::GetColorU32(fill), style->FrameRounding,
                                               0);

    if (style->FrameBorderSize > 0.0f) {
        ImGuiMCP::ImDrawListManager::AddRect(drawList, min, max, ImGuiMCP::GetColorU32(ImGuiMCP::ImGuiCol_Border),
                                             style->FrameRounding, 0, style->FrameBorderSize);
    }

    if (*a_value) {
        const float inset{(std::max)(1.0f, std::floor(square / 6.0f))};
        ImGuiMCP::RenderCheckMark(drawList, ImGuiMCP::ImVec2{min.x + inset, min.y + inset},
                                  ImGuiMCP::GetColorU32(ImGuiMCP::ImGuiCol_CheckMark), square - (inset * 2.0f));
    }

    const float textY{pos.y + ((height - ImGuiMCP::GetTextLineHeight()) * 0.5f)};
    ImGuiMCP::ImDrawListManager::AddText(drawList, ImGuiMCP::ImVec2{max.x + style->ItemInnerSpacing.x, textY},
                                         ImGuiMCP::GetColorU32(ImGuiMCP::ImGuiCol_Text), a_label);

    return clicked;
}

namespace {
    // Set by Esc with a dropdown open, the dropdown that's drawn next closes
    bool g_closeOpenCombo{false};
    // A dropdown was open when the last frame ended
    bool g_comboWasOpen{false};
}  // namespace

bool UI::Widgets::BeginCombo(const char* a_id, const char* a_preview, const ImGuiMCP::ImGuiComboFlags a_flags) {
    if (!ImGuiMCP::BeginCombo(a_id, a_preview, a_flags)) return false;

    if (g_closeOpenCombo) {
        g_closeOpenCombo = false;
        ImGuiMCP::CloseCurrentPopup();
    }

    return true;
}

void UI::Widgets::CloseOpenCombo() { g_closeOpenCombo = true; }

bool UI::Widgets::WasComboOpen() { return g_comboWasOpen; }

void UI::Widgets::EndFrame() {
    g_closeOpenCombo = false;
    g_comboWasOpen = ImGuiMCP::IsPopupOpen("", ImGuiMCP::ImGuiPopupFlags_AnyPopup);
}

namespace {
    float g_textScale{1.0f};
    float g_tabsFullWidth{0.0f};

    // Text lines a screen has to fit for full scale
    constexpr float kFullScaleLines{32.0f};
}  // namespace

void UI::Widgets::UpdateTextScale(const std::span<const std::string> a_tabs) {
    const auto* style{ImGuiMCP::GetStyle()};

    ImGuiMCP::SetWindowFontScale(1.0f);

    const float lines{ImGuiMCP::GetMainViewport()->Size.y / ImGuiMCP::GetFontSize()};
    g_textScale = std::clamp(lines / kFullScaleLines, kMinTextScale, 1.0f);

    float text{0.0f};

    for (const auto& tab : a_tabs) {
        text += ImGuiMCP::CalcTextSize(tab.c_str()).x;
    }

    const auto count{static_cast<float>(a_tabs.size())};
    const float padding{(count * ((style->FramePadding.x * 2.0f) + 1.0f)) +
                        ((count - 1.0f) * style->ItemInnerSpacing.x)};
    g_tabsFullWidth = text + padding;

    ImGuiMCP::SetWindowFontScale(g_textScale);
}

float UI::Widgets::GetTextScale() { return g_textScale; }

void UI::Widgets::SetTextScale(const float a_relative) { ImGuiMCP::SetWindowFontScale(g_textScale * a_relative); }

void UI::Widgets::PushScaledSpacing() {
    const auto* style{ImGuiMCP::GetStyle()};
    const auto scaled{[](const ImGuiMCP::ImVec2 a_value) {
        return ImGuiMCP::ImVec2{a_value.x * g_textScale, a_value.y * g_textScale};
    }};

    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding,
                           ImGuiMCP::ImVec2{style->FramePadding.x * g_textScale,
                                            (std::max)(1.0f, std::round(ImGuiMCP::GetFontSize() * 0.06f))});
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_ItemSpacing, scaled(style->ItemSpacing));
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_ItemInnerSpacing, scaled(style->ItemInnerSpacing));
}

void UI::Widgets::PopScaledSpacing() { ImGuiMCP::PopStyleVar(3); }

float UI::Widgets::GetTabsFullWidth() { return g_tabsFullWidth; }

namespace {
    constexpr ImGuiMCP::ImVec4 kKeyFill{0.19f, 0.18f, 0.17f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kKeyBorder{0.55f, 0.46f, 0.30f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kKeyText{0.96f, 0.84f, 0.58f, 1.0f};
    constexpr ImGuiMCP::ImVec4 kActionText{0.85f, 0.85f, 0.88f, 1.0f};

    constexpr float kHintScale{0.85f};

    struct HintMetrics {
        float keyPadding;
        float keyHeight;
        float gap;
        float between;
        float rowGap;
    };

    HintMetrics Metrics() {
        const float line{ImGuiMCP::GetTextLineHeight()};

        return {line * 0.25f, line * 1.15f, line * 0.25f, line * 0.9f, ImGuiMCP::GetStyle()->ItemSpacing.y};
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

    SetTextScale(kHintScale);

    const auto metrics{Metrics()};
    const auto rows{Rows(a_hints, ImGuiMCP::GetContentRegionAvail().x, metrics)};
    const auto count{static_cast<float>(rows.size())};

    const float height{(count * metrics.keyHeight) + ((count - 1.0f) * metrics.rowGap)};
    SetTextScale();

    return height;
}

void UI::Widgets::ControlHints(const std::vector<ControlHint>& a_hints) {
    if (a_hints.empty()) return;

    SetTextScale(kHintScale);

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
    SetTextScale();
}
