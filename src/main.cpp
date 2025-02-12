#include "Body/Body.h"
#include "Body/Event.h"
#include "Papyrus/Papyrus.h"
#include "JSONParser/JSONParser.h"
#include "PresetManager/PresetManager.h"
#include "SKEE.h"
#include "STL.h"

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
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        auto& obody{Body::OBody::GetInstance()};

        switch (a_msg->type) {
            // On kPostPostLoad, we can try to fetch the Racemenu interface
            case SKSE::MessagingInterface::kPostPostLoad: {
                SKEE::InterfaceExchangeMessage msg;
                const auto* const intfc{SKSE::GetMessagingInterface()};
                intfc->Dispatch(SKEE::InterfaceExchangeMessage::kExchangeInterface, &msg,
                                sizeof(SKEE::InterfaceExchangeMessage*), "skee");
                if (!msg.interfaceMap) {
                    logger::critical("Couldn't get interface map!");
                    return;
                }

                const auto morphInterface =
                    static_cast<SKEE::IBodyMorphInterface*>(msg.interfaceMap->QueryInterface("BodyMorph")); // NOLINT(*-pro-type-static-cast-downcast)
                if (!morphInterface) {
                    logger::critical("Couldn't get serialization MorphInterface!");
                    return;
                }

                logger::info("BodyMorph Version {}", morphInterface->GetVersion());
                if (!obody.SetMorphInterface(morphInterface)) logger::info("BodyMorphInterace not provided");

                return;
            }

            // When data is all loaded (this is by the time the Main Menu is visible), we can process the JSON and the
            // Bodyslide presets
            case SKSE::MessagingInterface::kDataLoaded: {
                auto& parser{Parser::JSONParser::GetInstance()};
                parser.ProcessJSONCategories();

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
            case SKSE::MessagingInterface::kPostLoad: {
                const REX::W32::HMODULE tweaks{REX::W32::GetModuleHandleA("po3_Tweaks")};
                stl::func = reinterpret_cast<stl::PO3_tweaks_GetFormEditorID>(
                    REX::W32::GetProcAddress(tweaks, "GetFormEditorID"));
                logger::info("Got po3_tweaks api: {}", stl::func != nullptr);
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

    if (const auto* const message = SKSE::GetMessagingInterface(); !message->RegisterListener(MessageHandler)) {
        return false;
    }

    Papyrus::Bind();
    auto& parser{Parser::JSONParser::GetInstance()};
    rapidjson::Document sd;
    {
        stl::FilePtrManager file{"Data/SKSE/Plugins/OBody_presetDistributionConfig_schema.json"};
        if (file.error() != 0) {
            SKSE::stl::report_and_fail(
                "Please Check the Obody.log. Seems like there is a issue with loading the schema");
        }
        char readBuffer[65535];
        rapidjson::FileReadStream bis(file.get(), readBuffer, std::size(readBuffer));

        rapidjson::AutoUTFInputStream<unsigned, rapidjson::FileReadStream> eis(bis);
        if (sd.ParseStream<0, rapidjson::AutoUTF<unsigned>>(eis).HasParseError()) {
            logger::info("Error(offset {}): {}", sd.GetErrorOffset(), rapidjson::GetParseError_En(sd.GetParseError()));
            SKSE::stl::report_and_fail(
                "Please Check the Obody.log. Seems like there is a issue with loading "
                "OBody_presetDistributionConfig_schema.json");
        }
    }
    stl::FilePtrManager file("Data/SKSE/Plugins/OBody_presetDistributionConfig.json");
    if (file.error() != 0) {
        SKSE::stl::report_and_fail(
            "Please Check the Obody.log. Seems like there is a issue with loading OBody_presetDistributionConfig.json");
    }
    rapidjson::SchemaDocument schema(sd);
    char readBuffer[65535];
    rapidjson::FileReadStream bis(file.get(), readBuffer, std::size(readBuffer));

    rapidjson::AutoUTFInputStream<unsigned, rapidjson::FileReadStream> eis(bis);
    if (parser.presetDistributionConfig.ParseStream<0, rapidjson::AutoUTF<unsigned>>(eis).HasParseError()) {
        logger::info("Config Error(offset {}): {}", parser.presetDistributionConfig.GetErrorOffset(),
                     rapidjson::GetParseError_En(parser.presetDistributionConfig.GetParseError()));
        SKSE::stl::report_and_fail(
            "Please Check the Obody.log. Seems like there is an error when parsing "
            "OBody_presetDistributionConfig.json");
    }
    if (rapidjson::SchemaValidator validator(schema); !parser.presetDistributionConfig.Accept(validator)) {
        rapidjson::StringBuffer sb;
        const auto invalidSchemaPointer = validator.GetInvalidSchemaPointer();
        invalidSchemaPointer.StringifyUriFragment(sb);
        logger::error("Invalid schema: {}", sb.GetString());
        logger::error("Invalid keyword: {}", validator.GetInvalidSchemaKeyword());
        sb.Clear();
        const auto invalidDocumentPointer = validator.GetInvalidDocumentPointer();
        invalidDocumentPointer.StringifyUriFragment(sb);
        logger::error("Invalid document: {}", sb.GetString());
        sb.Clear();
        if (auto* err_value_ptr = invalidDocumentPointer.Get(parser.presetDistributionConfig)) {
            rapidjson::PrettyWriter writer(sb);
            err_value_ptr->Accept(writer);
            logger::error("Error at: {}", sb.GetString());
            sb.Clear();
        }
        if (auto* err_values_schema_pointer = invalidSchemaPointer.Get(sd)) {
            rapidjson::PrettyWriter writer(sb);
            err_values_schema_pointer->Accept(writer);
            logger::error("Schema Definition of Error: {}", sb.GetString());
        }
        SKSE::stl::report_and_fail(
            "Please Check the Obody.log. Seems like there is an error when validating the config using the json schema");
    }
    logger::info("Validated Data/SKSE/Plugins/OBody_presetDistributionConfig.json successfully");
    logger::info("{} has finished loading.", plugin->GetName());

    return true;
}