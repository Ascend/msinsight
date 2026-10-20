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
import type { Theme } from '@emotion/react';
import { makeAutoObservable } from 'mobx';
import { ThemeItem } from '../theme';

interface ThemeType {
    light: Theme;
    dark: Theme;
}

export default class ThemeStore {
    currentTheme: ThemeItem = 'light';
    theme: ThemeType;
    constructor(theme = { light: {}, dark: {} }) {
        makeAutoObservable(this);
        let lsTheme = localStorage.getItem('theme') ?? 'light';
        try {
            lsTheme = JSON.parse(lsTheme);
        } catch (e) {
            lsTheme = 'light';
        }
        this.currentTheme = ['light', 'dark'].includes(lsTheme) ? lsTheme as ThemeItem : 'light';
        this.theme = theme as ThemeType;
    }

    getCurrentTheme(): ThemeItem {
        return this.currentTheme;
    }

    setCurrentTheme(currentTheme: ThemeItem): void {
        this.currentTheme = currentTheme;
        document.body.className = currentTheme === 'dark' ? 'theme_dark' : 'theme_light';
    }

    getTheme(): ThemeType {
        return this.theme;
    }

    getThemeType(): Theme {
        return this.theme[this.currentTheme];
    }

    get themeObject(): Theme {
        return this.theme[this.currentTheme];
    }
}
