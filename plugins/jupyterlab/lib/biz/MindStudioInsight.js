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
export const MindStudioInsightTab = (props) => {
    const [currentMindStudio, setCurrentMindStudio] = useState(null);
    const currentMindStudioRef = useRef(currentMindStudio);
    // currently inactive
    const [notActiveError, setNotActiveError] = useState(false);
    const [srcVal, setSrcVal] = useState('');
    const updateCurrentMindStudio = (model) => {
        props.updateCurrentModel(model);
        setCurrentMindStudio(model);
        currentMindStudioRef.current = model;
    };
    const refreshRunning = async () => {
        await props.mindstudioManager.refreshRunning();
        const runningMindStudios = [...props.mindstudioManager.running()];
        // hint: Using runningMindStudios directly may cause setState to fail to respond
        const modelList = [];
        for (const model of runningMindStudios) {
            modelList.push(model);
        }
        if (currentMindStudioRef.current) {
            if (!modelList.find(model => { var _a; return model.name === ((_a = currentMindStudioRef.current) === null || _a === void 0 ? void 0 : _a.name); })) {
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
    const getUrl = async () => {
        const val = await props.startIFrame();
        setSrcVal(val);
    };
    useEffect(() => {
        refreshRunning();
        getUrl();
    }, []);
    return (React.createElement("div", { style: { width: '100%', height: '100%' } },
        React.createElement("div", { style: { width: '100%', height: '100%' } },
            !currentMindStudio && (React.createElement("iframe", { style: { width: '100%', height: '100%' }, sandbox: "allow-scripts allow-forms allow-same-origin", referrerPolicy: "no-referrer", src: srcVal })),
            currentMindStudio && (React.createElement("div", null,
                React.createElement("p", null, "No instance for current directory yet, please create a new MindStudio Insight."))),
            notActiveError && (React.createElement("div", null,
                React.createElement("p", null, "Current Tensorboard is not active. Please select others or create a new one."))))));
};
//# sourceMappingURL=MindStudioInsight.js.map