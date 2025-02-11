#pragma once

namespace PresetManager {
    enum class BodyType { CBBE, UNP };

    struct Slider {
        Slider() = default;
        Slider(const char* a_name, const float a_val) : name(a_name), min(a_val), max(a_val) {}
        Slider(const char* a_name, float const a_min, const float a_max) : name(a_name), min(a_min), max(a_max) {}
        ~Slider() = default;

        Slider(const Slider& a_other) = default;
        Slider(Slider&& a_other) = default;

        Slider& operator=(const Slider& a_other) = default;
        Slider& operator=(Slider&& a_other) = default;

        std::string name;
        float min = 0.f;
        float max = 0.f;
    };

    using SliderSet = std::unordered_map<std::string, Slider>;

    struct Preset {
        Preset() = default;
        explicit Preset(const char* a_name) : name(a_name) {}
        Preset(const char* a_name, const char* a_body, SliderSet&& a_sliders)
            : name(a_name), body(a_body), sliders(std::move(a_sliders)) {}
        ~Preset() = default;

        std::string name;
        std::string body;
        SliderSet sliders;
    };

    using PresetSet = std::vector<Preset>;

    class PresetContainer {
    public:
        PresetContainer(PresetContainer&&) = delete;
        PresetContainer(const PresetContainer&) = delete;

        PresetContainer& operator=(PresetContainer&&) = delete;
        PresetContainer& operator=(const PresetContainer&) = delete;

        std::vector<std::string> defaultSliders;

        PresetSet femalePresets;
        PresetSet malePresets;

        PresetSet blacklistedFemalePresets;
        PresetSet blacklistedMalePresets;

        PresetSet allFemalePresets;
        PresetSet allMalePresets;

        static PresetContainer& GetInstance();

    private:
        static PresetContainer instance;

        PresetContainer() = default;
    };

    bool IsFemalePreset(const Preset& a_preset);
    bool IsClothedSet(std::string_view a_set);
    bool IsClothedSet(std::wstring_view a_set);

    Preset GetPresetByName(const PresetSet& a_presetSet, std::string_view a_name, bool female);
    Preset GetRandomPreset(const PresetSet& a_presetSet);
    Preset GetRandomPresetByName(const PresetSet& a_presetSet, std::vector<std::string_view> a_presetNames,
                                 bool female);

    std::optional<Preset> GetPresetByNameForRandom(const PresetSet& a_presetSet, std::string_view a_name);

    void GeneratePresets();
    std::optional<Preset> GeneratePreset(const pugi::xml_node& a_node);

    SliderSet SliderSetFromNode(const pugi::xml_node& a_node, BodyType a_body);
    void AddSliderToSet(SliderSet& a_sliderSet, Slider&& a_slider, bool a_inverted = false);

    BodyType GetBodyType(std::string_view a_body);
}  // namespace PresetManager
