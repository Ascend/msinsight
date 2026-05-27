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
#include "ModelGenConfigParser.h"
#include "FileReader.h"
#include "JsonUtil.h"
#include "SummaryErrorManager.h"

namespace Dic::Module::Summary {
ModelGenConfig ModelGenConfigParser::ParserModelGenConfigByFilePath(const std::string &filePath, std::string &error) {
    FileReader reader;
    std::string fileContext = reader.ReadJsonArray(filePath, 0, 0);
    ModelGenConfig res;
    if (fileContext.empty()) {
        error = "Fail to read model gen config file, the file is empty.";
        SetSummaryError(ErrorCode::READ_MODEL_GEN_CONFIG_FILE_FAILED);
        return res;
    }
    try {
        auto configJsonOpt = JsonUtil::TryParse(fileContext, error);
        if (!error.empty()) {
            return res;
        }
        if (!configJsonOpt.value().IsObject()) {
            error = "Fail to parser config file, data in wrong format.";
            SetSummaryError(ErrorCode::READ_MODEL_GEN_CONFIG_FILE_FAILED);
            return res;
        }
        int moeLayer = JsonUtil::GetInteger(configJsonOpt.value(), "num_moe_layers");
        if (moeLayer < 0) {
            error = "Fail to read model gen config file, the number of moe layer can't less than zero.";
            SetSummaryError(ErrorCode::READ_MODEL_GEN_CONFIG_FILE_FAILED);
            return res;
        }
        res.moeLayer = static_cast<uint64_t>(moeLayer);
        return res;
    } catch (const std::exception &e) {
        error = "Fail to parser meta data file context. Except: " + std::string(e.what());
        SetSummaryError(ErrorCode::PARSER_META_DATA_FILE_CONTEXT_FAILED);
        return res;
    }
}
}
