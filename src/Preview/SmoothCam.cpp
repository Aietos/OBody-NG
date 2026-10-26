#include "Preview/SmoothCam.h"

namespace SmoothCam {
    namespace {
        constexpr auto kPluginName{"SmoothCam"};

        // The messages SmoothCam exchanges, as its SmoothCamAPI.h lays them out
        struct PluginCommand {
            enum class Type : std::uint8_t { RequestInterface };

            std::uint32_t header{0x9007CA50};
            Type type{};
            void* commandStructure{};
        };

        struct InterfaceRequest {
            InterfaceVersion interfaceVersion{};
        };

        struct PluginResponse {
            enum class Type : std::uint8_t { Error, InterfaceProvider };

            Type type{};
            void* responseData{};
        };

        struct InterfaceContainer {
            void* interfaceInstance{};
            InterfaceVersion interfaceVersion{};
        };

        IVSmoothCam1* g_interface{nullptr};
        // Only what AcquireCamera was granted is given back, a camera SmoothCam had already given OBody included
        bool g_owned{false};

        void OnMessage(SKSE::MessagingInterface::Message* a_message) {
            if (!a_message || a_message->type != 0 || a_message->dataLen != sizeof(PluginResponse)) return;

            const auto* const response{static_cast<const PluginResponse*>(a_message->data)};
            if (response->type != PluginResponse::Type::InterfaceProvider || !response->responseData) {
                logger::info("SmoothCam didn't give its interface, the preset menu's preview can't take its camera");
                return;
            }

            const auto* const container{static_cast<const InterfaceContainer*>(response->responseData)};
            g_interface = static_cast<IVSmoothCam1*>(container->interfaceInstance);
            logger::info("Got SmoothCam's interface, version {}", std::to_underlying(container->interfaceVersion) + 1);
        }
    }  // namespace

    void Listen() { SKSE::GetMessagingInterface()->RegisterListener(kPluginName, OnMessage); }

    void RequestInterface() {
        // Answered by SmoothCam if it's installed, OnMessage gets the interface then
        static InterfaceRequest request{InterfaceVersion::V1};
        static PluginCommand command{.type = PluginCommand::Type::RequestInterface, .commandStructure = &request};
        if (!SKSE::GetMessagingInterface()->Dispatch(0, &command, sizeof(PluginCommand), kPluginName)) {
            logger::info("SmoothCam isn't installed");
        }
    }

    bool AcquireCamera() {
        if (!g_interface) return true;

        switch (const auto result{g_interface->RequestCameraControl(SKSE::GetPluginHandle())}) {
            case APIResult::OK:
                g_owned = true;
                return true;
            case APIResult::AlreadyGiven:
                return true;
            default:
                logger::info("SmoothCam didn't give up the camera ({}), the preview leaves it alone",
                             std::to_underlying(result));
                return false;
        }
    }

    void ReleaseCamera() {
        if (!g_interface || !g_owned) return;

        g_owned = false;
        if (const auto result{g_interface->ReleaseCameraControl(SKSE::GetPluginHandle())}; result != APIResult::OK) {
            logger::info("SmoothCam didn't take its camera back ({})", std::to_underlying(result));
        }
    }
}  // namespace SmoothCam
