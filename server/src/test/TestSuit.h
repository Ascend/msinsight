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
#ifndef PROFILER_SERVER_TESTSUIT_H
#define PROFILER_SERVER_TESTSUIT_H

#include <gtest/gtest.h>
#include "JsonFileParserManager.h"
#include "ClusterFileParser.h"
#include "FileUtil.h"
#include "DataBaseManager.h"
#include "ServerLog.h"
#include "ParamsParser.h"
#include "ParserStatusManager.h"
#include "KernelParse.h"
#include "MemoryParse.h"
#include "TimeUtil.h"
#include "WsSessionImpl.h"
#include "RepositoryFactory.h"
#include "DataEngine.h"
#include "RenderEngine.h"
#include "DtFramework.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::Summary;
using namespace Dic::Module::Memory;
using namespace Dic;
using namespace Dic::DT::Framework;

class TestSuit : public ::testing::Test {
  public:
    // 静态成员变量
    static std::string clusterPath;

    // 默认构造函数
    TestSuit() = default;

    // 测试套件初始化函数
    static void SetUpTestSuite();

    // 测试套件清理函数
    static void TearDownTestSuite();

    // 等待解析结束的函数
    static void WaitParseEnd(std::vector<std::string> statusList);

    // 主函数
    static int Main(int argc, char **argv);

    // 获取渲染引擎实例
    static std::shared_ptr<RenderEngine> GetRenderEngine();

    // 获取server(.../server)
    static std::string GetServerHome();
    static std::string GetSrcTestPath();
    static std::string GetRootTestPath();
    template <typename... Args> static std::string GetTestDataFile(Args... paths) {
        return FileUtil::SplicePath(DtFramework::GetTestDataDirPath(TestPathType::SRC_TEST_DATA), paths...);
    }

  private:
    // 禁止拷贝构造和赋值操作
    TestSuit(const TestSuit &) = delete;
    TestSuit &operator=(const TestSuit &) = delete;
    static std::string serverHome;
    static std::string srcTestPath;
    static std::string rootTestPath;
};

#endif // PROFILER_SERVER_TESTSUIT_H
