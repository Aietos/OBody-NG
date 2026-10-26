#pragma once
#include "SKSEMenuFramework.h"
#include "UI/Widgets.h"

// The preview UI's parts of the preset menu
namespace UI::PreviewPanel {
    void RenderOverlay(const ImGuiMCP::ImGuiViewport* a_viewport);

    void PlaceWindow(const ImGuiMCP::ImGuiViewport* a_viewport);

    void TrackWindow();

    // a_another when it's a different actor from before, or the menu just opened
    void OnTargetSelected(bool a_another);

    void Manage(RE::Actor* a_actor);

    void RenderControls(RE::Actor* a_actor);

    bool KeepsMenuOpen();

    // The Keep menu open checkbox itself, for the cosave. KeepsMenuOpen also needs the live preview on
    void SetKeepMenuOpen(bool a_keep);
    bool IsKeepMenuOpenSet();

    void Hover(RE::Actor* a_actor, const std::string& a_preset, const std::optional<std::string>& a_current);

    std::string GetShownPreset(RE::Actor* a_actor);

    std::vector<Widgets::ControlHint> GetHints(RE::Actor* a_actor);

    // a_apply sets the preset
    // a_announce sends the event for force immediate preset application option
    void Apply(RE::Actor* a_actor, const std::function<void()>& a_apply, std::function<void()> a_announce);

    // SKSE Menu Framework's menu closed, game's time unfreezes
    void OnMenuClosed();

    bool IsOverlayFocused();

    void End();
}  // namespace UI::PreviewPanel
