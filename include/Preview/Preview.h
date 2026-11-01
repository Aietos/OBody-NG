#pragma once

#include "PresetManager/PresetManager.h"

namespace Preview {
    // Shows presets live on an actor while the preset menu is open
    class Session {
    public:
        static Session& GetInstance();

        Session(Session&&) = delete;
        Session(const Session&) = delete;

        Session& operator=(Session&&) = delete;
        Session& operator=(const Session&) = delete;

        // Switches to a_actor if another actor is previewed. False if the actor can't be previewed, for example when
        // their 3D isn't loaded
        bool Begin(RE::Actor* a_actor);

        // a_keepMorphs: a preset is about to be applied for real, so the actor's morphs aren't put back
        void End(bool a_keepMorphs = false);

        // A preset is applied by a_apply while the menu stays open
        void Apply(const std::function<void()>& a_apply);

        // For a game being loaded the references are going away, so only what outlives them is put back
        void Abandon();

        // a_func runs once SKSE Menu Framework's menu closes, which lets time run again. Dropped when a game is loaded.
        // It replaces what waited for the same a_key, so an actor whose preset changed several times while the menu
        // was open is told once, about the last one
        void RunWhenMenuCloses(RE::FormID a_key, std::function<void()> a_func);
        void OnMenuClosed();

        // The copies Live Preview used and disabled are deleted, so the save doesn't keep them
        void OnSave();

        [[nodiscard]] bool IsActive() const;

        // For the preview equip events
        [[nodiscard]] bool IsPreviewing(const RE::Actor* a_actor) const;

        // Shows the preset preview, with a mouse hover debounce
        void RequestPreset(const std::string& a_name);

        // The preset shown on the actor, empty when it's their own body
        [[nodiscard]] std::string GetShownPreset() const;

        [[nodiscard]] bool IsClothed() const;
        void SetClothed(bool a_clothed);

        // A toggleable light for the live preview, useful for dark places
        [[nodiscard]] bool IsLit() const;
        void SetLit(bool a_lit);

        // The drag controls for the live preview camera
        // Orbit: drag across goes around the actor, drag down swings the camera over them
        // Pan: moves that point, so the actor follows the cursor
        void Orbit(float a_across, float a_down);
        void Pan(float a_across, float a_down);
        void Zoom(float a_steps);

        // Looks at the middle of the body again, from where the camera is around the actor
        void Recenter();

        // Everything back to how the camera started on this actor
        void ResetView();

        // Turns the actor to face the camera, which stays where it is.
        // Their heading is put back when the preview ends
        void FaceCamera();

        // Bring to me (BringToMe.cpp): a copy of the actor is previewed where the player stands, the actor stays where
        // they are. This feature is useful for when actors are in awkward places, such as against a bookshelf
        [[nodiscard]] bool CanBringToPlayer() const;
        [[nodiscard]] bool IsBroughtToPlayer() const;
        void SetBroughtToPlayer(bool a_brought);

        // A copy of the actor is previewed in their place, in a neutral pose
        // This is so we don't mess the current actor's AI or scenes
        static void SetAlwaysCopy(bool a_always);
        [[nodiscard]] static bool IsAlwaysCopy();

        // Whether a_form is the copy, or may be while it's being spawned
        [[nodiscard]] bool IsCopy(const RE::TESForm* a_form) const;
        [[nodiscard]] bool IsCopy(RE::FormID a_formID) const;
        // The disabled copies still waiting to be deleted, so the NPC lists leave them out.
        // Takes lock_, unlike IsCopy, which the event handlers call
        [[nodiscard]] bool IsAnyCopy(const RE::Actor* a_actor) const;

        void Update();

    private:
        struct MorphValue {
            std::string name;
            std::string key;
            float value;
        };

        struct UnequippedItem {
            RE::TESBoundObject* object;
            const RE::ExtraDataList* extraList;
        };

        // The camera as the preview found it, put back when it ends
        struct CameraBackup {
            // Whether the preview switched to the free camera, it switches back then
            // Otherwise the free camera was on already, and is put back where it was
            bool switched{};
            RE::NiPoint3 translation;
            RE::BSTPoint2<float> rotation;
        };

        // How the camera looks at the actor
        struct View {
            // Around the actor, zero is behind them
            float orbit{std::numbers::pi_v<float>};

            // Down when positive
            // Pitch is 0.3 default so the camera starts around head height, above counters and tables
            float pitch{0.3F};

            float zoom{1.0F};

            // How far the point looked at is moved from the middle of the body
            float panRight{0.0F};
            float panUp{0.0F};
        };

        // How far the camera is from the middle of the body, and how far it's moved from looking straight at it
        struct Framing {
            float distance;
            RE::NiPoint3 offset;
        };

        Session() = default;
        ~Session() = default;

        static Session instance_;

        // The game doesn't animate actors far from the player, so their skeleton can be in the A-pose
        static void SettlePose(RE::Actor* a_actor, RE::NiAVObject* a_root);

        void SnapshotMorphs();
        void RestoreMorphs();
        void Show(const std::string& a_name);

        void Unequip();
        void EquipBack();

        // What's worn is shown at once, also while time is frozen
        void QueueWornUpdate();

        // The light is made and moved in the game's task phase
        void QueueLightUpdate();
        void UpdateLight();

        void RemoveLight();

        void EndActor(bool a_keepMorphs);

