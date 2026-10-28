#include "Body/Body.h"

#include "JSONParser/JSONParser.h"
#include "STL.h"

#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>

using namespace PresetManager;

Body::OBody Body::OBody::instance_;

namespace Body {
    namespace {
        // Actors that already have an ORefit update queued, see UpdateClothePreset
        boost::concurrent_flat_set<RE::FormID> g_queuedClotheUpdates;

        // Actors generated before their 3D was built while wearing nothing. Their outfit can be equipped while their
        // 3D loads, so OnActorNaked is only sent once it's loaded if they are still naked
        boost::concurrent_flat_set<RE::FormID> g_nakedBeforeLoad3D;

        // Whether the body of a skin, worn by a race, is UBE's. Keyed by the skin's form ID in the upper 32 bits and
        // the race's in the lower ones. One map per sex, because each sex has its own mesh
        boost::concurrent_flat_map<std::uint64_t, bool> g_ubeBodies[RE::SEXES::kTotal];

        // UBE keeps all its body meshes in meshes\!UBE
        bool IsUBEMesh(const char* a_model) {
            return a_model && (stl::contains(a_model, "!UBE\\"sv) || stl::contains(a_model, "!UBE/"sv));
        }

        // The NPC records an actor's preset can be configured on, most specific first. An actor spawned from a
        // leveled list has a temporary base (0xFF______) that no config can name, so after the actor's own base this
        // adds the NPC the leveled list picked (ExtraLeveledCreature::templateBase) and the placed record
        // (originalBase), each followed by the NPCs it inherits from through its template (TPLT) chain
        std::vector<const RE::TESNPC*> GetLookupNPCs(RE::Actor* a_actor) {
            std::vector<const RE::TESNPC*> npcs;

            const auto addWithTemplates = [&](const RE::TESForm* a_form) {
                // Template chains are short; the cap only guards against a malformed cycle
                for (int depth{0}; a_form && depth < 16; ++depth) {
                    const auto* const npc{a_form->As<RE::TESNPC>()};
                    if (!npc) break;  // a leveled list (LVLN) ends the chain
                    if (std::ranges::find(npcs, npc) == npcs.end()) npcs.push_back(npc);
                    a_form = npc->baseTemplateForm;
                }
            };

            addWithTemplates(a_actor->GetActorBase());

            if (const auto* const leveled{a_actor->extraList.GetByType<RE::ExtraLeveledCreature>()}) {
                addWithTemplates(leveled->templateBase);
                addWithTemplates(leveled->originalBase);
            }

            return npcs;
        }
    }  // namespace

    OBody& OBody::GetInstance() { return instance_; }

    bool OBody::SetMorphInterface(SKEE::IBodyMorphInterface* a_morphInterface) {
        return (morphInterface = a_morphInterface->GetVersion() ? a_morphInterface : nullptr);
    }

