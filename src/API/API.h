#pragma once

#include <limits>
#include <string_view>

/**
    This header is a C++ API for interoperating with OBody via a SKSE plugin.

    The expected usage of this header is for you to copy it wholesale into your project and use it as-is.
    There is no need to involve any other portion of OBody's source code for use of this API.

    Skyrim game types, such as `Actor` and `TESForm` are used by this header, but are left undefined
    so that you can define them yourself before including this header. Such as by including the
    headers of the likes of CommonLibSSE or SKSE64, for example.

    The overall usage of this API is relatively simple:
        After SKSE has sent a `kPostPostLoad` message to your plugin, you can send a `RequestPluginInterface`
        message along with an `IOBodyReadinessEventListener` instance to OBody, and if OBody is installed,
        and your request is valid, OBody will write a pointer to an `IPluginInterface` instance
        through your supplied pointer.
        That object is your primary gateway to interoperating with OBody.

        But do note that an `IPluginInterface` instance can be used only when it is safe to do so--
        you can be notified of when it is safe to do so via the `IOBodyReadinessEventListener` instance
        that you supply in a `RequestPluginInterface` message.

        For example, this is how you would request a plugin interface when using CommonLibSSE:
        ```
            if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
                OBody::API::SKSEMessages::RequestPluginInterface req{};
                req.version = OBody::API::PluginAPIVersion::Latest;
                req.pluginInterface = &yourGlobalState.obodyAPI;
                req.readinessEventListener = &yourGlobalState.obodyReadinessListener;

                assert(yourGlobalState.obodyAPI == nullptr);

                SKSE::GetMessagingInterface()->Dispatch(decltype(req)::type, &req, sizeof(decltype(req)), "OBody");

                if (yourGlobalState.obodyAPI != nullptr) {
                    // OBody is installed!
                }
            }

            // ...

            class OBodyReadinessListener : public OBody::API::IOBodyReadinessEventListener {
            public:
                bool initialised = false;

                virtual void OBodyIsReady() override {
                    yourGlobalState.obodyIsSafeToUse = true;

                    if (!this->initialised) {
                        yourGlobalState.obodyAPI->RegisterEventListener(yourGlobalState.actorChangeListener);
                        this->initialised = true;
                    }
                }

                virtual void OBodyIsNoLongerReady() override {
                    yourGlobalState.obodyIsSafeToUse = false;
                }
            };
        ```
*/

/* A note for implementers:
   Be mindful of ABI-compatibility when making changes to this header, if you do any of the following
   without introducing a new `PluginAPIVersion` and corresponding versioned virtual classes, you risk
   breaking mods that use this API:
     * Changing the order of virtual methods or members in an aggregate type.
     * Removing virtual methods or members from an aggregate type.
     * Changing the values of an enum type or of constant values.
     * Changing how parameters are passed to a function, or how its result is returned;
         see: https://learn.microsoft.com/cpp/build/x64-calling-convention
     * Increasing the required alignment of an aggregate type.

   Stick to appending virtual methods and members to the end of aggregate types and all will be grand.
*/

namespace OBody {
    namespace API {
        /** This represents a version of the OBody plugin-API and is unrelated to the version of OBody proper.
            These version numbers signify how this API is to be used.
            New versions will be introduced when breaking changes are made to the API, so that it's feasible to
            update the API without breaking SKSE plugins that were compiled for older versions.
        */
        enum class PluginAPIVersion { Invalid = 0, v1 = 1, Latest = v1 };

        class IActorChangeEventListener;
        /** See the documentation for `IPluginInterface`, this is its base class purely to make it
            easier to maintain ABI-compatibility.

            As suggested by the name of this class, it's layout will remain
            compatible with all versions of the OBody plugin API.
        */
        class IPluginInterfaceVersionIndependent {
        public:
            /** This is a string that identifies the mod which requested this `IPluginInterface`.
                By default, this is the name of the mod's SKSE plugin.

                Do not change this string directly, if you must change it: use the `SetOwner` method.
                This is exposed as a field solely to reduce the overhead of reading it.

                This string must remain valid for the duration of the `IPluginInterface`'s lifetime. */
            const char* owner;

