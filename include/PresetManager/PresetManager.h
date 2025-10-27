#pragma once

#include <boost/unordered/unordered_flat_map.hpp>

namespace PresetManager {
    enum class BodyType { CBBE, UNP };
#pragma push_macro("max")
#pragma push_macro("min")
#undef max
#undef min
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
#pragma pop_macro("max")
#pragma pop_macro("min")

    using SliderSet = boost::unordered_flat_map<std::string, Slider>;

    struct Preset;

    // We can refer to presets by their index rather than their name.
    // We do this to reduce the memory usage for keeping track of which preset is assigned to each actor--
    // the memory footprint isn't a concern while the game is running, instead it's a concern for the
    // game's save files: the bigger the state for tracking preset assignment is, the bigger save files are,
    // and thus the longer it takes to save and load a game--not good for the players.
    //
    // As a bonus: integers are much easier to handle atomically than strings,
    // making thread-safety much more easily (and performantly) achievable.
    //
    // There is, as always, a downside however, to be able to refer to presets by index
    // we need to assign an index to each preset, and we need to keep that index consistent
    // between game saves and loads, even if the player installs or removes presets mid-game.
    // The way we do this is relatively simple: the first time we encounter a preset, we assign it an index.
    // When we save the game, we store to our SKSE cosave the preset index assignments,
    // and when we load the game we read them back from our SKSE cosave.
    //
    // The indexes we assign to preset names simply increment by one with each new preset name.
    // Each index is a 20-bit value, so we simply don't worry about running out of indexes.
    //
    // Note that because we keep preset indexes stable even if a player removes a preset, the preset indexes
    // are thus sparse when it comes to accessing a contiguous sequence of usable presets.
    //
    // We keep the preset indexes stable for removed presets to gracefully handle the event of a player
    // accidentally removing a preset and not realising until after they've played for a bit and saved a few times.
    // It would make for a poor UX if we clobbered their preset assignment in that scenario.
    struct AssignedPresetIndex {
        // This permits a player to have 1,048,576 BodySlide presets, per sex, active at a time.
        // That should be enough, surely?
        static constexpr uint32_t BitWidth = 20;

        uint32_t value = 0;

        [[nodiscard]] Preset* GetPreset(bool actorIsFemale) const;
        [[nodiscard]] std::string_view GetPresetNameView(bool actorIsFemale) const;
    };

    using SparsePresetIndex = uint32_t;

    struct Preset {
        Preset() = default;
        explicit Preset(const char* a_name) : name(a_name) {}
        Preset(const char* a_name, const char* a_body, SliderSet&& a_sliders)
            : name(a_name), body(a_body), sliders(std::move(a_sliders)) {}
        ~Preset() = default;

        std::string name;
        std::string body;
        SliderSet sliders;
        AssignedPresetIndex assignedIndex;
    };

    using PresetSet = std::vector<Preset>;
    using SparsePresetMapping = std::vector<SparsePresetIndex>;

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

        /* These map a sparse preset index to the dense storage of the presets proper.
           A value of -1 is used to signify the absence of a preset.
           If you're thinking that a hashtable may be more appropriate for this than an array,
           consider that we expect this be very dense, so dense that using a hashtable would
           likely use more memory in exchange for more expensive lookups. */
        SparsePresetMapping allFemalePresetsByIndex;
        SparsePresetMapping allMalePresetsByIndex;
        /* These maps are intended for preset index assignment,
           not general lookups, which are generally case-insensitive. */
        boost::unordered_flat_map<std::string, AssignedPresetIndex> femalePresetIndexByName;
        boost::unordered_flat_map<std::string, AssignedPresetIndex> malePresetIndexByName;
        AssignedPresetIndex nextFemalePresetIndex;
        AssignedPresetIndex nextMalePresetIndex;

        void AssignPresetIndexes();

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
    std::optional<Preset> GetRandomPresetByName(const PresetSet& a_presetSet, std::vector<std::string_view> a_presetNames,
                                 bool female);

    std::optional<Preset> GetPresetByNameForRandom(const PresetSet& a_presetSet, std::string_view a_name);

    void GeneratePresets();
    std::optional<Preset> GeneratePreset(const pugi::xml_node& a_node);

    SliderSet SliderSetFromNode(const pugi::xml_node& a_node, BodyType a_body);
    void AddSliderToSet(SliderSet& a_sliderSet, Slider&& a_slider, bool a_inverted = false);

    BodyType GetBodyType(std::string_view a_body);
}  // namespace PresetManager
