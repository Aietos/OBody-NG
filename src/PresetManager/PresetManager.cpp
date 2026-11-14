#include "PresetManager/PresetManager.h"

#include "JSONParser/JSONParser.h"
#include "STL.h"

PresetManager::PresetContainer PresetManager::PresetContainer::instance;

namespace PresetManager {
    // UNP's inverted sliders, all of them defaulting to 100, so their morph is 1 - the preset's value
    constexpr auto InvertedUNPSliders =
        stl::to_set<std::string_view, 11>({"Breasts", "BreastsSmall", "NippleDistance", "NippleSize", "ButtCrack",
                                           "Butt", "ButtSmall", "Legs", "Arms", "ShoulderWidth", "Ankles"});

    // Sliders whose default in the body's slider sets isn't 0. BodySlide builds the meshes, zeroed ones included,
    // with these defaults for every size a preset doesn't set, and morphs are added on top of the built mesh,
    // so a morph has to be the preset's value minus the default
    struct DefaultSlider {
        std::string_view name;
        float smallValue;
        float bigValue;
    };

    constexpr std::array CBBEDefaultSliders{DefaultSlider{"AnkleSize"sv, 1.0f, 0.0f},
                                            DefaultSlider{"WristSize"sv, 1.0f, 0.0f},
                                            DefaultSlider{"NeckSeam"sv, 1.0f, 0.0f}};
    constexpr std::array UNPDefaultSliders{DefaultSlider{"Neck"sv, 1.0f, 0.0f}};
    constexpr std::array UBEDefaultSliders{DefaultSlider{"NipplesShowUp"sv, 1.0f, 1.0f},
                                           DefaultSlider{"SkinnyMorph"sv, 1.0f, 0.0f}};
    // Necoco is built over UBE, with NipplesShowUp at 0
    constexpr std::array NecocoDefaultSliders{DefaultSlider{"SkinnyMorph"sv, 1.0f, 0.0f}};

    std::span<const DefaultSlider> GetDefaultSliders(const BodyType a_body) {
        switch (a_body) {
            case BodyType::UNP:
                return UNPDefaultSliders;
            case BodyType::UBE:
                return UBEDefaultSliders;
            case BodyType::Necoco:
                return NecocoDefaultSliders;
            default:
                return CBBEDefaultSliders;
        }
    }

    PresetContainer& PresetContainer::GetInstance() { return instance; }

    PresetContainer::PresetContainer() {
        allData_.push_back(std::make_unique<PresetData>());
        data_ = allData_.back().get();
    }

    // Only called at startup and from the menu, never at the same time
    void PresetContainer::Replace(std::unique_ptr<PresetData> a_data) {
        allData_.push_back(std::move(a_data));
        data_ = allData_.back().get();
    }

    // Necoco builds to UBE's body files, and its own sliders are named "Necoco_..."
    bool HasNecocoFemaleBody() {
        RE::BSResourceNiBinaryStream stream{R"(Meshes\!UBE\Body\femalebody_tangent.tri)"};

        if (!stream.good()) return false;

        constexpr std::string_view marker{"Necoco_"};
        std::string data;
        char buffer[65536];

        while (true) {
            std::uint64_t read{0};
            stream.stream->DoRead(buffer, sizeof(buffer), read);

            if (read == 0) return false;

            data.append(buffer, static_cast<std::size_t>(read));

            if (data.find(marker) != std::string::npos) return true;

            // Kept so a name split between two reads is still found
            if (data.size() >= marker.size()) {
                data.erase(0, data.size() - marker.size() + 1);
            }
        }
    }

