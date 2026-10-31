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
                    std::vector<std::string> bodyslidePresets;
                    bodyslidePresets.reserve(formValue.GetArray().Size());
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

    namespace {
        std::vector<std::string> ReadPresetNameList(const rapidjson::Value& a_value) {
            std::vector<std::string> names;
            if (a_value.IsArray()) {
                for (const auto& item : a_value.GetArray()) {
                    if (item.IsString() && item.GetStringLength() > 0) {
                        names.emplace_back(item.GetString(), item.GetStringLength());
                    }
                }
            }
            return names;
        }

        using RefitLists = std::vector<std::vector<std::string>>;

        using PresetManager::SexRefitBaseName;

        const PresetManager::Preset* FindPresetQuiet(const PresetManager::PresetSet& a_presets,
                                                     const std::string_view a_name) {
            for (const auto& preset : a_presets) {
                if (stl::cmp(preset.name, a_name)) {
                    return &preset;
                }
            }
            return nullptr;
        }

        // We check the armor's keyword EditorIDs and the default ones added by Skyrim (ArmorHeavy/Light/Clothing)
        std::vector<std::string> GetArmorKeywordNames(const RE::TESObjectARMO& a_armor) {
            std::vector<std::string> names;
            const auto add = [&](const std::string_view a_name) {
                if (a_name.empty() ||
                    std::ranges::any_of(names, [&](const std::string& n) { return stl::cmp(n, a_name); })) {
                    return;
                }
                names.emplace_back(a_name);
            };

            for (const auto* const keyword : a_armor.GetKeywords()) {
                if (!keyword) continue;
                if (const char* const editorID{keyword->GetFormEditorID()}) {
                    add(editorID);
                }
            }

            if (a_armor.IsHeavyArmor()) {
                add("ArmorHeavy"sv);
            } else if (a_armor.IsLightArmor()) {
                add("ArmorLight"sv);
            } else if (a_armor.IsClothing()) {
                add("ArmorClothing"sv);
            }
            return names;
        }

        // The armors the actor wears in the body slots, always in slot order (body, chest primary, chest secondary),
        // so the result doesn't depend on which event is being processed. While an equip event is being processed the
        // inventory isn't updated yet, so: an armor being equipped takes the slots it covers, and an armor being
        // removed is not counted
        std::vector<const RE::TESObjectARMO*> GetWornBodyArmors(RE::Actor* a_actor,
                                                                const RE::TESForm* a_changedArmor = nullptr,
                                                                const bool a_removing = false) {
            using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
            constexpr Slot slots[3] = {Slot::kBody, Slot::kModChestPrimary, Slot::kModChestSecondary};

            const auto* const changed{a_changedArmor ? a_changedArmor->As<RE::TESObjectARMO>() : nullptr};

            std::vector<const RE::TESObjectARMO*> armors;
            for (const auto slot : slots) {
                const RE::TESObjectARMO* armor{
                    changed && !a_removing && changed->HasPartOf(slot) ? changed : stl::GetWornArmor(a_actor, slot)};
                if (a_removing && changed && armor == changed) continue;
                if (armor && std::ranges::find(armors, armor) == armors.end()) {
                    armors.push_back(armor);
                }
            }
            return armors;
        }

        // A listed refit preset, if it's loaded and made for the actor's body
        std::optional<PresetManager::Preset> FindListedRefit(const PresetManager::PresetSet& a_allPresets,
                                                             const std::string& a_refitName, const bool a_ube) {
            if (const auto* const preset{PresetManager::FindRefitPreset(a_allPresets, a_refitName, a_ube)}) {
                return *preset;
            }
            if (!FindPresetQuiet(a_allPresets, a_refitName)) {
                logger::warn("Refit preset '{}' was not found among the loaded presets", a_refitName);
            }
            return std::nullopt;
        }

        // First listed entry "<ActorPreset>-Refit<anything>" that matches
        // These refit presets are applied differentialy
        std::optional<PresetManager::Preset> FindDifferentialRefit(const PresetManager::PresetSet& a_allPresets,
                                                                   const RefitLists& a_lists,
                                                                   const std::string_view a_actorPresetName,
                                                                   const bool a_female, const bool a_ube) {
            for (const auto& list : a_lists) {
                for (const auto& refitName : list) {
                    if (!PresetManager::IsRefitPreset(refitName)) {
                        logger::warn("Refit preset '{}' does not contain '-Refit' in its name, ignoring it", refitName);
                        continue;
                    }

                    const auto baseName{PresetManager::GetRefitBaseName(refitName)};
                    if (stl::cmp(baseName, SexRefitBaseName(a_female, a_ube)) ||
                        !stl::cmp(baseName, a_actorPresetName)) {
                        continue;
                    }

                    if (auto preset{FindListedRefit(a_allPresets, refitName, a_ube)}) {
                        return preset;
                    }
                }
            }
            return std::nullopt;
        }

        // First listed entry "Female-Refit<anything>" / "Male-Refit<anything>" that matches,
        // "UBE Female-Refit<anything>" / "UBE Male-Refit<anything>" for UBE bodies
        // Remember that these are applied additively
        std::optional<PresetManager::Preset> FindAdditiveRefitDefault(const PresetManager::PresetSet& a_allPresets,
                                                                      const RefitLists& a_lists, const bool a_female,
                                                                      const bool a_ube) {
            for (const auto& list : a_lists) {
                for (const auto& refitName : list) {
                    if (!PresetManager::IsRefitPreset(refitName) ||
                        !stl::cmp(PresetManager::GetRefitBaseName(refitName), SexRefitBaseName(a_female, a_ube))) {
                        continue;
                    }

                    if (auto preset{FindListedRefit(a_allPresets, refitName, a_ube)}) {
                        return preset;
                    }
                }
            }
            return std::nullopt;
        }
    }  // namespace

    void JSONParser::ProcessRefitOutfitPresetsFormID() {
        auto* const data_handler{RE::TESDataHandler::GetSingleton()};

        const auto load = [&](const char* configKey, RefitOutfitFormIDList& out) {
            out.clear();

            const auto configItr{presetDistributionConfig.FindMember(configKey)};
            if (configItr == presetDistributionConfig.MemberEnd() || !configItr->value.IsObject()) {
                return;
            }
            logger::info(TitleFormatSpecifier, configKey);

            auto& pluginsObject{configItr->value};
            for (auto pluginItr{pluginsObject.MemberBegin()}; pluginItr != pluginsObject.MemberEnd();) {
                const char* const plugin{pluginItr->name.GetString()};
                const auto* const file{data_handler->LookupModByName(plugin)};
                if (!file) {
                    logger::info("removed '{}'", plugin);
                    pluginItr = pluginsObject.EraseMember(pluginItr);
                    continue;
                }

                if (!pluginItr->value.IsObject()) {
                    logger::warn("{}: expected an object under '{}', ignoring it", configKey, plugin);
                    ++pluginItr;
                    continue;
                }

                auto& entries{pluginItr->value};
                for (auto entryItr{entries.MemberBegin()}; entryItr != entries.MemberEnd(); ++entryItr) {
                    const std::string_view formKey{entryItr->name.GetString(), entryItr->name.GetStringLength()};
                    std::vector<std::string> presetNames{ReadPresetNameList(entryItr->value)};
                    if (formKey.empty() || formKey.length() > 8 || presetNames.empty()) {
                        logger::warn("{}: ignoring invalid entry '{}' under '{}'", configKey, formKey, plugin);
                        continue;
                    }

                    const std::string formID{DiscardFormDigits(formKey, file)};
                    std::uint32_t hexnumber{};
                    sscanf_s(formID.data(), "%x", &hexnumber);

                    const auto* const form{data_handler->LookupForm(hexnumber, plugin)};
                    if (!form || form->GetFormType() != RE::FormType::Armor) {
                        logger::info("{} is not a valid armor FormID in '{}'!", formKey, plugin);
                        continue;
                    }

                    // Store the full-length runtime ID so the lookup at equip time is a plain integer compare.
                    // The order is the one of the JSON, and the first entry of an outfit is the one that is used
                    const auto runtimeID{form->GetFormID()};
                    if (std::ranges::any_of(out, [&](const auto& a_entry) { return a_entry.first == runtimeID; })) {
                        logger::info("{} is written more than once in {}, only the first one is used", formKey,
                                     configKey);
                        continue;
                    }
                    out.emplace_back(runtimeID, std::move(presetNames));
                }
                ++pluginItr;
            }
            logger::info("Loaded {} outfit preset(s) from {}", out.size(), configKey);
        };

        load("refitOutfitPresetsFemaleFormID", refitOutfitPresetFormIDListFemale);
        load("refitOutfitPresetsMaleFormID", refitOutfitPresetFormIDListMale);
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
        OBODY_DEFINITION(classFemale)
        OBODY_DEFINITION(classMale)
        OBODY_DEFINITION(blacklistedClassesFemale)
        OBODY_DEFINITION(blacklistedClassesMale)

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
                    stl::RemoveDuplicatesInJsonArray(it->value, presetDistributionConfig.GetAllocator());
                    ++it;
                }
            }

            logger::info(TitleFormatSpecifier, blacklistedNpcs->name.GetString());
            if (AnyNotEnd(end, blacklistedNpcs)) {
                auto& original = blacklistedNpcs->value;
                stl::RemoveDuplicatesInJsonArray(original, presetDistributionConfig.GetAllocator());
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

        logger::info(TitleFormatSpecifier, "classFemale|classMale|blacklistedClassesFemale|blacklistedClassesMale");
        if (AnyNotEnd(end, classFemale, classMale, blacklistedClassesFemale, blacklistedClassesMale)) {
            auto d =
                data_handler->GetFormArray<RE::TESClass>() | std::views::transform([&](const RE::TESClass* npcClass) {
                    return stl::get_editorID(npcClass->As<RE::TESForm>());
                });
            const std::set d_set(d.begin(), d.end());

            logger::info(TitleFormatSpecifier, classFemale->name.GetString());
            if (AnyNotEnd(end, classFemale)) {
                auto& original = classFemale->value;
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

            logger::info(TitleFormatSpecifier, classMale->name.GetString());
            if (AnyNotEnd(end, classMale)) {
                auto& original = classMale->value;
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

            logger::info(TitleFormatSpecifier, blacklistedClassesFemale->name.GetString());
            if (AnyNotEnd(end, blacklistedClassesFemale)) {
                auto& original = blacklistedClassesFemale->value;
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

            logger::info(TitleFormatSpecifier, blacklistedClassesMale->name.GetString());
            if (AnyNotEnd(end, blacklistedClassesMale)) {
                auto& original = blacklistedClassesMale->value;
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
        ProcessRefitOutfitPresetsFormID();
        ProcessDisablePresetDistribution();
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

    void JSONParser::ProcessDisablePresetDistribution() {
        distributionDisabledForMale = false;
        distributionDisabledForFemale = false;

        const auto distributionIterator = presetDistributionConfig.FindMember("disablePresetDistribution");
        if (distributionIterator == presetDistributionConfig.MemberEnd() || !distributionIterator->value.IsString()) {
            return;
        }

        const std::string_view value{distributionIterator->value.GetString(),
                                     distributionIterator->value.GetStringLength()};
        if (value == "male") {
            distributionDisabledForMale = true;
        } else if (value == "female") {
            distributionDisabledForFemale = true;
        } else if (value == "all") {
            distributionDisabledForMale = true;
            distributionDisabledForFemale = true;
        } else if (value != "none") {
            logger::warn("Unknown disablePresetDistribution value '{}', treating as 'none'", value);
        }
    }

    bool JSONParser::IsOutfitBlacklisted(const RE::TESObjectARMO& a_outfit) {
        return IsStringInJsonConfigKey(a_outfit.GetName(), "blacklistedOutfitsFromORefit") ||
               IsOutfitInBlacklistedOutfitCategorySet(a_outfit.GetFormID()) ||
               IsStringInJsonConfigKey(GetNthFormLocationName(a_outfit.As<RE::TESForm>(), 0),
                                       "blacklistedOutfitsFromORefitPlugin");
    }

    bool JSONParser::IsAnyForceRefitItemEquipped(RE::Actor* a_actor, const bool a_removingArmor,
                                                 const RE::TESForm* a_equippedArmor) {
        // The inventory isn't updated yet when an armor is being equipped
        if (!a_removingArmor && a_equippedArmor &&
            (IsStringInJsonConfigKey(a_equippedArmor->GetName(), "outfitsForceRefit") ||
             IsOutfitInForceRefitCategorySet(a_equippedArmor->GetFormID()))) {
            logger::info("Outfit {} is in force refit list", a_equippedArmor->GetName());

            return true;
        }

        for (auto* const inventory_entry_data : stl::GetWornEntries(a_actor)) {
            const auto* const bound_obj{inventory_entry_data->object};

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

        return false;
    }

    // ReSharper disable once CppPassValueParameterByConstReference
    bool JSONParser::IsNPCBlacklisted(const std::string_view actorName, const std::vector<RE::FormID>& actorIDs) {
        if (IsStringInJsonConfigKey(actorName.data(), "blacklistedNpcs")) {
            logger::info("{} is Blacklisted by blacklistedNpcs", actorName);
            return true;
        }

        for (const auto actorID : actorIDs) {
            if (IsActorInBlacklistedCharacterCategorySet(actorID)) {
                logger::info("{} is Blacklisted by character category set ({:08X})", actorName, actorID);
                return true;
            }
        }

        return false;
    }

    bool JSONParser::IsNPCBlacklistedGlobally(const RE::Actor* a_actor,
                                              const std::vector<const RE::TESNPC*>& a_lookupNPCs, const char* actorRace,
                                              const char* actorClass, const bool female) {
        auto actorOwningMod{GetNthFormLocationName(a_actor, 0)};

        // An actor spawned at runtime (0xFF ID) is in no plugin, so it goes by the plugin of the first
        // record it was generated from that has one. Placed actors still go by their reference
        if (actorOwningMod.empty()) {
            for (const auto* const npc : a_lookupNPCs) {
                actorOwningMod = GetNthFormLocationName(npc, 0);

                if (!actorOwningMod.empty()) break;
            }
        }

        if (female) {
            return IsStringInJsonConfigKey(actorOwningMod, "blacklistedNpcsPluginFemale") ||
                   IsStringInJsonConfigKey(actorRace, "blacklistedRacesFemale") ||
                   IsStringInJsonConfigKey(actorClass, "blacklistedClassesFemale");
        }
        return IsStringInJsonConfigKey(actorOwningMod, "blacklistedNpcsPluginMale") ||
               IsStringInJsonConfigKey(actorRace, "blacklistedRacesMale") ||
               IsStringInJsonConfigKey(actorClass, "blacklistedClassesMale");
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCFactionPreset(const RE::TESNPC* a_actor, const bool female,
                                                                         const bool a_ube) {
        auto actorRanks{a_actor->factions | std::views::transform(&RE::FACTION_RANK::faction)};

        const std::vector<RE::TESFaction*> actorFactions{actorRanks.begin(), actorRanks.end()};

        if (actorFactions.empty()) {
            return std::nullopt;
        }

        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        const auto& presetSet{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};

        auto& a_faction{presetDistributionConfig[female ? "factionFemale" : "factionMale"]};

        for (auto itr = a_faction.MemberBegin(); itr != a_faction.MemberEnd(); ++itr) {
            auto& [key, value]{*itr};
            if (std::ranges::find(actorFactions, RE::TESFaction::LookupByEditorID(key.GetString())) !=
                actorFactions.end()) {
                std::vector<std::string_view> copy_of_value;
                copy_of_value.reserve(value.Size());
                for (const auto& item : value.GetArray()) {
                    copy_of_value.emplace_back(item.GetString());
                }

                return PresetManager::GetRandomPresetByName(presetSet, copy_of_value, female, a_ube);
            }
        }

        return std::nullopt;
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCPreset(const char* actorName,
                                                                  const std::vector<RE::FormID>& formIDs,
                                                                  const bool female, const bool a_ube) {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        const auto& presetSet{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};

        std::vector<std::string_view> characterBodyslidePresets;

        // The first form ID with an npcFormID entry decides, the actor's own base before the records it was
        // generated from
        for (const auto formID : formIDs) {
            const auto character{GetNPCFromCategorySet(formID)};

            if (character.has_value() && !character->bodyslidePresets.empty()) {
                if (formID != formIDs.front()) {
                    logger::info("{} uses the npcFormID entry of {:08X}, the record it was generated from", actorName,
                                 formID);
                }

                characterBodyslidePresets.insert_range(characterBodyslidePresets.end(), character->bodyslidePresets);

                return PresetManager::GetRandomPresetByName(presetSet, characterBodyslidePresets, female, a_ube);
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

            return PresetManager::GetRandomPresetByName(presetSet, characterBodyslidePresets, female, a_ube);
        }

        return std::nullopt;
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCPluginPreset(const RE::TESNPC* a_actor,
                                                                        const char* actorName, const bool female,
                                                                        const bool a_ube) {
        const char* keyForPreset{female ? "npcPluginFemale" : "npcPluginMale"};

        if (presetDistributionConfig.HasMember(keyForPreset)) {
            auto& presets = presetDistributionConfig[keyForPreset];
            for (auto itr = presets.MemberBegin(); itr != presets.MemberEnd(); ++itr) {
                auto& [mod, presetList] = *itr;
                logger::info("Checking if actor {} is in mod {}", actorName, mod.GetString());

                if (IsActorInForm(a_actor, mod.GetString())) {
                    const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

                    const auto& presetSet{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};

                    std::vector<std::string_view> presets_copy;
                    presets_copy.reserve(presets[mod].Size());
                    for (const auto& item : presets[mod].GetArray()) {
                        presets_copy.emplace_back(item.GetString());
                    }

                    return GetRandomPresetByName(presetSet, presets_copy, female, a_ube);
                }
            }
        }

        return std::nullopt;
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCRacePreset(const char* actorRace, const bool female,
                                                                      const bool a_ube) {
        const char* key{female ? "raceFemale" : "raceMale"};

        if (IsSubKeyInJsonConfigKey(key, actorRace)) {
            const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

            const auto& presetSet{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};
            std::vector<std::string_view> presets_copy;
            presets_copy.reserve(presetDistributionConfig[key][actorRace].Size());
            for (const auto& item : presetDistributionConfig[key][actorRace].GetArray()) {
                presets_copy.emplace_back(item.GetString());
            }
            return PresetManager::GetRandomPresetByName(presetSet, presets_copy, female, a_ube);
        }

        return std::nullopt;
    }

    std::optional<PresetManager::Preset> JSONParser::GetNPCClassPreset(const char* actorClass, const bool female,
                                                                       const bool a_ube) {
        const char* key{female ? "classFemale" : "classMale"};

        if (IsSubKeyInJsonConfigKey(key, actorClass)) {
            const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

            const auto& presetSet{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};
            std::vector<std::string_view> presets_copy;
            presets_copy.reserve(presetDistributionConfig[key][actorClass].Size());
            for (const auto& item : presetDistributionConfig[key][actorClass].GetArray()) {
                presets_copy.emplace_back(item.GetString());
            }
            return PresetManager::GetRandomPresetByName(presetSet, presets_copy, female, a_ube);
        }

        return std::nullopt;
    }

    std::vector<const RE::TESObjectARMO*> JSONParser::GetRefitArmors(RE::Actor* a_actor,
                                                                     const RE::TESForm* a_changedArmor,
                                                                     const bool a_removingArmor) {
        auto armors{GetWornBodyArmors(a_actor, a_changedArmor, a_removingArmor)};

        // Worn armors with ORefit forced on, in any slot
        std::vector<const RE::TESObjectARMO*> forced;

        for (auto* const entry : stl::GetWornEntries(a_actor)) {
            const auto* const armor{entry->object->As<RE::TESObjectARMO>()};
            if (!armor || std::ranges::find(armors, armor) != armors.end()) continue;

            // The inventory isn't updated yet when an armor is being removed
            if (a_removingArmor && a_changedArmor && armor->GetFormID() == a_changedArmor->GetFormID()) continue;

            if (IsStringInJsonConfigKey(entry->GetDisplayName(), "outfitsForceRefit") ||
                IsOutfitInForceRefitCategorySet(armor->GetFormID())) {
                forced.push_back(armor);
            }
        }

        // The inventory has no meaningful order, so sort them to always get the same result
        std::ranges::sort(forced, {}, [](const RE::TESObjectARMO* a_armor) { return a_armor->GetFormID(); });
        armors.insert(armors.end(), forced.begin(), forced.end());

        return armors;
    }

    std::vector<std::vector<std::string>> JSONParser::CollectOutfitRefitLists(
        const std::vector<const RE::TESObjectARMO*>& a_armors, const bool female) const {
        std::vector<std::vector<std::string>> lists;

        // Outfits are taken in the order they are written in the JSON, not in the order of their slots,
        // because the slot doesn't tell which outfit is on top of the other
        const auto& formIDPresets{female ? refitOutfitPresetFormIDListFemale : refitOutfitPresetFormIDListMale};
        for (const auto& [formID, names] : formIDPresets) {
            if (std::ranges::any_of(a_armors,
                                    [&](const RE::TESObjectARMO* a_armor) { return a_armor->GetFormID() == formID; })) {
                lists.push_back(names);
            }
        }

        const auto nameNode{
            presetDistributionConfig.FindMember(female ? "refitOutfitPresetsFemale" : "refitOutfitPresetsMale")};
        if (nameNode != presetDistributionConfig.MemberEnd() && nameNode->value.IsObject()) {
            for (auto itr{nameNode->value.MemberBegin()}; itr != nameNode->value.MemberEnd(); ++itr) {
                const std::string_view outfitName{itr->name.GetString(), itr->name.GetStringLength()};
                if (outfitName.empty() || !std::ranges::any_of(a_armors, [&](const RE::TESObjectARMO* a_armor) {
                        const char* const name{a_armor->GetName()};
                        return name && outfitName == name;
                    })) {
                    continue;
                }

                if (auto names{ReadPresetNameList(itr->value)}; !names.empty()) {
                    lists.push_back(std::move(names));
                }
            }
        }

        return lists;
    }

    std::vector<std::string> JSONParser::CollectRefitKeywords(const std::vector<const RE::TESObjectARMO*>& a_armors,
                                                              const bool female) const {
        std::vector<std::string> wornKeywords;
        for (const auto* const armor : a_armors) {
            for (auto& name : GetArmorKeywordNames(*armor)) {
                wornKeywords.push_back(std::move(name));
            }
        }

        std::vector<std::string> keywords;
        const auto add = [&](const std::string_view a_keyword) {
            const auto matches = [&](const std::string& k) { return stl::cmp(k, a_keyword); };
            if (a_keyword.empty() || !std::ranges::any_of(wornKeywords, matches) ||
                std::ranges::any_of(keywords, matches)) {
                return;
            }
            keywords.emplace_back(a_keyword);
        };

        // The user's keywords come first, in the order they are written in the JSON
        // Keywords that come first have priority
        const auto keywordNode{
            presetDistributionConfig.FindMember(female ? "refitOutfitKeywordsFemale" : "refitOutfitKeywordsMale")};
        if (keywordNode != presetDistributionConfig.MemberEnd() && keywordNode->value.IsArray()) {
            for (const auto& item : keywordNode->value.GetArray()) {
                if (item.IsString()) {
                    add(std::string_view{item.GetString(), item.GetStringLength()});
                }
            }
        }

        // The vanilla armor types always come last, so they work without any JSON
        // An armor only has one of them, so they can't conflict with each other
        add("ArmorHeavy"sv);
        add("ArmorLight"sv);
        add("ArmorClothing"sv);

        return keywords;
    }

    std::optional<PresetManager::Preset> JSONParser::GetRefitPresetFromEquippedItems(
        RE::Actor* a_actor, const bool female, const bool a_ube, const std::string_view a_actorPresetName,
        const RE::TESForm* a_changedArmor, const bool a_removingArmor) {
        if (a_actorPresetName.empty()) {
            return std::nullopt;
        }

        const auto armors{GetRefitArmors(a_actor, a_changedArmor, a_removingArmor)};
        if (armors.empty()) {
            return std::nullopt;
        }

        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        const auto& allPresets{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};

        // Lists written for this specific outfit
        if (const auto lists{CollectOutfitRefitLists(armors, female)}; !lists.empty()) {
            if (auto preset{FindDifferentialRefit(allPresets, lists, a_actorPresetName, female, a_ube)}) {
                return preset;
            }

            std::string candidates;
            for (const auto& list : lists) {
                for (const auto& name : list) {
                    candidates += (candidates.empty() ? "'" : ", '") + name + "'";
                }
            }
            logger::info(
                "ORefit: the worn outfit has refit entries ({}), but none of them is a '-Refit' of the preset '{}'",
                candidates, a_actorPresetName);
        }

        // "<ActorPreset>-Refit <Keyword>"
        if (!stl::cmp(a_actorPresetName, SexRefitBaseName(female, a_ube))) {
            for (const auto& keyword : CollectRefitKeywords(armors, female)) {
                const std::string name{std::string{a_actorPresetName} + "-Refit " + keyword};
                if (const auto* const preset{PresetManager::FindRefitPreset(allPresets, name, a_ube)}) {
                    return *preset;
                }
            }
        }

        return std::nullopt;
    }

    std::optional<PresetManager::Preset> JSONParser::GetRefitDefaultFromEquippedItems(RE::Actor* a_actor,
                                                                                      const bool female,
                                                                                      const bool a_ube,
                                                                                      const RE::TESForm* a_changedArmor,
                                                                                      const bool a_removingArmor) {
        const auto armors{GetRefitArmors(a_actor, a_changedArmor, a_removingArmor)};

        if (armors.empty()) {
            return std::nullopt;
        }

        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        const auto& allPresets{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};

        // Defaults written in the lists of this specific outfit
        if (auto preset{FindAdditiveRefitDefault(allPresets, CollectOutfitRefitLists(armors, female), female, a_ube)}) {
            return preset;
        }

        // "Female-Refit <Keyword>" / "Male-Refit <Keyword>", "UBE Female-Refit <Keyword>" / "UBE Male-Refit <Keyword>"
        const std::string prefix{std::string{SexRefitBaseName(female, a_ube)} + "-Refit "};
        for (const auto& keyword : CollectRefitKeywords(armors, female)) {
            if (const auto* const preset{PresetManager::FindRefitPreset(allPresets, prefix + keyword, a_ube)}) {
                return *preset;
            }
        }

        return std::nullopt;
    }

    void JSONParser::ValidateRefitPresetNames() const {
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        const auto check = [&](const std::string& a_where, const std::string& a_name, const bool female) {
            const auto& allPresets{female ? presetContainer.allFemalePresets : presetContainer.allMalePresets};

            if (!PresetManager::IsRefitPreset(a_name)) {
                logger::warn("{}: '{}' has no '-Refit' in its name, so it is ignored", a_where, a_name);
                return;
            }

            if (!FindPresetQuiet(allPresets, a_name)) {
                logger::warn("{}: the refit preset '{}' was not found among the loaded presets", a_where, a_name);
            }

            // The text before "-Refit" has to be a loaded preset, or "Female"/"Male" ("UBE Female"/"UBE Male") for the
            // additive defaults
            const auto base{PresetManager::GetRefitBaseName(a_name)};
            if (!base.empty() && !stl::cmp(base, SexRefitBaseName(female, false)) &&
                !stl::cmp(base, SexRefitBaseName(female, true)) && !FindPresetQuiet(allPresets, base)) {
                logger::warn(
                    "{}: the base preset '{}' of '{}' was not found among the loaded presets, so this entry will "
                    "never match an NPC",
                    a_where, base, a_name);
            }
        };

        for (const bool female : {true, false}) {
            const char* const key{female ? "refitOutfitPresetsFemale" : "refitOutfitPresetsMale"};

            const auto node{presetDistributionConfig.FindMember(key)};
            if (node != presetDistributionConfig.MemberEnd() && node->value.IsObject()) {
                for (auto itr{node->value.MemberBegin()}; itr != node->value.MemberEnd(); ++itr) {
                    for (const auto& name : ReadPresetNameList(itr->value)) {
                        check(std::format("{}['{}']", key, itr->name.GetString()), name, female);
                    }
                }
            }

            const auto& formIDPresets{female ? refitOutfitPresetFormIDListFemale : refitOutfitPresetFormIDListMale};
            for (const auto& [formID, names] : formIDPresets) {
                for (const auto& name : names) {
                    check(std::format("{}FormID [{:08X}]", key, formID), name, female);
                }
            }
        }
    }

}  // namespace Parser
