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
import { customConsole as console, safeJSONParse } from '../utils';

export enum LocalStorageKey {
    LANGUAGE = 'language',
    THEME = 'theme',
    LAST_FILE_PATH = 'last_file_path',
    OVERALL_METRICS_CLASSIFICATION_RULES = 'overall_metrics_classification_rules',
}

class LocalStorageService {
    getItem(key: LocalStorageKey): any {
        try {
            const item = localStorage.getItem(key);
            return item !== null ? safeJSONParse(item) : null;
        } catch (error) {
            console.log(error);
            return null;
        }
    }

    setItem(key: LocalStorageKey, value: any): boolean {
        try {
            localStorage.setItem(key, JSON.stringify(value));
            return true;
        } catch (error) {
            console.log(error);
            return false;
        }
    }

    removeItem(key: LocalStorageKey): void {
        localStorage.removeItem(key);
    }

    clear(): void {
        localStorage.clear();
    }
}

export const localStorageService = new LocalStorageService();
export default localStorageService;
