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

#ifndef PROFILER_SERVER_COUNTEREVENTHELPER_H
#define PROFILER_SERVER_COUNTEREVENTHELPER_H
#include <string>
#include <vector>
#include "FullDbEnumUtil.h"
#include "TraceDatabaseDef.h"
namespace Dic::Module::Timeline {
/* CounterEventConfig是db格式展示Counter Event的配置项
 * 作用是当新增Counter Event时，只需要新增配置项，即可自动展示新的Counter Event，而避免了写繁琐的SQL
 * 实现了两个核心功能，一是解析成功后返回泳道的元数据的查询SQL，功能实现见GenerateHostMetadataSQL和GenerateDeviceMetadataSQL
 * 二是展示线程泳道具体数据的查询SQL，功能实现见GenerateHostCounterSQL和GenerateDeviceCounterSQL
 * 各配置项的含义
 * processName 进程名，Timeline中进程泳道的名称
 * tableName db文件的表名
 * valueName 作为柱状图值展现的列在db表中的列名称
 * 如果有多个列需要作为值进行展示，每一个展示的值需要新增一项配置项，所以会有multimap的数据结构
 * threadNameForamt 泳道线程名称，为格式字符串，{}外是固定内容，{}内的内容会被对应表中字段替换
 * 比如"CPU {cpuId}"表示列名固定字段为"CPU "，看tableName表中有多少个不同的cpuId值，假设有0 1两个值，泳道线程名称就会返回CPU 0和CPU 1两个泳道
 * 当{}内的内容以:s结尾时，表示{}内:s前面的部分会和STRING_IDS表JOIN，用对应的STRING_IDS表的value列替换
 * 该字符串可能含有/或者不含有/，当含有/时，/后的部分是展示的列名，当不含有/时，整个字符串表示展示的列名
 * 展示的列名和valueName为了可读性会有所不同，具体见displayNameToValueName，如果该map没有展示的列名对应的key，则valueName就是展示的列名
 * 保证当请求unit/counter时，同一进程的不同线程泳道请求的threadId一定不同
 * type 前端展示需要的信息
 * 即鼠标移动到柱状图上，展示的key-value内容中的key
 * SQL的具体例子可以看DT
 */
struct CounterEventConfig {
    std::string processName;
    std::string tableName;
    std::string valueName;
    std::string threadNameFormat;
    std::string type;
};

class CounterEventHelper {
  public:
    CounterEventHelper() = default;
    ~CounterEventHelper() = default;
    void RegisterHostMap();
    void RegisterDeviceMap();
    std::string GenerateHostMetadataSQL(const PROCESS_TYPE type);
    std::string GenerateHostCounterSQL(const PROCESS_TYPE type);
    std::string GetDeviceProcessName(const PROCESS_TYPE type);
    std::string GetDeviceTableName(const PROCESS_TYPE type);
    std::string GenerateDeviceMetadataSQL(const PROCESS_TYPE type);
    std::string GenerateDeviceCounterSQL(const PROCESS_TYPE type, const std::string &threadId);
    std::map<PROCESS_TYPE, CounterEventConfig> hostCounterEventMap;
    std::multimap<PROCESS_TYPE, CounterEventConfig> deviceCounterEventMap;

  protected:
    void RegisterDeviceAICoreFreqMap();
    void RegisterDeviceAccPMUMap();
    void RegisterDeviceDDRMap();
    void RegisterDeviceStarsSocMap();
    void RegisterDeviceNPUMEMMap();
    void RegisterDeviceHBMMap();
    void RegisterDeviceLLCMap();
    void RegisterDeviceSamplePMUMap();
    void RegisterDeviceNICMap();
    void RegisterDevicePCIeMap();
    void RegisterDeviceHCCSMap();
    void RegisterDeviceQOSMap();
    std::string SubstituteThreadNameFormat(const std::string &format, std::vector<std::string> &valueNamesToJoin);
    const static std::map<std::string, std::string> displayNameToValueName;
};
}
#endif // PROFILER_SERVER_COUNTEREVENTHELPER_H
