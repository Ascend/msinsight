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
import { Tooltip } from '@insight/lib/components';
import { localStorageService, LocalStorageKey } from '@insight/lib';
import i18n from '@insight/lib/i18n';
import { sendLanguage } from '@/connection/sendNotification';
import { LangEnIcon, LangZhIcon } from '@insight/lib/icon';
import { Language } from '@/utils/enum';

const useLanguage = (): [Language, (val: Language) => void] => {
    const [lang, setLang] = useState(localStorageService.getItem(LocalStorageKey.LANGUAGE) ?? Language.ZH);
    useEffect(() => {
        i18n.changeLanguage(lang);
        localStorageService.setItem(LocalStorageKey.LANGUAGE, lang);
        sendLanguage();
    }, [lang]);
    return [lang, setLang];
};

// 切换语言
function SwitchLanguage(): JSX.Element {
    const [lang, setLang] = useLanguage();
    const isChinese = lang === Language.ZH;

    const handleToggleLang = (): void => {
        setLang(isChinese ? Language.EN : Language.ZH);
    };

    return <Tooltip placement={'bottom'} title={'中文/English'}>
        <div onClick={handleToggleLang} data-testid="switch-lng">
            { isChinese ? <LangZhIcon className="size-20" /> : <LangEnIcon className="size-20" /> }
        </div>
    </Tooltip>;
}

export default SwitchLanguage;
