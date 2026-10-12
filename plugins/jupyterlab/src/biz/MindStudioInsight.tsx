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

import React, { useState, useEffect, useRef } from 'react';
import { MindStudioManager } from '../manager';
import * as MindStudio from '../mindstudio';

interface MindStudioInsightProps {
    setWidgetName?: (name: string) => void;
    createdModelName?: string;
    mindstudioManager: MindStudioManager;
    closeWidget: () => void;
    openMindStudio: (modelName: string) => void;
    updateCurrentModel: (model: MindStudio.IModel | null) => void;
    startNew: (name: string, options?: MindStudio.IOptions) => Promise<MindStudio.IMindStudio>;
    startIFrame: () => Promise<string>;
}

type LoadingPhase = 'starting' | 'loading' | 'ready' | 'error';
const STARTUP_TIMEOUT_MS = 45000;
const PAGE_TIMEOUT_MS = 45000;

export const MindStudioInsightTab = (props: MindStudioInsightProps): JSX.Element => {
    const [currentMindStudio, setCurrentMindStudio] = useState<MindStudio.IModel | null>(null);
    const currentMindStudioRef = useRef(currentMindStudio);
    // currently inactive
    const [notActiveError, setNotActiveError] = useState(false);
    const [srcVal, setSrcVal] = useState('');
    const [phase, setPhase] = useState<LoadingPhase>('starting');
    const [errorMessage, setErrorMessage] = useState('');
    const iframeRef = useRef<HTMLIFrameElement>(null);
    const isLoading = phase === 'starting' || phase === 'loading';

    const updateCurrentMindStudio = (model: MindStudio.IModel | null): void => {
        props.updateCurrentModel(model);
        setCurrentMindStudio(model);
        currentMindStudioRef.current = model;
    };

    const refreshRunning = async (isActive: () => boolean): Promise<void> => {
        await props.mindstudioManager.refreshRunning();
        if (!isActive()) {
            return;
        }
        const runningMindStudios = [...props.mindstudioManager.running()];

        // hint: Using runningMindStudios directly may cause setState to fail to respond
        const modelList = [];
        for (const model of runningMindStudios) {
            modelList.push(model);
        }

        if (currentMindStudioRef.current) {
            if (!modelList.find(model => model.name === currentMindStudioRef.current?.name)) {
                setNotActiveError(true);
            }
        }

        const model = props.createdModelName
            ? modelList.find(modelItem => modelItem.name === props.createdModelName)
            : null;
        if (model) {
            updateCurrentMindStudio(model);
            if (props.setWidgetName) {
                props.setWidgetName(model.name);
            }
        }
    };

    useEffect(() => {
        let active = true;
        let settled = false;
        let expectedOrigin = '';
        let expectedServerId = '';
        let timer: number;

        const fail = (message: string): void => {
            if (!active || settled) {
                return;
            }
            settled = true;
            window.clearTimeout(timer);
            setErrorMessage(message);
            setPhase('error');
        };

        const onMessage = (event: MessageEvent): void => {
            if (
                !active ||
                settled ||
                !expectedServerId ||
                event.source !== iframeRef.current?.contentWindow ||
                event.origin !== expectedOrigin
            ) {
                return;
            }
            const data = event.data;
            if (
                data === null ||
                typeof data !== 'object' ||
                data.type !== 'mindstudio:startup' ||
                data.profilerServerId !== expectedServerId
            ) {
                return;
            }
            if (data.status === 'ready') {
                settled = true;
                window.clearTimeout(timer);
                setPhase('ready');
            } else if (data.status === 'error') {
                fail(
                    'Could not connect to the analysis service. Close this tab and try again.'
                );
            }
        };

        // Register before mounting the iframe so an early ready message cannot be lost.
        window.addEventListener('message', onMessage);
        timer = window.setTimeout(() => {
            fail(
                'Starting the analysis service timed out. Close this tab and try again.'
            );
        }, STARTUP_TIMEOUT_MS);

        const openIframe = async (): Promise<void> => {
            try {
                const url = await props.startIFrame();
                if (!active || settled) {
                    return;
                }
                const parsed = new URL(url, window.location.href);
                const serverId = parsed.searchParams.get('profilerServerId');
                if (parsed.origin !== window.location.origin || !serverId) {
                    throw new Error(
                        'Invalid MindStudio Insight startup configuration.'
                    );
                }
                expectedOrigin = parsed.origin;
                expectedServerId = serverId;
                window.clearTimeout(timer);
                timer = window.setTimeout(() => {
                    fail(
                        'The interface did not become ready in time. Close this tab and try again.'
                    );
                }, PAGE_TIMEOUT_MS);
                setPhase('loading');
                setSrcVal(url);
            } catch (error) {
                const detail =
                    error instanceof Error
                        ? error.message
                        : 'Please close this tab and try again.';
                fail(`Unable to start MindStudio Insight. ${detail}`);
            }
        };

        void openIframe();
        void refreshRunning(() => active).catch(error => {
            if (active) {
                console.warn(
                    'Unable to refresh MindStudio Insight sessions.',
                    error
                );
            }
        });
        return () => {
            active = false;
            window.clearTimeout(timer);
            window.removeEventListener('message', onMessage);
        };
    }, []);

    return (
        <div
            className="mindstudio-insight-tab"
            aria-busy={!currentMindStudio && isLoading}
        >
            <div style={{ width: '100%', height: '100%' }}>
                { !currentMindStudio && srcVal && (
                <iframe
                    ref={iframeRef}
                    title="MindStudio Insight"
                    tabIndex={phase === 'ready' ? 0 : -1}
                    aria-hidden={phase !== 'ready'}
                    style={{ width: '100%', height: '100%', border: 0 }}
                    sandbox="allow-scripts allow-forms allow-same-origin"
                    referrerPolicy="no-referrer"
                    src={srcVal}
                />
                ) }
                { !currentMindStudio && isLoading && (
                <div
                    className="mindstudio-insight-overlay"
                    role="status"
                    aria-live="polite"
                >
                    <span
                        className="mindstudio-insight-spinner"
                        aria-hidden="true"
                    />
                    <span className="mindstudio-insight-title">
                        MindStudio Insight
                    </span>
                    <span className="mindstudio-insight-message">
                        {phase === 'starting'
                            ? 'Starting the analysis service…'
                            : 'Loading the interface and connecting…'}
                    </span>
                </div>
                ) }
                { !currentMindStudio && phase === 'error' && (
                <div className="mindstudio-insight-overlay" role="alert">
                    <span className="mindstudio-insight-title">
                        Unable to open MindStudio Insight
                    </span>
                    <p className="mindstudio-insight-message">
                        {errorMessage}
                    </p>
                    <button
                        className="mindstudio-insight-close"
                        type="button"
                        onClick={props.closeWidget}
                    >
                        Close tab
                    </button>
                </div>
                ) }
                { currentMindStudio && (
                <div>
                    <p>
                        No instance for current directory yet, please create a new MindStudio Insight.
                    </p>
                </div>
                ) }
                { notActiveError && (
                <div>
                    <p>
                    Current Tensorboard is not active. Please select others or create a new one.
                    </p>
                </div>
                ) }
            </div>
        </div>
    );
};
