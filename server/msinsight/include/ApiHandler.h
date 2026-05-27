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
#ifndef MSINSIGHT_APIHANDLER_H
#define MSINSIGHT_APIHANDLER_H
#include "string"
namespace Dic::Core {
enum class API_TYPE { GET, POST };
class ApiHandler {
  public:
    ApiHandler(API_TYPE type) : apiType_(type) {};
    virtual ~ApiHandler() = default;
    virtual bool run(std::string_view data, std::string &result) = 0;
    API_TYPE GetApiType() { return apiType_; };

  protected:
    API_TYPE apiType_;
};
class GetHandler : public ApiHandler {
  public:
    GetHandler() : ApiHandler(API_TYPE::GET) {};
    virtual ~GetHandler() = default;
    virtual bool run(std::string_view data, std::string &result) = 0;
};
class PostHandler : public ApiHandler {
  public:
    PostHandler() : ApiHandler(API_TYPE::POST) {};
    virtual ~PostHandler() = default;
    virtual bool run(std::string_view data, std::string &result) = 0;
};
}
#endif // MSINSIGHT_APIHANDLER_H
