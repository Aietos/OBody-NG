#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

// Note: add new modules here before using
#include <unordered_set>
#include <ranges>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <pugixml.hpp>
#include <random>
#include <boost/algorithm/string.hpp>

namespace logger = SKSE::log;
namespace fs = std::filesystem;

using namespace std::literals;

#define DLLEXPORT __declspec(dllexport)