            /** This is a pointer-sized field that you can use for whatever you want.
                OBody does not care about the value of the field, it does not use it.
                By default this is null. */
            void* context = nullptr;

            /** This returns the version of the OBody plugin-API that this `IPluginInterface` implements. */
            virtual PluginAPIVersion PluginAPIVersion() = 0;

            /** This is used to change the value of the `owner`field. It returns the value of the `owner` parameter. */
            virtual const char* SetOwner(const char* owner) = 0;
        };

        /** This is the primary interface of the plugin-API.
            This is what you get in return from sending a `RequestPluginInterface` message to `OBody`.

            You can acquire a plugin interface after SKSE has sent a `kPostPostLoad` message to your plugin,
            but note that it will not be safely usable until OBody sends your `IOBodyReadinessEventListener` instance
            an `OBodyIsReady` event.

            Unless otherwise stated, all the methods provided by this type are thread-safe.
        */
        class IPluginInterface : public IPluginInterfaceVersionIndependent {
        public:
            /** This is used to check whether OBody considers an actor to be naked or not.
                If you're calling this in the context of a `TESEquipEvent`
                event sink, see the 3-argument overload of this method. */
            virtual bool ActorIsNaked(Actor* actor) = 0;

            /** This is used to check whether OBody considers an actor to be naked or not.
                If you're calling this in the context of a `TESEquipEvent`
                event sink, note that the game will not yet have actually [un]equipped the form from
                the actor, thus you'll need to pass to OBody whether the event is an unequip or an equip event,
                and the armor that's being [un]equipped so that OBody can properly assess whether the
                actor is naked or not. */
            virtual bool ActorIsNaked(Actor* actor, bool actorIsEquippingArmor, const TESForm* armor) = 0;

            /** This is used to check whether ORefit is currently applied to an actor or not. */
            virtual bool ActorHasORefitApplied(Actor* actor) = 0;

            /** This is used to check whether OBody has processed an actor or not.
                Wherein an actor is considered processed if they have OBody morphs
                for the current distribution applied to them.
                A blacklisted actor may be considered as processed. */
            virtual bool ActorIsProcessed(Actor* actor) = 0;

            /** This is used to check whether OBody has blacklisted an actor or not.
                A blacklisted actor is an actor that OBody is not automatically applying presets to.
                A user may manually apply a preset to a blacklisted actor. */
            virtual bool ActorIsBlacklisted(Actor* actor) = 0;

            /** This is used to check whether ORefit is globally enabled for OBody or not. */
            virtual bool IsORefitEnabled() = 0;

            /** `RegisterEventListener` and `DeregisterEventListener` are used
                 to subscribe and unsubscribe to and from events from OBody.

                 Registering and deregistering event-listeners acquires an exclusive lock internally,
                 so if you want to disable and re-enable an event-listener frequently, you should do
                 so internally in that event-listener.

                 Whilst these methods are thread-safe, you MUST not call these methods for
                 an `IXYZEventListener` instance from within the context
                 of an `IXYZEventListener` method called by OBody,
                 because doing so will invalidate the iterator that OBody is acting upon.

                 Rely on the order in which OBody sends events to different listeners for the same event
                 at your own peril.
                 The order in which OBody send different events in is deterministic.
            */

            /** This will make OBody start sending events to `eventListener`,
                returning whether the registration was successful or not.
                If this is called multiple times with the same listener, that listener will receive duplicated events.

                The `eventListener` reference passed to this method MUST remain valid until it is passed
                to `DeregisterEventListener` (or until the game's process terminates). */
            virtual bool RegisterEventListener(IActorChangeEventListener& eventListener) = 0;

            /** This will make OBody stop sending events to `eventListener`,
                returning whether any listeners were deregistered or not. */
            virtual bool DeregisterEventListener(IActorChangeEventListener& eventListener) = 0;

