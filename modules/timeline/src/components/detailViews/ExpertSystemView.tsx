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

import { observer } from 'mobx-react';
import React from 'react';
import {
    queryOneKernel,
    queryAffinityOptimizer,
    affinityOptimizerColumns,
    queryAICPUOperators,
    queryACLNNOperators,
    aicpuOperatorColumns,
    aclnnOperatorColumns,
    queryAffinityAPI,
    affinityAPIColumns,
    queryOperatorFusion,
    fusionOperatorColumns,
    operatorDispatchColumns,
} from './Common';
import { BaseSummary, type BaseSummaryProps, type SelectContentViewProps } from './SystemView';
import { ExpertSummary } from './ExpertSummary';
import { queryExpertAnalysis, queryOperatorDispatch } from '../../api/request';
import type { BaseSummaryRowItemType } from '../../api/interface';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';
import type { CardMetaData } from '../../entity/data';
import { ProjectType } from '../../entity/insight';
import { StyledEmpty } from '@insight/lib';

const shouldHiddenContent = (props: SelectContentViewProps): boolean => {
    const targetInfo = props.session.units.find(unitItem => (unitItem.metadata as CardMetaData)?.cardId === props.card.cardId);
    return targetInfo === undefined || targetInfo.projectType === undefined || targetInfo.projectType === ProjectType.IE;
};

const HiddenContent = (): JSX.Element => {
    return <div style={{ display: 'flex', height: '100%' }}>
        <StyledEmpty style={{ margin: 'auto' }}/>
    </div>;
};

const ExpertAnalysis = observer((props: SelectContentViewProps) => {
    if (shouldHiddenContent(props)) {
        return <HiddenContent />;
    }
    return <ExpertSummary request={queryExpertAnalysis} {...props} />;
});

const AffinityAPI = observer((props: SelectContentViewProps) => {
    if (shouldHiddenContent(props)) {
        return <HiddenContent />;
    }
    return <BaseSummary request={queryAffinityAPI} columns={affinityAPIColumns} {...props} />;
});

const AffinityOptimizer = observer((props: SelectContentViewProps) => {
    if (shouldHiddenContent(props)) {
        return <HiddenContent />;
    }
    return <BaseSummary request={queryAffinityOptimizer} columns={affinityOptimizerColumns} {...props} />;
});

const AICPUOperator = observer((props: SelectContentViewProps) => {
    if (shouldHiddenContent(props)) {
        return <HiddenContent />;
    }
    return <BaseSummary request={queryAICPUOperators} columns={aicpuOperatorColumns} {...props} />;
});

const ACLNNOperator = observer((props: SelectContentViewProps) => {
    if (shouldHiddenContent(props)) {
        return <HiddenContent />;
    }
    return <BaseSummary request={queryACLNNOperators} columns={aclnnOperatorColumns} {...props} />;
});

const FusedOperator = observer((props: SelectContentViewProps) => {
    if (shouldHiddenContent(props)) {
        return <HiddenContent />;
    }
    return <BaseSummary request={queryOperatorFusion} columns={fusionOperatorColumns} {...props} />;
});

const OperatorDispatch = observer((props: SelectContentViewProps) => {
    if (shouldHiddenContent(props)) {
        return <HiddenContent />;
    }
    return <BaseSummary request={queryOperatorDispatch} columns={operatorDispatchColumns} {...props} />;
});

export const handleAdvisorSelected = async (rowData: BaseSummaryRowItemType, props: BaseSummaryProps): Promise<void> => {
    const queryName = rowData.name ?? rowData.originOptimizer ?? '';
    const res = await queryOneKernel({
        rankId: rowData.rankId,
        dbPath: rowData.dbPath,
        name: queryName,
        timestamp: rowData.startTime,
        duration: rowData.duration,
    });
    const depth = rowData.depth > res.depth ? rowData.depth : res.depth;

    jumpToUnitOperator({
        ...rowData,
        id: props.session.isFullDb as boolean ? rowData.id : res.id,
        cardId: rowData.rankId,
        dbPath: rowData.dbPath,
        timestamp: rowData.startTime,
        depth,
        duration: rowData.duration,
        name: queryName,
        tid: res.threadId,
        pid: res.pid,
        metaType: res.metaType ?? '',
    });
};

export const ExpertSystemView = [ExpertAnalysis, AffinityAPI, AffinityOptimizer, AICPUOperator, ACLNNOperator, FusedOperator, OperatorDispatch];
