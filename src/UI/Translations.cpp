#include "UI/Translations.h"
#include "SKSE/Translation.h"

void UI::Translations::Init() {
    SKSE::Translation::ParseTranslation("OBody");
}

std::string UI::Translations::Get(const std::string& key) {
    std::string result;
    if (SKSE::Translation::Translate("$" + key, result)) {
        return result;
    }
    return key;
}