            /** This is used to check whether OBody is sending events to `eventListener` or not. */
            virtual bool HasRegisteredEventListener(IActorChangeEventListener& eventListener) = 0;
        };

        /** This is an interface for receiving events from OBody regarding
            whether OBody is ready for other mods to interact with it via the plugin-API or not.

            This event-listener MUST be used to be notified of when it is and isn't safe to use `IPluginInterface`s.

            At various stages of the game's life-cycle, OBody may need to rearrange its state,
            and during those periods usage of OBody's plugin-API via an `IPluginInterface` will be unsafe,
            causing bugs at best and memory corruption at worst (if multi-threading is involved).
            The most notable period wherein this is so is when a game is saved or loaded.

            If you want to safely interact with OBody's plugin-API in response to the game saving or loading,
            you should do so by reacting to these events.

            Note that when Obody calls the methods of an instance of this class, that method and the functions it calls
            MUST not send a `RequestPluginInterface` SKSE message to OBody,
            because doing so will invalidate the iterator that OBody is acting upon.
        */
        class IOBodyReadinessEventListener {
        public:
            /** The OBodyIsReady event is sent just after OBody has become ready for the plugin-API
                to be used and has sent an `OBodyIsBecomingReady` event to every `IOBodyReadinessEventListener`,
                or when OBody reponds to a `RequestPluginInterface` SKSE message when it is already ready.

                It is safe to use `IPluginInterface` instances from the moment this method is called.
            */
            virtual void OBodyIsReady() = 0;

            /** The OBodyIsNoLongerReady event is sent when OBody stops being ready for the plugin-API to be used.

                It is not safe to use `IPluginInterface` instances from the moment this method is called.
            */
            virtual void OBodyIsNoLongerReady() = 0;

            /** The OBodyIsBecomingReady event is sent just before OBody transitions from being unready
                to being ready,
                or when OBody reponds to a `RequestPluginInterface` SKSE message when it is already ready.

                It is safe to use `IPluginInterface` instances after every `IOBodyReadinessEventListener`
                has handled this event, which is signaled via the `OBodyIsReady` event.

                The purpose of this event is to give you a chance to set-up any state that you may
                need to set-up in order to handle events originating from other `IPluginInterface`s
                _before_ you have received the `OBodyIsReady` event.

                For an example of why that may be needed, consider this scenario:
                    There are two mods using the OBody plugin-API: Mod-A, and Mod-B.
                    The game was saved by the player, and so OBody became unready, and Mod-B tore down
                    some of its state that it requires for its `IActorChangeEventListener` instance.
                    OBody then becomes ready again, and Mod-B sets up its state in response to the
                    `OBodyIsBecomingReady` event.
                    Then, when Mod-A receives its `OBodyIsReady` event, it uses its `IPluginInterface` to change
                    an actor, which causes Mod-B's `IActorChangeEventListener` to receive an event
                    BEFORE Mod-B has had a chance to receive the `OBodyIsReady` event.
                    If Mod-B hadn't had a chance to set-up its state via the `OBodyIsBecomingReady` event
                    a bug would have occurred.
            */
            virtual void OBodyIsBecomingReady() {};

            /** The OBodyIsBecomingUnready event is sent just before OBody transitions from being ready
                to being unready.

                It is safe to use `IPluginInterface` instances when this method is called,
                and it remains safe to do so until the `OBodyIsNoLongerReady` event is sent.

                This event is effectively OBody yelling, "Last orders, please!".
            */
            virtual void OBodyIsBecomingUnready() {};
        };

