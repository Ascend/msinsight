/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef DATA_INSIGHT_CORE_JSON_UTIL_H
#define DATA_INSIGHT_CORE_JSON_UTIL_H

#include <string>
#include <optional>
#include <vector>
#include <algorithm>
#include <cfloat>
#include "rapidjson.h"
#include "document.h"
#include "writer.h"
#include "stringbuffer.h"
#include "ServerLog.h"
#include "FileUtil.h"
#include "filereadstream.h"
namespace Dic {
using json_t = rapidjson::Value;
using document_t = rapidjson::Document;
using namespace rapidjson;
class JsonUtil {
  public:
    constexpr static size_t MAX_JSON_LEAF_NUMBER =
        500ull * 1000ull * 1000ull; // 限制最大叶节点数50000w，以int32为例，50000w节点占用内存2G
    static inline bool IsJsonKeyValid(const json_t &json, std::string_view key) {
        return json.HasMember(key.data()) && !json[key.data()].IsNull();
    }

    template <typename T> static inline void SetByJsonKeyValue(T &src, const json_t &json, std::string_view key) {
        SetByJsonKeyValueHelper(src, json, key);
    }

    template <typename T>
    static inline void AddMember(
        json_t &json, std::string_view key, T &&value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        AddMemberHelper(json, key, std::forward<T>(value), allocator);
    }

    template <typename T>
    static inline void AddConstMember(
        json_t &json, std::string_view key, T &&value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        AddConstMemberHelper(json, key, std::forward<T>(value), allocator);
    }

    template <typename T>
    static inline void AddMember(json_t &json, std::string_view key, T &value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        AddMemberHelper(json, key, value, allocator);
    }

    template <typename T>
    static inline void AddMember(
        json_t &json, std::string_view key, std::optional<T> &value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        if (value.has_value()) {
            AddMemberHelper(json, key, value.value(), allocator);
        }
    }

    template <typename T>
    static inline void AddMember(
        json_t &json, std::string_view key, std::vector<T> value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json_t temp(kArrayType);
        for (const T &item : value) {
            if constexpr (std::is_same_v<T, std::string>) {
                temp.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
            } else if constexpr (std::is_same_v<T, int>) {
                temp.PushBack(item, allocator);
            } else {
                temp.PushBack(json_t().SetString(std::to_string(item).c_str(), allocator), allocator);
            }
        }
        AddMemberHelper(json, key, temp, allocator);
    }

    static inline std::optional<document_t> TryParse(const std::string &jsonStr, std::string &error) {
        return TryParse<kParseDefaultFlags>(jsonStr, error);
    }

    template <unsigned parseFlags>
    static inline std::optional<document_t> TryParse(const std::string &jsonStr, std::string &error) {
        document_t doc;
        doc.Parse<parseFlags>(jsonStr.c_str(), jsonStr.length());
        if (doc.HasParseError()) {
            static const size_t PRINT_ERROR_SIZE = 10;
            auto offset = doc.GetErrorOffset();
            auto start = offset >= PRINT_ERROR_SIZE ? offset - PRINT_ERROR_SIZE : 0;
            error = "Error code:" + std::to_string(doc.GetParseError()) +
                ". str:" + jsonStr.substr(start, offset - start + PRINT_ERROR_SIZE);
            return std::nullopt;
        }
        return std::make_optional(std::move(doc));
    }

    static inline bool IsJsonArray(const json_t &json, std::string_view key) {
        return (json.HasMember(key.data()) && json[key.data()].IsArray());
    }

    template <typename T> static inline std::vector<T> GetVector(const json_t &json, std::string_view key) {
        std::vector<T> vec;
        if (!IsJsonArray(json, key)) {
            return vec;
        }
        const Value &array = json[key.data()];
        vec.reserve(array.Size());
        for (auto &item : array.GetArray()) {
            if constexpr (std::is_same_v<T, double>) {
                vec.push_back(GetDoubleWithoutKey(item));
            } else if constexpr (std::is_same_v<T, std::string>) {
                vec.push_back(GetStringWithoutKey(item));
            } else if constexpr (std::is_same_v<T, float>) {
                vec.push_back(GetFloatWithoutKey(item));
            } else if constexpr (std::is_same_v<T, int>) {
                vec.push_back(GetIntWithoutKey(item));
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                vec.push_back(GetUint64WithoutKey(item));
            } else {
                Server::ServerLog::Error("Get vector from json error: unsupported type!");
            }
        }
        return vec;
    }

    static inline std::string JsonDump(const json_t &json) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json.Accept(writer);
        return buffer.GetString();
    }

    // for timeline file parse
    static inline double GetDouble(const json_t &json, std::string_view key) {
        if (!json.HasMember(key.data())) {
            return 0;
        }
        const auto &value = json[key.data()];
        if (value.IsNumber()) {
            return value.GetDouble();
        }
        if (value.IsString()) {
            try {
                return std::stod(value.GetString());
            } catch (std::invalid_argument &e) {
                // 处理无效参数异常
                Server::ServerLog::Error("Invalid argument: ", e.what());
                return 0;
            } catch (std::out_of_range &e) {
                // 处理超出范围异常
                Server::ServerLog::Error("Out of range: ", e.what());
                return 0;
            }
        }
        return 0;
    }

