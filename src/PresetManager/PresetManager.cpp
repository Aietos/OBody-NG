#include "PresetManager/PresetManager.h"

#include "JSONParser/JSONParser.h"
#include "STL.h"

PresetManager::PresetContainer PresetManager::PresetContainer::instance;

namespace PresetManager {
    constexpr auto DefaultSliders =
        stl::to_set<std::string_view, 10>({"Breasts", "BreastsSmall", "NippleDistance", "NippleSize", "ButtCrack",
                                           "Butt", "ButtSmall", "Legs", "Arms", "ShoulderWidth"});

    PresetContainer& PresetContainer::GetInstance() { return instance; }

    void GeneratePresets() {
        const fs::path root_path(R"(Data\CalienteTools\BodySlide\SliderPresets)");

        auto& container{PresetManager::PresetContainer::GetInstance()};

        auto& femalePresets{container.femalePresets};
        auto& malePresets{container.malePresets};

        auto& allFemalePresets{container.allFemalePresets};
        auto& allMalePresets{container.allMalePresets};

        auto& blacklistedFemalePresets{container.blacklistedFemalePresets};
        auto& blacklistedMalePresets{container.blacklistedMalePresets};
        auto& parser{Parser::JSONParser::GetInstance()};
        auto& presetDistributionConfig{parser.presetDistributionConfig};

        auto& blacklistedPresets{presetDistributionConfig["blacklistedPresetsFromRandomDistribution"]};
        stl::RemoveDuplicatesInJsonArray(blacklistedPresets, presetDistributionConfig.GetAllocator());
        const auto blacklistedPresetsBegin = blacklistedPresets.Begin();
        const auto blacklistedPresetsEnd = blacklistedPresets.End();

        for (const auto& entry : fs::directory_iterator(root_path)) {
            const auto& path{entry.path()};
            if (path.extension().c_str() != L".xml"sv) continue;
            // Files with a "clothed" keyword in their name are still opened, but only their "-Refit" presets are kept.
            const bool clothedFile{IsClothedSet(path.wstring())};

            pugi::xml_document doc;
            if (auto result = doc.load_file(path.c_str(), pugi::parse_default, pugi::encoding_auto); !result) {
                wchar_t buffer[2048];
                swprintf_s(buffer, std::size(buffer), L"load failed: %s [%hs]", path.c_str(), result.description());
                SPDLOG_WARN(buffer);
                parser.invalid_presets++;
                continue;
            }

            auto presets = doc.child("SliderPresets");
            for (const auto& node : presets) {
                if (clothedFile && !IsRefitPreset(node.attribute("name").value())) continue;

                auto preset = GeneratePreset(node);
                if (!preset) continue;

                const bool blacklisted{std::find(blacklistedPresetsBegin, blacklistedPresetsEnd,
                                                 preset.value().name.c_str()) != blacklistedPresetsEnd ||
                                       IsRefitPreset(preset.value().name)};

                // The same UBE presets shape both sexes, so they go in both lists
                if (preset->ube || IsFemalePreset(*preset)) {
                    (blacklisted ? blacklistedFemalePresets : femalePresets).push_back(*preset);
                }
                if (preset->ube || !IsFemalePreset(*preset)) {
                    (blacklisted ? blacklistedMalePresets : malePresets).push_back(*preset);
                }
            }
        }
        // For performance reasons, PresetContainer::AssignPresetIndexes
        // relies on the blacklisted presets coming after the non-blacklisted presets.
        // (In order to not rely on this order, we'd instead have to perform string-key lookups
        //  via a hash-table, instead of direct array access. Which wouldn't be good for code
        //  that runs every time a saved-game is loaded).

        allFemalePresets = femalePresets;
        allFemalePresets.insert_range(allFemalePresets.end(), blacklistedFemalePresets);

        allMalePresets = malePresets;
        allMalePresets.insert_range(allMalePresets.end(), blacklistedMalePresets);

        for (const PresetSet* const presets : {&allFemalePresets, &allMalePresets}) {
            for (const auto& preset : *presets) {
                if (!IsRefitPreset(preset.name)) continue;

                for (const auto& sliderName : preset.sliders | std::views::keys) {
                    container.refitSliderNames.insert(sliderName);
                }
            }
        }

        logger::info("Female presets: {}, Male presets: {}", femalePresets.size(), malePresets.size());
        logger::info("Blacklisted: Female presets: {}, Male Presets: {}", blacklistedFemalePresets.size(),
                     blacklistedMalePresets.size());
        logger::info("UBE presets, in both lists: {}, blacklisted: {}",
                     std::ranges::count_if(femalePresets, &Preset::ube),
                     std::ranges::count_if(blacklistedFemalePresets, &Preset::ube));
    }

