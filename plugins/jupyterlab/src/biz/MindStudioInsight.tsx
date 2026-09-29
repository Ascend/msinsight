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

export const MindStudioInsightTab = (props: MindStudioInsightProps): JSX.Element => {
    const [currentMindStudio, setCurrentMindStudio] = useState<MindStudio.IModel | null>(null);
    const currentMindStudioRef = useRef(currentMindStudio);
    // currently inactive
    const [notActiveError, setNotActiveError] = useState(false);
    const [srcVal, setSrcVal] = useState('');

    const updateCurrentMindStudio = (model: MindStudio.IModel | null): void => {
        props.updateCurrentModel(model);
        setCurrentMindStudio(model);
        currentMindStudioRef.current = model;
    };

    const refreshRunning = async (): Promise<void> => {
        await props.mindstudioManager.refreshRunning();
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

    const getUrl = async (): Promise<void> => {
        const val = await props.startIFrame();
        setSrcVal(val);
    };

    useEffect(() => {
        refreshRunning();
        getUrl();
    }, []);

    return (
        <div style={{ width: '100%', height: '100%' }}>
            <div style={{ width: '100%', height: '100%' }}>
                { !currentMindStudio && (
                <iframe
                    style={{ width: '100%', height: '100%' }}
                    sandbox="allow-scripts allow-forms allow-same-origin allow-downloads"
                    referrerPolicy="no-referrer"
                    src={srcVal}
                />
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