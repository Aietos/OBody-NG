#include "UI/Translations.h"
#include "SKSE/Translation.h"

namespace {
    std::unordered_map<std::string, std::string> g_translations;

    std::wstring ReadAll(RE::BSResourceNiBinaryStream& a_stream) {
        std::wstring text;
        wchar_t buffer[2048];
        while (true) {
            std::uint64_t read{0};
            a_stream.stream->DoRead(buffer, sizeof(buffer), read);
            if (read == 0) break;
            text.append(buffer, static_cast<std::size_t>(read / sizeof(wchar_t)));
        }
        return text;
    }

    void ReadOwnCopy(const std::string& a_name) {
        auto* const settings{RE::INISettingCollection::GetSingleton()};
        const auto* const setting{settings ? settings->GetSetting("sLanguage:General") : nullptr};
        const std::string language{setting && setting->GetType() == RE::Setting::Type::kString ? setting->data.s
                                                                                               : "ENGLISH"};
        const auto path{std::format("Interface\\Translations\\{}_{}.txt", a_name, language)};

        RE::BSResourceNiBinaryStream stream{path};
        if (!stream.good()) {
            logger::warn("Couldn't read {} for the menu", path);
            return;
        }

        const auto text{ReadAll(stream)};
        std::wstring_view rest{text};
        if (!rest.empty() && rest.front() == L'\xFEFF') {
            rest.remove_prefix(1);
        }

        while (!rest.empty()) {
            const auto end{rest.find(L'\n')};
            auto line{rest.substr(0, end)};
            rest = end == std::wstring_view::npos ? std::wstring_view{} : rest.substr(end + 1);

            if (!line.empty() && line.back() == L'\r') line.remove_suffix(1);

            // The key ends at the last tab, as the game reads it
            const auto tab{line.rfind(L'\t')};

            if (line.size() < 4 || line.front() != L'$' || tab == std::wstring_view::npos || tab < 2) continue;

            auto key{SKSE::stl::utf16_to_utf8(line.substr(0, tab))};
            auto translation{SKSE::stl::utf16_to_utf8(line.substr(tab + 1))};

            if (key && translation) {
                g_translations.insert_or_assign(std::move(*key), std::move(*translation));
            }
        }

        logger::info("Read {} translations from {} for the menu", g_translations.size(), path);
    }
}  // namespace

void UI::Translations::Init() {
    SKSE::Translation::ParseTranslation("OBody");
    ReadOwnCopy("OBody");
}

std::string UI::Translations::Get(const std::string& key) {
    const std::string fullKey{"$" + key};
    if (const auto it{g_translations.find(fullKey)}; it != g_translations.end()) {
        return it->second;
    }

    std::string result;
    if (SKSE::Translation::Translate(fullKey, result)) {
        return result;
    }
    return key;
}
