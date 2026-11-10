#pragma once

#include <cstdint>

/**
    A C++ API for other SKSE plugins to work with OSkin. Copy this header into your project as it is.

    It uses CommonLibSSE's `RE::Actor`, so include CommonLibSSE first.

    After SKSE sends your plugin `kPostPostLoad`, send OSkin a `RequestInterface` message:
    ```
        OSkin::API::RequestInterface request{};
        request.version = OSkin::API::InterfaceVersion::Latest;
        request.oskinInterface = &yourOSkinInterface;
        SKSE::GetMessagingInterface()->Dispatch(OSkin::API::RequestInterface::type, &request, sizeof(request),
                                                "OSkin");
    ```
    When OSkin is installed and supports that version, it writes its interface through `oskinInterface` before
    `Dispatch` returns. Otherwise the pointer stays as it was. The interface is valid until the game exits.

    Only add methods at the end of `IInterface`, and raise `InterfaceVersion` when you do, so plugins built with an
    older copy of this header keep working.
*/
namespace OSkin::API {
    // v1 has OpenMenu and GetPreviewCopyTarget
    enum class InterfaceVersion : std::uint32_t { Invalid = 0, v1 = 1, Latest = v1 };

    // How another mod's menu showed the NPC it opens OSkin's menu on, so OSkin's preview shows them the same way.
    // size is sizeof(MenuState), so fields can be added at the end
    struct MenuState {
        std::uint32_t size{sizeof(MenuState)};
        // The preview showed a copy of the NPC where the player stands
        bool broughtToPlayer{false};
        // The preview showed the NPC in their clothes
        bool clothed{true};
        // The preview lit the NPC
        bool lit{false};
        // The mouse cursor, in the game window's pixels, negative when not known
        float cursorX{-1.0f};
        float cursorY{-1.0f};
    };

    class IInterface {
    public:
        // The version of the interface OSkin gave, the one requested
        virtual InterfaceVersion GetVersion() = 0;

        // Opens OSkin's menu on the actor, like it opens on the NPC in the crosshair. a_state is how your menu showed
        // them, or null. When it returns true, close your menu the same frame. False when it can't be opened, such as
        // when SKSE Menu Framework isn't installed, or for an actor OSkin doesn't skin (a child, a creature, OBody's
        // preview copy). Call it from the main thread, such as from an SKSE Menu Framework window
        virtual bool OpenMenu(RE::Actor* a_actor, const MenuState* a_state) = 0;

        // When the actor is the copy OSkin's menu spawned to preview an NPC, returns that NPC, else null.
        // A mod that changes actors as they load can give the copy what the NPC has. Thread safe
        virtual RE::Actor* GetPreviewCopyTarget(RE::Actor* a_actor) = 0;
    };

    struct RequestInterface {
        // The SKSE message's type
        static constexpr std::uint32_t type = 0x05C1A001;

        InterfaceVersion version;
        // OSkin writes its interface through it
        IInterface** oskinInterface;
    };
}  // namespace OSkin::API
