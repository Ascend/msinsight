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

export enum ProjectError {
    NO_ERRORS = 0,
    PROJECT_NAME_CONFLICT = 1,
    IS_UNSAFE_PATH = 2,
    EXISTING_LARGE_FILES = 3,
    // 文件/文件夹作已导入
    IMPORTED = 4,
    EXCEEDS_MXIMUN_LENGTH = 5,
    FILE_NOT_EXIST = 6,
    IS_NOT_REGULAR_FILE = 7,
    OTHER = -1,
}

export enum ProjectAction {
    // 切换项目
    SWITCH_PROJECT = 0,
    // 导入新文件
    ADD_FILE = 1,
};

export enum SessionAction {
    ADD_DATA_UNDER_PROJECT = 'add data under project',
    SWITCH_ACTIVE_MODULE = 'switch active module',
    IMPORT_MOE_LOAD_DATA = 'import MOE load data',
    NO_ACTION = 'no action',
}

export enum ThemeName {
    DARK = 'dark',
    LIGHT = 'light',
}

export enum Language {
    ZH = 'zhCN',
    EN = 'enUS',
}

export enum ProjectType {
    DB = 0,
    BIN = 1,
    IPYNB = 2,
    TEXT_CLUSTER = 3,
    SIMULATION = 4,
    TRACE = 5,
    DB_CLUSTER = 6,
    IE = 7,
    DB_NPUMONITOR = 8,
    DB_FTRACE = 13,
    OTHER = 9,
}
