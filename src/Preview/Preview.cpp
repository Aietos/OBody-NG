#include "Preview/Preview.h"

#include "Body/Body.h"
#include "Preview/Physics.h"
#include "Preview/SmoothCam.h"
#include "SKSEMenuFramework.h"
#include "STL.h"

Preview::Session Preview::Session::instance_;

namespace Preview {
    namespace {
        // How long the hovered preset has to stay the same before it's shown, ApplyBodyMorphs rebuilds the body
        constexpr auto kPresetDelay{std::chrono::milliseconds{120}};

        constexpr float kPi{std::numbers::pi_v<float>};

        // How much of the screen's height the body fills when not zoomed
        constexpr float kBodyFill{0.85F};
        // Where the actor is put across the screen, from -1 (left edge) to 1 (right edge). The menu is on the left
        constexpr float kScreenPosition{0.3F};
        // The camera goes around the middle of the body, this far up the actor's height
        constexpr float kBodyMiddle{0.5F};
        // The largest object hidden for standing between the camera and the actor, in the radius of its bounds, in
        // actor heights. Bigger ones are pieces of the building
        constexpr float kMaxOccluderSize{2.5F};
        // How far the camera swings over or under the actor, from looking a little up to nearly straight down
        constexpr float kMinPitch{-0.25F};
        constexpr float kMaxPitch{1.3F};

        // A drag across the whole screen's height turns the camera this far around the actor, or over them
        constexpr float kOrbitPerScreen{kPi};
        constexpr float kPitchPerScreen{kPi * 0.75F};

        // How fast the camera eases to where it's dragged, most of the way in a fifth of a second
        constexpr float kEaseRate{12.0F};

        constexpr float kMinZoom{0.35F};
        constexpr float kMaxZoom{1.5F};
        constexpr float kZoomStep{0.08F};

        // The light: a soft white, reaching this many times the actor's height, aimed at this far up their body
        constexpr auto kLightName{"OBodyPreviewLight"};
        constexpr RE::NiColor kLightColor{1.0F, 0.95F, 0.88F};
        constexpr float kLightRadius{4.5F};
        constexpr float kLightFade{1.0F};
        constexpr float kLitHeight{0.6F};

        // What's taken off to show the body. Hair, jewelry, helmets and the like stay on
        constexpr std::array kClothingSlots{
            RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
            RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
            RE::BGSBipedObjectForm::BipedObjectSlot::kForearms,
            RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
            RE::BGSBipedObjectForm::BipedObjectSlot::kCalves,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModChestPrimary,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisPrimary,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight,
            RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft,
        };

        bool IsClothing(const RE::TESObjectARMO* a_armor) {
            return std::ranges::any_of(kClothingSlots, [&](const auto a_slot) { return a_armor->HasPartOf(a_slot); });
        }

        float ActorHeight(RE::Actor* a_actor) {
            const float height{a_actor->GetHeight()};
            return height > 10.0F ? height : 128.0F;
        }

        // The tangents of half the field of view across and up the screen
        struct FOV {
            float tanHalfWidth{0.9F};
            float tanHalfHeight{0.55F};
        };

        FOV FieldOfView() {
            FOV fov;
            if (auto* const niCamera{RE::Main::WorldRootCamera()}) {
                const auto& frustum{niCamera->GetRuntimeData2().viewFrustum};
                if (frustum.fRight > 0.0F) fov.tanHalfWidth = frustum.fRight;
                if (frustum.fTop > 0.0F) fov.tanHalfHeight = frustum.fTop;
            }
            return fov;
        }

        // Null when the free camera isn't on
        RE::FreeCameraState* GetActiveFreeCameraState(RE::PlayerCamera* a_camera) {
            if (!a_camera || REL::Module::IsVR() || !a_camera->IsInFreeCameraMode()) return nullptr;
            return static_cast<RE::FreeCameraState*>(a_camera->currentState.get());  // NOLINT(*-static-cast-downcast)
        }

        // The free camera is placed where the preview wants it and nothing else moves it: the third person camera
        // turned out to have its free rotation turned off for some actors whatever the preview did, and its collision
        // to pull it in front of pillars. Switching to it or back can set whether time is frozen, so that's kept
        void ToggleFreeCamera(RE::PlayerCamera* a_camera) {
            auto& mainData{RE::Main::GetSingleton()->GetRuntimeData()};
            const bool frozen{mainData.freezeTime};
            a_camera->ToggleFreeCameraMode(false);
            mainData.freezeTime = frozen;
        }

        // Taken from Light Placer: attaching and updating nodes go through the game's task queue when it says they
        // must, the preview's tasks can run off the main thread
        void UpdateNode(RE::NiAVObject* a_object) {
            if (RE::TaskQueueInterface::ShouldUseTaskQueue()) {
                RE::TaskQueueInterface::GetSingleton()->QueueUpdateNiObject(a_object);
            } else {
                RE::NiUpdateData updateData;
                a_object->Update(updateData);
            }
        }

        void AttachNode(RE::NiNode* a_root, RE::NiAVObject* a_object) {
            if (RE::TaskQueueInterface::ShouldUseTaskQueue()) {
                RE::TaskQueueInterface::GetSingleton()->QueueNodeAttach(a_object, a_root);
            } else {
                a_root->AttachChild(a_object, false);
            }
            UpdateNode(a_object);
        }

