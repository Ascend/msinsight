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
import React, { useState } from 'react';
import { observer } from 'mobx-react';
import { defaultOperator, type CommunicationApi } from '../api';
import type { Scope } from '../domain/types';
import { Result, Section } from '../components/Section';
import { Selector, options } from '../components/Selector';
import { Matrix } from '../features/matrix';
import { useCommunicationStore } from '../store/context';
import { useQuery } from '../state/useQuery';
import { useTranslation } from 'react-i18next';

export const MatrixSection = observer(function MatrixSection({ scope, api }: { scope: Scope; api: CommunicationApi }): JSX.Element {
    const { dark } = useCommunicationStore();
    const { t } = useTranslation('communication');
    const key = JSON.stringify(scope);
    const matrixOperators = useQuery(`${key}:matrix-operators`, true, signal => api.operators(scope, true, signal));
    const [matrixChoice, setMatrixChoice] = useState('');
    const matrixOp = matrixOperators.data?.includes(matrixChoice) ? matrixChoice : defaultOperator(matrixOperators.data ?? []);
    const matrix = useQuery(`${key}:${matrixOp}`, !!matrixOp, signal => api.matrix(scope, matrixOp, signal));
    return (
        <Section title={t('modern.matrix')}>
            <Result query={matrixOperators}>{() => <Matrix stage={scope.group.stage} query={matrix} dark={dark}
                operatorFilter={<Selector label={t('searchCriteria.OperatorName')} value={matrixOp} options={options(matrixOperators.data ?? [])}
                    loading={matrixOperators.loading} onChange={setMatrixChoice} />} />}</Result>
        </Section>
    );
});