        void EnterScene();
        void LeaveScene();
        void RestoreBlur();
        void RestoreHUD();

        // Starts framing the actor, once the free camera is on
        void Present();

        // Picks where around the actor the camera starts, with nothing in between them
        void ChooseView();

        [[nodiscard]] Framing ComputeFraming(const View& a_view) const;

        // Where the camera is for a view, and which way it looks
        [[nodiscard]] RE::NiPoint3 CameraPosition(const View& a_view) const;
        [[nodiscard]] float CameraYaw(const View& a_view) const;

        // Moves the camera a frame closer to the target view, false when it's there already
        bool EaseView();
        void ApplyFraming();
        void RestoreCamera();

        // The camera is updated in the game's task phase, at most once per frame
        void QueueCameraUpdate();
        void UpdateCamera();

        // Every other actor is hidden while previewing, so no one stands in front of the camera
        void HideOtherActors();

        // Hides what's between the camera and the actor's body
        // Useful for when NPCs are for example behind counters or leaning against a pillar/column
        void UpdateOccluders(const RE::NiPoint3& a_cameraPosition);

        static void ShowAll(std::vector<RE::NiPointer<RE::NiAVObject>>& a_hidden);

        // The copy spawned while the target is previewed, then shown in their place once it's ready
        [[nodiscard]] bool WantsCopy() const;
        // Spawns, shows, moves or drops the copy for what's wanted, once per frame
        void UpdateCopy();
        void SpawnCopy();
        void QueueDressCopy();
        [[nodiscard]] bool IsCopyReady();
        // The preview moves from the target to the copy, or back, with what was shown on them
        void ShowCopy();
        void ShowTarget();
        // Where the player stands, or where the target does
        void PlaceCopy(bool a_atPlayer);
        void DeleteCopy();
        // Under lock_, in the game's task phase or before a save. a_all: also those disabled moments ago
        void DeleteDisabledCopies(bool a_all);
        void ResetCopy();

        void ResetActor();
        void ResetScene();

        mutable std::recursive_mutex lock_;

        // The form ID of the previewed actor, zero when no actor is previewed
        std::atomic<RE::FormID> formID_{0};

        // The previewed actor, whose presets are shown and applied
        RE::NiPointer<RE::Actor> target_;
        // Who's shown, the target or their copy, and their 3D
        RE::NiPointer<RE::Actor> actor_;
        RE::NiPointer<RE::NiAVObject> root_;

        static inline std::atomic_bool alwaysCopy_{false};

        // See RunWhenMenuCloses
        std::vector<std::pair<RE::FormID, std::function<void()>>> onMenuClosed_;

        // Bring to me is on, the copy is shown where the player stands
        bool bring_{false};

        RE::NiPointer<RE::Actor> copy_;
        // Tells a queued copy task whether it's still wanted
        std::uint32_t copyRequest_{0};
        bool copySpawnQueued_{false};
        bool copyDressQueued_{false};
        bool copyDressed_{false};
        bool copyShown_{false};
        bool copyAtPlayer_{false};
        std::chrono::steady_clock::time_point copyRequestedAt_;
        std::chrono::steady_clock::time_point copyDressedAt_;

        RE::FormID copyFailedFor_{0};

        // For IsCopy
        std::atomic<RE::FormID> copyID_{0};
        // A deleted copy is left alone a while longer, its unequip events come after it's gone
        std::atomic<RE::FormID> deletedCopyID_{0};
        std::atomic<std::int64_t> deletedCopyAt_{0};

        std::vector<std::pair<RE::NiPointer<RE::Actor>, std::int64_t>> disabledCopies_;
        std::atomic<const RE::TESNPC*> spawningBase_{nullptr};
        std::atomic<RE::FormID> spawningFrom_{0};

        std::vector<MorphValue> morphs_;
        PresetManager::SliderSet extraSliders_;
        std::string requestedPreset_;
        std::string shownPreset_;
        std::chrono::steady_clock::time_point requestTime_;

        bool clothed_{true};
        std::vector<UnequippedItem> unequipped_;

        // The heading the actor had before FaceCamera turned them
        std::optional<float> originalHeading_;

        bool lit_{false};

        // Attached to the actor's 3D, so the scene graph updates it with them
        RE::NiPointer<RE::NiPointLight> light_;
        RE::NiPointer<RE::BSLight> bsLight_;
        std::atomic_bool lightUpdateQueued_{false};

        std::vector<RE::NiPointer<RE::NiAVObject>> hiddenActors_;
        std::vector<RE::NiPointer<RE::NiAVObject>> hiddenOccluders_;

        // The camera moved, so what's in front of the actor is looked for again after its next update
        bool occludersDirty_{false};

        bool sceneActive_{false};
        bool frozeTime_{false};

        bool removedBlur_{false};

        // The HUD with the subtitles, crosshair prompts and compass, hidden while previewing
        RE::GPtr<RE::IMenu> hiddenHUD_;

        // The camera is left alone in VR, or when SmoothCam or another mod keeps it
        bool framingAvailable_{false};
        bool presented_{false};
        std::optional<CameraBackup> cameraBackup_;
        std::atomic_bool cameraUpdateQueued_{false};

        // Where the camera is now, where it eases to, and what ChooseView picked for Reset view
        View view_;
        View targetView_;
        View defaultView_;
        std::chrono::steady_clock::time_point lastEase_;
    };
}  // namespace Preview