        /** This is an interface for receiving events from OBody regarding the state of actors.

            If you want to keep your plugin's state in sync with OBody's state for actors you should
            implement this class and pass an instance of it to `IPluginInterface::RegisterEventListener`.

            When registered with OBody, OBody will call the methods defined by an instance of this class
            to signal the occurrence of certain events.

            Note that when Obody calls the methods of an instance of this class, that method and the functions it calls
            MUST not call `IPluginInterface::RegisterEventListener` or `IPluginInterface::DeregisterEventListener`
            for an `IActorChangeEventListener` instance, because doing so will invalidate the iterator that OBody
            is acting upon.

            The virtual methods of this class all have default implementations, so you need only implement the events
            you care about.

            These events have a consistent interface; an actor is passed as the first parameter;
            then a 64-bit bit-packed structure; and then a reference to a payload with extra data.
            This maximises performance as all the arguments are passed via registers, and the payload structure
            can be expanded without breaking ABI compatibility.
            Every event returns a response, which OBody may or may not use.
            The payload is mutable as it may be used as an extended return-channel in future, if needed.
        */
        class IActorChangeEventListener {
        public:
            /** The OnActorGenerated event is sent just after the assignment of a preset to an actor,
                and after OBody has either: applied the preset's morphs to the actor;
                or queued those morphs to be applied to the actor.
                (That is to say, the morphs may or not be visible to the player when you receive this event).

                This event is not sent when an actor's preset is reassigned but the actor is not regenerated.
                See the `OnActorPresetChangedWithoutGeneration` event for that scenario.
            */
            struct OnActorGenerated {
                struct Payload {
                    /** The name of the BodySlide preset that was assigned to the actor.
                        Note that this is the name of the BodySlide preset as defined within the XML
                        of the BodySlide slider presets file, and not the name of the slider presets file itself.

                        This is a `string_view`, but the string it points to is null-terminated,
                        so it can be used as C-style string.
                        The data that this `string_view` points to is guaranteed to be valid
                        only until the event-listener's method returns.

                        For the `OnActorGenerated` event specifically, this is guaranteed to be non-null and not empty,
                        for other events this may be null if the actor has no preset applied to them. */
                    const std::string_view presetName;
                };

                enum Flags : uint64_t {
                    None = 0,
                    /** This bit will be set if OBody considers the actor to be clothed. Elsewise the actor is naked. */
                    IsClothed = 1 << 0,
                    /** This bit will be set if ORefit is currently applied to the actor. */
                    IsORefitApplied = 1 << 1,
                    /** This bit will be set if ORefit is globally enabled for OBody. */
                    IsORefitEnabled = 1 << 2
                };

                enum class Response : uint64_t {
                    /** The default response: nothing special happens if you return it. */
                    None = 0
                };
            };

            /** OBody will call this method to notify the listener of `OnActorGenerated` events. */
            virtual OnActorGenerated::Response OnActorGenerated(Actor* actor, OnActorGenerated::Flags flags,
                                                                OnActorGenerated::Payload& payload) {
                return OnActorGenerated::Response::None;
            }

            /** The OnActorPresetChangedWithoutGeneration event is sent just after the assignment of a preset
                to an actor, if the actor is not also being regenerated.
                This event is also sent when a preset is unassigned from an actor.
            */
            struct OnActorPresetChangedWithoutGeneration {
                struct Payload {
                    /** Refer to the documentation of `responsiblePluginInterface` for the `OnActorGenerated` event. */
                    IPluginInterfaceVersionIndependent* responsiblePluginInterface;

                    /** The name of the BodySlide preset that was assigned to the actor.
                        Note that this is the name of the BodySlide preset as defined within the XML
                        of the BodySlide slider presets file, and not the name of the slider presets file itself.

                        This is a `string_view`, but the string it points to is null-terminated,
                        so it can be used as C-style string.
                        The data that this `string_view` points to is guaranteed to be valid
                        only until the event-listener's method returns.

                        If this string is null or empty,
                        it means that a preset has been unassigned from the actor
                        and the actor did not previously have a preset assigned to them. */
                    const std::string_view presetName;
                };

                enum Flags : uint64_t {
                    None = 0,
                    /** If this bit is set, this event signals that a preset was unassigned from the actor.
                        And that the `presetName` field contains the name of the preset that the actor had
                        before it was unassigned. */
                    PresetWasUnassigned = 1 << 0
                };

