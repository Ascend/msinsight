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
import _ from 'lodash';
import { autorun, runInAction } from 'mobx';
import * as React from 'react';
import type { DetailDescriptor, InsightUnit, SingleDataDesc } from '../../entity/insight';
import type { SelectedParams, Session } from '../../entity/session';
import type { TabState } from '../../entity/tabDependency';
import { useTranslation } from 'react-i18next';
import { platform } from '../../platforms';
import { logger } from '../../utils/Logger';
import { EMPTY_TABLE_STATE, type TableState } from './types';
import { onExpandForChildren, parseColDef, treeAttachInfo } from './utils';
import i18n from '@insight/lib/i18n';
import { getSelectedDataUnit } from '../../utils/selectionContext';

interface HandleFetchDataParams {
    result: Array<Record<string, unknown>>;
    session: Session;
    recentUnits: React.MutableRefObject<InsightUnit[]>;
    recentRange: React.MutableRefObject<[number, number] | undefined>;
    tabState?: TabState;
    selectedUnit?: InsightUnit;
    detail: DetailDescriptor<unknown>;
    setState: React.Dispatch<React.SetStateAction<TableState>>;
    onDataLoaded?: (data: unknown[]) => void;
}

const handleFetchedData = ({
    result, session, recentUnits, recentRange, tabState, selectedUnit, detail, setState, onDataLoaded,
}: HandleFetchDataParams): void => {
    runInAction(() => {
        session.selectedMultiSlice = '';
    }); // 更新表格后，More表格需要删除数据
    const { selectedUnits, selectedRange } = session;
    if (recentUnits.current !== selectedUnits || recentRange.current !== selectedRange) { return; }
    result.forEach(treeAttachInfo);
    if (tabState?.data !== undefined && tabState?.filter !== undefined) {
        runInAction(() => { tabState.data = result; });
    } else {
        const columns = parseColDef(detail, session, tabState ?? selectedUnit?.tabState).map(col => ({
            ...col,
            title: i18n.t(`sliceList.${col.title}`, { ns: 'timeline', defaultValue: col.title }),
        }));
        setState({
            dataSource: result,
            columns,
            rowKey: (detail.rowKey ?? undefined) as (row: object) => string,
            onExpand: onExpandForChildren(session, detail.onExpand, setState),
            loading: false,
        });
        onDataLoaded?.(result);
    }
};

export const useDetailUpdater = (session: Session, detail: DetailDescriptor<unknown> | undefined, tabState: TabState | undefined,
    dep: unknown = [], onDataLoaded?: (data: unknown[]) => void): TableState => {
    const [state, setState] = React.useState<TableState>(EMPTY_TABLE_STATE);
    const { selectedUnitKeys, selectedUnits, selectedRange } = session;
    const selectedUnit = selectedUnits.length > 0 ? selectedUnits[0] : undefined;
    const fetchData = session.phase === 'error' ? undefined : detail?.fetchData;
    const trigger = tabState?.trigger !== undefined ? Object.values(tabState.trigger) : [];
    const depsList = Array.isArray(dep) ? dep : [dep];

    const onDataFetched = React.useMemo(() => ([selectedUnits, selectedRange].filter(_.isEmpty).length === 0
        ? fetchData?.(session, selectedUnit?.metadata)
        : undefined), [selectedUnitKeys, selectedRange, detail]);

    const recentUnits = React.useRef(selectedUnits);
    const recentRange = React.useRef(selectedRange);
    recentUnits.current = selectedUnits;
    recentRange.current = selectedRange;

    const loadData = (): void => {
        if (detail && onDataFetched && selectedRange !== undefined) {
            logger('DetailPanel', `[DetailPanel] calling ${selectedUnit?.name ?? ''}'s fetchData`);
            setState({ ...EMPTY_TABLE_STATE, loading: true });
            onDataFetched?.then(result => {
                handleFetchedData({ result, session, recentUnits, recentRange, tabState, selectedUnit, detail, setState, onDataLoaded });
            }).catch(() => {
                setState(EMPTY_TABLE_STATE);
                platform.dialog(i18n.t('error:4004'));
            });
        } else {
            setState(EMPTY_TABLE_STATE);
        }
    };

    React.useEffect(() => {
        if (session.selectedData?.showSelectedData !== true) {
            loadData();
        }
    }, [selectedUnitKeys, selectedRange, detail, ...trigger, ...depsList, session.language]);
    // 需要进行多选过滤的才会执行下面的代码
    React.useEffect(() =>
        autorun(() => {
            if (detail === undefined || tabState?.filter === undefined) { return; }
            setState({
                dataSource: tabState.getFilterData(),
                columns: parseColDef(detail, session, tabState),
                rowKey: (detail.rowKey ?? undefined) as (row: object) => string,
                onExpand: onExpandForChildren(session, detail.onExpand, setState),
                loading: false,
            });
        }), []);
    return state;
};

