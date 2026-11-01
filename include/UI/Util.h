#pragma once

namespace UI::Util {
    std::string_view TrimWhitespace(std::string_view text);

    std::wstring Utf8ToWide(std::string_view text);

    // Unicode-aware lowercase, invariant locale is needed so non-Latin systems don't break on latin letters
    std::wstring ToLowerWide(const std::wstring& text);

    // Locale-aware, case-insensitive comparison so that accented letters sort next to their base letter
    int CompareLinguistic(const std::wstring& a, const std::wstring& b);

    bool IsPresetTarget(RE::Actor* a_actor);

    // How the preset lists show and order a preset, the preset menu and the Config Editor alike
    struct PresetOrder {
        std::string name;         // exact preset name, the only thing used for lookups
        std::string displayName;  // name without leading/trailing whitespace, shown in the list
        std::wstring searchText;  // displayName as lowercase UTF-16, used for searching
        std::wstring sortKey;     // displayName as UTF-16 without leading non-letters/digits, used for ordering
        bool hasAlnum{false};     // false for names with no letters/digits at all, so they go last
    };

    PresetOrder MakePresetOrder(const std::string& a_name);

    // Alphabetical by the first letter or digit, so "(HIMBO) Foo" and "--((HIMBO) Foo" both sort under H. The names
    // without letters or digits go last
    bool IsBeforeInPresetOrder(const PresetOrder& a, const PresetOrder& b);
}  // namespace UI::Util