                enum class Response : uint64_t {
                    /** The default response: nothing special happens if you return it. */
                    None = 0
                };
            };

            /** OBody will call this method to notify the listener of `OnActorPresetChangedWithoutGeneration` events. */
            virtual OnActorPresetChangedWithoutGeneration::Response OnActorPresetChangedWithoutGeneration(
                Actor* actor, OnActorPresetChangedWithoutGeneration::Flags flags,
                OnActorPresetChangedWithoutGeneration::Payload& payload) {
                return OnActorPresetChangedWithoutGeneration::Response::None;
            }

            /** The OnActorClothingUpdate event is sent when the state of an actor's equipped clothing/armour changes.
                This event allows a listener to keep up-to-date on whether or not ORefit is active on actors,
                and whether or not OBody considers an actor to be naked or clothed.

                Note that internally this event is called from within the context of a `TESEquipEvent` event sink,
                and thus if the listener is querying the worn equipment of the actor, it may need to consider the
                equipment that is being [un]equipped by the actor, which can be accessed in the payload of this event.
                (See also `IPluginInterface::ActorIsNaked`).
            */
            struct OnActorClothingUpdate {
                struct Payload {
                    /** The equipment that is being equipped or unequipped by the actor,
                        check the flags for which it is. This will not be null. */
                    const TESForm* changedEquipment;
                };

                enum Flags : uint64_t {
                    None = 0,
                    /** This bit will be set if OBody considers the actor to be clothed. Elsewise the actor is naked. */
                    IsClothed = 1 << 0,
                    /** This bit will be set if ORefit is currently applied to the actor. */
                    IsORefitApplied = 1 << 1,
                    /** This bit will be set if ORefit is globally enabled for OBody. */
                    IsORefitEnabled = 1 << 2,
                    /** This bit will be set if OBody considers the actor to be processed.
                        (See `IPluginInterface::ActorIsProcessed`). */
                    IsProcessed = 1 << 3,
                    /** This bit will be set if OBody considers the actor to be blacklisted.
                        (See `IPluginInterface::ActorIsBlacklisted`). */
                    IsBlacklisted = 1 << 4,
                    /** This bit will be set if the actor is equipping equipment, otherwise the actor is unequipping. */
                    ActorIsEquipping = 1 << 5
                };

                enum class Response : uint64_t {
                    /** The default response: nothing special happens if you return it. */
                    None = 0
                };
            };

            /** OBody will call this method to notify the listener of `OnActorClothingUpdate` events. */
            virtual OnActorClothingUpdate::Response OnActorClothingUpdate(Actor* actor,
                                                                          OnActorClothingUpdate::Flags flags,
                                                                          OnActorClothingUpdate::Payload& payload) {
                return OnActorClothingUpdate::Response::None;
            }

            /** The OnORefitForcefullyChanged event is sent when ORefit is forcefully enabled or disabled
                for an actor; typically as the result of a Papyrus script calling
                `OBodyNative.AddClothesOverlay` or `OBodyNative.RemoveClothesOverlay`.
            */
            struct OnORefitForcefullyChanged {
                struct Payload {};

                enum Flags : uint64_t {
                    None = 0,
                    /** This bit will be set if ORefit is currently applied to the actor. */
                    IsORefitApplied = 1 << 1,
                    /** This bit will be set if ORefit is globally enabled for OBody. */
                    IsORefitEnabled = 1 << 2
                };

                enum class Response : uint64_t {
                    /** The default response: nothing special happens if you return it. */
                    None = 0
                };
            };

            /** OBody will call this method to notify the listener of `OnORefitForcefullyChanged` events. */
            virtual OnORefitForcefullyChanged::Response OnORefitForcefullyChanged(
                Actor* actor, OnORefitForcefullyChanged::Flags flags, OnORefitForcefullyChanged::Payload& payload) {
                return OnORefitForcefullyChanged::Response::None;
            }

