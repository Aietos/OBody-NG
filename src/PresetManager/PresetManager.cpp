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
            if (IsClothedSet(path.wstring())) continue;

            pugi::xml_document doc;
            if (auto result = doc.load_file(path.c_str(), pugi::parse_default, pugi::encoding_auto); !result) {
                wchar_t buffer[1024];
                swprintf_s(buffer, std::size(buffer), L"load failed: %s [%hs]", path.c_str(), result.description());
                SPDLOG_WARN(buffer);
                parser.invalid_presets++;
                continue;
            }

            auto presets = doc.child("SliderPresets");
            for (const auto& node : presets) {
                auto preset = GeneratePreset(node);
                if (!preset) continue;

                if (IsFemalePreset(*preset)) {
                    if (std::find(blacklistedPresetsBegin, blacklistedPresetsEnd, preset.value().name.c_str()) !=
                        blacklistedPresetsEnd) {
                        blacklistedFemalePresets.push_back(*preset);
                    } else {
                        femalePresets.push_back(*preset);
                    }
                } else {
                    if (std::find(blacklistedPresetsBegin, blacklistedPresetsEnd, preset.value().name.c_str()) !=
                        blacklistedPresetsEnd) {
                        blacklistedMalePresets.push_back(*preset);
                    } else {
                        malePresets.push_back(*preset);
                    }
                }
            }
        }

        allFemalePresets = femalePresets;
        allFemalePresets.insert_range(allFemalePresets.end(), blacklistedFemalePresets);

        allMalePresets = malePresets;
        allMalePresets.insert_range(allMalePresets.end(), blacklistedMalePresets);

        logger::info("Female presets: {}, Male presets: {}", femalePresets.size(), malePresets.size());
        logger::info("Blacklisted: Female presets: {}, Male Presets: {}", blacklistedFemalePresets.size(),
                     blacklistedMalePresets.size());
    }

    std::optional<Preset> GeneratePreset(const pugi::xml_node& a_node) {
        const std::string_view name{a_node.attribute("name").value()};

        if (IsClothedSet(name)) return {};

        const std::string_view body{a_node.attribute("set").value()};

        return Preset{name.data(), body.data(), SliderSetFromNode(a_node, GetBodyType(body))};
    }

    Preset GetPresetByName(const PresetSet& a_presetSet, const std::string_view a_name, const bool female) {
        logger::info("Looking for preset: {}", a_name);

        for (auto& preset : a_presetSet) {
            if (stl::cmp(preset.name, a_name)) return preset;
        }

        logger::info("Preset not found, choosing a random one.");
        const auto& container{PresetManager::PresetContainer::GetInstance()};
        return GetRandomPreset(female ? container.femalePresets : container.malePresets);
    }

    Preset GetRandomPreset(const PresetSet& a_presetSet) {
        static_assert(std::is_same_v<decltype(0llu), decltype(a_presetSet.size())>,
                      "Ensure that below literal is of type std::size_t");
        return a_presetSet[stl::random(0llu, a_presetSet.size())];
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

    Preset GetRandomPresetByName(const PresetSet& a_presetSet, std::vector<std::string_view> a_presetNames,
                                 const bool female) {
        if (a_presetNames.empty()) {
            logger::info("Preset names size is empty, returning a random one");
            const auto& container{PresetManager::PresetContainer::GetInstance()};
            return GetRandomPreset(female ? container.femalePresets : container.malePresets);
        }

        static_assert(std::is_same_v<decltype(0llu), decltype(a_presetNames.size())>,
                      "Ensure that below literal is of type std::size_t");
        const std::string_view chosenPreset{a_presetNames[stl::random(0llu, a_presetNames.size())]};

        const std::optional<Preset> preset{GetPresetByNameForRandom(a_presetSet, chosenPreset)};

        if (!preset.has_value()) {
            if (const auto iterator{std::ranges::find(a_presetNames, chosenPreset)}; iterator != a_presetNames.end()) {
                a_presetNames.erase(iterator);
            }

            return GetRandomPresetByName(a_presetSet, a_presetNames, female);
        }

        return *preset;
    }

    bool IsFemalePreset(const Preset& a_preset) {
        constexpr std::array body{"himbo"sv, "talos"sv, "sam"sv, "sos"sv, "savren"sv};
        return !stl::contains(a_preset.body, body);
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
