#include "UI/GamepadCamera.h"
#include "SKSEMenuFramework.h"
#include "UI/Translations.h"
#include "UI/UI.h"
#include "Preview/Preview.h"

namespace {
    struct Button {
        ImGuiMCP::ImGuiKey key;
        std::uint32_t macro;
        const char* name;
    };

    constexpr Button kRB{ImGuiMCP::ImGuiKey_GamepadR1, SKSE::InputMap::kGamepadButtonOffset_RIGHT_SHOULDER, "RB"};
    constexpr Button kLB{ImGuiMCP::ImGuiKey_GamepadL1, SKSE::InputMap::kGamepadButtonOffset_LEFT_SHOULDER, "LB"};
    constexpr Button kR3{ImGuiMCP::ImGuiKey_GamepadR3, SKSE::InputMap::kGamepadButtonOffset_RIGHT_THUMB, "R3"};
    constexpr Button kL3{ImGuiMCP::ImGuiKey_GamepadL3, SKSE::InputMap::kGamepadButtonOffset_LEFT_THUMB, "L3"};
    constexpr Button kRT{ImGuiMCP::ImGuiKey_GamepadR2, SKSE::InputMap::kGamepadButtonOffset_RT, "RT"};
    constexpr Button kLT{ImGuiMCP::ImGuiKey_GamepadL2, SKSE::InputMap::kGamepadButtonOffset_LT, "LT"};

    // SKSE Menu Framework gives ImGui each stick direction and trigger from 0 to 1, without a deadzone
    constexpr float kStickDeadzone{0.15f};
    constexpr float kTriggerDeadzone{0.05f};

    constexpr float kOrbitScreensPerSecond{0.6f};
    constexpr float kPanScreensPerSecond{0.5f};
    constexpr float kZoomStepsPerSecond{6.0f};

    constexpr float kInUseThreshold{0.3f};

    struct Bindings {
        const Button* pan{nullptr};
        const Button* zoomIn{nullptr};
        const Button* zoomOut{nullptr};
        const Button* recenter{nullptr};
    };

    Bindings g_bindings;
    bool g_resolved{false};
    std::uint32_t g_resolvedHotkey{0};

    // SKSE Menu Framework's gamepad toggle
    std::optional<std::uint32_t> g_menuToggle;
    bool g_inUse{false};