        // Yaw is a heading: zero looks along +Y and it turns clockwise seen from above. Pitch looks down when positive
        RE::NiPoint3 Direction(const float a_yaw, const float a_pitch) {
            return {std::sin(a_yaw) * std::cos(a_pitch), std::cos(a_yaw) * std::cos(a_pitch), -std::sin(a_pitch)};
        }

        // Calls a_onHit with the reference of everything between the points, nearest first, null for the landscape,
        // until it returns false
        template <class OnHit>
        void ForEachHit(RE::bhkWorld* a_world, RE::NiPoint3 a_from, const RE::NiPoint3& a_to, OnHit&& a_onHit) {
            const float scale{RE::bhkWorld::GetWorldScale()};

            for (int i = 0; i < 8; ++i) {
                const auto segment{a_to - a_from};
                const float length{segment.Length()};
                if (length < 1.0F) return;

                RE::bhkPickData pick{};
                pick.rayInput.from = RE::hkVector4{a_from * scale};
                pick.rayInput.to = RE::hkVector4{a_to * scale};
                pick.rayInput.filterInfo.SetCollisionLayer(RE::COL_LAYER::kLOS);
                a_world->PickObject(pick);
                if (!pick.rayOutput.HasHit()) return;

                const auto* const collidable{pick.rayOutput.rootCollidable};
                if (!a_onHit(collidable ? RE::TESHavokUtilities::FindCollidableRef(*collidable) : nullptr)) return;

                // A bit past the hit, so the next ray doesn't hit the same thing
                a_from = a_from + segment * pick.rayOutput.hitFraction + segment * (4.0F / length);
            }
        }

        // How far the pose of a previewed NPC is played on, in steps of a frame, before time stays frozen for them
        constexpr int kSettleSteps{10};
        constexpr float kSettleStep{1.0F / 60.0F};

        // Whether nothing but actors is between the points
        bool IsClear(RE::bhkWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to) {
            bool clear{true};
            ForEachHit(a_world, a_from, a_to, [&](RE::TESObjectREFR* a_ref) {
                if (a_ref && a_ref->As<RE::Actor>()) return true;
                clear = false;
                return false;
            });
            return clear;
        }
    }  // namespace

    Session& Session::GetInstance() { return instance_; }

    // Their skeleton can be left in the default A-pose or halfway out of it.
    // Their animation is played for a few frames, so their skeleton takes the pose they're in
    void Session::SettlePose(RE::Actor* a_actor, RE::NiAVObject* a_root) {
        if (a_actor->IsPlayerRef()) return;

        for (int i = 0; i < kSettleSteps; ++i) {
            a_actor->UpdateAnimation(kSettleStep);
        }
        RE::NiUpdateData updateData;
        a_root->Update(updateData);
    }

    bool Session::Begin(RE::Actor* a_actor) {
        std::lock_guard lock{lock_};

        const auto& obody{Body::OBody::GetInstance()};
        auto* const root{a_actor && a_actor->Is3DLoaded() ? a_actor->Get3D(false) : nullptr};
        const bool female{a_actor && Body::OBody::IsFemale(a_actor)};

        if (!root || !obody.morphInterface) {
            End();
            return false;
        }

        // Nothing to show: the presets are for a body the actor doesn't have, UBE or not
        if (const bool ube{Body::OBody::IsUBE(a_actor)};
            !PresetManager::PresetContainer::GetInstance().HasPresetsFor(female, ube)) {
            logger::info("Preview: no {} {} presets for {}, nothing to preview", ube ? "UBE" : "non-UBE",
                         female ? "female" : "male", a_actor->GetDisplayFullName());
            End();
            return false;
        }

        EndActor(false);
        if (!sceneActive_) {
            EnterScene();
        }

        target_ = RE::NiPointer<RE::Actor>{a_actor};
        actor_ = target_;
        root_ = RE::NiPointer<RE::NiAVObject>{root};
        formID_ = a_actor->GetFormID();

        SettlePose(a_actor, root);

        SnapshotMorphs();

        // Rolled once, so the nipples and genitals don't change with every preset hovered
        if (female) {
            const auto merge = [&](const PresetManager::SliderSet& a_set) {
                for (const auto& [name, slider] : a_set) extraSliders_.insert_or_assign(name, slider);
            };
            if (obody.setNippleRand) merge(Body::OBody::GenerateRandomNippleSliders());
            if (obody.setGenitalRand) merge(Body::OBody::GenerateRandomGenitalSliders());
        }

        Present();

        logger::info(
            "Preview: began on {} ({:08X}), camera {}", a_actor->GetDisplayFullName(), a_actor->GetFormID(),
            presented_ ? "frames them" : (framingAvailable_ ? "waiting for the free camera" : "left as it was"));

        // Their copy is spawned now if it's wanted, they're shown until it's ready
        UpdateCopy();
        return true;
    }

    void Session::End(const bool a_keepMorphs) {
        std::lock_guard lock{lock_};
        EndActor(a_keepMorphs);
        LeaveScene();
    }