    bool PresetContainer::HasPresetsFor(const bool a_female, const bool a_ube) const {
        return std::ranges::any_of(a_female ? femalePresets : malePresets,
                                   [=](const Preset& a_preset) { return a_preset.ube == a_ube; });
    }

    std::optional<Preset> GeneratePreset(const pugi::xml_node& a_node) {
        const std::string_view name{a_node.attribute("name").value()};

        if (IsClothedSet(name) && !IsRefitPreset(name)) return {};

        const std::string_view body{a_node.attribute("set").value()};

        Preset preset{name.data(), body.data(), SliderSetFromNode(a_node, GetBodyType(body))};
        preset.ube = IsUBEPreset(a_node);
        return preset;
    }

    void PresetContainer::AssignPresetIndexes() {
        auto assignIndexes = [&](PresetSet& allLoadedPresets, PresetSet& nonBlacklistedPresets,
                                 PresetSet& blacklistedPresets, auto& presetIndexMap, auto& sparseIndexMap,
                                 auto& nextPresetIndex) {
            assert(allLoadedPresets.size() == nonBlacklistedPresets.size() + blacklistedPresets.size());

            // We ensure that absent presets have an index of -1 to signify their absence.
            sparseIndexMap.resize(0);
            sparseIndexMap.resize(nextPresetIndex.value, -1);

            size_t loadedIndexOffset = 0;

            auto assignSubset = [&](PresetSet& subset) {
                size_t loadedIndex = 0;

                for (; loadedIndex < subset.size(); ++loadedIndex) {
                    auto& presetInSubset = subset[loadedIndex];
                    auto& presetInAll = allLoadedPresets[loadedIndexOffset + loadedIndex];

                    assert(presetInSubset.name == presetInAll.name);

                    auto indexAssignment = presetIndexMap.emplace(presetInSubset.name, nextPresetIndex.value);

                    if (indexAssignment.second) {
                        // This is a preset name we haven't seen before.
                        ++nextPresetIndex.value;
                        sparseIndexMap.resize(nextPresetIndex.value);
                    }

                    auto assignedIndex = indexAssignment.first->second;
                    sparseIndexMap[assignedIndex.value] = loadedIndexOffset + loadedIndex;

                    presetInSubset.assignedIndex = assignedIndex;
                    presetInAll.assignedIndex = assignedIndex;
                }

                loadedIndexOffset += loadedIndex;
            };

            assignSubset(nonBlacklistedPresets);
            assignSubset(blacklistedPresets);
        };

        assignIndexes(this->allFemalePresets, this->femalePresets, this->blacklistedFemalePresets,
                      this->femalePresetIndexByName, this->allFemalePresetsByIndex, this->nextFemalePresetIndex);
        assignIndexes(this->allMalePresets, this->malePresets, this->blacklistedMalePresets,
                      this->malePresetIndexByName, this->allMalePresetsByIndex, this->nextMalePresetIndex);

        logger::info("Assigned indexes to all the loaded presets.");
    }

    Preset* AssignedPresetIndex::GetPreset(bool actorIsFemale) const {
        auto& presetContainer{PresetContainer::GetInstance()};

        const auto& sparseMap =
            actorIsFemale ? presetContainer.allFemalePresetsByIndex : presetContainer.allMalePresetsByIndex;

        // If the actor's sex has not changed, then this index must be in bounds, but an actor's sex may have changed.
        if (value >= sparseMap.size()) {
            return nullptr;
        }

        auto denseIndex = sparseMap[value];
        auto& presets = actorIsFemale ? presetContainer.allFemalePresets : presetContainer.allMalePresets;

        // The user may have removed this preset since it was assigned,
        // if they did the sparse-map will have mapped it to -1.
        assert(denseIndex < presets.size() || denseIndex == -1);

        return denseIndex < presets.size() ? &presets[denseIndex] : nullptr;
    }

    std::string_view AssignedPresetIndex::GetPresetNameView(bool actorIsFemale) const {
        const auto preset = GetPreset(actorIsFemale);

        if (preset != nullptr) {
            return {preset->name.data(), preset->name.size()};
        }

        return {};
    }

