#include "UI/Util.h"

std::string_view UI::Util::TrimWhitespace(std::string_view text) {
    constexpr std::string_view whitespace{" \t\r\n\f\v"};
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::wstring UI::Util::Utf8ToWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

std::wstring UI::Util::ToLowerWide(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    std::wstring lower(text.size(), L'\0');
    const int length =
        LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, text.data(),
                      static_cast<int>(text.size()), lower.data(), static_cast<int>(lower.size()), nullptr, nullptr, 0);
    if (length <= 0) {
        return text;
    }
    lower.resize(static_cast<std::size_t>(length));
    return lower;
}

int UI::Util::CompareLinguistic(const std::wstring& a, const std::wstring& b) {
    const int result =
        CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS, a.data(),
                        static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), nullptr, nullptr, 0);
    if (result == 0) {
        return a.compare(b);  // fallback in case the API fails, realistically it shouldn't happen
    }
    return result - CSTR_EQUAL;
}

bool UI::Util::IsPresetTarget(RE::Actor* a_actor) {
    return a_actor && a_actor->HasKeywordString("ActorTypeNPC") && !a_actor->IsChild();
}

UI::Util::PresetOrder UI::Util::MakePresetOrder(const std::string& a_name) {
    const auto trimmed = TrimWhitespace(a_name);
    const std::wstring wide = Utf8ToWide(trimmed);

    PresetOrder order;
    order.name = a_name;
    order.displayName = trimmed.empty() ? a_name : std::string{trimmed};
    order.searchText = ToLowerWide(wide);

    // Skip leading characters that aren't letters or digits for ordering,
    // so "(HIMBO) Foo" and "--((HIMBO) Foo" both sort under H.
    std::size_t firstAlnum = wide.size();
    if (!wide.empty()) {
        std::vector<WORD> types(wide.size());
        if (GetStringTypeW(CT_CTYPE1, wide.data(), static_cast<int>(wide.size()), types.data())) {
            for (std::size_t i = 0; i < types.size(); ++i) {
                if (types[i] & (C1_ALPHA | C1_DIGIT)) {
                    firstAlnum = i;
                    break;
                }
            }
        }
    }

    order.hasAlnum = firstAlnum < wide.size();
    order.sortKey = order.hasAlnum ? wide.substr(firstAlnum) : wide;
    return order;
}

bool UI::Util::IsBeforeInPresetOrder(const PresetOrder& a, const PresetOrder& b) {
    if (a.hasAlnum != b.hasAlnum) {
        return a.hasAlnum;  // names with letters/digits first, symbol-only names last
    }
    if (const int cmp = CompareLinguistic(a.sortKey, b.sortKey); cmp != 0) {
        return cmp < 0;
    }
    return a.name < b.name;
}