    std::string Normalized(std::string_view a_text) {
        std::string result;

        for (const char c : a_text) {
            if (c != ' ' && c != '_' && c != '-' && c != '\t') {
                result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
        }

        return result;
    }

    std::string_view Trimmed(std::string_view a_text) {
        const auto first{a_text.find_first_not_of(" \t\r")};

        if (first == std::string_view::npos) return {};

        return a_text.substr(first, a_text.find_last_not_of(" \t\r") - first + 1);
    }

    std::optional<std::uint32_t> ParseGamepadButton(std::string_view a_value) {
        using namespace SKSE::InputMap;
        static const std::unordered_map<std::string, std::uint32_t> names{
            {"A", kGamepadButtonOffset_A},
            {"B", kGamepadButtonOffset_B},
            {"X", kGamepadButtonOffset_X},
            {"Y", kGamepadButtonOffset_Y},
            {"LB", kGamepadButtonOffset_LEFT_SHOULDER},
            {"LEFTSHOULDER", kGamepadButtonOffset_LEFT_SHOULDER},
            {"L1", kGamepadButtonOffset_LEFT_SHOULDER},
            {"RB", kGamepadButtonOffset_RIGHT_SHOULDER},
            {"RIGHTSHOULDER", kGamepadButtonOffset_RIGHT_SHOULDER},
            {"R1", kGamepadButtonOffset_RIGHT_SHOULDER},
            {"LT", kGamepadButtonOffset_LT},
            {"LEFTTRIGGER", kGamepadButtonOffset_LT},
            {"L2", kGamepadButtonOffset_LT},
            {"RT", kGamepadButtonOffset_RT},
            {"RIGHTTRIGGER", kGamepadButtonOffset_RT},
            {"R2", kGamepadButtonOffset_RT},
            {"LS", kGamepadButtonOffset_LEFT_THUMB},
            {"LSB", kGamepadButtonOffset_LEFT_THUMB},
            {"L3", kGamepadButtonOffset_LEFT_THUMB},
            {"LEFTTHUMB", kGamepadButtonOffset_LEFT_THUMB},
            {"LEFTSTICK", kGamepadButtonOffset_LEFT_THUMB},
            {"RS", kGamepadButtonOffset_RIGHT_THUMB},
            {"RSB", kGamepadButtonOffset_RIGHT_THUMB},
            {"R3", kGamepadButtonOffset_RIGHT_THUMB},
            {"RIGHTTHUMB", kGamepadButtonOffset_RIGHT_THUMB},
            {"RIGHTSTICK", kGamepadButtonOffset_RIGHT_THUMB},
            {"START", kGamepadButtonOffset_START},
            {"MENU", kGamepadButtonOffset_START},
            {"BACK", kGamepadButtonOffset_BACK},
            {"SELECT", kGamepadButtonOffset_BACK},
            {"VIEW", kGamepadButtonOffset_BACK},
            {"UP", kGamepadButtonOffset_DPAD_UP},
            {"DPADUP", kGamepadButtonOffset_DPAD_UP},
            {"DOWN", kGamepadButtonOffset_DPAD_DOWN},
            {"DPADDOWN", kGamepadButtonOffset_DPAD_DOWN},
            {"LEFT", kGamepadButtonOffset_DPAD_LEFT},
            {"DPADLEFT", kGamepadButtonOffset_DPAD_LEFT},
            {"RIGHT", kGamepadButtonOffset_DPAD_RIGHT},
            {"DPADRIGHT", kGamepadButtonOffset_DPAD_RIGHT}};

        const auto name{Normalized(a_value)};

        if (const auto it{names.find(name)}; it != names.end()) {
            return it->second;
        }

        std::uint32_t number{0};

        if (const auto [end, error]{std::from_chars(name.data(), name.data() + name.size(), number)};
            error == std::errc{} && end == name.data() + name.size()) {
            if (number >= kGamepadButtonOffset_DPAD_UP && number <= kGamepadButtonOffset_RT) return number;

            if (const auto macro{GamepadMaskToKeycode(number)}; macro != kMaxMacros) return macro;
        }

        return std::nullopt;
    }

    // The menu opens often, what it found is logged when that changes
    std::string g_loggedToggle;
    std::string g_loggedBindings;

    void LogChange(std::string& a_logged, std::string a_message) {
        if (a_message == a_logged) return;
        logger::info("Gamepad camera: {}", a_message);
        a_logged = std::move(a_message);
    }

    // Read with SKSE Menu Framework's own names. Missing or unreadable, its toggle is just not known
    void ReadMenuToggle() {
        g_menuToggle.reset();

        std::ifstream file{"Data/SKSE/Plugins/SKSEMenuFramework.ini"};

        if (!file) {
            LogChange(g_loggedToggle, "SKSE Menu Framework's ini wasn't found, its gamepad toggle isn't known");
            return;
        }

        std::string section;
        std::string key;
        std::string mode;

        for (std::string line; std::getline(file, line);) {
            auto text{Trimmed(line)};

            if (text.empty() || text.front() == ';' || text.front() == '#') continue;

            if (text.front() == '[') {
                section = Normalized(text.substr(1, text.find(']') - 1));
                continue;
            }

            const auto equals{text.find('=')};

            if (section != "GENERAL" || equals == std::string_view::npos) continue;

            const auto name{Normalized(text.substr(0, equals))};
            const auto value{Trimmed(text.substr(equals + 1))};

            if (name == "TOGGLEKEYGAMEPAD") {
                key = value;
            } else if (name == "TOGGLEMODEGAMEPAD") {
                mode = Normalized(value);
            }
        }

        if (key.empty() || mode == "OFF") {
            LogChange(g_loggedToggle, "SKSE Menu Framework has no gamepad toggle");
            return;
        }
        g_menuToggle = ParseGamepadButton(key);
        LogChange(g_loggedToggle,
                  g_menuToggle ? std::format("SKSE Menu Framework's gamepad toggle is {} ({}), mode {}", key,
                                             *g_menuToggle, mode)
                               : std::format("SKSE Menu Framework's gamepad toggle '{}' isn't a known button", key));
    }

    void Resolve(const std::uint32_t a_hotkey) {
        g_resolved = true;
        g_resolvedHotkey = a_hotkey;

        const auto reserved{
            [&](const Button& a_button) { return a_button.macro == a_hotkey || a_button.macro == g_menuToggle; }};
        const auto first{[&](std::initializer_list<const Button*> a_candidates, const Button* a_taken) {
            for (const auto* const candidate : a_candidates) {
                if (candidate != a_taken && !reserved(*candidate)) return candidate;
            }

            return static_cast<const Button*>(nullptr);
        }};

        Bindings bindings;
        bindings.pan = first({&kRB, &kLB, &kL3}, nullptr);

        const bool rt{!reserved(kRT)};
        const bool lt{!reserved(kLT)};

        if (rt && lt) {
            bindings.zoomIn = &kRT;
            bindings.zoomOut = &kLT;
        } else if (rt || lt) {
            bindings.zoomIn = rt ? &kRT : &kLT;
            bindings.zoomOut = bindings.pan ? bindings.zoomIn : nullptr;
        }

        bindings.recenter = first({&kR3, &kL3}, bindings.pan);
        g_bindings = bindings;

        const auto name{[](const Button* a_button) { return a_button ? a_button->name : "none"; }};
        LogChange(
            g_loggedBindings,
            std::format("hotkey {}, SKSE Menu Framework's toggle {}. Pan with {}, zoom in with {}, out with {}{}, "
                        "recenter with {}",
                        a_hotkey, g_menuToggle ? std::to_string(*g_menuToggle) : "none", name(bindings.pan),
                        name(bindings.zoomIn), name(bindings.zoomOut),
                        bindings.zoomOut && bindings.zoomOut == bindings.zoomIn ? " and pan held" : "",
                        name(bindings.recenter)));
    }

    // Past the deadzone, from 0 to 1, squared so a slight tilt is slow enough to aim
    float Shape(const float a_value, const float a_deadzone) {
        const float past{(std::abs(a_value) - a_deadzone) / (1.0f - a_deadzone)};

        return past <= 0.0f ? 0.0f : std::copysign(past * past, a_value);
    }

    float Analog(const ImGuiMCP::ImGuiKey a_key) {
        const auto* const data{ImGuiMCP::GetKeyData(a_key)};

        return data ? data->AnalogValue : 0.0f;
    }

    // The hint follows what was used last
    void TrackInUse(const ImGuiMCP::ImGuiIO* a_io) {
        if (a_io->MouseDelta.x != 0.0f || a_io->MouseDelta.y != 0.0f || a_io->MouseWheel != 0.0f ||
            ImGuiMCP::IsMouseDown(0) || ImGuiMCP::IsMouseDown(1) || ImGuiMCP::IsMouseDown(2)) {
            g_inUse = false;
        }

        for (int key{ImGuiMCP::ImGuiKey_GamepadStart}; key <= ImGuiMCP::ImGuiKey_GamepadRStickDown; ++key) {
            if (Analog(static_cast<ImGuiMCP::ImGuiKey>(key)) > kInUseThreshold) {
                g_inUse = true;
                return;
            }
        }
    }

    // The right stick, as gamepad prompts name it
    constexpr const char* kRightStick{"RS"};
}  // namespace

void UI::GamepadCamera::OnMenuOpened() {
    ReadMenuToggle();
    g_resolved = false;
}

void UI::GamepadCamera::Control() {
    const auto* io{ImGuiMCP::GetIO()};
    TrackInUse(io);

    // Make sure typing doesn't move the gamepad
    if (io->DeltaTime <= 0.0f) {
        return;
    }

    if (const auto hotkey{UI::PresetList::GetHotkeyScanCode()}; !g_resolved || hotkey != g_resolvedHotkey) {
        Resolve(hotkey);
    }

    auto& preview{Preview::Session::GetInstance()};
    const auto& bindings{g_bindings};
    const bool panHeld{bindings.pan && ImGuiMCP::IsKeyDown(bindings.pan->key)};

    const float across{Shape(
        Analog(ImGuiMCP::ImGuiKey_GamepadRStickRight) - Analog(ImGuiMCP::ImGuiKey_GamepadRStickLeft), kStickDeadzone)};
    const float down{Shape(Analog(ImGuiMCP::ImGuiKey_GamepadRStickDown) - Analog(ImGuiMCP::ImGuiKey_GamepadRStickUp),
                           kStickDeadzone)};

    if (across != 0.0f || down != 0.0f) {
        if (panHeld) {
            const float screens{kPanScreensPerSecond * io->DeltaTime};
            preview.Pan(across * screens, down * screens);
        } else {
            const float screens{kOrbitScreensPerSecond * io->DeltaTime};
            preview.Orbit(across * screens, down * screens);
        }
    }

    if (bindings.zoomIn) {
        const float in{Shape(Analog(bindings.zoomIn->key), kTriggerDeadzone)};
        float zoom{in};

        if (bindings.zoomOut == bindings.zoomIn) {
            zoom = panHeld ? -in : in;
        } else if (bindings.zoomOut) {
            zoom -= Shape(Analog(bindings.zoomOut->key), kTriggerDeadzone);
        }

        if (zoom != 0.0f) {
            preview.Zoom(zoom * kZoomStepsPerSecond * io->DeltaTime);
        }
    }

    if (bindings.recenter && ImGuiMCP::IsKeyPressed(bindings.recenter->key, false)) {
        preview.Recenter();
    }
}

bool UI::GamepadCamera::IsInUse() { return g_inUse; }

std::vector<UI::Widgets::ControlHint> UI::GamepadCamera::Hints() {
    const auto& bindings{g_bindings};
    const auto get{[](const char* a_key) { return UI::Translations::Get(a_key); }};

    std::vector<Widgets::ControlHint> hints;
    hints.push_back({{kRightStick}, "", get("obody_hint_orbit")});

    if (bindings.pan) {
        hints.push_back({{bindings.pan->name, kRightStick}, "+", get("obody_hint_pan")});
    }

    if (bindings.zoomIn) {
        if (bindings.zoomOut == bindings.zoomIn) {
            hints.push_back({{bindings.zoomIn->name}, "", get("obody_hint_zoom_in")});
            hints.push_back({{bindings.pan->name, bindings.zoomIn->name}, "+", get("obody_hint_zoom_out")});
        } else if (bindings.zoomOut) {
            hints.push_back({{bindings.zoomOut->name, bindings.zoomIn->name}, "/", get("obody_hint_zoom")});
        } else {
            hints.push_back({{bindings.zoomIn->name}, "", get("obody_hint_zoom_in")});
        }
    }

    if (bindings.recenter) {
        hints.push_back({{bindings.recenter->name}, "", get("obody_recenter")});
    }

    return hints;
}
