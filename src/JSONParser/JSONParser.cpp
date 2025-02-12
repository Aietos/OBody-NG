#include "JSONParser/JSONParser.h"
#include "STL.h"

Parser::JSONParser Parser::JSONParser::instance;

namespace Parser {
    constexpr auto TitleFormatSpecifier{"{:-^100}"};

    JSONParser& JSONParser::GetInstance() { return instance; }

    bool GetHasSourceFileArray(const RE::TESForm* form) {
        return form->sourceFiles.array;  // Check if the source files array exists
    }

    std::string GetNthFormLocationName(const RE::TESForm* form, const uint32_t n) {
        std::string formName;

        if (GetHasSourceFileArray(form) && form->sourceFiles.array->size() > n) {
            RE::TESFile** sourceFiles = form->sourceFiles.array->data();
            formName = sourceFiles[n]->fileName;
        }

        // fix for weird bug where refs first defined in Skyrim.Esm aren't always detected properly
        // Refs from Skyrim.ESM will have 00 for the first two hexadecimal digits
        // And refs from all other mods will have a non-zero value, so a bitwise && of
        // those two digits with FF will be nonzero for all non Skyrim.ESM mods
        if (((form->formID & 0xFF000000) == 0) && formName != "Skyrim.esm") {
            return "Skyrim.esm";
        }

        return formName;
    }

    bool IsActorInForm(const RE::TESNPC* form, const std::string_view target) {
        if (GetHasSourceFileArray(form) && !form->sourceFiles.array->empty()) {
            RE::TESFile** sourceFiles{form->sourceFiles.array->data()};

            for (int i{}; i < form->sourceFiles.array->size(); i++) {
                if (sourceFiles[i]->fileName == target) {
                    return true;
                }
            }
        }

        return false;
    }

    bool JSONParser::IsActorInBlacklistedCharacterCategorySet(const uint32_t formID) const {
        for (const auto a_formID : blacklistedCharacterCategorySet | std::views::transform(&categorizedList::formID)) {
            if (a_formID == formID) {
                return true;
            }
        }

        return false;
    }

    bool JSONParser::IsOutfitInBlacklistedOutfitCategorySet(const uint32_t formID) {
        for (const auto a_formID : blacklistedOutfitCategorySet | std::views::transform(&categorizedList::formID)) {
            if (a_formID == formID) {
                return true;
            }
        }

        return false;
    }

    bool JSONParser::IsOutfitInForceRefitCategorySet(const uint32_t formID) const {
        for (const auto a_formID : forceRefitOutfitCategorySet | std::views::transform(&categorizedList::formID)) {
            if (a_formID == formID) {
                return true;
            }
        }

        return false;
    }

    std::optional<categorizedList> JSONParser::GetNPCFromCategorySet(const uint32_t formID) const {
        for (const categorizedList& character : characterCategorySet) {
            if (character.formID == formID) {
                return character;
            }
        }

        return {};
    }

    inline std::string DiscardFormDigits(const std::string_view formID, const RE::TESFile* mod) {
        char temp[9]{"00000000"};
        std::memcpy(temp + (8 - formID.length()), formID.data(), formID.length());
        return std::string(temp + (mod->IsLight() ? 5 : 2));
    }

    void JSONParser::ProcessNPCsFormID() {
        const auto npcFormIDItr{presetDistributionConfig.FindMember("npcFormID")};
        logger::info(TitleFormatSpecifier, npcFormIDItr->name.GetString());
        if (npcFormIDItr != presetDistributionConfig.MemberEnd()) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& npcFormID{npcFormIDItr->value};
            for (auto itr{npcFormID.MemberBegin()}; itr != npcFormID.MemberEnd();) {
                auto& [owningMod, value]{*itr};
                const auto* const file{data_handler->LookupModByName(owningMod.GetString())};
                if (!file) {
                    logger::info("removed '{}'", owningMod.GetString());
                    itr = npcFormID.EraseMember(itr);
                    continue;
                }
                for (auto valueItr = value.MemberBegin(); valueItr != value.MemberEnd(); ++valueItr) {
                    auto& [formKey, formValue]{*valueItr};
                    stl::RemoveDuplicatesInJsonArray(formValue, presetDistributionConfig.GetAllocator());
                    std::string formID{DiscardFormDigits(formKey.GetString(), file)};

                    uint32_t hexnumber;
                    sscanf_s(formID.data(), "%x", &hexnumber);

                    const auto actorform = data_handler->LookupForm(hexnumber, owningMod.GetString());

                    if (!actorform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = actorform->GetFormID();
                    std::vector<std::string> bodyslidePresets(formValue.GetArray().Size());
                    for (const auto& item : formValue.GetArray()) {
                        bodyslidePresets.emplace_back(item.GetString());
                    }

                    characterCategorySet.emplace_back(owningMod.GetString(), ID, std::move(bodyslidePresets));
                }
                ++itr;
            }
        }
    }

