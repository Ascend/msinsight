/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import guideOne from '../icons/guide-1.svg';
import guideTwo from '../icons/guide-2.svg';
import guideThree from '../icons/guide-3.svg';
import logo from '../icons/logo.png';
import welcomeBackground from '../icons/welcome-bg.png';

const Container = styled.section`
    min-height: 0;
    height: 100%;
    overflow-x: hidden;
    overflow-y: auto;
    display: flex;
    justify-content: center;
    padding: 24px;

    .welcome-content {
        position: relative;
        width: min(100%, 410px);
        display: flex;
        flex-direction: column;
        align-items: center;
        margin: auto 0;
    }

    .welcome-visual {
        position: absolute;
        z-index: 0;
        top: -80px;
        left: 50%;
        width: 361px;
        height: 388px;
        transform: translateX(-50%);
        background: center / 100% 100% no-repeat url(${welcomeBackground});
        pointer-events: none;
    }

    .welcome-logo,
    .welcome-title,
    .guide-list {
        position: relative;
        z-index: 1;
    }

    .welcome-logo {
        width: 68px;
        height: 68px;
        object-fit: contain;
        animation: welcome-rise 0.35s ease-out both;
    }

    .welcome-title {
        margin: 22px 0 22px;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 24px;
        font-weight: 600;
        line-height: 1.35;
        letter-spacing: 0.01em;
        text-align: center;
        animation: welcome-rise 0.4s 0.04s ease-out both;
    }

    .guide-list {
        width: 100%;
        display: grid;
        gap: 16px;
    }

    .guide-card {
        min-height: 86px;
        display: grid;
        grid-template-columns: 48px minmax(0, 1fr);
        align-items: center;
        gap: 14px;
        padding: 16px 20px;
        border: 1px solid ${(props): string => props.theme.agentWelcomeCardBackgroundColor};
        border-radius: 18px;
        background: ${(props): string => props.theme.agentWelcomeCardBackgroundColor};
        box-shadow: 0 1px 2px rgba(18, 25, 38, 0.02);
        animation: welcome-rise 0.42s ease-out both;
    }

    .guide-card:nth-of-type(2) { animation-delay: 0.08s; }
    .guide-card:nth-of-type(3) { animation-delay: 0.12s; }

    .guide-icon {
        width: 44px;
        height: 44px;
        object-fit: contain;
    }

    .guide-copy {
        min-width: 0;
        display: grid;
        gap: 6px;
    }

    .guide-title {
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
        font-weight: 600;
        line-height: 1.35;
    }

    .guide-description {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        line-height: 1.45;
    }

    .guide-tag {
        display: inline-flex;
        align-items: center;
        margin-right: 8px;
        padding: 1px 6px;
        border: 1px solid ${(props): string => props.theme.primaryColorLight4};
        border-radius: 4px;
        background: ${(props): string => props.theme.primaryColorLight5};
        color: ${(props): string => props.theme.textColorPrimary};
        font-weight: 400;
        white-space: nowrap;
    }

    .guide-tag-mark {
        margin-right: 3px;
        color: ${(props): string => props.theme.primaryColor};
    }

    @keyframes welcome-rise {
        from { opacity: 0; transform: translateY(8px); }
        to { opacity: 1; transform: translateY(0); }
    }

    @media (max-height: 760px) {
        .welcome-title { margin: 14px 0 16px; }
        .guide-list { gap: 10px; }
        .guide-card { min-height: 72px; padding: 11px 16px; }
    }
`;

const guides = [
    { icon: guideOne, title: 'memoryGuideTitle', tag: 'memoryGuideTag', description: 'memoryGuideDescription' },
    { icon: guideTwo, title: 'slowGuideTitle', tag: 'slowGuideTag', description: 'slowGuideDescription' },
    { icon: guideThree, title: 'ragGuideTitle', description: 'ragGuideDescription' },
] as const;

export const WelcomePanel = (): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');

    return <Container>
        <div className="welcome-content">
            <div className="welcome-visual" />
            <img className="welcome-logo" src={logo} alt="" />
            <h1 className="welcome-title">{t('welcomeTitle')}</h1>
            <div className="guide-list">
                {guides.map((guide) => <article className="guide-card" key={guide.title}>
                    <img className="guide-icon" src={guide.icon} alt="" />
                    <div className="guide-copy">
                        <div className="guide-title">{t(guide.title)}</div>
                        <div className="guide-description">
                            {'tag' in guide ? <span className="guide-tag"><span className="guide-tag-mark">#</span>{t(guide.tag)}</span> : null}
                            {t(guide.description)}
                        </div>
                    </div>
                </article>)}
            </div>
        </div>
    </Container>;
};