    static inline double GetDoubleWithoutKey(const json_t &json) {
        if (json.IsNumber()) {
            return json.GetDouble();
        }
        Server::ServerLog::Error("JSON is not a number when getting double without key. Returning 0.0.");
        return 0.0;
    }

    static inline long double GetLongDouble(const json_t &json, std::string_view key) {
        if (!json.HasMember(key.data())) {
            return 0;
        }
        const auto &value = json[key.data()];
        if (value.IsNumber()) {
            return value.GetDouble();
        }
        if (value.IsString()) {
            try {
                return std::stold(value.GetString());
            } catch (std::invalid_argument &e) {
                // 处理无效参数异常
                Server::ServerLog::Error("Invalid argument: ", e.what());
                return 0;
            } catch (std::out_of_range &e) {
                // 处理超出范围异常
                Server::ServerLog::Error("Out of range: ", e.what());
                return 0;
            }
        }
        return 0;
    }

    static inline int64_t GetInteger(const json_t &json, std::string_view key) {
        if (!json.HasMember(key.data())) {
            return 0;
        }
        const auto &value = json[key.data()];
        if (value.IsInt()) {
            return value.GetInt64();
        }
        if (value.IsDouble()) {
            double dValue = value.GetDouble();
            if (dValue >= INT64_MIN && dValue <= INT64_MAX) {
                return static_cast<int64_t>(dValue);
            }
            Server::ServerLog::Error("Value out of range: ", dValue);
            return 0;
        }
        if (value.IsString()) {
            try {
                return std::stoll(value.GetString());
            } catch (std::invalid_argument &e) {
                // 处理无效参数异常
                Server::ServerLog::Error("Invalid argument: ", e.what());
                return 0;
            } catch (std::out_of_range &e) {
                // 处理超出范围异常
                Server::ServerLog::Error("Out of range: ", e.what());
                return 0;
            }
        }
        return 0;
    }

    static inline int GetIntWithoutKey(const json_t &json) {
        if (json.IsInt()) {
            return json.GetInt();
        }
        Server::ServerLog::Error("JSON is not a int when getting int without key. Returning 0.");
        return 0;
    }

    static inline std::string GetString(const json_t &json, std::string_view key) {
        if (!json.HasMember(key.data())) {
            return "";
        }
        const auto &value = json[key.data()];
        if (value.IsString()) {
            return value.GetString();
        }
        return "";
    }

    static inline std::string GetStringWithoutKey(const json_t &json) {
        if (json.IsString()) {
            return json.GetString();
        }
        Server::ServerLog::Error("JSON is not a string when getting string without key. Returning empty string.");
        return "";
    }

    static inline float GetFloat(const json_t &json, std::string_view key) {
        if (!json.HasMember(key.data())) {
            return 0;
        }
        auto const &value = json[key.data()];
        if (value.IsFloat()) {
            return value.GetFloat();
        }
        if (value.IsString()) {
            try {
                std::string str = value.GetString();
                double data = std::stod(str);
                if (data > FLT_MAX || data < -FLT_MAX) {
                    Server::ServerLog::Error("Value out of range.");
                    return 0;
                }
                return static_cast<float>(data);
            } catch (std::exception &e) {
                return 0;
            }
        }
        return 0;
    }

    static inline float GetFloatWithoutKey(const json_t &json) {
        if (json.IsNumber()) {
            return json.GetFloat();
        }
        Server::ServerLog::Error("JSON is not a number when getting float without key. Returning 0.0.");
        return 0.0;
    }

    static inline uint64_t GetUint64WithoutKey(const json_t &json) {
        if (json.IsUint64()) {
            return json.GetUint64();
        }
        Server::ServerLog::Error("JSON is not a uint64_t when getting uint64_t without key. Returning 0.");
        return 0;
    }

    static inline std::optional<std::string> GetOptionalString(const json_t &json, std::string_view key) {
        if (!json.HasMember(key.data())) {
            return std::nullopt;
        }
        const auto &value = json[key.data()];
        if (value.IsString()) {
            return value.GetString();
        } else {
            return JsonDump(value);
        }
    }

    static inline std::string GetDumpString(const json_t &json, std::string_view key) {
        if (!json.HasMember(key.data())) {
            return "";
        }
        const auto &value = json[key.data()];
        if (value.IsString()) {
            return value.GetString();
        } else {
            return JsonDump(value);
        }
    }

    static inline std::string MapToJsonStr(const std::unordered_map<std::string, std::string> &data) {
        document_t doc;
        auto &allocator = doc.GetAllocator();
        doc.SetObject();

        for (const auto &kv : data) {
            AddMember(doc, kv.first.c_str(), kv.second, allocator);
        }

        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    }

