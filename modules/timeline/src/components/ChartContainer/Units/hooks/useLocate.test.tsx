/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2
 */
import { act, renderHook } from '@testing-library/react';
import { observable, runInAction } from 'mobx';
import type { Session } from '../../../../entity/session';
import type { InsightUnit, UnitMatcher } from '../../../../entity/insight';
import { useJumpTarget, type OrderOptions } from './useLocate';

jest.mock('antd', () => ({ message: { warn: jest.fn() } }));
jest.mock('../../../../utils', () => ({ getRootUnit: (units: unknown) => units }));

const order: OrderOptions = { preOrderFlatten: () => [], options: {} };

test.each([false, true])('unmatched or hidden navigation reports failure once (hidden=%s)', hidden => {
    const onFailure = jest.fn();
    const onSuccess = jest.fn();
    const unit = { isUnitVisible: !hidden, isMerged: false } as InsightUnit;
    const session = observable({
        units: [unit],
        selectedUnits: [],
        locateUnit: undefined as UnitMatcher | undefined,
    }) as unknown as Session;
    const dom = document.createElement('div');
    const { rerender, unmount } = renderHook(() => useJumpTarget(session, session.units, true, order, dom));
    act(() => runInAction(() => {
        session.locateUnit = { target: () => hidden, onSuccess, onFailure };
    }));
    expect(onFailure).toHaveBeenCalledTimes(1);
    expect(onSuccess).not.toHaveBeenCalled();
    expect(session.locateUnit).toBeUndefined();
    rerender();
    expect(onFailure).toHaveBeenCalledTimes(1);
    unmount();
});