    void JSONParser::ProcessNPCsFormIDBlacklist() {
        const auto blacklistedNpcsFormIDItr{presetDistributionConfig.FindMember("blacklistedNpcsFormID")};
        logger::info(TitleFormatSpecifier, blacklistedNpcsFormIDItr->name.GetString());
        if (blacklistedNpcsFormIDItr != presetDistributionConfig.MemberEnd()) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& blacklistedNpcsFormID{blacklistedNpcsFormIDItr->value};
            for (auto itr{blacklistedNpcsFormID.MemberBegin()}; itr != blacklistedNpcsFormID.MemberEnd();) {
                auto& [plugin, val] = *itr;
                const auto* const file{data_handler->LookupModByName(plugin.GetString())};
                if (!file) {
                    logger::info("removed '{}'", plugin.GetString());
                    itr = blacklistedNpcsFormID.EraseMember(itr);
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val, presetDistributionConfig.GetAllocator());
                for (const auto& formIDRaw : val.GetArray()) {
                    std::string formID{DiscardFormDigits(formIDRaw.GetString(), file)};
                    uint32_t hexnumber;
                    sscanf_s(formID.data(), "%x", &hexnumber);

                    const auto actorform = data_handler->LookupForm(hexnumber, plugin.GetString());

                    if (!actorform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = actorform->GetFormID();

                    blacklistedCharacterCategorySet.emplace_back(plugin.GetString(), ID);
                }
                ++itr;
            }
        }
    }

    void JSONParser::ProcessOutfitsFormIDBlacklist() {
        const auto blacklistedOutfitsFromORefitFormIDItr{
            presetDistributionConfig.FindMember("blacklistedOutfitsFromORefitFormID")};
        logger::info(TitleFormatSpecifier, blacklistedOutfitsFromORefitFormIDItr->name.GetString());
        if (blacklistedOutfitsFromORefitFormIDItr != presetDistributionConfig.MemberEnd()) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& blacklistedOutfitsFromORefitFormID{blacklistedOutfitsFromORefitFormIDItr->value};
            for (auto itr{blacklistedOutfitsFromORefitFormID.MemberBegin()};
                 itr != blacklistedOutfitsFromORefitFormID.MemberEnd();) {
                auto& [plugin, val]{*itr};
                const auto* const file{data_handler->LookupModByName(plugin.GetString())};
                if (!file) {
                    logger::info("removed '{}'", plugin.GetString());
                    itr = blacklistedOutfitsFromORefitFormID.EraseMember(itr);
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val, presetDistributionConfig.GetAllocator());
                for (const auto& formIDRaw : val.GetArray()) {
                    std::string formID{DiscardFormDigits(formIDRaw.GetString(), file)};
                    uint32_t hexnumber;
                    sscanf_s(formID.data(), "%x", &hexnumber);

                    const auto outfitform = data_handler->LookupForm(hexnumber, plugin.GetString());

                    if (!outfitform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = outfitform->GetFormID();

                    blacklistedOutfitCategorySet.emplace_back(plugin.GetString(), ID);
                }
                ++itr;
            }
        }
    }

    void JSONParser::ProcessOutfitsForceRefitFormIDBlacklist() {
        const auto outfitsForceRefitFormIDItr{presetDistributionConfig.FindMember("outfitsForceRefitFormID")};
        logger::info(TitleFormatSpecifier, outfitsForceRefitFormIDItr->name.GetString());
        if (outfitsForceRefitFormIDItr != presetDistributionConfig.MemberEnd()) {
            auto* const data_handler{RE::TESDataHandler::GetSingleton()};
            auto& outfitsForceRefitFormID{outfitsForceRefitFormIDItr->value};
            for (auto itr{outfitsForceRefitFormID.MemberBegin()}; itr != outfitsForceRefitFormID.MemberEnd();) {
                auto& [plugin, val]{*itr};
                const auto* const file{data_handler->LookupModByName(plugin.GetString())};
                if (!file) {
                    logger::info("removed '{}'", plugin.GetString());
                    itr = outfitsForceRefitFormID.EraseMember(itr);
                    continue;
                }
                stl::RemoveDuplicatesInJsonArray(val, presetDistributionConfig.GetAllocator());
                for (const auto& formIDRaw : val.GetArray()) {
                    std::string formID{DiscardFormDigits(formIDRaw.GetString(), file)};
                    uint32_t hexnumber;
                    sscanf_s(formID.data(), "%x", &hexnumber);

                    const auto outfitform = data_handler->LookupForm(hexnumber, plugin.GetString());

                    if (!outfitform) {
                        logger::info("{} is not a valid key!", formID);
                        continue;
                    }

                    // We have to use this full-length ID in order to identify them.
                    auto ID = outfitform->GetFormID();

                    forceRefitOutfitCategorySet.emplace_back(plugin.GetString(), ID);
                }
                ++itr;
            }
        }
    }

