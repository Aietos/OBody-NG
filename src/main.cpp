#include "BackwardsCompatibility/BackwardsCompatibility.h"
#include "Body/Body.h"
#include "Body/Event.h"
#include "Papyrus/Papyrus.h"
#include "JSONParser/JSONParser.h"
#include "PresetManager/PresetManager.h"
#include "Preview/Preview.h"
#include "Preview/SmoothCam.h"
#include "SaveFileState/SaveFileState.h"
#include "SKEE.h"
#include "STL.h"
#include "UI/UI.h"
#include "UI/Translations.h"

namespace {
    void InitializeLogging() {
        // ReSharper disable once CppLocalVariableMayBeConst
        auto path{logger::log_directory()};
        if (!path) {
            SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
        }
        *path /= std::format("{}.log", SKSE::PluginDeclaration::GetSingleton()->GetName());

        auto log{std::make_shared<spdlog::logger>("Global")};
        auto& log_sinks{log->sinks()};

        if (REX::W32::IsDebuggerPresent()) {
            log_sinks.reserve(2);
            const auto msvc_sink{std::make_shared<spdlog::sinks::msvc_sink_mt>()};
            msvc_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [OBody.dll,%s:%#] %v");
            log_sinks.emplace_back(msvc_sink);
        }
        const auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
        log_sinks.emplace_back(file_sink);

        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void PluginInterfaceMessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type) {
            case OBody::API::SKSEMessages::RequestPluginInterface::type: {
                // To maintain ABI-compatibility we specify the size of the first version of the
                // `OBody::API::SKSEMessages::RequestPluginInterface` structure, instead of using `sizeof`.
                if (a_msg->dataLen < sizeof(void*) * 3) {
                    logger::error("An invalid RequestPluginInterface message of only {} bytes was sent by {}.",
                                  a_msg->dataLen, a_msg->sender);
                    return;
                }

                using Version = OBody::API::PluginAPIVersion;

                auto request = reinterpret_cast<OBody::API::SKSEMessages::RequestPluginInterface*>(a_msg->data);

                if (request->version == Version::Invalid || request->version > Version::Latest) {
                    logger::error("An unsupported plugin-API version of {} was requested by {}.",
                                  std::to_underlying(request->version), a_msg->sender);
                    return;
                }

                if (request->readinessEventListener == nullptr) {
                    logger::error(
                        "No `IOBodyReadinessEventListener` instance was supplied with a `RequestPluginInterface` "
                        "message sent by {}.",
                        a_msg->sender);
                    return;
                }

                auto requestedVersion = request->version;

                switch (requestedVersion) {
                    case Version::v1: {
                        auto& obody{Body::OBody::GetInstance()};
                        auto& readinessListener = *request->readinessEventListener;

                        *request->pluginInterface = new OBody::API::PluginInterface(a_msg->sender);

                        obody.AttachEventListener(readinessListener);

                        std::lock_guard<std::recursive_mutex> lock(obody.readinessListenerLock);

                        if (obody.readyForPluginAPIUsage) {
                            readinessListener.OBodyIsBecomingReady();
                            readinessListener.OBodyIsReady();
                        }

                        break;
                    }
                    default:
                        break;
                }

                logger::info("A plugin interface of plugin-API version {} was supplied to {}.",
                             std::to_underlying(requestedVersion), a_msg->sender);
                return;
            }
            default:
                return;
        }
    }