    std::unique_ptr<PresetData> GeneratePresets() {
        const fs::path root_path(R"(Data\CalienteTools\BodySlide\SliderPresets)");

        auto data{std::make_unique<PresetData>()};
        auto& allFemalePresets{data->allFemalePresets};
        auto& allMalePresets{data->allMalePresets};

        std::vector<std::unique_ptr<pugi::xml_document>> docs;
        std::vector<pugi::xml_node> nodes;

        for (const auto& entry : fs::directory_iterator(root_path)) {
            const auto& path{entry.path()};
            if (path.extension().c_str() != L".xml"sv) continue;
            // Files with a "clothed" keyword in their name are still opened, but only their "-Refit" presets are kept.
            const bool clothedFile{IsClothedSet(path.wstring())};

            auto doc{std::make_unique<pugi::xml_document>()};
            if (auto result = doc->load_file(path.c_str(), pugi::parse_default, pugi::encoding_auto); !result) {
                wchar_t buffer[2048];
                swprintf_s(buffer, std::size(buffer), L"load failed: %s [%hs]", path.c_str(), result.description());
                SPDLOG_WARN(buffer);
                const auto name{path.filename().u8string()};
                data->unreadFiles.emplace_back(name.begin(), name.end());
                continue;
            }

            for (const auto& node : doc->child("SliderPresets")) {
                if (clothedFile && !IsRefitPreset(node.attribute("name").value())) continue;

                nodes.push_back(node);
            }

            docs.push_back(std::move(doc));
        }

        const bool necoco{HasNecocoFemaleBody()};

        if (necoco) {
            logger::info("The female UBE body is Necoco, female UBE presets will use its slider defaults");
        }

        PresetBodyTypes baseBodies;

        for (const auto& node : nodes) {
            const std::string_view name{node.attribute("name").value()};

            if (IsRefitPreset(name) || IsClothedSet(name)) continue;

            std::string lower{name};
            boost::algorithm::to_lower(lower);
            const auto body{IsUBEPreset(node) ? BodyType::UBE : GetPresetBodyType(node, node.attribute("set").value())};
            baseBodies.try_emplace(std::move(lower), body);
        }

        for (const auto& node : nodes) {
            auto preset = GeneratePreset(node, baseBodies);
            if (!preset) continue;

            // The same UBE presets shape both sexes, so they go in both lists
            if (preset->ube || IsFemalePreset(*preset)) {
                allFemalePresets.push_back(*preset);

                if (preset->ube && necoco) {
                    allFemalePresets.back().sliders = SliderSetFromNode(node, BodyType::Necoco);
                }
            }
            if (preset->ube || !IsFemalePreset(*preset)) {
                allMalePresets.push_back(*preset);
            }
        }

        for (const PresetSet* const presets : {&allFemalePresets, &allMalePresets}) {
            for (const auto& preset : *presets) {
                if (!IsRefitPreset(preset.name)) continue;

                for (const auto& sliderName : preset.sliders | std::views::keys) {
                    data->refitSliderNames.insert(sliderName);
                }

                // Differential refits also reset the base preset's sliders
                const auto baseName{GetRefitBaseName(preset.name)};
                const auto base{std::ranges::find_if(*presets, [&](const Preset& a_preset) {
                    return !IsRefitPreset(a_preset.name) && stl::cmp(a_preset.name, baseName);
                })};

                if (base == presets->end()) continue;

                for (const auto& sliderName : base->sliders | std::views::keys) {
                    data->refitSliderNames.insert(sliderName);
                }
            }
        }

        logger::info("Read {} female presets and {} male presets, UBE ones in both: {}", allFemalePresets.size(),
                     allMalePresets.size(), std::ranges::count_if(allFemalePresets, &Preset::ube));

        return data;
    }

    bool PresetContainer::HasPresetsFor(const bool a_female, const bool a_ube) const {
        return Parser::JSONParser::GetInstance().GetConfig()->HasPresetsFor(a_female, a_ube);
    }

    BodyType GetPresetBodyType(const pugi::xml_node& a_node, const std::string_view a_set) {
        constexpr std::array cbbe{"cbbe"sv, "3ba"sv};

        if (GetBodyType(a_set) == BodyType::UNP) return BodyType::UNP;

        if (stl::contains(a_set, cbbe)) return BodyType::CBBE;

        for (auto& node : a_node) {
            if (!stl::cmp(node.name(), "Group")) continue;

            if (GetBodyType(node.attribute("name").value()) == BodyType::UNP) return BodyType::UNP;
        }

        return BodyType::CBBE;
    }

    std::optional<Preset> GeneratePreset(const pugi::xml_node& a_node, const PresetBodyTypes& a_baseBodies) {
        const std::string_view name{a_node.attribute("name").value()};

        if (IsClothedSet(name) && !IsRefitPreset(name)) return {};

        const std::string_view body{a_node.attribute("set").value()};

        const bool ube{IsUBEPreset(a_node)};
        auto bodyType{ube ? BodyType::UBE : GetPresetBodyType(a_node, body)};

        // A differential refit must be read for the same body as its base preset
        if (!ube && IsRefitPreset(name)) {
            std::string baseName{GetRefitBaseName(name)};
            boost::algorithm::to_lower(baseName);

            if (const auto it{a_baseBodies.find(baseName)};
                it != a_baseBodies.end() && it->second != BodyType::UBE && it->second != bodyType) {
                logger::info("Refit preset {} is read as {}, like its base preset (its set '{}' and groups tell {})",
                             name, it->second == BodyType::UNP ? "UNP" : "CBBE", body,
                             bodyType == BodyType::UNP ? "UNP" : "CBBE");
                bodyType = it->second;
            }
        }

        Preset preset{name.data(), body.data(), SliderSetFromNode(a_node, bodyType)};
        preset.ube = ube;

        return preset;
    }

