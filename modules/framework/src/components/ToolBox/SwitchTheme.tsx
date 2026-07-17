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
import React, { useEffect, useState } from 'react';
import styled from '@emotion/styled';
import { Tooltip } from '@insight/lib/components';
import { MoonOutlined, SunOutlined } from '@ant-design/icons';
import { localStorageService, LocalStorageKey } from '@insight/lib';
import { ThemeName } from '@/utils/enum';
import { sendTheme } from '@/connection/sendNotification';
import { useTranslation } from 'react-i18next';

const IconWrapper = styled.div`
    display: flex;
    align-items: center;
    justify-content: center;
    svg {
        width: 16px !important;
        height: 16px !important;
    }
`;

const useThemeState = (): [ThemeName, (val: ThemeName) => void] => {
    const [theme, setTheme] = useState(localStorageService.getItem(LocalStorageKey.THEME) ?? ThemeName.DARK);

    useEffect(() => {
        window.setTheme(theme === ThemeName.DARK);
        localStorageService.setItem(LocalStorageKey.THEME, theme);
        sendTheme();
    }, [theme]);
    return [theme, setTheme];
};

// 切换主题
function SwitchTheme(): JSX.Element {
    const { t } = useTranslation('framework');
    const [theme, setTheme] = useThemeState();
    const isDarkTheme = theme === ThemeName.DARK;
    const onChange = (): void => {
        setTheme(isDarkTheme ? ThemeName.LIGHT : ThemeName.DARK);
    };

    return <Tooltip placement={'bottom'} title={isDarkTheme ? t('Light Theme') : t('Dark Theme')}>
        <IconWrapper onClick={onChange} className="switch-theme" data-testid="switch-theme">
            {isDarkTheme ? <SunOutlined /> : <MoonOutlined />}
        </IconWrapper>
    </Tooltip>;
}

export default SwitchTheme;