export const useSelectedParamsDetailUpdater = (session: Session, detail: DetailDescriptor<unknown> | undefined):
TableState => {
    const [state, setState] = React.useState<TableState>(EMPTY_TABLE_STATE);
    const { selectedUnits } = session;
    const dependenciesKeys = Object.keys(session.selectedParams) as unknown as Array<keyof SelectedParams>;
    const hasDependencies = _.isEmpty(dependenciesKeys.filter((dependenciesKey) => session.selectedParams[dependenciesKey] === undefined));
    const dependencies = dependenciesKeys.map((dependencyKey) => session.selectedParams[dependencyKey]);
    const { t } = useTranslation();
    const loadData = (): void => {
        const selectedUnit = selectedUnits.length > 0 ? selectedUnits[0] : undefined;
        const fetchData = detail?.fetchData;
        if (fetchData && hasDependencies && session.phase === 'download') {
            logger('DetailPanel', `[DetailPanel] calling ${selectedUnit?.name ?? ''}'s fetchData`);
            platform.trace('useComparison', {});
            setState({ ...EMPTY_TABLE_STATE, loading: true });
            fetchData(session, selectedUnit?.metadata).then(result => {
                result.forEach(treeAttachInfo);
                setState({
                    dataSource: result,
                    columns: parseColDef(detail, session),
                    rowKey: (detail.rowKey ?? undefined) as (row: object) => string,
                    onExpand: onExpandForChildren(session, detail.onExpand, setState),
                    loading: false,
                });
            }).catch(() => {
                setState(EMPTY_TABLE_STATE);
                platform.dialog(t('error:4004'));
            });
        } else {
            setState(EMPTY_TABLE_STATE);
        }
    };
    React.useEffect(loadData, [...dependencies, detail]);
    return state;
};

export const useExtraDataUpdater = <T extends DetailDescriptor<unknown>>(
    session: Session, detail: T | undefined,
): Record<string, unknown> => {
    const [state, setState] = React.useState({});
    const { selectedUnits, endTimeAll } = session;
    const selectedUnit = selectedUnits.length > 0 ? selectedUnits[0] : undefined;
    const loadData = (): void => {
        const fetchExtraData = detail?.fetchExtraData;
        if (detail && fetchExtraData && session.phase === 'download') {
            logger('DetailPanel', `[DetailPanel] calling ${selectedUnit?.name ?? ''}'s fetchData`);
            fetchExtraData(session, selectedUnit?.metadata).then(result => {
                if (result !== undefined) {
                    setState({ result });
                }
            }).catch(() => {
                setState({});
                logger('useExtraDataUpdater', 'fetchExtraData occurred an exception.');
            });
        } else {
            setState({});
        }
    };
    React.useEffect(loadData, [endTimeAll, detail]);
    return { ...state };
};

export const useSelectedDataDetailUpdater = (session: Session, detail: SingleDataDesc<Record<string, unknown>, unknown>, selectedData: unknown):
{ renderFields?: Array<[string, string | JSX.Element]>; data: Record<string, unknown> } => {
    const [renderFields, setRenderFields] = React.useState<Array<[string, string | JSX.Element]>>();
    const [state, setState] = React.useState<Record<string, unknown>>({});
    const selectedUnit = getSelectedDataUnit(session);
    const fetchData = session.phase === 'error' ? undefined : detail?.fetchData;
    const onDataFetched = React.useMemo(() => (selectedUnit !== undefined && !_.isEmpty(selectedData)
        ? fetchData?.(session, selectedUnit?.metadata)
        : undefined), [selectedUnit, selectedData, detail]);

    const recentUnit = React.useRef(selectedUnit);
    const recentData = React.useRef(selectedData);
    const { t } = useTranslation();
    recentUnit.current = selectedUnit; recentData.current = selectedData;

    const loadData = (): void => {
        if (onDataFetched !== undefined && selectedData !== undefined && session.phase === 'download') {
            logger('DetailPanel', `[DetailPanel] calling ${selectedUnit?.name ?? ''}'s fetchData`);
            onDataFetched?.then(result => {
                if (recentUnit.current !== selectedUnit || recentData.current !== selectedData) { return; }
                setState(result);
                const renderField: Array<[string, string | JSX.Element]> = [];
                detail.renderFields.forEach(item => {
                    const render = item[1];
                    if (item[2] !== undefined) {
                        const isHiden = item[2];
                        if (!isHiden(result, session)) {
                            renderField.push([item[0], render(result, session)]);
                        }
                    } else {
                        renderField.push([item[0], render(result, session)]);
                    }
                });
                setRenderFields(renderField);
            }).catch(() => {
                platform.dialog(t('error:4004'));
            });
        }
    };

    React.useEffect(loadData, [selectedUnit, selectedData, detail]);
    return { renderFields, data: state };
};