    void OBody::CacheSkinMorphFiles() const {
        // When Racemenu attempts to morph a mesh, it must first read the .tri file of that mesh
        // These meshes can be rather heavy (for example, BHUNP female body mesh is 14 MB),
        // so reading them can lead to hitches/stutters.
        // Therefore, we cache the body meshes for the entirety of the game's session, so Racemenu doesn't evict them
        // This eliminates any stutter related to reading the body meshes, which are always needed for OBody to function
        const auto start{std::chrono::steady_clock::now()};

        const auto& presetContainer{PresetContainer::GetInstance()};

        std::vector<std::string> files;
        const auto addFileOf = [&](const char* a_model) {
            // BodySlide names the .tri like the mesh, without its _0/_1 weight suffix
            std::string file{a_model ? a_model : ""};
            boost::algorithm::to_lower(file);
            if (!file.ends_with(".nif")) return;

            file.resize(file.size() - 4);
            if (file.ends_with("_0") || file.ends_with("_1")) file.resize(file.size() - 2);
            file += ".tri";

            if (std::ranges::contains(files, file) || !RE::BSResourceNiBinaryStream{"meshes\\" + file}.good()) return;
            files.push_back(std::move(file));
        };

        for (const auto* const race : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESRace>()) {
            if (!race || !race->skin || !race->HasKeywordString("ActorTypeNPC")) continue;

            for (const auto* const addon : race->skin->armorAddons) {
                if (!addon) continue;

                // Only the meshes that OBody has presets for: a UBE mesh needs UBE presets, the others the rest
                for (const auto sex : {RE::SEXES::kFemale, RE::SEXES::kMale}) {
                    const char* const model{addon->bipedModels[sex].GetModel()};
                    if (!presetContainer.HasPresetsFor(sex == RE::SEXES::kFemale, IsUBEMesh(model))) continue;

                    addFileOf(model);
                    addFileOf(addon->bipedModel1stPersons[sex].GetModel());
                }
            }
        }

        // Never released: the engine doesn't own the string given to the extra data, so it must never destroy it
        auto* const node{RE::NiNode::Create()};
        node->IncRefCount();
        auto* const bodyTri{RE::NiExtraData::Create<RE::NiStringExtraData>()};
        bodyTri->SetName("BODYTRI");
        node->AddExtraData(bodyTri);

        // The node has no meshes, so nothing gets morphed, the files are only read into Racemenu's cache
        auto* const player{RE::PlayerCharacter::GetSingleton()};
        for (auto& file : files) {
            bodyTri->value = file.data();
            morphInterface->ApplyVertexDiff(player, node, true);
        }
        bodyTri->value = nullptr;

        logger::info(
            "Had Racemenu read the .tri files of {} skin meshes in {} ms", files.size(),
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    }

    void OBody::SetMorph(RE::Actor* a_actor, const char* a_morphName, const char* a_key, const float a_value) const {
        morphInterface->SetMorph(a_actor, a_morphName, a_key, a_value);
    }

    float OBody::GetMorph(RE::Actor* a_actor, const char* a_morphName) const {
        return morphInterface->GetMorph(a_actor, a_morphName, "OBody");
    }

    void OBody::NotifyMorphApplied(RE::Actor* actor) const {
        if (auto* evSrc = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent ev{};
            ev.eventName = "Obody_ApplyMorph";
            ev.strArg = "ApplyMorphs";
            ev.numArg = 1.0F;
            ev.sender = actor;
            evSrc->SendEvent(&ev);
        }
    }

    void OBody::ApplyMorphs(RE::Actor* a_actor, const bool updateMorphsWithoutTimer,
                            const bool applyProcessedMorph) const {
        // If updateMorphsWithoutTimer is true, OBody NG will call the ApplyBodyMorphs function without waiting a random
        // amount of time. That is useful for undressing/redressing.
        // If performance mode is turned off, we also apply morphs randomly immediately no matter the context.

        RE::ActorHandle actorHandle{a_actor->GetHandle()};

        if (updateMorphsWithoutTimer || !setPerformanceMode) {
            if (RE::Actor* actor = actorHandle.get().get()) {
                if (applyProcessedMorph) {
                    SetMorph(actor, distributionKey.c_str(), "OBody", 1.0F);
                }

                // ReSharper disable once CppDFAConstantConditions
                if (actor && actor->Is3DLoaded()) {
                    morphInterface->ApplyBodyMorphs(actor, true);
                    NotifyMorphApplied(actor);
                }
            }
        } else {
            // ReSharper disable CppDFAUnreadVariable
            // ReSharper disable CppDFAUnusedValue
            auto actorName{a_actor->GetActorBase()->GetName()};

            // We do this to prevent stutters due to Racemenu attempting to update morphs for too many NPCs
            std::thread([this, actorHandle, actorName] {
                // Held for the whole thread, so the actor can't be deleted while it's used
                const auto actorPtr{actorHandle.get()};
                if (RE::Actor * actor{actorPtr.get()}) {
                    logger::info("Actor {} is valid, updating morphs now", actorName);

                    SetMorph(actor, distributionKey.c_str(), "OBody", 1.0F);

                    // ReSharper disable once CppDFAConstantConditions
                    if (actor && actor->Is3DLoaded() &&
                        !morphInterface->HasBodyMorph(actor, "obody_synthebd", "OBody")) {
                        morphInterface->ApplyBodyMorphs(actor, true);

                        NotifyMorphApplied(actor);
                    }
                } else {
                    logger::info("Actor {} is no longer valid, not updating morphs", actorName);
                }
            }).detach();
        }
    }

    void OBody::UpdateClothePreset(RE::Actor* a_actor, const bool a_removingArmor,
                                   const RE::TESForm* a_changedArmor) const {
        // Nothing attached yet, Racemenu morphs the armor addons as they are attached
        if (!a_actor->Is3DLoaded()) {
            RefreshClothePreset(a_actor, a_removingArmor, a_changedArmor);
            return;
        }

        // Equip events come in bursts, from threads that run other tasks, where ApplyBodyMorphs makes the game
        // stutter. For example, an outfit being put on piece by piece or an armor being taken off and put back again,
        // which ends up changing nothing. This makes sure the refit is only picked once for what the actor
        // ends up wearing, and the body is only updated if it actually ended up changing
        if (!g_queuedClotheUpdates.insert(a_actor->GetFormID())) {
            return;
        }

        SKSE::GetTaskInterface()->AddTask([this, formID = a_actor->GetFormID(), actorHandle = a_actor->GetHandle()] {
            g_queuedClotheUpdates.erase(formID);

            const auto actor{actorHandle.get()};
            if (!actor || !IsProcessed(actor.get()) || IsBlacklisted(actor.get())) {
                return;
            }

            if (RefreshClothePreset(actor.get(), false, nullptr)) {
                ApplyMorphs(actor.get(), true);
            }
        });
    }

    bool OBody::RefreshClothePreset(RE::Actor* a_actor, const bool a_removingArmor,
                                    const RE::TESForm* a_changedArmor) const {
        if (setRefit && !IsNaked(a_actor, a_removingArmor, a_changedArmor)) {
            // The refit is picked again for what they wear right now, because swapping outfits can change it
            if (!ApplyClothePreset(a_actor, true, a_removingArmor, a_changedArmor)) {
                return false;
            }

            logger::info("Applied clothed preset to actor {}", a_actor->GetName());
            return true;
        }

        if (!IsClotheActive(a_actor)) {
            return false;
        }

        logger::info("Removing clothed preset to actor {}", a_actor->GetName());
        RemoveClothePreset(a_actor);
        return true;
    }

    void OBody::ProcessActorLoaded3D(const RE::FormID a_formID) const {
        if (!g_nakedBeforeLoad3D.erase(a_formID)) {
            return;
        }

        if (auto* const actor{RE::TESForm::LookupByID<RE::Actor>(a_formID)}; actor && IsNaked(actor, false, nullptr)) {
            OnActorNaked.SendEvent(actor);
        }
    }

    void OBody::ProcessActorEquipEvent(RE::Actor* a_actor, const bool a_removingArmor,
                                       const RE::TESForm* a_equippedArmor) const {
        const bool isProcessed = IsProcessed(a_actor);
        const bool isBlacklisted = IsBlacklisted(a_actor);
        const bool clotheActive = IsClotheActive(a_actor);
        const bool naked = IsNaked(a_actor, a_removingArmor, a_equippedArmor);
        bool orefitIsApplied = clotheActive;
        bool female;

        if (!isProcessed | isBlacklisted) goto notifyNativeEventListeners;

        if (IsRemovingClothes(a_actor, a_removingArmor, a_equippedArmor)) {
            OnActorRemovingClothes.SendEvent(a_actor);
        }

        // if ORefit is disabled and actor has ORefit morphs, clear them right away.
        if (!setRefit & clotheActive) {
            UpdateClothePreset(a_actor, a_removingArmor, a_equippedArmor);
            orefitIsApplied = false;
            goto notifyNativeEventListeners;
        }

        female = IsFemale(a_actor);

        if (!PresetManager::PresetContainer::GetInstance().HasPresetsFor(female, IsUBE(a_actor))) {
            goto notifyNativeEventListeners;
        }

        if (!naked && a_removingArmor) {
            // Fires when removing their armor
            OnActorNaked.SendEvent(a_actor);
        }

        if ((clotheActive && naked) || (!naked && setRefit)) {
            UpdateClothePreset(a_actor, a_removingArmor, a_equippedArmor);
            orefitIsApplied = !naked;
        }

    notifyNativeEventListeners:
        // It's particularly important that we avoid sending events recursively for this event,
        // because if an event-listener equips or unequips armour in response to it it can
        // easily cause an infinite loop of `TESEquipEvent`s, which would freeze the game
        // until it crashes from a stack overflow.
        SendActorChangeEvent(
            a_actor,
            [&] {
                using Event = ::OBody::API::IActorChangeEventListener;

                Event::OnActorClothingUpdate::Payload payload{nullptr, a_equippedArmor};

                Event::OnActorClothingUpdate::Flags flags{};
                static_assert(Event::OnActorClothingUpdate::Flags::IsClothed == (1 << 0));
                static_assert(Event::OnActorClothingUpdate::Flags::IsORefitApplied == (1 << 1));
                static_assert(Event::OnActorClothingUpdate::Flags::IsORefitEnabled == (1 << 2));
                static_assert(Event::OnActorClothingUpdate::Flags::IsProcessed == (1 << 3));
                static_assert(Event::OnActorClothingUpdate::Flags::IsBlacklisted == (1 << 4));
                static_assert(Event::OnActorClothingUpdate::Flags::ActorIsEquipping == (1 << 5));
                flags = static_cast<Event::OnActorClothingUpdate::Flags>(flags | uint64_t(!naked));
                flags = static_cast<Event::OnActorClothingUpdate::Flags>(flags | (uint64_t(orefitIsApplied) << 1));
                flags = static_cast<Event::OnActorClothingUpdate::Flags>(flags | (uint64_t(setRefit) << 2));
                flags = static_cast<Event::OnActorClothingUpdate::Flags>(flags | (uint64_t(isProcessed) << 3));
                flags = static_cast<Event::OnActorClothingUpdate::Flags>(flags | (uint64_t(isBlacklisted) << 4));
                flags = static_cast<Event::OnActorClothingUpdate::Flags>(flags | (uint64_t(!a_removingArmor) << 5));

                return std::make_pair(flags, payload);
            },
            [](auto listener, auto actor, auto&& args) {
                listener->OnActorClothingUpdate(actor, args.first, args.second);
            });
    }

    void OBody::GenerateActorBody(RE::Actor* a_actor, ::OBody::API::IPluginInterface* responsibleInterface,
                                  const bool a_beforeLoad3D) const {
        // The main function of OBody NG

        // If actor is already processed, no need to do anything
        if (IsProcessed(a_actor)) {
            return;
        }

        bool female{IsFemale(a_actor)};
        const bool ube{IsUBE(a_actor)};

        auto& presetContainer{PresetManager::PresetContainer::GetInstance()};

        // If we have no presets for the actor's sex and body, then don't do anything
        if (!presetContainer.HasPresetsFor(female, ube)) {
            return;
        }

        auto& jsonParser{Parser::JSONParser::GetInstance()};

        auto actorBase{a_actor->GetActorBase()};
        auto actorName{actorBase->GetName()};

        // FormID-keyed config (npcFormID, blacklistedNpcsFormID) and npcPlugin* are also checked against the
        // records a leveled actor was generated from, see GetLookupNPCs
        const auto lookupNPCs{GetLookupNPCs(a_actor)};
        std::vector<RE::FormID> lookupIDs;
        lookupIDs.reserve(lookupNPCs.size());
        for (const auto* const npc : lookupNPCs) {
            lookupIDs.push_back(npc->GetFormID());
        }

        logger::info("Trying to find and apply preset to {}{}", actorName, ube ? " (UBE body)" : "");

        auto blacklistNPC = [&] {
            SetMorph(a_actor, distributionKey.c_str(), "OBody", 1.0F);
            SetMorph(a_actor, "obody_blacklisted", "OBody", 1.0F);

            // Clear their preset assignment, if they have one.
            auto& registry{ActorTracker::Registry::GetInstance()};
            uint32_t previousPresetIndex = 0;
            registry.stateForActor.visit(a_actor->formID, [&](auto& entry) {
                previousPresetIndex = entry.second.presetIndex;
                entry.second.presetIndex = 0;
            });

            if (previousPresetIndex != 0) {
                SendActorChangeEvent(
                    a_actor,
                    [&] {
                        using Event = ::OBody::API::IActorChangeEventListener;

                        Event::OnActorPresetChangedWithoutGeneration::Payload payload{
                            responsibleInterface,
                            // Note that the plugin-API mandates that this be a null-terminated string.
                            // Minus one because an index of zero assigned to the actor signifies the absence of a
                            // preset.
                            PresetManager::AssignedPresetIndex{previousPresetIndex - 1}.GetPresetNameView(female)};

                        auto flags = Event::OnActorPresetChangedWithoutGeneration::Flags::PresetWasUnassigned;

                        return std::make_pair(flags, payload);
                    },
                    [](auto listener, auto actor, auto&& args) {
                        listener->OnActorPresetChangedWithoutGeneration(actor, args.first, args.second);
                    });
            }
        };

        // If NPC is blacklisted, set him as processed
        if (jsonParser.IsNPCBlacklisted(actorName, lookupIDs)) {
            blacklistNPC();
            return;
        }

        // First, we attempt to get the NPC's preset from the keys npcFormID and npc from the JSON
        std::optional<Preset> preset{jsonParser.GetNPCPreset(actorName, lookupIDs, female, ube)};

        if (!preset.has_value()) {
            auto actorRace{stl::get_editorID(actorBase->GetRace()->As<RE::TESForm>())};
            auto actorClass{actorBase->npcClass ? stl::get_editorID(actorBase->npcClass->As<RE::TESForm>())
                                                : std::string{}};

            // if we can't find it, we check if the NPC is blacklisted by plugin name or by race
            if (jsonParser.IsNPCBlacklistedGlobally(a_actor, actorRace.c_str(), actorClass.c_str(), female)) {
                blacklistNPC();
                return;
            }

            // Next up, we check if we have a preset defined in one of the NPC's factions
            preset = jsonParser.GetNPCFactionPreset(actorBase, female, ube);

            // If that also fails, we check if we have a preset in the NPC's class
            if (!preset.has_value() && !actorClass.empty()) {
                preset = jsonParser.GetNPCClassPreset(actorClass.c_str(), female, ube);
            }

            // If that also fails, we check if we have a preset in the NPC's plugin
            // A leveled actor's temporary base belongs to no plugin, so its source records are checked too
            for (const auto* const npc : lookupNPCs) {
                if (preset.has_value()) break;
                preset = jsonParser.GetNPCPluginPreset(npc, actorName, female, ube);
            }

            // And if that also fails, we check if we have a preset in the NPC's race
            if (!preset.has_value()) {
                preset = jsonParser.GetNPCRacePreset(actorRace.c_str(), female, ube);
            }
        }

        // If we got here without a preset, then we just fetch one randomly
        if (!preset.has_value()) {
            logger::info("No preset defined for this actor, getting it randomly");
            preset = PresetManager::GetRandomPreset(
                female ? presetContainer.femalePresets : presetContainer.malePresets, ube);
        }

        // Can't happen, HasPresetsFor made sure there's one
        if (!preset.has_value()) {
            return;
        }

        logger::info("Preset {} will be applied to {}", preset->name, actorName);

        GenerateBodyByPreset(a_actor, *preset, false, responsibleInterface, a_beforeLoad3D);
    }

    void OBody::GenerateBodyByName(RE::Actor* a_actor, const std::string& a_name,
                                   ::OBody::API::IPluginInterface* responsibleInterface) const {
        const auto& presetContainer{PresetContainer::GetInstance()};

        // This is needed to prevent a crash with SynthEBD/Synthesis
        if (synthesisInstalled && a_actor != nullptr) {
            SetMorph(a_actor, "obody_synthebd", "OBody", 1.0F);
        }

        const bool female{IsFemale(a_actor)};
        auto preset{GetPresetByName(female ? presetContainer.allFemalePresets : presetContainer.allMalePresets, a_name,
                                    female, IsUBE(a_actor))};
        if (!preset) {
            logger::info("No preset fits {}, not changing their body", a_actor->GetName());
            return;
        }

        GenerateBodyByPreset(a_actor, *preset, true, responsibleInterface);
    }

    void OBody::GenerateBodyByPreset(RE::Actor* a_actor, PresetManager::Preset& a_preset,
                                     const bool updateMorphsWithoutTimer,
                                     ::OBody::API::IPluginInterface* responsibleInterface,
                                     const bool a_beforeLoad3D) const {
        auto& registry{ActorTracker::Registry::GetInstance()};
        auto formID = a_actor->formID;

        // Assign the preset to the actor.
        // Plus one because an index of zero on the actor signifies the absence of a preset.
        uint32_t actorPresetIndex = a_preset.assignedIndex.value + 1;
        ActorTracker::ActorState fallbackActorState{};
        fallbackActorState.presetIndex = actorPresetIndex;

        registry.stateForActor.emplace_or_visit(formID, fallbackActorState,
                                                [&](auto& entry) { entry.second.presetIndex = actorPresetIndex; });

        morphInterface->ClearBodyMorphKeys(a_actor, "OBody");
        morphInterface->ClearBodyMorphKeys(a_actor, "OClothe");

        // Apply the preset's sliders
        ApplySliderSet(a_actor, a_preset.sliders, "OBody");

        logger::info("Applying preset: {}; index: {}", a_preset.name, a_preset.assignedIndex.value);

        if (IsFemale(a_actor)) {
            // Generate random nipple sliders if needed
            if (setNippleRand) {
                PresetManager::SliderSet set{GenerateRandomNippleSliders()};
                ApplySliderSet(a_actor, set, "OBody");
            }

            if (setGenitalRand) {
                // Generate random genital sliders if needed
                PresetManager::SliderSet set{GenerateRandomGenitalSliders()};
                ApplySliderSet(a_actor, set, "OBody");
            }
        }

        bool isNaked = IsNaked(a_actor, false, nullptr);
        bool orefitIsApplied = false;

        // If not naked and if ORefit is turned on, apply ORefit morphing
        if (!isNaked) {
            if (setRefit) {
                logger::info("Not naked, adding cloth preset");
                ApplyClothePreset(a_actor);
                orefitIsApplied = true;
            }
        } else {
            logger::info("Actor is naked, not applying cloth preset");
            if (a_beforeLoad3D) {
                g_nakedBeforeLoad3D.insert(a_actor->GetFormID());
            } else {
                OnActorNaked.SendEvent(a_actor);
            }
        }

        if (a_beforeLoad3D) {
            // Racemenu morphs each armor addon as it's attached to the 3D that is about to be built,
            // so the costly ApplyBodyMorphs, which morphs all of them again, isn't needed
            logger::info("The 3D of {} isn't built yet, Racemenu morphs it as it's attached", a_actor->GetName());
            SetMorph(a_actor, distributionKey.c_str(), "OBody", 1.0F);
            NotifyMorphApplied(a_actor);
        } else {
            ApplyMorphs(a_actor, updateMorphsWithoutTimer);
        }

        SendActorChangeEvent(
            a_actor,
            [&] {
                using Event = ::OBody::API::IActorChangeEventListener;

                Event::OnActorGenerated::Payload payload{
                    responsibleInterface,
                    // Note that the plugin-API mandates that this be a null-terminated string.
                    {a_preset.name.data(), a_preset.name.size()}};

                Event::OnActorGenerated::Flags flags{};
                static_assert(Event::OnActorGenerated::Flags::IsClothed == (1 << 0));
                static_assert(Event::OnActorGenerated::Flags::IsORefitApplied == (1 << 1));
                static_assert(Event::OnActorGenerated::Flags::IsORefitEnabled == (1 << 2));
                flags = static_cast<Event::OnActorGenerated::Flags>(flags | uint64_t(!isNaked));
                flags = static_cast<Event::OnActorGenerated::Flags>(flags | (uint64_t(orefitIsApplied) << 1));
                flags = static_cast<Event::OnActorGenerated::Flags>(flags | (uint64_t(setRefit) << 2));

                return std::make_pair(flags, payload);
            },
            [](auto listener, auto actor, auto&& args) { listener->OnActorGenerated(actor, args.first, args.second); });

        OnActorGenerated.SendEvent(a_actor, a_preset.name);
    }

    void OBody::ApplySlider(RE::Actor* a_actor, const PresetManager::Slider& a_slider, const char* a_key,
                            const float a_weight) const {
        const float val{((a_slider.max - a_slider.min) * a_weight) + a_slider.min};
        morphInterface->SetMorph(a_actor, a_slider.name.c_str(), a_key, val);
    }

    void OBody::ApplySliderSet(RE::Actor* a_actor, PresetManager::SliderSet& a_sliders, const char* a_key) const {
        const float weight{GetWeight(a_actor)};
        for (const auto& slider : a_sliders | std::views::values) ApplySlider(a_actor, slider, a_key, weight);
    }

    bool OBody::ApplyClothePreset(RE::Actor* a_actor, const bool a_onlyIfChanged, const bool a_removingArmor,
                                  const RE::TESForm* a_changedArmor) const {
        const auto& presetContainer{PresetContainer::GetInstance()};

        const bool isFemale = IsFemale(a_actor);
        // UBE bodies only use refit presets made for UBE, the additive defaults being "UBE Female-Refit..."
        const bool ube = IsUBE(a_actor);
        const auto& allPresets{isFemale ? presetContainer.allFemalePresets : presetContainer.allMalePresets};

        // "Female-Refit" / "Male-Refit", or "UBE Female-Refit" / "UBE Male-Refit"
        const std::string sexDefaultName{std::string{SexRefitBaseName(isFemale, ube)} + "-Refit"};

        const auto findRefit = [&](const std::string_view a_name) -> std::optional<Preset> {
            logger::info("Looking for preset: {}", a_name);
            if (const auto* const preset{FindRefitPreset(allPresets, a_name, ube)}) {
                return *preset;
            }
            return std::nullopt;
        };

        auto& jsonParser{Parser::JSONParser::GetInstance()};

        // Pick the refit first, without touching the actor
        std::optional<Preset> refitPreset;
        const auto actorPreset{ActorTracker::Registry::GetInstance().GetPresetForActor(a_actor, isFemale)};
        bool differential{false};

        // "<Preset>-Refit<anything>" presets are full copies of a base preset
        // with some sliders adjusted. What gets applied while clothed is the difference between those sliders
        if (actorPreset) {
            // Outfit / keyword specific refit (refitOutfitPresets*, refitOutfitKeywords*)
            // picked by matching the actor's current preset
            refitPreset = jsonParser.GetRefitPresetFromEquippedItems(a_actor, isFemale, ube, actorPreset->name,
                                                                     a_changedArmor, a_removingArmor);

            // Otherwise the plain "<Preset>-Refit"
            if (!refitPreset) {
                const std::string perPresetName{actorPreset->name + "-Refit"};
                // "Female-Refit"/"Male-Refit" are additive defaults
                if (!stl::cmp(perPresetName, sexDefaultName)) {
                    refitPreset = findRefit(perPresetName);
                }
            }

            differential = refitPreset.has_value();
        }

        // "Female-Refit <Keyword>" / "Male-Refit <Keyword>"
        // or whatever the user listed in the JSON. Applied additively, so it works for any preset
        if (!refitPreset) {
            refitPreset =
                jsonParser.GetRefitDefaultFromEquippedItems(a_actor, isFemale, ube, a_changedArmor, a_removingArmor);
        }

        // Universal additive default: Female-Refit / Male-Refit (UBE Female-Refit / UBE Male-Refit)
        // Applied additively, so it works for any preset
        if (!refitPreset) {
            refitPreset = findRefit(sexDefaultName);
        }

        // The sliders of what was picked. If nothing was configured by the user, it's the hardcoded ORefit values,
        // which are made for the sliders of CBBE and UNP bodies, so a UBE body gets nothing
        SliderSet set;

        if (!refitPreset) {
            if (!ube) {
                set = GenerateClotheSliders(a_actor);
            }
        } else if (differential) {
            set = MakeRefitDifferential(actorPreset->sliders, refitPreset->sliders);
        } else {
            set = refitPreset->sliders;
        }

        // Compared with the morphs themselves, which are kept in the save, so that NPCs putting their outfit
        // back on as they load don't get their body updated for nothing
        if (a_onlyIfChanged && IsClotheSetApplied(a_actor, set)) {
            return false;
        }

        // Clear the previous refit before applying the new one
        RemoveClothePreset(a_actor);

        if (!refitPreset && ube) {
            logger::info(
                "No UBE -Refit preset found for actor {} (preset '{}'), and the default ORefit values aren't made for "
                "UBE, so ORefit doesn't change their body",
                a_actor->GetName(), actorPreset ? actorPreset->name : "none");
        } else if (!refitPreset) {
            logger::info("No -Refit preset found for actor {} (preset '{}'), using the default ORefit values",
                         a_actor->GetName(), actorPreset ? actorPreset->name : "none");
        } else if (differential) {
            logger::info("Applying differential refit '{}' on top of '{}'", refitPreset->name, actorPreset->name);
        } else {
            logger::info("Applying additive refit '{}'", refitPreset->name);
        }

        ApplySliderSet(a_actor, set, "OClothe");
        return true;
    }

    void OBody::ClearActorMorphs(RE::Actor* a_actor, bool updateMorphsWithoutTimer,
                                 ::OBody::API::IPluginInterface* responsibleInterface) const {
        morphInterface->ClearBodyMorphKeys(a_actor, "OBody");
        morphInterface->ClearBodyMorphKeys(a_actor, "OClothe");
        ApplyMorphs(a_actor, updateMorphsWithoutTimer, false);

        SendActorChangeEvent(
            a_actor,
            [&] {
                using Event = ::OBody::API::IActorChangeEventListener;
                Event::OnActorMorphsCleared::Payload payload{responsibleInterface};
                Event::OnActorMorphsCleared::Flags flags{};

                return std::make_pair(flags, payload);
            },
            [](auto listener, auto actor, auto&& args) {
                listener->OnActorMorphsCleared(actor, args.first, args.second);
            });
    }

    void OBody::ReapplyActorMorphs(RE::Actor* a_actor, ::OBody::API::IPluginInterface* responsibleInterface) const {
        std::optional<PresetManager::Preset> preset =
            ActorTracker::Registry::GetInstance().GetPresetForActor(a_actor, IsFemale(a_actor));

        if (preset) {
            GenerateBodyByPreset(a_actor, *preset, true, responsibleInterface);
            return;
        }

        // No preset is assigned to the actor, we fallback to GenerateActorBody.
        GenerateActorBody(a_actor, responsibleInterface);
    }

    void OBody::ForcefullyChangeORefit(RE::Actor* a_actor, bool applied,
                                       ::OBody::API::IPluginInterface* responsibleInterface) const {
        if (applied) {
            ApplyClothePreset(a_actor);
        } else {
            RemoveClothePreset(a_actor);
        }
        ApplyMorphs(a_actor, true);

        SendActorChangeEvent(
            a_actor,
            [&] {
                using Event = ::OBody::API::IActorChangeEventListener;
                Event::OnORefitForcefullyChanged::Payload payload{responsibleInterface};

                Event::OnORefitForcefullyChanged::Flags flags{};
                static_assert(Event::OnORefitForcefullyChanged::Flags::IsORefitApplied == (1 << 1));
                static_assert(Event::OnORefitForcefullyChanged::Flags::IsORefitEnabled == (1 << 2));
                flags = static_cast<Event::OnORefitForcefullyChanged::Flags>(flags | (uint64_t(applied) << 1));
                flags = static_cast<Event::OnORefitForcefullyChanged::Flags>(flags | (uint64_t(setRefit) << 2));

                return std::make_pair(flags, payload);
            },
            [](auto listener, auto actor, auto&& args) {
                listener->OnORefitForcefullyChanged(actor, args.first, args.second);
            });
    }

    void OBody::RemoveClothePreset(RE::Actor* a_actor) const { morphInterface->ClearBodyMorphKeys(a_actor, "OClothe"); }

    float OBody::GetWeight(RE::Actor* a_actor) { return a_actor->GetActorBase()->GetWeight() / 100.0F; }

    bool OBody::IsClotheActive(RE::Actor* a_actor) const { return morphInterface->HasBodyMorphKey(a_actor, "OClothe"); }

    bool OBody::IsClotheSetApplied(RE::Actor* a_actor, const PresetManager::SliderSet& a_sliders) const {
        constexpr float epsilon{0.00001F};

        // The same value ApplySlider sets
        const float weight{GetWeight(a_actor)};
        for (const auto& slider : a_sliders | std::views::values) {
            const float value{((slider.max - slider.min) * weight) + slider.min};
            if (std::abs(morphInterface->GetMorph(a_actor, slider.name.c_str(), "OClothe") - value) > epsilon) {
                return false;
            }
        }

        // A previous refit may have set sliders that this one doesn't have
        const auto isLeftOver = [&](const std::string& a_name) {
            return !a_sliders.contains(a_name) && morphInterface->HasBodyMorph(a_actor, a_name.c_str(), "OClothe");
        };
        return std::ranges::none_of(PresetContainer::GetInstance().refitSliderNames, isLeftOver) &&
               std::ranges::none_of(GenerateClotheSliders(a_actor) | std::views::keys, isLeftOver);
    }

    bool OBody::IsNaked(RE::Actor* a_actor, const bool a_removingArmor, const RE::TESForm* a_equippedArmor) {
        using BipedObjectSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
        auto& jsonParser{Parser::JSONParser::GetInstance()};

        const auto* outfitBody{stl::GetWornArmor(a_actor, BipedObjectSlot::kBody)};
        const auto* outergarmentChest{stl::GetWornArmor(a_actor, BipedObjectSlot::kModChestPrimary)};
        const auto* undergarmentChest{stl::GetWornArmor(a_actor, BipedObjectSlot::kModChestSecondary)};

        // When the TES EquipEvent is sent, the inventory isn't updated yet
        // So we have to check if any of these armors is being removed...
        if (a_removingArmor) {
            if (outfitBody == a_equippedArmor) {
                outfitBody = nullptr;
            } else if (outergarmentChest == a_equippedArmor) {
                outergarmentChest = nullptr;
            } else if (undergarmentChest == a_equippedArmor) {
                undergarmentChest = nullptr;
            }
        } else if (const auto* const equippedArmor{a_equippedArmor ? a_equippedArmor->As<RE::TESObjectARMO>()
                                                                   : nullptr}) {
            // ...or if one is being equipped, which takes the slots it covers
            if (equippedArmor->HasPartOf(BipedObjectSlot::kBody)) outfitBody = equippedArmor;
            if (equippedArmor->HasPartOf(BipedObjectSlot::kModChestPrimary)) outergarmentChest = equippedArmor;
            if (equippedArmor->HasPartOf(BipedObjectSlot::kModChestSecondary)) undergarmentChest = equippedArmor;
        }

        // if outfit is blacklisted from ORefit, we assume as not having the outfit so ORefit is not applied
        const auto countsAsClothing = [&](const RE::TESObjectARMO* a_armor) {
            if (!a_armor) return false;
            if (jsonParser.IsOutfitBlacklisted(*a_armor)) {
                logger::info("ORefit: '{}' ({:08X}) is blacklisted from ORefit, so it doesn't count as clothing",
                             a_armor->GetName(), a_armor->GetFormID());
                return false;
            }
            return true;
        };
        const bool hasBodyOutfit = countsAsClothing(outfitBody);
        const bool hasOutergarment = countsAsClothing(outergarmentChest);
        const bool hasUndergarment = countsAsClothing(undergarmentChest);

        // Actor counts as naked if:
        // he has no clothing in the slots defined above / they are blacklisted from ORefit
        // if the items in the outfitsForceRefit key are not equipped
        return !hasBodyOutfit && !hasOutergarment && !hasUndergarment &&
               !jsonParser.IsAnyForceRefitItemEquipped(a_actor, a_removingArmor, a_equippedArmor);
    }

    bool OBody::IsRemovingClothes(RE::Actor* a_actor, const bool a_removingArmor, const RE::TESForm* a_equippedArmor) {
        using BipedObjectSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
        const auto* outfitBody{stl::GetWornArmor(a_actor, BipedObjectSlot::kBody)};
        const auto* outergarmentChest{stl::GetWornArmor(a_actor, BipedObjectSlot::kModChestPrimary)};
        const auto* undergarmentChest{stl::GetWornArmor(a_actor, BipedObjectSlot::kModChestSecondary)};
        const auto* outergarmentPelvis{stl::GetWornArmor(a_actor, BipedObjectSlot::kModPelvisPrimary)};
        const auto* undergarmentPelvis{stl::GetWornArmor(a_actor, BipedObjectSlot::kModPelvisSecondary)};

        bool isRemovingBody{false};
        bool isRemovingOuterGarmentChest{false};
        bool isRemovingUnderGarmentChest{false};
        bool isRemovingOuterGarmentPelvis{false};
        bool isRemovingUnderGarmentPelvis{false};

        // When the TES EquipEvent is sent, the inventory isn't updated yet
        // So we have to check if any of these armors is being removed...
        if (a_removingArmor) {
            if (outfitBody == a_equippedArmor) {
                isRemovingBody = true;
            } else if (outergarmentChest == a_equippedArmor) {
                isRemovingOuterGarmentChest = true;
            } else if (undergarmentChest == a_equippedArmor) {
                isRemovingUnderGarmentChest = true;
            } else if (outergarmentPelvis == a_equippedArmor) {
                isRemovingOuterGarmentPelvis = true;
            } else if (undergarmentPelvis == a_equippedArmor) {
                isRemovingUnderGarmentPelvis = true;
            }
        }

        return isRemovingBody || isRemovingOuterGarmentChest || isRemovingUnderGarmentChest ||
               isRemovingOuterGarmentPelvis || isRemovingUnderGarmentPelvis;
    }

    bool OBody::IsFemale(RE::Actor* a_actor) { return a_actor->GetActorBase()->GetSex() == RE::SEX::kFemale; }

    bool OBody::IsUBE(RE::Actor* a_actor) {
        const auto* const base{a_actor->GetActorBase()};
        auto* const race{a_actor->GetRace()};
        if (!base || !race) return false;

        // An NPC's own skin replaces the one of their race
        const auto* const skin{base->skin ? base->skin : race->skin};
        if (!skin) return false;

        const auto sex{base->GetSex() == RE::SEX::kFemale ? RE::SEXES::kFemale : RE::SEXES::kMale};
        const std::uint64_t key{(static_cast<std::uint64_t>(skin->GetFormID()) << 32) | race->GetFormID()};

        bool ube{false};
        if (g_ubeBodies[sex].visit(key, [&](const auto& a_entry) { ube = a_entry.second; })) {
            return ube;
        }

        // The body is the first addon of the skin that covers the body slot and is made for the race
        for (const auto* const addon : skin->armorAddons) {
            if (addon && addon->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kBody) && addon->IsValidRace(race)) {
                ube = IsUBEMesh(addon->bipedModels[sex].GetModel());
                break;
            }
        }

        if (g_ubeBodies[sex].emplace(key, ube) && ube) {
            logger::info("Skin {:08X} on race {} has a UBE body, only UBE presets are used for it", skin->GetFormID(),
                         race->GetFormEditorID());
        }
        return ube;
    }