    void Session::Apply(const std::function<void()>& a_apply) {
        std::lock_guard lock{lock_};
        if (!actor_) {
            a_apply();
            return;
        }

        const bool undressed{!clothed_};
        EquipBack();

        a_apply();

        // The preset went on the target, their copy takes their new body
        // It wears what they wear, so their refit fits it too
        if (copyShown_) {
            auto* const morphInterface{Body::OBody::GetInstance().morphInterface};
            auto* const copy{actor_.get()};
            morphInterface->ClearBodyMorphKeys(copy, "OBody");
            morphInterface->ClearBodyMorphKeys(copy, "OClothe");

            // SnapshotMorphs reads who's shown
            morphs_.clear();
            actor_ = target_;
            SnapshotMorphs();
            actor_ = RE::NiPointer<RE::Actor>{copy};
            for (const auto& morph : morphs_) {
                morphInterface->SetMorph(copy, morph.name.c_str(), morph.key.c_str(), morph.value);
            }
        }

        // Their new body is what's put back when the preview ends, and what's shown until another preset is hovered
        morphs_.clear();
        SnapshotMorphs();
        requestedPreset_.clear();
        shownPreset_.clear();

        if (undressed) {
            Unequip();
        }
        // Their new refit only fits them while they wear their clothes. The copy's 3D is morphed again for its new
        // body
        if (undressed || copyShown_) {
            RestoreMorphs();
        }

        logger::info("Preview: a preset was applied to {}, the preview goes on{}{}", target_->GetDisplayFullName(),
                     copyShown_ ? " with their copy" : "", undressed ? ", undressed again" : "");
    }

    void Session::EndActor(const bool a_keepMorphs) {
        if (!actor_) return;

        RemoveLight();

        ShowAll(hiddenActors_);
        ShowAll(hiddenOccluders_);

        // The target was left as they were while their copy was shown, it only goes away
        if (!copyShown_) {
            EquipBack();

            if (!a_keepMorphs) {
                RestoreMorphs();
            }

            if (originalHeading_) {
                actor_->SetHeading(*originalHeading_);
                actor_->Update3DPosition(true);
                Physics::Reset();
            }
        }
        DeleteCopy();

        logger::info("Preview: ended on {}{}", target_->GetDisplayFullName(),
                     a_keepMorphs ? ", a preset was chosen" : "");
        ResetActor();
    }

    void Session::RunWhenMenuCloses(std::function<void()> a_func) {
        std::lock_guard lock{lock_};
        onMenuClosed_.push_back(std::move(a_func));
    }

    void Session::OnMenuClosed() {
        std::vector<std::function<void()>> ready;
        {
            std::lock_guard lock{lock_};
            ready.swap(onMenuClosed_);
        }
        if (ready.empty()) return;

        logger::info("Preview: the menu closed, doing the {} things that waited for it", ready.size());
        for (const auto& func : ready) {
            func();
        }
    }

    void Session::Abandon() {
        std::lock_guard lock{lock_};

        // What waited for the menu to close, and the disabled copies, were references that are going away
        onMenuClosed_.clear();
        disabledCopies_.clear();
        if (!sceneActive_) return;

        // The frozen time outlives the game that's being loaded. The free camera is switched off before, so the loaded
        // game doesn't start in it
        if (auto* const camera{RE::PlayerCamera::GetSingleton()};
            camera && cameraBackup_ && cameraBackup_->switched && camera->IsInFreeCameraMode()) {
            ToggleFreeCamera(camera);
        }
        if (frozeTime_) {
            RE::Main::GetSingleton()->GetRuntimeData().freezeTime = false;
        }
        SmoothCam::ReleaseCamera();
        // The framework still counts its blur, it takes it off once its windows close
        RestoreBlur();
        RestoreHUD();
        RemoveLight();
        // The player's 3D stays through the load
        ShowAll(hiddenActors_);
        ShowAll(hiddenOccluders_);

        logger::info("Preview: abandoned for a game load");
        ResetActor();
        ResetScene();
    }

    void Session::EnterScene() {
        sceneActive_ = true;

        auto* const camera{RE::PlayerCamera::GetSingleton()};

        // SmoothCam is asked last, it would keep the camera for nothing otherwise
        framingAvailable_ = camera && !REL::Module::IsVR() && SmoothCam::AcquireCamera();

        // Present frames the actor once the camera switched, which can take it a frame
        if (framingAvailable_) {
            cameraBackup_.emplace();
            if (const auto* const state{GetActiveFreeCameraState(camera)}) {
                cameraBackup_->translation = state->translation;
                cameraBackup_->rotation = state->rotation;
            } else {
                ToggleFreeCamera(camera);
                cameraBackup_->switched = true;
            }
        }

        // SKSE Menu Framework freezes time while a blocking window is open, unless its FreezeTimeOnMenu setting is
        // off, then the actor walks away
        if (auto& mainData{RE::Main::GetSingleton()->GetRuntimeData()}; !mainData.freezeTime) {
            mainData.freezeTime = true;
            frozeTime_ = true;
        }

        // SKSE Menu Framework blurs the game for as long as a blocking window is open, unless its
        // BlurBackgroundOnMenu setting is off. Its blur is taken off here and given back in LeaveScene, which always
        // runs before the framework takes its own off, the frame after its windows close
        if (auto* const blurManager{RE::UIBlurManager::GetSingleton()};
            blurManager && blurManager->blurCount > 0 && SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
            blurManager->DecrementBlurCount();
            removedBlur_ = true;
        }

        // Subtitles, crosshair prompts and the compass would cover the actor
        if (auto* const ui{RE::UI::GetSingleton()}) {
            if (auto hud{ui->GetMenu(RE::HUDMenu::MENU_NAME)}; hud && hud->uiMovie && hud->uiMovie->GetVisible()) {
                hud->uiMovie->SetVisible(false);
                hiddenHUD_ = std::move(hud);
            }
        }

        logger::info("Preview: scene began, the camera {}, time {}, blur {}, HUD {}",
                     !framingAvailable_        ? "is left alone"
                     : cameraBackup_->switched ? (camera->IsInFreeCameraMode() ? "switched to the free camera"
                                                                               : "is switching to the free camera")
                                               : "was in the free camera already",
                     frozeTime_ ? "frozen" : "already frozen", removedBlur_ ? "removed" : "not found",
                     hiddenHUD_ ? "hidden" : "not shown");
    }

