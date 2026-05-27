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

#ifndef DATA_INSIGHT_CORE_ID_BUILDER_H
#define DATA_INSIGHT_CORE_ID_BUILDER_H

#include <mutex>

namespace Dic {
class IdBuilder {
  public:
    static IdBuilder &RequestIdBuilder();
    static IdBuilder &ResponseIdBuilder();
    static IdBuilder &EventIdBuilder();
    static IdBuilder &SessionIdBuilder();
    int Build();

  private:
    IdBuilder() = default;
    ~IdBuilder() = default;

    const int maxId = 0x7dffffff;
    int id = 0;
    std::mutex idMutex;
};
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_ID_BUILDER_H