    bool RequestMorphInterface(Body::OBody& a_obody) {
        SKEE::InterfaceExchangeMessage msg;
        const auto* const intfc{SKSE::GetMessagingInterface()};
        intfc->Dispatch(SKEE::InterfaceExchangeMessage::kExchangeInterface, &msg,
                        sizeof(SKEE::InterfaceExchangeMessage*), "skee");
        if (!msg.interfaceMap) {
            logger::critical("Couldn't get Racemenu's interface map!");
            return false;
        }

        const auto morphInterface = static_cast<SKEE::IBodyMorphInterface*>(
            msg.interfaceMap->QueryInterface("BodyMorph"));  // NOLINT(*-pro-type-static-cast-downcast)
        if (!morphInterface) {
            logger::critical("Couldn't get Racemenu's BodyMorph interface!");
            return false;
        }

        logger::info("BodyMorph Version {}", morphInterface->GetVersion());

        if (!a_obody.SetMorphInterface(morphInterface)) {
            logger::critical("Racemenu's BodyMorph interface has no version, it can't be used");
            return false;
        }

        return true;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void SKSEMessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        auto& obody{Body::OBody::GetInstance()};

        switch (a_msg->type) {
            // On kPostPostLoad, we can try to fetch the Racemenu interface
            case SKSE::MessagingInterface::kPostPostLoad: {
                SmoothCam::RequestInterface();

                // Racemenu doesn't always answer this early (seen mostly on 1.5.97), it's asked again at kDataLoaded
                if (!RequestMorphInterface(obody)) {
                    logger::warn("Racemenu's BodyMorph interface is asked for again once the game data is loaded");
                }

                return;
            }

            // When data is all loaded (this is by the time the Main Menu is visible), we can process the JSON and the
            // Bodyslide presets
            case SKSE::MessagingInterface::kDataLoaded: {
                auto& parser{Parser::JSONParser::GetInstance()};

                try {
                    PresetManager::GeneratePresets();
                    parser.bodyslidePresetsParsingValid = true;
                } catch (const std::runtime_error& re) {
                    logger::info("{} ", re.what());
                    parser.bodyslidePresetsParsingValid = false;
                } catch (const std::exception& ex) {
                    logger::info("{} ", ex.what());
                    parser.bodyslidePresetsParsingValid = false;
                } catch (...) {
                    logger::info("An unknown error has occurred while parsing the bodyslide presets files.");
                    parser.bodyslidePresetsParsingValid = false;
                }

                if (const auto result{parser.Load()}; result.error) {
                    SKSE::stl::report_and_fail(std::format("{}. Please check OBody.log.", *result.error));
                }

                RE::TESDataHandler* pDataHandler = RE::TESDataHandler::GetSingleton();

                obody.synthesisInstalled = pDataHandler->LookupModByName("SynthEBD.esp") != nullptr;

                logger::info("Synthesis installed value is {}.", obody.synthesisInstalled);

                // Before UI::Register, which can show a translated message
                UI::Translations::Init();

                // Without Racemenu's interface every body change would crash the game
                if (!obody.morphInterface && !RequestMorphInterface(obody)) {
                    logger::critical(
                        "OBody can't work without Racemenu's BodyMorph interface: no menu, no hooks and no events "
                        "this session");
                    RE::DebugMessageBox(UI::Translations::Get("obody_racemenu_failure").c_str());
                    return;
                }

                UI::Register();

                Event::InstallHooks();

                obody.CacheSkinMorphFiles();

                return;
            }

            // We can only register for events after the game is loaded
            // The game doesn't send a Load game event on new game, so we need to listen for this one in specific
            case SKSE::MessagingInterface::kNewGame: {
                logger::info("New Game started");

                BackwardsCompatibility::State::GetInstance().SetStateUponStartOfNewGame();

                PresetManager::PresetContainer::GetInstance().AssignPresetIndexes();

                // Without Racemenu's interface, OBody stays out of the game (see kDataLoaded)
                if (!obody.morphInterface) return;

                Event::OBodyEventHandler::Register();

                logger::info("Becoming ready for plugin-API usage.");
                // This should be the last state change in this message-handler,
                // so that the listeners work as expected.
                if (obody.BecomingReadyForPluginAPIUsage()) {
                    obody.ReadyForPluginAPIUsage();
                }
                logger::info("Now ready for plugin-API usage.");
                return;
            }

            // The preset menu's preview must not end up in the save
            case SKSE::MessagingInterface::kSaveGame: {
                Preview::Session::GetInstance().OnSave();
                return;
            }

            case SKSE::MessagingInterface::kPreLoadGame: {
                logger::info("Game beginning to load");

                Preview::Session::GetInstance().Abandon();

                logger::info("Becoming unready for plugin-API usage.");
                if (obody.BecomingUnreadyForPluginAPIUsage()) {
                    obody.NoLongerReadyForPluginAPIUsage();
                }
                logger::info("No longer ready for plugin-API usage.");

                return;
            }

            case SKSE::MessagingInterface::kPostLoadGame: {
                logger::info("Game finished loading");

                // Now that our cosave has loaded, we can assign indexes to the presets that we loaded earlier.
                PresetManager::PresetContainer::GetInstance().AssignPresetIndexes();

                BackwardsCompatibility::State::GetInstance().FixupAfterLoadingGame();

                // Without Racemenu's interface, OBody stays out of the game (see kDataLoaded)
                if (!obody.morphInterface) return;

                Event::OBodyEventHandler::Register();

                logger::info("Becoming ready for plugin-API usage.");
                // This should be the last state change in this message-handler,
                // so that the listeners work as expected.
                if (obody.BecomingReadyForPluginAPIUsage()) {
                    obody.ReadyForPluginAPIUsage();
                }
                logger::info("Now ready for plugin-API usage.");

                // The NPCs that loaded during the loading screen, now that the presets have this save's indexes
                Event::GenerateWaitingBodies();
                return;
            }
            case SKSE::MessagingInterface::kPostLoad: {
                SmoothCam::Listen();

                const REX::W32::HMODULE tweaks{REX::W32::GetModuleHandleA("po3_Tweaks")};
                stl::func = reinterpret_cast<stl::PO3_tweaks_GetFormEditorID>(
                    REX::W32::GetProcAddress(tweaks, "GetFormEditorID"));
                logger::info("Got po3_tweaks api: {}", stl::func != nullptr);

                auto serialization = SKSE::GetSerializationInterface();

                serialization->SetUniqueID(SaveFileState::CosaveUID);
                serialization->SetRevertCallback(SaveFileState::RevertState);
                serialization->SetSaveCallback(SaveFileState::SaveState);
                serialization->SetLoadCallback(SaveFileState::LoadState);
                logger::info("Set our SKSE serialization callbacks, with a unique ID of {:#010x}.",
                             SaveFileState::CosaveUID);

                SKSE::GetMessagingInterface()->RegisterListener(nullptr, PluginInterfaceMessageHandler);
                logger::info("Registered the PluginInterfaceMessageHandler.");

                return;
            }
            default:
                return;
        }
    }
}  // namespace

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    [[maybe_unused]] stl::timeit const t;
    InitializeLogging();

    const auto* const plugin{SKSE::PluginDeclaration::GetSingleton()};
    logger::info("{} {} is loading...", plugin->GetName(), plugin->GetVersion().string("."));

    SKSE::Init(a_skse, false);

    if (const auto* const message = SKSE::GetMessagingInterface(); !message->RegisterListener(SKSEMessageHandler)) {
        return false;
    }

    Papyrus::Bind();

    logger::info("{} has finished loading.", plugin->GetName());

    return true;
}
