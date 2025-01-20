#pragma once

namespace stl
{
    static bool contains(std::string_view const a_text, std::string_view const a_sub)
    {
        if (a_sub.length() > a_text.length()) return false;

        auto it = std::ranges::search(a_text, a_sub, [](const char ch1, const char ch2)
        {
            return std::toupper(ch1) == std::toupper(ch2);
        }).begin();

        return it != a_text.end();
    }

    template <class T, std::size_t N>
    static bool contains(std::string_view const a_text, std::array<T, N> const& a_subs)
    {
        for (auto& sub : a_subs)
        {
            if (contains(a_text, sub)) return true;
        }

        return false;
    }

    static bool cmp(const std::string_view a_str1, const std::string_view a_str2)
    {
        return std::ranges::equal(a_str1, a_str2, [](const char a, const char b) { return tolower(a) == tolower(b); });
    }

    // static bool cmp(const char* a_str1, const char* a_str2) { return cmp(std::string{a_str1}, std::string{a_str2}); }

    // ReSharper disable once CppNotAllPathsReturnValue
    template <class T>
        requires std::is_integral_v<T> || std::is_floating_point_v<T>
    T random(T min, T max)
    {
        // non-inclusive i.e., [min, max)
        if (min >= max)
        {
            char errorMessage[256];
            if constexpr (std::is_floating_point_v<T>)
            {
                sprintf_s(errorMessage, std::size(errorMessage),
                          "The Value of min: '%f' must be lesser than the value of max: '%f'", min, max);
            }
            else
            {
                sprintf_s(errorMessage, std::size(errorMessage),
                          "The Value of min: '%lld' must be lesser than the value of max: '%lld'",
                          static_cast<long long>(min), static_cast<long long>(max));
            }
            throw std::invalid_argument(errorMessage);
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        if constexpr (std::is_integral_v<T>)
        {
            std::uniform_int_distribution<T> distrib(min, max - 1);
            return distrib(gen);
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            std::uniform_real_distribution<T> distrib(min, max);
            T res = distrib(gen);
            while (res == max) res = distrib(gen); // to get around rounding issue with floating point numbers
            return res;
        }
    }

    static bool chance(int a_chance)
    {
        auto roll = random(0.0f, 99.0f);
        return roll <= static_cast<float>(a_chance);
    }

    template <typename T, std::size_t N>
    constexpr std::array<T, N> to_set(std::initializer_list<T> const& input)
    {
        std::array<T, N> elements{};
        std::size_t size = 0;

        for (auto& value : input)
        {
            if (std::find(elements.begin(), elements.begin() + size, value) == elements.begin() + size)
            {
                if (size >= N) throw "Set is full, not enough space";
                elements[size++] = value;
            }
        }

        if (size != N) throw std::out_of_range("Not the smallest possible set");
        return elements;
    }

    inline std::string get_editorID(const RE::TESForm* a_form)
    {
        using _GetFormEditorID = const char* (*)(std::uint32_t);
        switch (a_form->GetFormType())
        {
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
        default:
            {
                static auto tweaks = GetModuleHandle(L"po3_Tweaks");
                static auto func = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));
                if (func)
                {
                    return func(a_form->formID);
                }
                return {};
            }
        }
    }

    static void RemoveDuplicatesInJsonArray(nlohmann::ordered_json& json_array)
    {
        std::unordered_set<nlohmann::ordered_json> seen;
        json_array.erase(
            std::ranges::remove_if(
                json_array,
                [&seen](const nlohmann::ordered_json& item)
                {
                    return !seen.insert(item).second; // Keep only unseen items
                }
            ).begin(),
            json_array.end()
        );
    }
} // namespace stl
