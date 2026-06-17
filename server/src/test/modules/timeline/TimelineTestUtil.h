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

#ifndef PROFILER_SERVER_TIMELINETESTUTIL_CPP
#define PROFILER_SERVER_TIMELINETESTUTIL_CPP
const uint64_t SYS_PAGE_SIZE = 10;
const uint64_t CUR_PAGE_1 = 1;
const uint64_t CUR_PAGE_2 = 2;
const uint64_t EVENTS_VIEW_COLUMN_SIZE = 5;
const uint64_t EVENTS_VIEW_COLUMN_NAME = 0;
const uint64_t EVENTS_VIEW_COLUMN_START = 1;
const uint64_t EVENTS_VIEW_COLUMN_DURATION = 2;
const uint64_t EVENTS_VIEW_COLUMN_4 = 3;
const uint64_t EVENTS_VIEW_COLUMN_5 = 4;

inline void CheckEventsViewBaseColumns(Dic::Protocol::EventsViewBody &body)
{
    EXPECT_EQ(body.columnList.size(), EVENTS_VIEW_COLUMN_SIZE);
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_NAME).name, "Name");
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_START).name, "Start");
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_DURATION).name, "Duration(ns)");
}

inline void CheckEventsViewColumns4Api(Dic::Protocol::EventsViewBody &body)
{
    CheckEventsViewBaseColumns(body);
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_4).name, "TID");
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_5).name, "PID");
}

inline void CheckEventsViewColumns4Hardware(Dic::Protocol::EventsViewBody &body)
{
    CheckEventsViewBaseColumns(body);
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_4).name, "Stream Name");
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_5).name, "Rank ID");
}

inline void CheckEventsViewColumns4Group(Dic::Protocol::EventsViewBody &body)
{
    CheckEventsViewBaseColumns(body);
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_4).name, "Group Name");
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_5).name, "Rank ID");
}

inline void CheckEventsViewColumns4Overlap(Dic::Protocol::EventsViewBody &body)
{
    CheckEventsViewBaseColumns(body);
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_4).name, "Analysis Type");
    EXPECT_EQ(body.columnList.at(EVENTS_VIEW_COLUMN_5).name, "Rank ID");
}

#endif // PROFILER_SERVER_TIMELINETESTUTIL_CPP
