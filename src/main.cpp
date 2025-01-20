#include "Body/Body.h"
#include "Body/Event.h"
#include "Papyrus/Papyrus.h"
#include "JSONParser/JSONParser.h"
#include "PresetManager/PresetManager.h"
#include "SKEE.h"

namespace {
    void InitializeLogging() {
        // ReSharper disable once CppLocalVariableMayBeConst
        auto path{logger::log_directory()};
        if (!path) {
            SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
        }
        *path /= std::format("{}.log", SKSE::PluginDeclaration::GetSingleton()->GetName());

        std::shared_ptr<spdlog::logger> log =
            (IsDebuggerPresent() != 0)
                ? std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>())
                : std::make_shared<spdlog::logger>(
                    "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));

        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
        spdlog::set_default_logger(std::move(log));
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        auto& obody = Body::OBody::GetInstance();

        switch (a_msg->type) {
            // On kPostPostLoad, we can try to fetch the Racemenu interface
            case SKSE::MessagingInterface::kPostPostLoad: {
                SKEE::InterfaceExchangeMessage msg;
                const auto* const intfc = SKSE::GetMessagingInterface();
                intfc->Dispatch(SKEE::InterfaceExchangeMessage::kExchangeInterface, &msg,
                                sizeof(SKEE::InterfaceExchangeMessage*), "skee");
                if (!msg.interfaceMap) {
                    logger::critical("Couldn't get interface map!");
                    return;
                }

                auto morphInterface =
                    static_cast<SKEE::IBodyMorphInterface*>(msg.interfaceMap->QueryInterface("BodyMorph"));
                if (!morphInterface) {
                    logger::critical("Couldn't get serialization MorphInterface!");
                    return;
                }

                logger::info("BodyMorph Version {}", morphInterface->GetVersion());
                if (!obody.SetMorphInterface(morphInterface)) logger::info("BodyMorphInterace not provided");

                return;
            }

            // When data is all loaded (this is by the time the Main Menu is visible), we can parse the JSON and the
            // Bodyslide presets
            case SKSE::MessagingInterface::kDataLoaded: {
                auto& parser = Parser::JSONParser::GetInstance();

                std::ifstream f(L"Data/SKSE/Plugins/OBody_presetDistributionConfig.json");

                try {
                    f >> parser.presetDistributionConfig;
                    parser.ProcessJSONCategories();
                    parser.presetDistributionConfigValid = true;
                } catch (const std::runtime_error& re) {
                    logger::info("{} ", re.what());
                    parser.presetDistributionConfigValid = false;
                } catch (const std::exception& ex) {
                    logger::info("{} ", ex.what());
                    parser.presetDistributionConfigValid = false;
                } catch (...) {
                    logger::info("An unknown error has occurred while parsing the JSON file.");
                    parser.presetDistributionConfigValid = false;
                }

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

                if (parser.presetDistributionConfigValid) {
                    logger::info("OBody has finished parsing the JSON config file.");
                } else {
                    logger::info("There are errors in the OBody JSON config file! OBody will not work properly.");
                }

                RE::TESDataHandler* pDataHandler = RE::TESDataHandler::GetSingleton();

                obody.synthesisInstalled = pDataHandler->LookupModByName("SynthEBD.esp") != nullptr;

                logger::info("Synthesis installed value is {}.", obody.synthesisInstalled);

                return;
            }

            // We can only register for events after the game is loaded
            // The game doesn't send a Load game event on new game, so we need to listen for this one in specific
            case SKSE::MessagingInterface::kNewGame: {
                logger::info("New Game started");
                Event::OBodyEventHandler::Register();
                return;
            }

            case SKSE::MessagingInterface::kPostLoadGame: {
                logger::info("Game finished loading");
                Event::OBodyEventHandler::Register();
                return;
            }
            default:
                return;
        }
    }
} // namespace

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    InitializeLogging();

    const auto* const plugin = SKSE::PluginDeclaration::GetSingleton();
    logger::info("{} {} is loading...", plugin->GetName(), plugin->GetVersion().string("."));

    Init(a_skse, false); // Passing 'false' prevents Init from setting up its own logging, allowing us to use our custom setup

    if (const auto* const message{SKSE::GetMessagingInterface()}; !message->RegisterListener(MessageHandler)) {
        return false;
    }

    Papyrus::Bind();

    logger::info("{} has finished loading.", plugin->GetName());

    return true;
}