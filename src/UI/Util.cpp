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
