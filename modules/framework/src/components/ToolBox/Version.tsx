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
import React, { useState } from 'react';
import { Modal } from 'antd';
import VersionInfo from '@/version_info.json';
import { useTranslation } from 'react-i18next';
import { HelpIcon } from '@insight/lib/icon';
import styled from '@emotion/styled';
import { Popover } from '@insight/lib/components';
import { ShortcutsModal } from '@/components/KbdShortcuts';
import { HELP_LINKS } from '@/utils';
import {
    GoldOutlined,
    FileTextOutlined,
    QuestionCircleOutlined,
    EditOutlined,
    InfoCircleOutlined,
} from '@ant-design/icons';

// 软件首发年分
const LAUNCH_YEAR = '2023';
const { version, modifyTime } = VersionInfo;
const modifyYear = modifyTime.split('/')[0];
const copyrightYear = modifyYear === LAUNCH_YEAR ? modifyYear : `${LAUNCH_YEAR}-${modifyYear}`;

const VersionContainer = styled.ul`
    padding: 0;
    list-style: none;
    li {
        margin-bottom: 10px;
        line-height: 1.5em;
        color: ${(p): string => p.theme.textColor};
    }
`;

const PopoverItem = styled.div`
    display: flex;
    align-items: center;
    gap: 8px;

    .anticon {
        color: ${(p): string => p.theme.textColorTertiary};
    }

    &:hover {
        .anticon {
            color: #ffffff;
        }
    }
`;

// 版本信息
function Version(): JSX.Element {
    const { t } = useTranslation('framework');
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [popoverVisible, setPopoverVisible] = useState(false);
    const [shortcutsVisible, setShortcutsVisible] = useState(false);

    const showVersion = (): void => {
        setIsModalOpen(true);
    };
    const closeVersion = (): void => {
        setIsModalOpen(false);
    };

    const handleHelpIconClick = (): void => {
        setPopoverVisible(true);
    };

    const handleBlur = (): void => {
        setTimeout(() => {
            setPopoverVisible(false);
        }, 200);
    };

    const showShortcutsModal = (): void => {
        setShortcutsVisible(true);
    };

    const showLogInExplorer = (): void => {
        window.ipc.postMessage('showLogInExplorer');
    };

    const showDocumentation = (): void => {
        window.ipc.postMessage(`openUrl|${HELP_LINKS.documentation}`);
    };

    const showHelpFeedback = (): void => {
        window.ipc.postMessage(`openUrl|${HELP_LINKS.help_feedback}`);
    };

    const handleShortcutsModalClose = (): void => {
        setShortcutsVisible(false);
    };

    const HelpPopoverContent = (): JSX.Element => {
        return <div>
            <PopoverItem className="mi-popover-item" onClick={showShortcutsModal}>
                <GoldOutlined />
                {t('Keyboard shortcuts')}
            </PopoverItem>
            <PopoverItem className="mi-popover-item" onClick={showLogInExplorer}>
                <FileTextOutlined />
                {t('Show Log in Explorer')}
            </PopoverItem>
            <PopoverItem className="mi-popover-item" onClick={showDocumentation}>
                <QuestionCircleOutlined />
                {t('Documentation')}
            </PopoverItem>
            <PopoverItem className="mi-popover-item" onClick={showHelpFeedback}>
                <EditOutlined />
                {t('Help & Feedback')}
            </PopoverItem>
            <PopoverItem className="mi-popover-item" onClick={showVersion}>
                <InfoCircleOutlined />
                {t('About')}
            </PopoverItem>
        </div>;
    };

    return <>
        <Popover
            content={HelpPopoverContent}
            placement="bottomRight"
            arrowPointAtCenter
            open={popoverVisible}
            getPopupContainer={(triggerNode: HTMLElement): HTMLElement => triggerNode}
        >
            <HelpIcon color="currentColor" className="size-20" tabIndex={-1} onBlur={handleBlur} onClick={handleHelpIconClick} data-testid="help-icon"/>
        </Popover>
        <Modal title={`${t('About')} MindStudio Insight`} open={isModalOpen} onCancel={closeVersion} destroyOnClose={true} footer={null}>
            <VersionContainer className="help-ul">
                <li>{t('buildVersion', { version, modifyTime })}</li>
                <li>{t('copyRight', { copyrightYear })}</li>
            </VersionContainer>
        </Modal>
        <ShortcutsModal
            open={shortcutsVisible}
            onClose={handleShortcutsModalClose}
        />
    </>;
}

export default Version;