            /** The OnActorMorphsCleared event is sent when an actor's OBody morphs are cleared;
                typically as the result of a Papyrus script calling `OBodyNative.ResetActorOBodyMorphs`.
                (Implicitly, this means that ORefit is not active for the actor).
            */
            struct OnActorMorphsCleared {
                struct Payload {};

                enum Flags : uint64_t { None = 0 };

                enum class Response : uint64_t {
                    /** The default response: nothing special happens if you return it. */
                    None = 0
                };
            };

            /** OBody will call this method to notify the listener of `OnActorMorphsCleared` events. */
            virtual OnActorMorphsCleared::Response OnActorMorphsCleared(Actor* actor, OnActorMorphsCleared::Flags flags,
                                                                        OnActorMorphsCleared::Payload& payload) {
                return OnActorMorphsCleared::Response::None;
            }
        };

        /** These structures are to be used to send messages to OBody via SKSE's messaging interface.
            Their general usage is such that you allocate the structure somewhere--likely on the stack--
            and the structure's address is then used for the message's `data` pointer, and the `sizeof`
            of the structure is used for the message's `dataLen`.

            See the top of this header for an example involving the `RequestPluginInterface` message.
        */
        namespace SKSEMessages {
            /** The `RequestPluginInterface` message is used to request an `IPluginInterface` instance from OBody,
                thus this can be thought of as the entry-point to OBody's plugin-API.

                Before sending this message, you must set the `version` field to the version of the plugin API
                that your SKSE plugin supports; this allows OBody to return a different `IPluginInterface` instance
                to your plugin according to that version, which permits OBody to update and alter its API without
                breaking backwards compatibility with your already-compiled mod.

                Secondly, you must supply a valid pointer to an `IOBodyReadinessEventListener` instance
                via the `readinessEventListener` field.
                The `IOBodyReadinessEventListener` instance pointed-to by this field must remain valid
                until the process terminates.

                In response to this message, OBody will write through the pointer of the `pluginInterface` field.
                If your message was valid and OBody can satisfy it, the pointed-to `pluginInterface` will be a pointer
                to a valid `IPluginInterface` instance.
                Otherwise, if your message was invalid or could not be satisfied, such as if you requested a version
                that is not a valid `PluginAPIVersion` value, or OBody has stopped supporting your requested version,
                then `pluginInterface` will not be written through.
                Likewise if you failed to supply an `IOBodyReadinessEventListener`.
                If the `dataLen` value you send is smaller than three pointers,
                then OBody will not respond to the message.

                The `IPluginInterface` instance you receive is not safe to use
                until the `IOBodyReadinessEventListener` instance you supplied receives an `OBodyIsReady` event.
                See the documentation for `IOBodyReadinessEventListener` for more detail.

                Whilst the handler that receives this message is thread-safe,
                you MUST not send this message to OBody from within the context
                of an `IOBodyReadinessEventListener` method called by OBody,
                because doing so will invalidate the iterator that OBody is acting upon.

                The reason why `pluginInterface` is a pointer to a pointer that is written through,
                instead of simply returning a pointer via the message, is so that the `IPluginInterface*`
                can be written directly to a location accessible by your `IOBodyReadinessEventListener` instance.
                This is important as the `OBodyIsReady` event can be sent before you receive a response for the message.
            */
            struct RequestPluginInterface {
                /** The value for the `type` of the SKSE message. */
                static constexpr uint32_t type = 0xc0B0D9cc;

                /** The version of the plugin that you support; see above. (You send this). */
                PluginAPIVersion version;

                /** A pointer to a pointer to an `IPluginInterface` instance; see above. (You send this).
                    The `IPluginInterface*` that is written through this pointer will have been allocated by `new`. */
                IPluginInterface** pluginInterface;

                /** A pointer to an `IOBodyReadinessEventListener` instance; see above. (You send this). */
                IOBodyReadinessEventListener* readinessEventListener;
            };
        }  // namespace SKSEMessages
    }  // namespace API
}  // namespace OBody