    bool OBody::IsProcessed(RE::Actor* a_actor) const {
        return morphInterface->HasBodyMorph(a_actor, distributionKey.c_str(), "OBody");
    }

    bool OBody::IsBlacklisted(RE::Actor* a_actor) const {
        return morphInterface->HasBodyMorph(a_actor, "obody_blacklisted", "OBody");
    }

    PresetManager::SliderSet OBody::GenerateRandomNippleSliders() {
        PresetManager::SliderSet set;

        if (stl::chance(15))
            AddSliderToSet(set, Slider{"AreolaSize", stl::random(-1.0f, 0.0f)});
        else
            AddSliderToSet(set, Slider{"AreolaSize", stl::random(0.0f, 1.0f)});

        if (stl::chance(75)) AddSliderToSet(set, Slider{"AreolaPull_v2", stl::random(-0.25f, 1.0f)});

        if (stl::chance(15))
            AddSliderToSet(set, Slider{"NippleLength", stl::random(0.2f, 0.3f)});
        else
            AddSliderToSet(set, Slider{"NippleLength", stl::random(0.0f, 0.1f)});

        AddSliderToSet(set, Slider{"NippleManga", stl::random(-0.3f, 0.8f)});

        if (stl::chance(25)) AddSliderToSet(set, Slider{"NipplePerkManga", stl::random(-0.3f, 1.2f)});

        if (stl::chance(15)) AddSliderToSet(set, Slider{"NipBGone", stl::random(0.6f, 1.0f)});

        AddSliderToSet(set, Slider{"NippleSize", stl::random(-0.5f, 0.3f)});
        AddSliderToSet(set, Slider{"NippleDip", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"NippleCrease_v2", stl::random(-0.4f, 1.0f)});

        if (stl::chance(6)) AddSliderToSet(set, Slider{"NipplePuffy_v2", stl::random(0.4f, 0.7f)});

        if (stl::chance(35)) AddSliderToSet(set, Slider{"NippleThicc_v2", stl::random(0.0f, 0.9f)});

        if (stl::chance(2)) {
            if (stl::chance(50))
                AddSliderToSet(set, Slider{"NippleInvert_v2", 1.0f});
            else
                AddSliderToSet(set, Slider{"NippleInvert_v2", stl::random(0.65f, 0.8f)});
        }

        return set;
    }

