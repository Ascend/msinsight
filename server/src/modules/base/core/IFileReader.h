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

#ifndef PROFILER_SERVER_IFILEREADER_H
#define PROFILER_SERVER_IFILEREADER_H
#include <string>
namespace Dic {
namespace Module {
class IFileReader {
  public:
    /* *
    * 获取文件大小
    * @param filePath
    * @return
    */
    virtual int64_t GetFileSize(const std::string &filePath) = 0;
    /* *
     * 从文件中读取json数组
     * @param filePath 文件路径
     * @param startPosition 要读取的开始位置
     * @param endPosition 要读取的结束位置
     * @return
     * 如果startPosition和endPosition都为0就是读取整个文件
     * startPosition必须小于等于endPosition
     */
    virtual std::string ReadJsonArray(const std::string &filePath, int64_t startPosition, int64_t endPosition) = 0;
    virtual ~IFileReader() = default;
};
}
}
#endif // PROFILER_SERVER_IFILEREADER_H
