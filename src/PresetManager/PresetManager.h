#pragma once

namespace PresetManager {
    enum class BodyType : int32_t { CBBE, UNP };

    struct Slider {
        Slider() = default;
        Slider(std::string a_name, float a_val) : name(std::move(a_name)), min(a_val), max(a_val) {}
        Slider(std::string a_name, float a_min, float a_max) : name(std::move(a_name)), min(a_min), max(a_max) {}
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
        Preset(std::string a_name) : name(std::move(a_name)) {}
        Preset(std::string a_name, std::string a_body) : name(std::move(a_name)), body(std::move(a_body)) {}
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
    bool IsClothedSet(std::string a_set);

    Preset GetPresetByName(PresetSet a_presetSet, std::string a_name, bool female);
    Preset GetRandomPreset(PresetSet a_presetSet);
    Preset GetRandomPresetByName(const PresetSet& a_presetSet, std::vector<std::string_view> a_presetNames, bool female);

    Preset GetPresetByNameForRandom(const PresetSet& a_presetSet, std::string a_name, bool female);

    void GeneratePresets();
    std::optional<Preset> GeneratePreset(pugi::xml_node a_node);

    SliderSet SliderSetFromNode(const pugi::xml_node& a_node, BodyType a_body);
    void AddSliderToSet(SliderSet& a_sliderSet, Slider&& a_slider, bool a_inverted = false);

    BodyType GetBodyType(std::string_view a_body);
}  // namespace PresetManager
