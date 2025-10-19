#pragma once

namespace stl {
    inline bool contains(const std::string_view a_text, const std::string_view a_sub) {
        return boost::algorithm::icontains(a_text, a_sub);
    }

    inline bool contains(const std::wstring_view a_text, const std::wstring_view a_sub) {
        return boost::algorithm::icontains(a_text, a_sub);
    }

    template <class T, std::size_t N>
    bool contains(const std::string_view a_text, std::array<T, N> const& a_subs) {
        for (auto& sub : a_subs) {
            if (contains(a_text, sub)) return true;
        }

        return false;
    }

    template <class T, std::size_t N>
    bool contains(std::wstring_view const a_text, std::array<T, N> const& a_subs) {
        for (auto& sub : a_subs) {
            if (contains(a_text, sub)) return true;
        }

        return false;
    }

    inline bool cmp(const std::string_view a_str1, const std::string_view a_str2) {
        return boost::algorithm::iequals(a_str1, a_str2);
    }

    // ReSharper disable once CppNotAllPathsReturnValue
    template <class T>
        requires std::is_integral_v<T> || std::is_floating_point_v<T>
    T random(T min, T max) {
        // non-inclusive i.e., [min, max)
        if (min >= max) {
            char errorMessage[256];
            if constexpr (std::is_floating_point_v<T>) {
                sprintf_s(errorMessage, std::size(errorMessage),
                          "The Value of min: '%f' must be lesser than the value of max: '%f'", min,
                          max);  // max length possible: 153
            } else {
                sprintf_s(errorMessage, std::size(errorMessage),
                          "The Value of min: '%lld' must be lesser than the value of max: '%lld'",
                          static_cast<long long>(min), static_cast<long long>(max));  // max length possible: 99
            }
            throw std::invalid_argument(errorMessage);
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        if constexpr (std::is_integral_v<T>) {
            std::uniform_int_distribution<T> distrib(min, max - 1);
            return distrib(gen);
        } else if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> distrib(min, std::nextafter(max, min));
            return distrib(gen);
        }
    }

    inline bool chance(const int a_chance) {
        const auto roll = random(0.0f, 99.0f);
        return roll <= static_cast<float>(a_chance);
    }

    template <typename T, std::size_t N>
    constexpr std::array<T, N> to_set(std::initializer_list<T> const& input) {
        std::array<T, N> elements{};
        std::size_t size = 0;

        for (auto& value : input) {
            if (std::find(elements.begin(), elements.begin() + size, value) == elements.begin() + size) {
                elements[size++] = value;
            }
        }

        if (size != N) throw std::out_of_range("Not the smallest possible set");
        return elements;
    }

    using PO3_tweaks_GetFormEditorID = const char* (*)(std::uint32_t);  // NOLINT(*-reserved-identifier)
    static PO3_tweaks_GetFormEditorID func{};

    inline std::string get_editorID(const RE::TESForm* a_form) {
        switch (a_form->GetFormType()) {
            case RE::FormType::Keyword:
            case RE::FormType::LocationRefType:
            case RE::FormType::Action:
            case RE::FormType::MenuIcon:
            case RE::FormType::Global:
            case RE::FormType::HeadPart:
            case RE::FormType::Race:
            case RE::FormType::Sound:
            case RE::FormType::Script:
            case RE::FormType::Navigation:
            case RE::FormType::Cell:
            case RE::FormType::WorldSpace:
            case RE::FormType::Land:
            case RE::FormType::NavMesh:
            case RE::FormType::Dialogue:
            case RE::FormType::Quest:
            case RE::FormType::Idle:
            case RE::FormType::AnimatedObject:
            case RE::FormType::ImageAdapter:
            case RE::FormType::VoiceType:
            case RE::FormType::Ragdoll:
            case RE::FormType::DefaultObject:
            case RE::FormType::MusicType:
            case RE::FormType::StoryManagerBranchNode:
            case RE::FormType::StoryManagerQuestNode:
            case RE::FormType::StoryManagerEventNode:
            case RE::FormType::SoundRecord:
                return a_form->GetFormEditorID();
            default: {
                if (func) [[likely]] {
                    return func(a_form->formID);
                }
                return {};
            }
        }
    }

    inline void RemoveDuplicatesInJsonArray(rapidjson::Value& json_array, rapidjson::Value::AllocatorType& allocator) {
        if (!json_array.IsArray()) return;

        std::unordered_set<std::string_view> seen;
        rapidjson::Value uniqueArray(rapidjson::kArrayType);

        for (auto& item : json_array.GetArray()) {
            if (item.IsString()) {
                if (const std::string_view strValue{item.GetString()};
                    strValue.data() && seen.insert(strValue).second) {
                    uniqueArray.PushBack(item, allocator);
                }
            }
        }

        json_array.Swap(uniqueArray);
    }


    class FilePtrManager {
    public:
        explicit FilePtrManager(const char* path, const char* mode = "rb") noexcept;

        explicit FilePtrManager(const wchar_t* path, const wchar_t* mode = L"rb") noexcept;

        ~FilePtrManager();

        FilePtrManager(const FilePtrManager&) = delete;
        FilePtrManager& operator=(const FilePtrManager&) = delete;

        FilePtrManager(FilePtrManager&& other) noexcept;

        FilePtrManager& operator=(FilePtrManager&& other) noexcept;

        [[nodiscard]] FILE* get() noexcept;
        [[nodiscard]] FILE* get() const noexcept;
        [[nodiscard]] errno_t error() const noexcept;

    private:
        FILE* fp{};
        errno_t err{};
    };

    class timeit {
    public:
        explicit timeit(const std::source_location& a_curr = std::source_location::current()) : curr(a_curr) {}

        ~timeit();

    private:
        std::source_location curr;
        std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
    };
}  // namespace stl