    inline bool ValidateActor(const RE::Actor* const actor) {
        if (actor == nullptr || (actor->formFlags & RE::TESForm::RecordFlags::kDeleted) ||
            (actor->inGameFormFlags & RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted) ||
            (actor->inGameFormFlags & RE::TESForm::InGameFormFlag::kWantsDelete) || actor->GetFormID() == 0 ||
            (actor->formFlags & RE::TESForm::RecordFlags::kDisabled))
            return false;

        return true;
    }

    template <typename Iterator, typename... Iterators>
        requires(sizeof...(Iterators) > 0) && (std::same_as<Iterator, std::decay_t<Iterators>> && ...)
    constexpr bool AnyNotEnd(Iterator end, Iterators... iterators) {
        return ((iterators != end) || ...);
    }

    void JSONParser::FilterOutNonLoaded() {
        auto* const data_handler{RE::TESDataHandler::GetSingleton()};
        const auto end{presetDistributionConfig.MemberEnd()};

#define OBODY_DEFINITION(var) const auto var{presetDistributionConfig.FindMember(#var)};

        OBODY_DEFINITION(npc)
        OBODY_DEFINITION(blacklistedNpcs)
        OBODY_DEFINITION(factionFemale)
        OBODY_DEFINITION(factionMale)
        OBODY_DEFINITION(npcPluginFemale)
        OBODY_DEFINITION(npcPluginMale)
        OBODY_DEFINITION(raceFemale)
        OBODY_DEFINITION(raceMale)
        OBODY_DEFINITION(blacklistedRacesFemale)
        OBODY_DEFINITION(blacklistedRacesMale)
        OBODY_DEFINITION(blacklistedNpcsPluginFemale)
        OBODY_DEFINITION(blacklistedNpcsPluginMale)
        OBODY_DEFINITION(blacklistedOutfitsFromORefit)
        OBODY_DEFINITION(outfitsForceRefit)
        OBODY_DEFINITION(blacklistedOutfitsFromORefitPlugin)

#undef OBODY_DEFINITION

        logger::info(TitleFormatSpecifier, "npc|blacklistedNpcs");
        if (AnyNotEnd(end, npc, blacklistedNpcs)) {
            std::set<std::string> npc_names;
            {
                const auto& [hashtable, lock]{RE::TESForm::GetAllForms()};
                const RE::BSReadLockGuard locker{lock};
                if (hashtable) {
                    for (auto& [_, form] : *hashtable) {
                        if (form) {
                            if (auto* const actor{form->As<RE::Actor>()};
                                ValidateActor(actor) && actor->HasKeywordString("ActorTypeNPC")) {
                                if (const char* const name{actor->GetBaseObject()->GetName()}) {
                                    npc_names.emplace(name);
                                }
                            }
                        }
                    }
                }
            }

            logger::info(TitleFormatSpecifier, npc->name.GetString());
            if (AnyNotEnd(end, npc)) {
                auto& original = npc->value;
                for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                    if (!npc_names.contains(it->name.GetString())) {
                        logger::info("removed '{}'", it->name.GetString());
                        it = original.EraseMember(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                        ++it;
                    }
                }
            }

            logger::info(TitleFormatSpecifier, blacklistedNpcs->name.GetString());
            if (AnyNotEnd(end, blacklistedNpcs)) {
                auto& original = blacklistedNpcs->value;
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!npc_names.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        logger::info(TitleFormatSpecifier, factionFemale->name.GetString());
        if (AnyNotEnd(end, factionFemale)) {
            auto& original = factionFemale->value;
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!RE::TESForm::LookupByEditorID(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }

        logger::info(TitleFormatSpecifier, factionMale->name.GetString());
        if (AnyNotEnd(end, factionMale)) {
            auto& original = factionMale->value;
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!RE::TESForm::LookupByEditorID(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }

        logger::info(TitleFormatSpecifier, npcPluginFemale->name.GetString());
        if (AnyNotEnd(end, npcPluginFemale)) {
            auto& original{npcPluginFemale->value};
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!data_handler->LookupModByName(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }

        logger::info(TitleFormatSpecifier, npcPluginMale->name.GetString());
        if (AnyNotEnd(end, npcPluginMale)) {
            auto& original{npcPluginMale->value};
            for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                if (!data_handler->LookupModByName(it->name.GetString())) {
                    logger::info("removed '{}'", it->name.GetString());
                    it = original.EraseMember(it);
                } else {
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }
        }
        logger::info(TitleFormatSpecifier, "raceFemale|raceMale|blacklistedRacesFemale|blacklistedRacesMale");
        if (AnyNotEnd(end, raceFemale, raceMale, blacklistedRacesFemale, blacklistedRacesMale)) {
            auto d = data_handler->GetFormArray<RE::TESRace>() | std::views::transform([&](const RE::TESRace* race) {
                         return stl::get_editorID(race->As<RE::TESForm>());
                     });
            const std::set d_set(d.begin(), d.end());

            logger::info(TitleFormatSpecifier, raceFemale->name.GetString());
            if (AnyNotEnd(end, raceFemale)) {
                auto& original = raceFemale->value;
                for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                    if (!d_set.contains(it->name.GetString())) {
                        logger::info("removed '{}'", it->name.GetString());
                        it = original.EraseMember(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                        ++it;
                    }
                }
            }

            logger::info(TitleFormatSpecifier, raceMale->name.GetString());
            if (AnyNotEnd(end, raceMale)) {
                auto& original = raceMale->value;
                for (auto it = original.MemberBegin(); it != original.MemberEnd();) {
                    if (!d_set.contains(it->name.GetString())) {
                        logger::info("removed '{}'", it->name.GetString());
                        it = original.EraseMember(it);
                    } else {
                        stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                        ++it;
                    }
                }
            }

            logger::info(TitleFormatSpecifier, blacklistedRacesFemale->name.GetString());
            if (AnyNotEnd(end, blacklistedRacesFemale)) {
                auto& original = blacklistedRacesFemale->value;
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            logger::info(TitleFormatSpecifier, blacklistedRacesMale->name.GetString());
            if (AnyNotEnd(end, blacklistedRacesMale)) {
                auto& original = blacklistedRacesMale->value;
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        logger::info(TitleFormatSpecifier, blacklistedNpcsPluginFemale->name.GetString());
        if (AnyNotEnd(end, blacklistedNpcsPluginFemale)) {
            auto& original = blacklistedNpcsPluginFemale->value;
            stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
            for (auto it = original.Begin(); it != original.End();) {
                if (!data_handler->LookupModByName(it->GetString())) {
                    logger::info("removed '{}'", it->GetString());
                    it = original.Erase(it);
                } else {
                    ++it;
                }
            }
        }

        logger::info(TitleFormatSpecifier, blacklistedNpcsPluginMale->name.GetString());
        if (AnyNotEnd(end, blacklistedNpcsPluginMale)) {
            auto& original = blacklistedNpcsPluginMale->value;
            stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
            for (auto it = original.Begin(); it != original.End();) {
                if (!data_handler->LookupModByName(it->GetString())) {
                    logger::info("removed '{}'", it->GetString());
                    it = original.Erase(it);
                } else {
                    ++it;
                }
            }
        }

        logger::info(TitleFormatSpecifier, "blacklistedOutfitsFromORefit|outfitsForceRefit");
        if (AnyNotEnd(end, blacklistedOutfitsFromORefit, outfitsForceRefit)) {
            auto d = data_handler->GetFormArray<RE::TESObjectARMO>() |
                     std::views::transform([](const RE::TESObjectARMO* outfit) { return outfit->GetName(); });
            const std::set<std::string> d_set(d.begin(), d.end());

            logger::info(TitleFormatSpecifier, blacklistedOutfitsFromORefit->name.GetString());
            if (AnyNotEnd(end, blacklistedOutfitsFromORefit)) {
                auto& original = blacklistedOutfitsFromORefit->value;
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            logger::info(TitleFormatSpecifier, outfitsForceRefit->name.GetString());
            if (AnyNotEnd(end, outfitsForceRefit)) {
                auto& original = outfitsForceRefit->value;
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
                for (auto it = original.Begin(); it != original.End();) {
                    if (!d_set.contains(it->GetString())) {
                        logger::info("removed '{}'", it->GetString());
                        it = original.Erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        logger::info(TitleFormatSpecifier, blacklistedOutfitsFromORefitPlugin->name.GetString());
        if (AnyNotEnd(end, blacklistedOutfitsFromORefitPlugin)) {
            auto& original = blacklistedOutfitsFromORefitPlugin->value;
            stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
            for (auto it = original.Begin(); it != original.End();) {
                if (!data_handler->LookupModByName(it->GetString())) {
                    logger::info("removed '{}'", it->GetString());
                    it = original.Erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void JSONParser::ProcessJSONCategories() {
        [[maybe_unused]] stl::timeit const t;
        logger::info(TitleFormatSpecifier, "Starting: Removing Not-Loaded Items");
        ProcessNPCsFormIDBlacklist();
        ProcessNPCsFormID();
        ProcessOutfitsFormIDBlacklist();
        ProcessOutfitsForceRefitFormIDBlacklist();
        FilterOutNonLoaded();
        logger::info(TitleFormatSpecifier, "Finished: Removing Not-Loaded Items");
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter writer(buffer);
        presetDistributionConfig.Accept(writer);

        logger::info("After Filtering: \n{}", buffer.GetString());
    }

    bool JSONParser::IsStringInJsonConfigKey(const std::string_view a_value, const char* key) {
        const auto obj{presetDistributionConfig.FindMember(key)};
        if (obj == presetDistributionConfig.MemberEnd()) {
            return false;
        }
        const auto& objValue = obj->value;
        return std::find(objValue.Begin(), objValue.End(), a_value.data()) != objValue.End();
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsSubKeyInJsonConfigKey(const char* key, const std::string_view subKey) {
        const auto obj{presetDistributionConfig.FindMember(key)};
        return obj != presetDistributionConfig.MemberEnd() && obj->value.HasMember(subKey.data());
    }

    bool JSONParser::IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit) {
        return IsStringInJsonConfigKey(a_outfit.GetName(), "blacklistedOutfitsFromORefit") ||
               IsOutfitInBlacklistedOutfitCategorySet(a_outfit.GetFormID()) ||
               IsStringInJsonConfigKey(GetNthFormLocationName(a_outfit.As<RE::TESForm>(), 0),
                                       "blacklistedOutfitsFromORefitPlugin");
    }

    bool JSONParser::IsAnyForceRefitItemEquipped(RE::Actor* a_actor, const bool a_removingArmor,
                                                 const RE::TESForm* a_equippedArmor) {
        auto inventory = a_actor->GetInventory() | std::views::transform([](const auto& pair) {
                             return std::pair<RE::TESBoundObject*, const std::unique_ptr<RE::InventoryEntryData>&>(
                                 pair.first, pair.second.second);  // Return the unique_ptr directly
                         });

        std::vector<std::string> wornItems;

        for (const auto& [bound_obj, inventory_entry_data] : inventory) {
            if (inventory_entry_data->IsWorn()) {
                // Check if the item is being unequipped or not first
                if (a_removingArmor && bound_obj->GetFormID() == a_equippedArmor->GetFormID()) {
                    continue;
                }

                if (const RE::FormType itemFormType = bound_obj->GetFormType();
                    (itemFormType == RE::FormType::Armor || itemFormType == RE::FormType::Armature) &&
                        IsStringInJsonConfigKey(inventory_entry_data->GetDisplayName(), "outfitsForceRefit") ||
                    IsOutfitInForceRefitCategorySet(bound_obj->GetFormID())) {
                    logger::info("Outfit {} is in force refit list", inventory_entry_data->GetDisplayName());

                    return true;
                }
            }
        }

        return false;
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsNPCBlacklisted(const std::string_view actorName, const uint32_t actorID) {
        if (IsStringInJsonConfigKey(actorName.data(), "blacklistedNpcs")) {
            logger::info("{} is Blacklisted by blacklistedNpcs", actorName);
            return true;
        }

        if (IsActorInBlacklistedCharacterCategorySet(actorID)) {
            logger::info("{} is Blacklisted by character category set", actorName);
            return true;
        }

        return false;
    }

    bool JSONParser::IsNPCBlacklistedGlobally(const RE::Actor* a_actor, const char* actorRace, const bool female) {
        const auto actorOwningMod{GetNthFormLocationName(a_actor, 0)};

        if (female) {
            return IsStringInJsonConfigKey(actorOwningMod, "blacklistedNpcsPluginFemale") ||
                   IsStringInJsonConfigKey(actorRace, "blacklistedRacesFemale");
        }
        return IsStringInJsonConfigKey(actorOwningMod, "blacklistedNpcsPluginMale") ||
               IsStringInJsonConfigKey(actorRace, "blacklistedRacesMale");
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCFactionPreset(const RE::TESNPC* a_actor, const bool female) {
        auto actorRanks{a_actor->factions | std::views::transform(&RE::FACTION_RANK::faction)};

        const std::vector<RE::TESFaction*> actorFactions{actorRanks.begin(), actorRanks.end()};

        if (actorFactions.empty()) {
            return {};
        }

        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                        : presetContainer.allMalePresets};

        auto& a_faction{presetDistributionConfig[female ? "factionFemale" : "factionMale"]};

        for (auto itr = a_faction.MemberBegin(); itr != a_faction.MemberEnd(); ++itr) {
            auto& [key, value]{*itr};
            if (std::ranges::find(actorFactions, RE::TESFaction::LookupByEditorID(key.GetString())) !=
                actorFactions.end()) {
                std::vector<std::string_view> copy_of_value{value.Size()};
                for (const auto& item : value.GetArray()) {
                    copy_of_value.emplace_back(item.GetString());
                }

                return PresetManager::GetRandomPresetByName(presetSet, copy_of_value, female);
            }
        }

        return {};
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCPreset(const char* actorName, const uint32_t formID,
                                                                  const bool female) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                        : presetContainer.allMalePresets};

        const auto character{GetNPCFromCategorySet(formID)};

        std::vector<std::string_view> characterBodyslidePresets;

        if (character.has_value()) {
            if (!character->bodyslidePresets.empty()) {
                characterBodyslidePresets.insert_range(characterBodyslidePresets.end(), character->bodyslidePresets);
                return PresetManager::GetRandomPresetByName(presetSet, characterBodyslidePresets, female);
            }
        }
        if (const auto npcItr{presetDistributionConfig.FindMember("npc")};
            npcItr != presetDistributionConfig.MemberEnd()) {
            const auto npcActorItr{npcItr->value.FindMember(actorName)};

            if (npcActorItr == npcItr->value.MemberEnd()) return {};

            characterBodyslidePresets.reserve(npcActorItr->value.Size());
            for (const auto& item : npcActorItr->value.GetArray()) {
                characterBodyslidePresets.emplace_back(item.GetString());
            }
            return PresetManager::GetRandomPresetByName(presetSet, characterBodyslidePresets, female);
        }
        return {};
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCPluginPreset(const RE::TESNPC* a_actor,
                                                                        const char* actorName, const bool female) {
        const char* keyForPreset{female ? "npcPluginFemale" : "npcPluginMale"};

        if (presetDistributionConfig.HasMember(keyForPreset)) {
            auto& presets = presetDistributionConfig[keyForPreset];
            for (auto itr = presets.MemberBegin(); itr != presets.MemberEnd(); ++itr) {
                auto& [mod, presetList] = *itr;
                logger::info("Checking if actor {} is in mod {}", actorName, mod.GetString());

                if (IsActorInForm(a_actor, mod.GetString())) {
                    const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

                    const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                                    : presetContainer.allMalePresets};

                    std::vector<std::string_view> presets_copy{presets[mod].Size()};
                    for (const auto& item : presets[mod].GetArray()) {
                        presets_copy.emplace_back(item.GetString());
                    }

                    return GetRandomPresetByName(presetSet, presets_copy, female);
                }
            }
        }

        return {};
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCRacePreset(const char* actorRace, const bool female) {
        const char* key{female ? "raceFemale" : "raceMale"};

        if (IsSubKeyInJsonConfigKey(key, actorRace)) {
            const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

            const PresetManager::PresetSet presetSet{female ? presetContainer.allFemalePresets
                                                            : presetContainer.allMalePresets};
            std::vector<std::string_view> presets_copy{presetDistributionConfig[key][actorRace].Size()};
            for (const auto& item : presetDistributionConfig[key][actorRace].GetArray()) {
                presets_copy.emplace_back(item.GetString());
            }
            return PresetManager::GetRandomPresetByName(presetSet, presets_copy, female);
        }

        return {};
    }
}  // namespace Parser