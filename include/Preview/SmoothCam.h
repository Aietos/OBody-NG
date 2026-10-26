#pragma once

// SmoothCam's modder interface, in header form like SKEE.h. Only what OBody uses is declared, but the order of the
// virtual functions up to the last one called has to stay as SmoothCam publishes it in its SmoothCamAPI.h

namespace SmoothCam {
    enum class InterfaceVersion : std::uint8_t { V1, V2, V3 };

    enum class APIResult : std::uint8_t { OK, NotOwner, MustKeep, AlreadyGiven, AlreadyTaken, BadThread };

    class IVSmoothCam1 {
    public:
        [[nodiscard]] virtual unsigned long GetSmoothCamThreadId() const noexcept = 0;
        [[nodiscard]] virtual APIResult RequestCameraControl(SKSE::PluginHandle a_handle) noexcept = 0;
        [[nodiscard]] virtual APIResult RequestCrosshairControl(SKSE::PluginHandle a_handle,
                                                                bool a_restoreDefaults = true) noexcept = 0;
        [[nodiscard]] virtual APIResult RequestStealthMeterControl(SKSE::PluginHandle a_handle,
                                                                   bool a_restoreDefaults = true) noexcept = 0;
        virtual SKSE::PluginHandle GetCameraOwner() const noexcept = 0;
        virtual SKSE::PluginHandle GetCrosshairOwner() const noexcept = 0;
        virtual SKSE::PluginHandle GetStealthMeterOwner() const noexcept = 0;
        virtual APIResult ReleaseCameraControl(SKSE::PluginHandle a_handle) noexcept = 0;
    };

    // On kPostLoad, listens for SmoothCam's answer
    void Listen();

    // On kPostPostLoad, asks SmoothCam for its interface. Nothing happens if it isn't installed
    void RequestInterface();

    // Takes the camera from SmoothCam before the preview moves it. True when it's OBody's, also when SmoothCam
    // isn't installed. False when SmoothCam or another mod keeps it, the camera must be left alone then
    bool AcquireCamera();

    // Gives the camera back, if AcquireCamera took it
    void ReleaseCamera();
}  // namespace SmoothCam
