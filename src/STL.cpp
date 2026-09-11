//
// Created by Judah on 19-10-2025.
//
#include "STL.h"

#pragma warning(push)
#pragma warning(disable : 4'996)

stl::FilePtrManager::FilePtrManager(const char* path, const char* mode) noexcept : err(fopen_s(&fp, path, mode)) {
    // ReSharper disable CppDeprecatedEntity
    if (err != 0) {
        logger::error("Warning: Failed to open file '{}' pointer. Error: {}", path, strerror(err));
    }
}

stl::FilePtrManager::FilePtrManager(const wchar_t* path, const wchar_t* mode) noexcept
    : err(_wfopen_s(&fp, path, mode)) {
    if (err != 0) {
        wchar_t buffer[2048];
        swprintf_s(buffer, std::size(buffer), L"Failed to open file '%s' pointer. Error: %hs", path, strerror(err));
        SPDLOG_ERROR(buffer);
    }
}

stl::FilePtrManager::~FilePtrManager() {
    if (fp && (err = fclose(fp)) != 0) {
        logger::error("Warning: Failed to close file pointer: {}", strerror(err));
    }
}

stl::FilePtrManager::FilePtrManager(FilePtrManager&& other) noexcept : fp(other.fp), err(other.err) {
    other.fp = nullptr;
    other.err = 0;
}

stl::FilePtrManager& stl::FilePtrManager::operator=(FilePtrManager&& other) noexcept {
    if(this != &other) {
        if(fp) {
            if(fclose(fp) == EOF) {
                SPDLOG_ERROR("Failed to close file pointer in move constructor: {}", strerror(errno));
                // ReSharper restore CppDeprecatedEntity
            }
        }
        fp        = other.fp;
        err       = other.err;
        other.fp  = nullptr;
        other.err = 0;
    }
    return *this;
}

FILE* stl::FilePtrManager::get() noexcept { return fp; }

FILE* stl::FilePtrManager::get() const noexcept { return fp; }

errno_t stl::FilePtrManager::error() const noexcept { return err; }

#pragma warning(pop)

void stl::MergeJsonDocument(rapidjson::Value& target, rapidjson::Value& source,
                            rapidjson::Document::AllocatorType& allocator) {
    if (!source.IsObject() || !target.IsObject()) return;

    for (auto srcIt = source.MemberBegin(); srcIt != source.MemberEnd(); ++srcIt) {
        auto dstIt = target.FindMember(srcIt->name);
        if (dstIt == target.MemberEnd()) {
            rapidjson::Value key;
            key.CopyFrom(srcIt->name, allocator);
            rapidjson::Value val;
            val.CopyFrom(srcIt->value, allocator);
            target.AddMember(key, val, allocator);
        } else if (srcIt->value.IsObject() && dstIt->value.IsObject()) {
            MergeJsonDocument(dstIt->value, srcIt->value, allocator);
        } else {
            dstIt->value.CopyFrom(srcIt->value, allocator);
        }
    }
}

void stl::MergeJsonArray(rapidjson::Value& target, rapidjson::Value& source,
                          rapidjson::Document::AllocatorType& allocator) {
    if (!source.IsArray() || !target.IsArray()) return;

    for (auto& item : source.GetArray()) {
        const bool alreadyPresent = std::any_of(
            target.Begin(), target.End(), [&item](const rapidjson::Value& existing) { return existing == item; });
        if (!alreadyPresent) {
            rapidjson::Value val;
            val.CopyFrom(item, allocator);
            target.PushBack(val, allocator);
        }
    }
}

stl::timeit::~timeit() {
    const auto stop{std::chrono::steady_clock::now() - start};
    logger::info(
        "Time Taken in '{}' is {} nanoseconds or {} microseconds or {} milliseconds or {} seconds or "
        "{} minutes",
        curr.function_name(), stop.count(), std::chrono::duration_cast<std::chrono::microseconds>(stop).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(stop).count(),
        std::chrono::duration_cast<std::chrono::seconds>(stop).count(),
        std::chrono::duration_cast<std::chrono::minutes>(stop).count());
}