    static inline std::unordered_map<std::string, std::string> JsonStrToMap(const std::string &jsonStr) {
        if (jsonStr.empty()) {
            return {};
        }

        document_t doc;
        if (doc.Parse(jsonStr.c_str()).HasParseError()) {
            return {};
        }
        std::unordered_map<std::string, std::string> data;
        for (auto &member : doc.GetObj()) {
            std::string key = member.name.GetString();
            std::string value = member.value.GetString();
            data[key] = value;
        }

        return data;
    }

    static std::vector<std::string> JsonToVector(const std::string &jsonStr) {
        rapidjson::Document document;
        if (document.Parse(jsonStr.c_str()).HasParseError()) {
            Server::ServerLog::Error("Fail to parse json");
            return {};
        }
        return JsonToVector(document);
    }

    static std::vector<std::string> JsonToVector(const json_t &json) {
        if (!json.IsArray()) {
            Server::ServerLog::Error("Fail to convert json data to vector, invalid json type.");
            return {};
        }
        std::vector<std::string> result;
        for (rapidjson::SizeType i = 0; i < json.Size(); i++) {
            if (json[i].IsString()) {
                result.emplace_back(json[i].GetString());
            } else {
                Server::ServerLog::Warn("Unsupported type when transfer json to vector.");
            }
        }
        return result;
    }

    static inline document_t ReadJsonFromFile(const std::string &filePath) {
        document_t document;
        document.SetNull();
        if (!FileUtil::CheckPathSecurity(filePath, CHECK_FILE_READ)) {
            Server::ServerLog::Error("Invalid path, can't read json from file, path=", filePath);
            return document;
        }
        FILE *file = std::fopen(filePath.c_str(), "r");
        if (!file) {
            Server::ServerLog::Error("open json file failed, path=", filePath);
            return document;
        }
        std::string readBuffer;
        readBuffer.reserve(65536);
        rapidjson::FileReadStream is(file, readBuffer.data(), readBuffer.capacity());
        rapidjson::Document doc;
        doc.ParseStream(is);
        std::fclose(file);

        if (doc.HasParseError()) {
            Server::ServerLog::Error("Parse failed when read file, error:", doc.GetParseError());
            doc.SetNull();
            return doc;
        }
        return doc;
    }

  private:
    template <typename T>
    static inline void SetByJsonKeyValueHelper(std::optional<T> &src, const json_t &json, std::string_view key) {
        SetByJsonKeyValueHelper(*src, json, key);
    }

    template <typename T> static inline void SetByJsonKeyValueHelper(T &src, const json_t &json, std::string_view key) {
        if (json.HasMember(key.data()) && json[key.data()].Is<T>()) {
            src = json[key.data()].Get<T>();
        }
    }

    static inline void SetByJsonKeyValueHelper(std::string &src, const json_t &json, std::string_view key) {
        if (json.HasMember(key.data()) && json[key.data()].IsString()) {
            src = json[key.data()].GetString();
        }
    }

    static inline void SetByJsonKeyValueHelper(double &src, const json_t &json, std::string_view key) {
        if (json.HasMember(key.data()) && json[key.data()].IsNumber()) {
            src = json[key.data()].GetDouble();
        }
    }

    static inline void SetByJsonKeyValueHelper(float &src, const json_t &json, std::string_view key) {
        if (json.HasMember(key.data()) && json[key.data()].IsNumber()) {
            src = json[key.data()].GetFloat();
        }
    }

    template <typename T>
    static inline void AddMemberHelper(
        json_t &json, std::string_view key, T &&value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json.AddMember(rapidjson::StringRef(key.data(), key.length()), std::forward<T>(value), allocator);
    }

    static inline void AddMemberHelper(
        json_t &json, std::string_view key, std::string &&value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json.AddMember(rapidjson::StringRef(key.data(), key.length()),
            json_t().SetString(value.data(), value.length(), allocator), allocator);
    }

    static inline void AddMemberHelper(
        json_t &json, std::string_view key, const std::string &&value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json.AddMember(rapidjson::StringRef(key.data(), key.length()),
            json_t().SetString(value.c_str(), value.length(), allocator), allocator);
    }

    template <typename T>
    static inline void AddMemberHelper(
        json_t &json, std::string_view key, T &value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json.AddMember(rapidjson::StringRef(key.data(), key.length()), value, allocator);
    }

    static inline void AddMemberHelper(
        json_t &json, std::string_view key, std::string &value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json.AddMember(rapidjson::StringRef(key.data(), key.length()),
            json_t().SetString(value.data(), value.length(), allocator), allocator);
    }

    static inline void AddMemberHelper(
        json_t &json, std::string_view key, const std::string &value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json.AddMember(rapidjson::StringRef(key.data(), key.length()),
            json_t().SetString(value.c_str(), value.length(), allocator), allocator);
    }

    static inline void AddConstMemberHelper(
        json_t &json, std::string_view key, const std::string &value, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        std::string strKey{key};
        json.AddMember(
            Value(strKey.c_str(), allocator), json_t().SetString(value.c_str(), value.length(), allocator), allocator);
    }
};
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_JSON_UTIL_H
