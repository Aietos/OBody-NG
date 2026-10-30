#pragma once

namespace UI::Util {
    std::string_view TrimWhitespace(std::string_view text);

    std::wstring Utf8ToWide(std::string_view text);

    // Unicode-aware lowercase, invariant locale is needed so non-Latin systems don't break on latin letters
    std::wstring ToLowerWide(const std::wstring& text);

    // Locale-aware, case-insensitive comparison so that accented letters sort next to their base letter
    int CompareLinguistic(const std::wstring& a, const std::wstring& b);

    bool IsPresetTarget(RE::Actor* a_actor);
}  // namespace UI::Util
