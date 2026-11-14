#include "API/OSkin.h"

#include "API/OSkinAPI.h"

namespace {
    ::OSkin::API::IInterface* g_oskin{nullptr};
}  // namespace

void API::OSkin::Request() {
    ::OSkin::API::RequestInterface request{};
    request.version = ::OSkin::API::InterfaceVersion::v1;
    request.oskinInterface = &g_oskin;

    SKSE::GetMessagingInterface()->Dispatch(::OSkin::API::RequestInterface::type, &request, sizeof(request), "OSkin");

    logger::info("OSkin's interface {}", g_oskin ? "was given" : "isn't there");
}

bool API::OSkin::IsAvailable() { return g_oskin != nullptr; }

bool API::OSkin::OpenMenu(RE::Actor* a_actor, const OBody::API::MenuState& a_state) {
    if (!g_oskin) return false;

    // Each mod's header is copied as it is, so the two structs stay separate
    ::OSkin::API::MenuState state;
    state.broughtToPlayer = a_state.broughtToPlayer;
    state.clothed = a_state.clothed;
    state.lit = a_state.lit;
    state.cursorX = a_state.cursorX;
    state.cursorY = a_state.cursorY;

    return g_oskin->OpenMenu(a_actor, &state);
}

RE::Actor* API::OSkin::GetPreviewCopyTarget(RE::Actor* a_actor) {
    return g_oskin && a_actor ? g_oskin->GetPreviewCopyTarget(a_actor) : nullptr;
}