    PresetManager::SliderSet OBody::GenerateRandomGenitalSliders() {
        PresetManager::SliderSet set;

        if (stl::chance(20)) {
            // innie
            AddSliderToSet(set, Slider{"Innieoutie", stl::random(0.95f, 1.1f)});

            if (stl::chance(50)) AddSliderToSet(set, Slider{"Labiapuffyness", stl::random(0.75f, 1.25f)});

            if (stl::chance(40)) AddSliderToSet(set, Slider{"LabiaMorePuffyness_v2", stl::random(0.0f, 1.0f)});

            AddSliderToSet(set, Slider{"Labiaprotrude", stl::random(0.0f, 0.5f)});
            AddSliderToSet(set, Slider{"Labiaprotrude2", stl::random(0.0f, 0.1f)});
            AddSliderToSet(set, Slider{"Labiaprotrudeback", stl::random(0.0f, 0.1f)});
            AddSliderToSet(set, Slider{"Labiaspread", 0.0F});
            AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 0.3f)});
            AddSliderToSet(set, Slider{"LabiaBulgogi_v2", 0.0F});
            AddSliderToSet(set, Slider{"LabiaNeat_v2", 0.0F});
            AddSliderToSet(set, Slider{"VaginaHole", stl::random(-0.2f, 0.05f)});
            AddSliderToSet(set, Slider{"Clit", stl::random(-0.4f, 0.25f)});
        } else if (stl::chance(75)) {
            // average
            AddSliderToSet(set, Slider{"Innieoutie", stl::random(0.4f, 0.75f)});

            if (stl::chance(40)) AddSliderToSet(set, Slider{"Labiapuffyness", stl::random(0.5f, 1.0f)});

            if (stl::chance(30)) AddSliderToSet(set, Slider{"LabiaMorePuffyness_v2", stl::random(0.0f, 0.75f)});

            AddSliderToSet(set, Slider{"Labiaprotrude", stl::random(0.0f, 0.5f)});
            AddSliderToSet(set, Slider{"Labiaprotrude2", stl::random(0.0f, 0.75f)});
            AddSliderToSet(set, Slider{"Labiaprotrudeback", stl::random(0.0f, 1.0f)});

            if (stl::chance(50)) {
                AddSliderToSet(set, Slider{"Labiaspread", stl::random(0.0f, 1.0f)});
                AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 0.7f)});

                if (stl::chance(60)) AddSliderToSet(set, Slider{"LabiaBulgogi_v2", stl::random(0.0f, 0.1f)});
            } else {
                AddSliderToSet(set, Slider{"Labiaspread", 0.0F});
                AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 0.2f)});

                if (stl::chance(45)) AddSliderToSet(set, Slider{"LabiaBulgogi_v2", stl::random(0.0f, 0.3f)});
            }

            AddSliderToSet(set, Slider{"LabiaNeat_v2", 0.0F});
            AddSliderToSet(set, Slider{"VaginaHole", stl::random(-0.2f, 0.40f)});
            AddSliderToSet(set, Slider{"Clit", stl::random(-0.2f, 0.25f)});
        } else {
            // outie
            AddSliderToSet(set, Slider{"Innieoutie", stl::random(-0.25f, 0.30f)});

            if (stl::chance(30)) AddSliderToSet(set, Slider{"Labiapuffyness", stl::random(0.20f, 0.50f)});

            if (stl::chance(10)) AddSliderToSet(set, Slider{"LabiaMorePuffyness_v2", stl::random(0.0f, 0.35f)});

            AddSliderToSet(set, Slider{"Labiaprotrude", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Labiaprotrude2", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Labiaprotrudeback", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Labiaspread", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"LabiaCrumpled_v2", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"LabiaBulgogi_v2", stl::random(0.0f, 1.0f)});

            if (stl::chance(40)) AddSliderToSet(set, Slider{"LabiaNeat_v2", stl::random(0.0f, 0.25f)});

            AddSliderToSet(set, Slider{"VaginaHole", stl::random(0.0f, 1.0f)});
            AddSliderToSet(set, Slider{"Clit", stl::random(-0.4f, 0.25f)});
        }

        AddSliderToSet(set, Slider{"Vaginasize", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"ClitSwell_v2", stl::random(-0.3f, 1.1f)});
        AddSliderToSet(set, Slider{"Cutepuffyness", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"LabiaTightUp", stl::random(0.0f, 1.0f)});

        if (stl::chance(60))
            AddSliderToSet(set, Slider{"CBPC", stl::random(-0.25f, 0.25f)});
        else
            AddSliderToSet(set, Slider{"CBPC", stl::random(0.6f, 1.0f)});

        AddSliderToSet(set, Slider{"AnalPosition_v2", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"AnalTexPos_v2", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"AnalTexPosRe_v2", stl::random(0.0f, 1.0f)});
        AddSliderToSet(set, Slider{"AnalLoose_v2", -0.1F});

        return set;
    }

    PresetManager::SliderSet OBody::GenerateClotheSliders(RE::Actor* a_actor) const {
        PresetManager::SliderSet set;
        // breasts
        // make area on sides behind breasts not sink in
        AddSliderToSet(set, DeriveSlider(a_actor, "BreastSideShape", 0.0F));
        // make area under breasts not sink in
        AddSliderToSet(set, DeriveSlider(a_actor, "BreastUnderDepth", 0.0F));
        // push breasts together
        AddSliderToSet(set, DeriveSlider(a_actor, "BreastCleavage", 1.0F));
        // push up smaller breasts more
        AddSliderToSet(set, Slider{"BreastGravity2", -0.1F, -0.05F});
        // Make top of breast rise higher
        AddSliderToSet(set, Slider{"BreastTopSlope", -0.2F, -0.35F});
        // push breasts together
        AddSliderToSet(set, Slider{"BreastsTogether", 0.3F, 0.35F});
        // push breasts up
        // AddSliderToSet(set, Slider{ "PushUp", 0.6f, 0.4f });
        // Shrink breasts slightly
        AddSliderToSet(set, Slider{"Breasts", -0.05F});
        // Move breasts up on body slightly
        AddSliderToSet(set, Slider{"BreastHeight", 0.15F});

        // butt
        // remove butt impressions
        AddSliderToSet(set, DeriveSlider(a_actor, "ButtDimples", 0.0F));
        AddSliderToSet(set, DeriveSlider(a_actor, "ButtUnderFold", 0.0F));
        // shrink ass slightly
        AddSliderToSet(set, Slider{"AppleCheeks", -0.05F});
        AddSliderToSet(set, Slider{"Butt", -0.05F});

        // Torso
        // remove definition on clavical bone
        AddSliderToSet(set, DeriveSlider(a_actor, "Clavicle_v2", 0.0F));
        // Push out navel
        AddSliderToSet(set, DeriveSlider(a_actor, "NavelEven", 1.0F));

        // hip
        // remove defintion on hip bone
        AddSliderToSet(set, DeriveSlider(a_actor, "HipCarved", 0.0F));

        if (setNippleSlidersRefitEnabled) {
            // nipple
            // sublte change to tip shape
            AddSliderToSet(set, DeriveSlider(a_actor, "NippleDip", 0.0F));
            AddSliderToSet(set, DeriveSlider(a_actor, "NippleTip", 0.0F));
            // flatten areola
            AddSliderToSet(set, DeriveSlider(a_actor, "NipplePuffy_v2", 0.0F));
            // shrink areola
            AddSliderToSet(set, DeriveSlider(a_actor, "AreolaSize", -0.3F));
            // flatten nipple
            AddSliderToSet(set, DeriveSlider(a_actor, "NipBGone", 1.0F));
            // AddSliderToSet(set, DeriveSlider(a_actor, "NippleManga", -0.75f));
            //  push nipples together
            AddSliderToSet(set, Slider{"NippleDistance", 0.05F, 0.08F});
            // Lift large breasts up
            AddSliderToSet(set, Slider{"NippleDown", 0.0F, -0.1F});
            // Flatten nipple + areola
            AddSliderToSet(set, DeriveSlider(a_actor, "NipplePerkManga", -0.25F));
            // Flatten nipple
            // AddSliderToSet(set, DeriveSlider(a_actor, "NipplePerkiness", 0.0f));
        }

        return set;
    }

    Slider OBody::DeriveSlider(RE::Actor* a_actor, const char* a_morph, float a_target) const {
        return Slider{a_morph, a_target - GetMorph(a_actor, a_morph)};
    }

    bool OBody::BecomingReadyForPluginAPIUsage() {
        std::lock_guard<std::recursive_mutex> lock(readinessListenerLock);

        if (readyForPluginAPIUsage) {
            return false;
        }

        for (auto eventListener : readinessEventListeners) {
            eventListener->OBodyIsBecomingReady();
        }

        readyForPluginAPIUsage = true;

        return true;
    }

    void OBody::ReadyForPluginAPIUsage() {
        std::lock_guard<std::recursive_mutex> lock(readinessListenerLock);

        for (auto eventListener : readinessEventListeners) {
            eventListener->OBodyIsReady();
        }
    }

    bool OBody::BecomingUnreadyForPluginAPIUsage() {
        std::lock_guard<std::recursive_mutex> lock(readinessListenerLock);

        if (!readyForPluginAPIUsage) {
            return false;
        }

        for (auto eventListener : readinessEventListeners) {
            eventListener->OBodyIsBecomingUnready();
        }

        return true;
    }

    void OBody::NoLongerReadyForPluginAPIUsage() {
        std::lock_guard<std::recursive_mutex> lock(readinessListenerLock);

        readyForPluginAPIUsage = false;

        for (auto eventListener : readinessEventListeners) {
            eventListener->OBodyIsNoLongerReady();
        }
    }

    bool OBody::AttachEventListener(::OBody::API::IOBodyReadinessEventListener& eventListener) {
        std::lock_guard<std::recursive_mutex> lock(readinessListenerLock);

        readinessEventListeners.push_back(&eventListener);

        return true;
    }

    bool OBody::DetachEventListener(::OBody::API::IOBodyReadinessEventListener& eventListener) {
        std::lock_guard<std::recursive_mutex> lock(readinessListenerLock);

        return std::erase(readinessEventListeners, &eventListener) != 0;
    }

    bool OBody::AttachEventListener(::OBody::API::IActorChangeEventListener& eventListener) {
        std::lock_guard<std::recursive_mutex> lock(actorChangeListenerLock);

        actorChangeEventListeners.push_back(&eventListener);

        return true;
    }

    bool OBody::DetachEventListener(::OBody::API::IActorChangeEventListener& eventListener) {
        std::lock_guard<std::recursive_mutex> lock(actorChangeListenerLock);

        return std::erase(actorChangeEventListeners, &eventListener) != 0;
    }

    bool OBody::IsEventListenerAttached(::OBody::API::IActorChangeEventListener& eventListener) {
        std::lock_guard<std::recursive_mutex> lock(actorChangeListenerLock);

        return std::find(actorChangeEventListeners.begin(), actorChangeEventListeners.end(), &eventListener) !=
               actorChangeEventListeners.end();
    }

    void OBody::AssignPresetToActor(RE::Actor* a_actor, const std::string& a_presetName,
                                    bool a_forceImmediateApplicationOfMorphs, bool a_doNotApplyMorphs) const {
        auto& registry{ActorTracker::Registry::GetInstance()};
        auto formID = a_actor->formID;

        if (a_presetName.size() == 0) {
            // Clear their preset assignment, if they have one.
            uint32_t previousPresetIndex = 0;
            registry.stateForActor.visit(formID, [&](auto& entry) {
                previousPresetIndex = entry.second.presetIndex;
                entry.second.presetIndex = 0;
            });

            if (!a_doNotApplyMorphs) {
                ClearActorMorphs(a_actor, a_forceImmediateApplicationOfMorphs, &specialPapyrusPluginInterface);
            }

            if (previousPresetIndex != 0) {
                SendActorChangeEvent(
                    a_actor,
                    [&] {
                        using Event = ::OBody::API::IActorChangeEventListener;

                        Event::OnActorPresetChangedWithoutGeneration::Payload payload{
                            &specialPapyrusPluginInterface,
                            // Note that the plugin-API mandates that this be a null-terminated string.
                            // Minus one because an index of zero assigned to the actor signifies the absence of a
                            // preset.
                            PresetManager::AssignedPresetIndex{previousPresetIndex - 1}.GetPresetNameView(
                                IsFemale(a_actor))};

                        auto flags = Event::OnActorPresetChangedWithoutGeneration::Flags::PresetWasUnassigned;

                        return std::make_pair(flags, payload);
                    },
                    [](auto listener, auto actor, auto&& args) {
                        listener->OnActorPresetChangedWithoutGeneration(actor, args.first, args.second);
                    });
            }

            return;
        }

        bool isFemale = Body::OBody::IsFemale(a_actor);

        const auto& presetContainer{PresetManager::PresetContainer::GetInstance()};
        auto preset = GetPresetByNameForRandom(
            isFemale ? presetContainer.allFemalePresets : presetContainer.allMalePresets, a_presetName);

        if (!preset) {
            return;
        }

        // Like OBody::GenerateBodyByName, we set this morph to prevent a crash with SynthEBD/Synthesis.
        if (synthesisInstalled) {
            SetMorph(a_actor, "obody_synthebd", "OBody", 1.0F);
        }

        if (!a_doNotApplyMorphs) {
            GenerateBodyByPreset(a_actor, *preset, a_forceImmediateApplicationOfMorphs, &specialPapyrusPluginInterface);
        } else {
            // Assign the preset to the actor.
            auto assignedPresetIndex = preset->assignedIndex;
            // Plus one because an index of zero on the actor signifies the absence of a preset.
            uint32_t actorPresetIndex = assignedPresetIndex.value + 1;
            ActorTracker::ActorState fallbackActorState{};
            fallbackActorState.presetIndex = actorPresetIndex;

            registry.stateForActor.emplace_or_visit(formID, fallbackActorState,
                                                    [&](auto& entry) { entry.second.presetIndex = actorPresetIndex; });

            SendActorChangeEvent(
                a_actor,
                [&] {
                    using Event = ::OBody::API::IActorChangeEventListener;

                    Event::OnActorPresetChangedWithoutGeneration::Payload payload{
                        &specialPapyrusPluginInterface,
                        // Note that the plugin-API mandates that this be a null-terminated string.
                        assignedPresetIndex.GetPresetNameView(isFemale)};

                    Event::OnActorPresetChangedWithoutGeneration::Flags flags{};

                    return std::make_pair(flags, payload);
                },
                [](auto listener, auto actor, auto&& args) {
                    listener->OnActorPresetChangedWithoutGeneration(actor, args.first, args.second);
                });
        }
    }

}  // namespace Body
