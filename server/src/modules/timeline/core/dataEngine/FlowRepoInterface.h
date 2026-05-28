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

#ifndef PROFILER_SERVER_FLOWREPOINTERFACE_H
#define PROFILER_SERVER_FLOWREPOINTERFACE_H
#include <vector>
#include "DominQuery.h"
#include "DomainObject.h"
namespace Dic::Module::Timeline {
class FlowRepoInterface {
  public:
    virtual ~FlowRepoInterface() = default;
    /* *
     * 根据连线类别查询所有连线
     * @param flowQuery
     * @param flowPointVec
     */
    virtual void QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) = 0;
    /* *
     * 查询时间范围内的所有连线点
     * @param flowQuery
     * @param flowPointVec
     */
    virtual void QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) = 0;

    /* *
     * 根据连线点id查询所有连线点
     * @param flowQuery
     * @param flowPointVec
     */
    virtual void QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) = 0;
};
}
#endif // PROFILER_SERVER_FLOWREPOINTERFACE_H
