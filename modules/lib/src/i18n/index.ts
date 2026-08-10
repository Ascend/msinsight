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
import { localStorageService, LocalStorageKey } from '../LocalStorage';
import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';
import en from './en.json';
import zh from './zh.json';
import frameworkEn from './framework/en.json';
import frameworkZh from './framework/zh.json';
import communicationEn from './communication/en.json';
import communicationZh from './communication/zh.json';
import sourceEn from './source/en.json';
import sourceZh from './source/zh.json';
import detailsEn from './details/en.json';
import detailsZh from './details/zh.json';
import operatorEn from './operator/en.json';
import operatorZh from './operator/zh.json';
import summaryEn from './summary/en.json';
import summaryZh from './summary/zh.json';
import memoryEn from './memory/en.json';
import memoryZh from './memory/zh.json';
import timelineEn from './timeline/en.json';
import timelineZh from './timeline/zh.json';
import libEn from './lib/en.json';
import libZh from './lib/zh.json';
import leaksEn from './leaks/en.json';
import leaksZh from './leaks/zh.json';
import statisticEn from './statistic/en.json';
import statisticZh from './statistic/zh.json';
import rlEn from './reinforcement-learning/en.json';
import rlZh from './reinforcement-learning/zh.json';
import tritonEn from './triton/en.json';
import tritonZh from './triton/zh.json';
import insightWebAgentEn from './insight-web-agent/en.json';
import insightWebAgentZh from './insight-web-agent/zh.json';

export const resources = {
    enUS: {
        ...en,
        ...frameworkEn,
        ...communicationEn,
        ...sourceEn,
        ...detailsEn,
        ...operatorEn,
        ...summaryEn,
        ...memoryEn,
        ...timelineEn,
        ...libEn,
        ...leaksEn,
        ...statisticEn,
        ...rlEn,
        ...tritonEn,
        ...insightWebAgentEn,
    },
    zhCN: {
        ...zh,
        ...frameworkZh,
        ...communicationZh,
        ...sourceZh,
        ...detailsZh,
        ...operatorZh,
        ...summaryZh,
        ...memoryZh,
        ...timelineZh,
        ...libZh,
        ...leaksZh,
        ...statisticZh,
        ...rlZh,
        ...tritonZh,
        ...insightWebAgentZh,
    },
};

i18n.use(initReactI18next).init({
    resources,
    lng: localStorageService.getItem(LocalStorageKey.LANGUAGE) ?? 'enUS',
    interpolation: {
        escapeValue: false, // react already safes from xss
    },
});

export default i18n;