    std::optional<Preset> GetPresetByName(const PresetSet& a_presetSet, const std::string_view a_name,
                                          const bool female, const bool a_ube) {
        logger::info("Looking for preset: {}", a_name);

        for (auto& preset : a_presetSet) {
            if (stl::cmp(preset.name, a_name)) return preset;
        }

        logger::info("Preset not found, choosing a random one.");
        const auto& container{PresetManager::PresetContainer::GetInstance()};
        return GetRandomPreset(female ? container.femalePresets : container.malePresets, a_ube);
    }

    std::optional<Preset> GetRandomPreset(const PresetSet& a_presetSet, const bool a_ube) {
        const auto fits = [=](const Preset& a_preset) { return a_preset.ube == a_ube; };

        std::size_t count{static_cast<std::size_t>(std::ranges::count_if(a_presetSet, fits))};
        if (count == 0) {
            return std::nullopt;
        }

        static_assert(std::is_same_v<decltype(0llu), decltype(count)>,
                      "Ensure that below literal is of type std::size_t");
        auto chosen{stl::random(0llu, count)};
        for (const auto& preset : a_presetSet) {
            if (fits(preset) && chosen-- == 0) {
                return preset;
            }
        }
        return std::nullopt;
    }

    std::optional<Preset> GetPresetByNameForRandom(const PresetSet& a_presetSet, const std::string_view a_name) {
        logger::info("Looking for preset: {}", a_name);

        for (const auto& preset : a_presetSet) {
            if (stl::cmp(preset.name, a_name)) {
                return preset;
            }
        }

        return {};
    }

    bool IsRefitPreset(const std::string_view a_name) {
        std::string lower{a_name};
        boost::algorithm::to_lower(lower);
        return lower.find("-refit") != std::string::npos;
    }

    std::string_view GetRefitBaseName(const std::string_view a_refitName) {
        std::string lower{a_refitName};
        boost::algorithm::to_lower(lower);
        const auto pos{lower.find("-refit")};
        if (pos == std::string::npos) {
            return {};
        }
        return a_refitName.substr(0, pos);
    }

    std::string_view SexRefitBaseName(const bool a_female, const bool a_ube) {
        if (a_ube) {
            return a_female ? "UBE Female"sv : "UBE Male"sv;
        }
        return a_female ? "Female"sv : "Male"sv;
    }

    const Preset* FindRefitPreset(const PresetSet& a_presets, const std::string_view a_name, const bool a_ube) {
        for (const auto& preset : a_presets) {
            if (!stl::cmp(preset.name, a_name)) continue;

            if (preset.ube != a_ube) {
                logger::info("Refit preset '{}' is {}made for UBE, and the actor's body {}, so it isn't used",
                             preset.name, preset.ube ? "" : "not ", a_ube ? "is UBE" : "isn't");
                return nullptr;
            }
            return &preset;
        }
        return nullptr;
    }

    SliderSet MakeRefitDifferential(const SliderSet& a_base, const SliderSet& a_refit) {
        constexpr float epsilon{0.00001F};

        // Slider names are matched case-insensitively
        boost::unordered_flat_map<std::string, const Slider*> baseByLowerName;
        baseByLowerName.reserve(a_base.size());
        for (const auto& slider : a_base | std::views::values) {
            std::string lower{slider.name};
            boost::algorithm::to_lower(lower);
            baseByLowerName[std::move(lower)] = &slider;
        }

        SliderSet differential;
        for (const auto& refitSlider : a_refit | std::views::values) {
            std::string lower{refitSlider.name};
            boost::algorithm::to_lower(lower);

            // A slider the base preset doesn't have is treated as 0
            const auto baseItr{baseByLowerName.find(lower)};
            const float baseMin{baseItr != baseByLowerName.end() ? baseItr->second->min : 0.0F};
            const float baseMax{baseItr != baseByLowerName.end() ? baseItr->second->max : 0.0F};

            // Refit - base: base 30 -> refit 32 gives +2, base 30 -> refit 28 gives -2
            const float deltaMin{refitSlider.min - baseMin};
            const float deltaMax{refitSlider.max - baseMax};

            if (std::abs(deltaMin) < epsilon && std::abs(deltaMax) < epsilon) {
                continue;
            }

            differential[refitSlider.name] = Slider{refitSlider.name.c_str(), deltaMin, deltaMax};
        }

        return differential;
    }