    void PresetContainer::AssignPresetIndexes(PresetData& a_data) {
        auto assignIndexes = [&](PresetSet& allLoadedPresets, auto& presetIndexMap, auto& sparseIndexMap,
                                 auto& nextPresetIndex) {
            // We ensure that absent presets have an index of -1 to signify their absence.
            sparseIndexMap.resize(0);
            sparseIndexMap.resize(nextPresetIndex.value, -1);

            for (size_t loadedIndex = 0; loadedIndex < allLoadedPresets.size(); ++loadedIndex) {
                auto& preset = allLoadedPresets[loadedIndex];
                auto indexAssignment = presetIndexMap.emplace(preset.name, nextPresetIndex.value);

                if (indexAssignment.second) {
                    // This is a preset name we haven't seen before.
                    ++nextPresetIndex.value;
                    sparseIndexMap.resize(nextPresetIndex.value);
                }

                auto assignedIndex = indexAssignment.first->second;
                sparseIndexMap[assignedIndex.value] = loadedIndex;

                preset.assignedIndex = assignedIndex;
            }
        };

        assignIndexes(a_data.allFemalePresets, femalePresetIndexByName, a_data.allFemalePresetsByIndex,
                      nextFemalePresetIndex);
        assignIndexes(a_data.allMalePresets, malePresetIndexByName, a_data.allMalePresetsByIndex, nextMalePresetIndex);

        logger::info("Assigned indexes to all the loaded presets.");
    }

    Preset* AssignedPresetIndex::GetPreset(bool actorIsFemale) const {
        auto& data{PresetContainer::GetInstance().Data()};

        const auto& sparseMap = actorIsFemale ? data.allFemalePresetsByIndex : data.allMalePresetsByIndex;

        // If the actor's sex has not changed, then this index must be in bounds, but an actor's sex may have changed.
        if (value >= sparseMap.size()) {
            return nullptr;
        }

        auto denseIndex = sparseMap[value];
        auto& presets = actorIsFemale ? data.allFemalePresets : data.allMalePresets;

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
        const auto config{Parser::JSONParser::GetInstance().GetConfig()};

        return GetRandomPreset(config->GetPresetLists(female).allowed, a_ube);
    }

    std::optional<Preset> GetRandomPreset(const PresetPointers& a_presets, const bool a_ube) {
        const auto fits = [=](const Preset* a_preset) { return a_preset->ube == a_ube; };

        std::size_t count{static_cast<std::size_t>(std::ranges::count_if(a_presets, fits))};
        if (count == 0) {
            return std::nullopt;
        }

        static_assert(std::is_same_v<decltype(0llu), decltype(count)>,
                      "Ensure that below literal is of type std::size_t");
        auto chosen{stl::random(0llu, count)};
        for (const auto* const preset : a_presets) {
            if (fits(preset) && chosen-- == 0) {
                return *preset;
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
                logger::info(
                    "Refit preset '{}' is {}made for UBE, and it would refit a body or preset that {}, so it "
                    "isn't used",
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
        boost::unordered_flat_set<std::string> refitLowerNames;
        refitLowerNames.reserve(a_refit.size());

        for (const auto& refitSlider : a_refit | std::views::values) {
            std::string lower{refitSlider.name};
            boost::algorithm::to_lower(lower);

            // A slider the base preset doesn't have is treated as 0
            const auto baseItr{baseByLowerName.find(lower)};
            refitLowerNames.insert(std::move(lower));
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

        for (const auto& [lower, baseSlider] : baseByLowerName) {
            if (refitLowerNames.contains(lower)) continue;

            if (std::abs(baseSlider->min) < epsilon && std::abs(baseSlider->max) < epsilon) continue;

            differential[baseSlider->name] = Slider{baseSlider->name.c_str(), -baseSlider->min, -baseSlider->max};
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
                if (std::ranges::contains(InvertedUNPSliders, name)) inverted = true;
            }

            float min{0}, max{0};
            float val{node.attribute("value").as_float() / 100.0f};
            const auto size{node.attribute("size").value()};
            const bool big{stl::cmp(size, "big")};

            // A size the preset doesn't set keeps the default, so its morph stays 0
            const auto defaults{GetDefaultSliders(a_body)};
            if (const auto it{std::ranges::find(defaults, name, &DefaultSlider::name)}; it != defaults.end()) {
                const float def{big ? it->bigValue : it->smallValue};
                logger::info("Preset {}: {} {} is {}, the body's default is {}, so its morph is {}",
                             a_node.attribute("name").value(), name, size, val, def, val - def);
                val -= def;
            }

            (big ? max : min) = inverted ? 1.0f - val : val;

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