    void Session::RestoreBlur() {
        if (!removedBlur_) return;
        removedBlur_ = false;

        if (auto* const blurManager{RE::UIBlurManager::GetSingleton()}) {
            blurManager->IncrementBlurCount();
        }
    }

    void Session::RestoreHUD() {
        if (hiddenHUD_ && hiddenHUD_->uiMovie) {
            hiddenHUD_->uiMovie->SetVisible(true);
        }
        hiddenHUD_.reset();
    }

    void Session::LeaveScene() {
        if (!sceneActive_) return;

        RestoreCamera();
        SmoothCam::ReleaseCamera();
        RestoreBlur();
        RestoreHUD();

        if (frozeTime_) {
            RE::Main::GetSingleton()->GetRuntimeData().freezeTime = false;
        }

        ResetScene();
    }

    void Session::Present() {
        if (!actor_ || !framingAvailable_) return;

        if (!GetActiveFreeCameraState(RE::PlayerCamera::GetSingleton())) {
            presented_ = false;
            return;
        }

        presented_ = true;
        HideOtherActors();
        ChooseView();
        ApplyFraming();
    }

    void Session::HideOtherActors() {
        ShowAll(hiddenActors_);

        const auto hide = [&](RE::Actor* a_actor) {
            if (!a_actor || a_actor == actor_.get() || !a_actor->Is3DLoaded()) return;
            if (auto* const object{a_actor->Get3D()}; object && !object->GetAppCulled()) {
                object->SetAppCulled(true);
                hiddenActors_.emplace_back(object);
            }
        };

        // Their body can still be hidden from first person, which the camera's update undoes, so it's taken either
        // way. The camera puts it back as it should be once the preview gives it back
        if (auto* const player{RE::PlayerCharacter::GetSingleton()}; player != actor_.get()) {
            if (auto* const body{player->Get3D(false)}) {
                body->SetAppCulled(true);
                hiddenActors_.emplace_back(body);

                // Each piece of them too: something, likely the game's camera, shows their root again every frame,
                // after the camera update that hides it, so hiding the root alone depends on what runs first. Pieces
                // the game hides itself are left to it
                RE::BSVisit::TraverseScenegraphGeometries(body, [&](RE::BSGeometry* a_geometry) {
                    if (!a_geometry->GetAppCulled()) {
                        a_geometry->SetAppCulled(true);
                        hiddenActors_.emplace_back(a_geometry);
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            }
        }
        if (auto* const processLists{RE::ProcessLists::GetSingleton()}) {
            for (const auto& handle : processLists->highActorHandles) {
                hide(handle.get().get());
            }
        }
    }

    void Session::UpdateOccluders(const RE::NiPoint3& a_cameraPosition) {
        auto* const actor{actor_.get()};
        auto* const cell{actor->GetParentCell()};
        auto* const world{cell ? cell->GetbhkWorld() : nullptr};
        if (!world) return;

        // Rays to the body from the knees to the head
        std::vector<RE::NiAVObject*> inTheWay;
        const auto origin{actor->GetPosition()};
        const float height{ActorHeight(actor)};
        for (const float part : {0.2F, 0.35F, 0.5F, 0.65F, 0.85F}) {
            const RE::NiPoint3 to{origin.x, origin.y, origin.z + height * part};
            ForEachHit(world, a_cameraPosition, to, [&](RE::TESObjectREFR* a_ref) {
                if (!a_ref) return true;
                if (a_ref == actor) return false;
                // The other actors are hidden already
                if (a_ref->As<RE::Actor>()) return true;

                // Pillars, barrels and counters are hidden, not pieces of the building: one often has the floor the
                // actor stands on with its walls, and hiding it left them over nothing
                auto* const object{a_ref->Get3D()};
                if (object && object->worldBound.radius > height * kMaxOccluderSize) return true;

                if (object && !std::ranges::contains(inTheWay, object)) {
                    inTheWay.push_back(object);
                }
                return true;
            });
        }

        // What's still in the way stays hidden, what no longer is is shown
        std::vector<RE::NiPointer<RE::NiAVObject>> hidden;
        for (auto& object : hiddenOccluders_) {
            if (std::ranges::contains(inTheWay, object.get())) {
                hidden.push_back(std::move(object));
            } else {
                object->SetAppCulled(false);
            }
        }
        for (auto* const object : inTheWay) {
            // Already culled by the game, not the preview's to show again
            if (object->GetAppCulled()) continue;
            object->SetAppCulled(true);
            logger::info("Preview: hid {}, {:.0f} units across, in front of the actor", object->name.c_str(),
                         object->worldBound.radius * 2.0F);
            hidden.emplace_back(object);
        }
        hiddenOccluders_ = std::move(hidden);
    }

    void Session::ShowAll(std::vector<RE::NiPointer<RE::NiAVObject>>& a_hidden) {
        for (const auto& object : a_hidden) {
            object->SetAppCulled(false);
        }
        a_hidden.clear();
    }

    void Session::ChooseView() {
        // In front of them, unless something is in the way
        View chosen{};

        auto* const actor{actor_.get()};
        auto* const cell{actor->GetParentCell()};
        if (auto* const world{cell ? cell->GetbhkWorld() : nullptr}) {
            const auto origin{actor->GetPosition()};
            const RE::NiPoint3 middle{origin.x, origin.y, origin.z + ActorHeight(actor) * kBodyMiddle};
            bool found{false};

            // The nearest to their front, as far out as possible. What's in the way would be hidden, but a view
            // without it shows them where they stand
            for (const float zoom : {1.0F, 0.75F, 0.55F}) {
                View candidate{};
                candidate.zoom = zoom;

                for (const float degrees :
                     {0.0F, 30.0F, -30.0F, 60.0F, -60.0F, 90.0F, -90.0F, 120.0F, -120.0F, 150.0F, -150.0F, 180.0F}) {
                    candidate.orbit = kPi + degrees * kPi / 180.0F;

                    if (IsClear(world, middle, CameraPosition(candidate))) {
                        chosen = candidate;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        // Where the camera starts, it doesn't ease in from the last actor's
        defaultView_ = targetView_ = view_ = chosen;
    }

    Session::Framing Session::ComputeFraming(const View& a_view) const {
        const float height{ActorHeight(actor_.get())};
        const auto fov{FieldOfView()};

        // Far enough for the body to fill kBodyFill of the screen's height
        const float distance{height * 0.5F / (kBodyFill * fov.tanHalfHeight) * a_view.zoom};

        return {
            .distance = distance,
            // Moving the camera to the actor's left puts them on the right of the screen. The pan moves it from there
            .offset = {-kScreenPosition * fov.tanHalfWidth * distance + a_view.panRight, 0.0F, a_view.panUp},
        };
    }

    float Session::CameraYaw(const View& a_view) const { return actor_->GetAngleZ() + a_view.orbit; }

    RE::NiPoint3 Session::CameraPosition(const View& a_view) const {
        const auto origin{actor_->GetPosition()};
        const RE::NiPoint3 middle{origin.x, origin.y, origin.z + ActorHeight(actor_.get()) * kBodyMiddle};
        const auto framing{ComputeFraming(a_view)};
        const float yaw{CameraYaw(a_view)};
        const RE::NiPoint3 right{std::cos(yaw), -std::sin(yaw), 0.0F};

        // Back from the middle of the body along the way it looks, so turning or pitching swings it around them
        return middle - Direction(yaw, a_view.pitch) * framing.distance + right * framing.offset.x +
               RE::NiPoint3{0.0F, 0.0F, framing.offset.z};
    }

    void Session::ApplyFraming() {
        if (!presented_) return;

        occludersDirty_ = true;
        QueueCameraUpdate();
    }

    void Session::QueueCameraUpdate() {
        if (cameraUpdateQueued_.exchange(true)) return;

        // Updating the camera while ImGui draws can change how the frame renders, the task runs with the game's
        SKSE::GetTaskInterface()->AddTask([this] {
            std::lock_guard lock{lock_};
            cameraUpdateQueued_ = false;
            UpdateCamera();
        });
    }

    void Session::UpdateCamera() {
        if (!presented_) return;

        auto* const camera{RE::PlayerCamera::GetSingleton()};
        auto* const state{GetActiveFreeCameraState(camera)};
        if (!state) return;

        // Set right before the update, so whatever else ran since can't have it anywhere else.
        // rotation.x is the pitch and rotation.y the yaw, as Photo Mode sets them
        // pitch looks down when positive, the yaw is a heading, as Direction has them
        state->translation = CameraPosition(view_);
        state->rotation.x = view_.pitch;
        state->rotation.y = CameraYaw(view_);
        camera->Update();

        // The camera's update can show the player again, and fade in actors near it
        for (const auto& object : hiddenActors_) {
            object->SetAppCulled(true);
        }

        if (occludersDirty_) {
            occludersDirty_ = false;
            UpdateOccluders(state->translation);
        }
    }

    void Session::RestoreCamera() {
        auto* const camera{RE::PlayerCamera::GetSingleton()};

        if (!cameraBackup_ || !camera) return;
        const auto& backup{*cameraBackup_};

        // Switching back returns to the camera it was switched from, first or third person, as the player left it.
        // Only if it's still on, switching it would turn it on otherwise
        if (backup.switched) {
            if (camera->IsInFreeCameraMode()) {
                ToggleFreeCamera(camera);
            }
        } else if (auto* const state{GetActiveFreeCameraState(camera)}) {
            state->translation = backup.translation;
            state->rotation = backup.rotation;
        }

        // Once, so the camera is back where it was before the game moves on
        SKSE::GetTaskInterface()->AddTask([] {
            if (auto* const playerCamera{RE::PlayerCamera::GetSingleton()}) {
                playerCamera->Update();
            }
        });
    }

    bool Session::IsActive() const {
        std::lock_guard lock{lock_};
        return sceneActive_ || actor_ != nullptr;
    }

    bool Session::IsPreviewing(const RE::Actor* a_actor) const {
        const auto formID{formID_.load()};
        return a_actor && formID != 0 && a_actor->GetFormID() == formID;
    }

    void Session::RequestPreset(const std::string& a_name) {
        std::lock_guard lock{lock_};
        if (a_name != requestedPreset_) {
            requestedPreset_ = a_name;
            requestTime_ = std::chrono::steady_clock::now();
        }
    }

    std::string Session::GetShownPreset() const {
        std::lock_guard lock{lock_};
        return shownPreset_;
    }

    bool Session::IsClothed() const {
        std::lock_guard lock{lock_};
        return clothed_;
    }

    bool Session::IsLit() const {
        std::lock_guard lock{lock_};
        return lit_;
    }

    void Session::SetLit(const bool a_lit) {
        std::lock_guard lock{lock_};
        lit_ = a_lit;

        if (!a_lit) {
            RemoveLight();
        } else if (actor_) {
            QueueLightUpdate();
        }
    }

    void Session::QueueLightUpdate() {
        if (lightUpdateQueued_.exchange(true)) return;

        // Adding to the scene while ImGui draws could change the frame being rendered, the task runs with the game's
        SKSE::GetTaskInterface()->AddTask([this] {
            std::lock_guard lock{lock_};
            lightUpdateQueued_ = false;
            UpdateLight();
        });
    }

    void Session::UpdateLight() {
        if (!lit_ || !actor_ || !root_) return;

        auto* const root{root_->AsNode()};
        const auto* const camera{RE::PlayerCamera::GetSingleton()};
        if (!root || !camera || !camera->cameraRoot) return;

        const float height{ActorHeight(actor_.get())};

        if (!light_) {
            auto* const shadowSceneNode{RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]};
            if (!shadowSceneNode) return;

            light_ = RE::NiPointer{RE::NiPointLight::Create()};
            if (!light_) return;

            light_->name = kLightName;
            auto& data{light_->GetLightRuntimeData()};
            // Ambient would brighten the whole scene evenly
            data.ambient = RE::NiColor{};
            data.diffuse = kLightColor;
            const float radius{height * kLightRadius};
            data.radius = RE::NiPoint3{radius, radius, radius};
            data.fade = kLightFade;
            light_->SetLightAttenuation(radius);

            AttachNode(root, light_.get());

            // No shadows: the game draws only a few shadowed lights at a time, and they cost the most
            const RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{
                .dynamic = true,
                .shadowLight = false,
                .portalStrict = false,
                .affectLand = true,
                .affectWater = false,
                .neverFades = true,
                .fov = RE::deg_to_rad(90.0F),
                .falloff = 1.0F,
                .nearDistance = 5.0F,
                .depthBias = 0.0F,
                .sceneGraphIndex = 0,
                .restrictedNode = nullptr,
                .lensFlareData = nullptr,
            };
            bsLight_ = RE::NiPointer{shadowSceneNode->AddLight(light_.get(), params)};
            if (!bsLight_) {
                logger::warn("Preview: the scene didn't take the light added on {}", actor_->GetDisplayFullName());
            }
        }

        // Between the body and the camera, a little above, so the side seen is the side lit wherever the camera goes
        const auto origin{actor_->GetPosition()};
        const RE::NiPoint3 aimed{origin.x, origin.y, origin.z + height * kLitHeight};
        auto toCamera{camera->cameraRoot->world.translate - aimed};
        const float distance{toCamera.Unitize()};
        const auto position{aimed + toCamera * (std::min)(distance * 0.6F, height * 1.2F) +
                            RE::NiPoint3{0.0F, 0.0F, height * 0.25F}};

        light_->local.translate = root->world.Invert() * position;
        UpdateNode(light_.get());
    }

    void Session::RemoveLight() {
        if (bsLight_) {
            if (auto* const shadowSceneNode{RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]}) {
                shadowSceneNode->RemoveLight(bsLight_);
            }
        }
        if (light_ && light_->parent) {
            light_->parent->DetachChild(light_.get());
        }
        bsLight_.reset();
        light_.reset();
    }

    void Session::SetClothed(const bool a_clothed) {
        std::lock_guard lock{lock_};
        if (!actor_ || a_clothed == clothed_) return;

        if (a_clothed) {
            EquipBack();
        } else {
            Unequip();
        }

        // The refit depends on what's worn
        Show(shownPreset_);
    }

    void Session::Orbit(const float a_across, const float a_down) {
        std::lock_guard lock{lock_};
        if (!presented_) return;

        // The camera goes around the actor rather than the actor turning: while time is frozen, physics bones stay
        // where they were last simulated, and turning the actor would leave them behind
        targetView_.orbit = std::remainder(targetView_.orbit + a_across * kOrbitPerScreen, 2.0F * kPi);
        targetView_.pitch = std::clamp(targetView_.pitch + a_down * kPitchPerScreen, kMinPitch, kMaxPitch);
    }

    void Session::Pan(const float a_across, const float a_down) {
        std::lock_guard lock{lock_};
        if (!presented_) return;

        // What the screen's height spans where the camera looks, so the actor moves along with the cursor
        const auto framing{ComputeFraming(targetView_)};
        const float perScreen{2.0F * framing.distance * FieldOfView().tanHalfHeight};
        const float height{ActorHeight(actor_.get())};

        // Dragging right shows the actor further right, so the camera looks further left
        targetView_.panRight = std::clamp(targetView_.panRight - a_across * perScreen, -height, height);
        // From the feet to the top of the head
        targetView_.panUp = std::clamp(targetView_.panUp + a_down * perScreen, -0.55F * height, 0.55F * height);
    }

    void Session::Zoom(const float a_steps) {
        std::lock_guard lock{lock_};
        if (!presented_) return;

        targetView_.zoom = std::clamp(targetView_.zoom - a_steps * kZoomStep, kMinZoom, kMaxZoom);
    }

    void Session::ResetView() {
        std::lock_guard lock{lock_};
        if (!presented_) return;

        targetView_ = defaultView_;
    }

    void Session::Recenter() {
        std::lock_guard lock{lock_};
        if (!presented_) return;

        // Back to looking at the middle of the body, from where the camera is around them and as close
        targetView_.panRight = defaultView_.panRight;
        targetView_.panUp = defaultView_.panUp;
        targetView_.pitch = defaultView_.pitch;
    }

    bool Session::EaseView() {
        const auto now{std::chrono::steady_clock::now()};
        const float seconds{(std::min)(std::chrono::duration<float>(now - lastEase_).count(), 0.1F)};
        lastEase_ = now;

        // Most of the way there in a fifth of a second, however often frames come
        const float blend{1.0F - std::exp(-kEaseRate * seconds)};
        bool moved{false};
        const auto ease = [&](float& a_current, const float a_target, const float a_close) {
            const float left{a_target - a_current};
            if (std::abs(left) <= a_close) {
                moved = moved || left != 0.0F;
                a_current = a_target;
                return;
            }
            a_current += left * blend;
            moved = true;
        };

        // The short way around
        const float orbitLeft{std::remainder(targetView_.orbit - view_.orbit, 2.0F * kPi)};
        float orbit{0.0F};
        ease(orbit, orbitLeft, 0.001F);
        view_.orbit = std::remainder(view_.orbit + orbit, 2.0F * kPi);

        ease(view_.pitch, targetView_.pitch, 0.001F);
        ease(view_.zoom, targetView_.zoom, 0.001F);
        ease(view_.panRight, targetView_.panRight, 0.05F);
        ease(view_.panUp, targetView_.panUp, 0.05F);
        return moved;
    }

    void Session::FaceCamera() {
        std::lock_guard lock{lock_};
        if (!presented_) return;

        // The camera stands at the actor's heading plus the orbit. Facing it is the orbit of their front, so the
        // heading takes what the orbit gives up and the camera doesn't move
        auto* const actor{actor_.get()};
        const float heading{actor->GetAngleZ()};
        if (!originalHeading_) {
            originalHeading_ = heading;
        }

        const float turn{std::remainder(view_.orbit - kPi, 2.0F * kPi)};
        actor->SetHeading(std::remainder(heading + turn, 2.0F * kPi));
        actor->Update3DPosition(true);
        Physics::Reset();
        // At once, the camera mustn't swing around while they turn. Reset view keeps them facing it
        targetView_.orbit = std::remainder(targetView_.orbit - turn, 2.0F * kPi);
        view_.orbit = kPi;
        defaultView_.orbit = kPi;

        ApplyFraming();
    }

    void Session::Update() {
        std::lock_guard lock{lock_};
        if (!actor_) return;

        // Unloaded, or their 3D was rebuilt, so what the session holds of it is stale. The menu begins again with
        // them next frame if they're still loaded
        if (actor_->IsDeleted() || !actor_->Is3DLoaded() || actor_->Get3D(false) != root_.get()) {
            logger::info("Preview: the 3D of {}{} changed, ending the preview", copyShown_ ? "the copy of " : "",
                         actor_->GetDisplayFullName());
            EndActor(false);
            return;
        }

        UpdateCopy();

        if (framingAvailable_) {
            if (!presented_) {
                // Waiting for the switch to the free camera
                Present();
            } else if (EaseView()) {
                ApplyFraming();
            } else {
                // Placed again even when it doesn't move, so nothing else moves it and the player stays hidden
                QueueCameraUpdate();
            }
        }

        // The light follows the camera
        if (lit_) {
            QueueLightUpdate();
        }

        if (requestedPreset_ != shownPreset_ && std::chrono::steady_clock::now() - requestTime_ >= kPresetDelay) {
            Show(requestedPreset_);
        }
    }

    void Session::SnapshotMorphs() {
        auto* const morphInterface{Body::OBody::GetInstance().morphInterface};

        // Racemenu holds its lock while visiting, so the keys are read after the names are collected
        struct NameCollector final : SKEE::IBodyMorphInterface::MorphVisitor {
            void Visit(RE::TESObjectREFR*, const char* a_name) override {
                if (a_name) names.emplace_back(a_name);
            }
            std::vector<std::string> names;
        } nameCollector;
        morphInterface->VisitMorphs(actor_.get(), nameCollector);

        struct KeyCollector final : SKEE::IBodyMorphInterface::MorphKeyVisitor {
            void Visit(const char* a_key, const float a_value) override {
                if (a_key && (stl::cmp(a_key, "OBody") || stl::cmp(a_key, "OClothe"))) {
                    morphs->push_back({*name, a_key, a_value});
                }
            }
            std::vector<MorphValue>* morphs{};
            const std::string* name{};
        } keyCollector;
        keyCollector.morphs = &morphs_;

        for (const auto& name : nameCollector.names) {
            keyCollector.name = &name;
            morphInterface->VisitKeys(actor_.get(), name.c_str(), keyCollector);
        }
    }

    void Session::RestoreMorphs() {
        auto* const morphInterface{Body::OBody::GetInstance().morphInterface};
        auto* const actor{actor_.get()};

        morphInterface->ClearBodyMorphKeys(actor, "OBody");
        morphInterface->ClearBodyMorphKeys(actor, "OClothe");

        for (const auto& morph : morphs_) {
            // Their refit only fits them while they wear their clothes
            if (!clothed_ && morph.key == "OClothe") continue;
            morphInterface->SetMorph(actor, morph.name.c_str(), morph.key.c_str(), morph.value);
        }

        if (actor->Is3DLoaded()) {
            morphInterface->ApplyBodyMorphs(actor, true);
        }
    }

    void Session::Show(const std::string& a_name) {
        // Not shown again even if it fails, so it isn't attempted every frame
        shownPreset_ = a_name;

        if (a_name.empty()) {
            RestoreMorphs();
            return;
        }

        auto* const actor{actor_.get()};
        const bool female{Body::OBody::IsFemale(actor)};
        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        // GetPresetByName falls back to a random preset, which isn't what was hovered
        auto preset{
            PresetManager::GetPresetByName(female ? presetContainer.allFemalePresets : presetContainer.allMalePresets,
                                           a_name, female, Body::OBody::IsUBE(actor))};
        if (!preset || preset->name != a_name) return;

        Body::OBody::GetInstance().PreviewPreset(actor, *preset, extraSliders_,
                                                 clothed_ && !Body::OBody::IsNaked(actor, false, nullptr));
    }

    void Session::Unequip() {
        auto* const actor{actor_.get()};

        // Collected first, the entries change as items are unequipped
        for (const auto* const entry : stl::GetWornEntries(actor)) {
            const auto* const armor{entry->object->As<RE::TESObjectARMO>()};
            if (!armor || !IsClothing(armor)) continue;

            const RE::ExtraDataList* wornList{nullptr};
            if (entry->extraLists) {
                for (const auto* const extraList : *entry->extraLists) {
                    if (extraList && (extraList->HasType(RE::ExtraDataType::kWorn) ||
                                      extraList->HasType(RE::ExtraDataType::kWornLeft))) {
                        wornList = extraList;
                        break;
                    }
                }
            }
            unequipped_.push_back({entry->object, wornList});
        }

        auto* const equipManager{RE::ActorEquipManager::GetSingleton()};
        for (const auto& item : unequipped_) {
            if (!equipManager->UnequipObject(actor, item.object, const_cast<RE::ExtraDataList*>(item.extraList), 1,
                                             nullptr, false, true, false, true)) {
                logger::warn("Preview: {} ({:08X}) couldn't be taken off {}", item.object->GetName(),
                             item.object->GetFormID(), actor->GetDisplayFullName());
            }
        }

        clothed_ = false;
        if (!unequipped_.empty()) {
            QueueWornUpdate();
        }
    }

    void Session::QueueWornUpdate() {
        // The inventory changes at once, but the game builds the worn 3D for it as the actor updates, which they
        // don't while time is frozen, so the clothes stayed on screen until the menu closed. The 3D is rebuilt in the
        // game's task phase, as SKSE does for QueueNiNodeUpdate. Racemenu morphs the pieces as they're attached
        SKSE::GetTaskInterface()->AddTask([this, handle{actor_->GetHandle()}] {
            const auto actor{handle.get()};
            if (!actor || !actor->Is3DLoaded()) return;

            actor->Update3DModel();

            // The actor's nodes aren't updated while time is frozen either, so the new pieces had no place or bounds
            // and weren't drawn
            if (auto* const root{actor->Get3D(false)}) {
                RE::NiUpdateData updateData;
                root->Update(updateData);
            }
        });
    }

    void Session::EquipBack() {
        auto* const actor{actor_.get()};
        auto* const equipManager{RE::ActorEquipManager::GetSingleton()};
        const auto* const changes{actor->GetInventoryChanges(true)};

        for (const auto& item : unequipped_) {
            // The item's list may be gone since it was unequipped, so it's only used if the inventory still has it
            RE::ExtraDataList* extraList{nullptr};
            if (item.extraList && changes && changes->entryList) {
                for (const auto* const entry : *changes->entryList) {
                    if (!entry || entry->object != item.object || !entry->extraLists) continue;
                    for (auto* const candidate : *entry->extraLists) {
                        if (candidate == item.extraList) {
                            extraList = candidate;
                            break;
                        }
                    }
                }
            }

            equipManager->EquipObject(actor, item.object, extraList, 1, nullptr, false, false, false, true);
        }
        if (!unequipped_.empty()) {
            QueueWornUpdate();
        }

        unequipped_.clear();
        clothed_ = true;
    }

    void Session::ResetActor() {
        formID_ = 0;
        ResetCopy();
        target_.reset();
        actor_.reset();
        root_.reset();
        morphs_.clear();
        extraSliders_.clear();
        requestedPreset_.clear();
        shownPreset_.clear();
        clothed_ = true;
        unequipped_.clear();
        originalHeading_.reset();
        bring_ = false;
        presented_ = false;
    }

    void Session::ResetScene() {
        sceneActive_ = false;
        frozeTime_ = false;
        framingAvailable_ = false;
        presented_ = false;
        cameraBackup_.reset();
        view_ = targetView_ = defaultView_ = View{};
    }
}  // namespace Preview