    std::optional<Preset> GetRandomPresetByName(const PresetSet& a_presetSet,
                                                std::vector<std::string_view> a_presetNames, const bool female,
                                                const bool a_ube) {
        if (a_presetNames.empty()) {
            logger::info("Preset names size is empty, returning none");
            return std::nullopt;
        }

        static_assert(std::is_same_v<decltype(0llu), decltype(a_presetNames.size())>,
                      "Ensure that below literal is of type std::size_t");
        const std::string_view chosenPreset{a_presetNames[stl::random(0llu, a_presetNames.size())]};

        std::optional<Preset> preset{GetPresetByNameForRandom(a_presetSet, chosenPreset)};

        // A UBE preset on a body that isn't UBE, or the other way around, has sliders that body doesn't have
        if (preset && preset->ube != a_ube) {
            logger::info("Preset {} is {}a UBE preset, and the actor {} a UBE body, skipping it", preset->name,
                         preset->ube ? "" : "not ", a_ube ? "has" : "doesn't have");
            preset.reset();
        }

        if (!preset.has_value()) {
            if (const auto iterator{std::ranges::find(a_presetNames, chosenPreset)}; iterator != a_presetNames.end()) {
                a_presetNames.erase(iterator);
            }

            return GetRandomPresetByName(a_presetSet, a_presetNames, female, a_ube);
        }

        return *preset;
    }

    bool IsFemalePreset(const Preset& a_preset) {
        constexpr std::array body{"himbo"sv, "talos"sv, "sam"sv, "sos"sv, "savren"sv};
        return !stl::contains(a_preset.body, body);
    }

    bool IsUBEPreset(const pugi::xml_node& a_node) {
        // Check whether it's a UBE preset by their sliders, since they have the naming convention "<Name> n|p"
        constexpr std::array ubeSliders{"n|p"sv, "p|n"sv};
        bool hasSliders{false};

        for (auto& node : a_node) {
            if (!stl::cmp(node.name(), "SetSlider")) continue;

            hasSliders = true;
            if (stl::contains(std::string_view{node.attribute("name").value()}, ubeSliders)) return true;
        }

        if (hasSliders) return false;

        // Presets without sliders, like UBE's "-Zeroed Sliders-", only have their groups
        // to determine whether they belong to UBE
        for (auto& node : a_node) {
            if (!stl::cmp(node.name(), "Group")) continue;

            const std::string_view group{node.attribute("name").value()};
            if (stl::cmp(group, "UBE"sv) || (group.size() > 4 && stl::cmp(group.substr(0, 4), "UBE "sv))) return true;
        }

        return false;
    }

    bool IsClothedSet(const std::string_view a_set) {
        constexpr std::array clothed{"cloth"sv, "outfit"sv, "nevernude"sv, "bikini"sv, "feet"sv,
                                     "hands"sv, "push"sv,   "cleavage"sv,  "armor"sv};
        return stl::contains(a_set, clothed);
    }

    bool IsClothedSet(const std::wstring_view a_set) {
        constexpr std::array clothed{L"cloth"sv, L"outfit"sv, L"nevernude"sv, L"bikini"sv, L"feet"sv,
                                     L"hands"sv, L"push"sv,   L"cleavage"sv,  L"armor"sv};
        return stl::contains(a_set, clothed);
    }

    SliderSet SliderSetFromNode(const pugi::xml_node& a_node, const BodyType a_body) {
        SliderSet ret;

        for (auto& node : a_node) {
            if (!stl::cmp(node.name(), "SetSlider")) continue;

            std::string_view name{node.attribute("name").value()};

            bool inverted{false};
            if (a_body == BodyType::UNP) {
                if (std::ranges::contains(DefaultSliders, name)) inverted = true;
            }

            float min{0}, max{0};
            const float val{node.attribute("value").as_float() / 100.0f};
            const auto size{node.attribute("size").value()};

            (stl::cmp(size, "big") ? max : min) = inverted ? 1.0f - val : val;

            AddSliderToSet(ret, Slider(name.data(), min, max), inverted);
        }

        return ret;
    }

    void AddSliderToSet(SliderSet& a_sliderSet, Slider&& a_slider, [[maybe_unused]] bool a_inverted) {
        if (const auto it = a_sliderSet.find(a_slider.name); it != a_sliderSet.end()) {
            constexpr float val{};
            auto& current = it->second;
            if ((current.min == val) && (a_slider.min != val)) current.min = a_slider.min;
            if ((current.max == val) && (a_slider.max != val)) current.max = a_slider.max;
        } else {
            a_sliderSet[a_slider.name] = std::move(a_slider);
        }
    }

    BodyType GetBodyType(const std::string_view a_body) {
        constexpr std::array unp{"unp"sv, "coco"sv, "bhunp"sv, "uunp"sv};
        return stl::contains(a_body, unp) ? BodyType::UNP : BodyType::CBBE;
    }
}  // namespace PresetManager
