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
import { ClusterNavigation } from './navigation';
it('delivers only the latest navigation once the page is ready', () => {
    const navigation = new ClusterNavigation(); const intent = { context: 'p/c', event: 'locateCommunication', body: { operatorName: 'last' } };
    navigation.defer('Communication', { ...intent, body: { operatorName: 'old' } });
    navigation.begin('Communication'); navigation.defer('Communication', intent);
    expect(navigation.mounted('Communication', 'p/c')).toEqual(intent);
    expect(navigation.defer('Communication', intent)).toBe(false);
    expect(navigation.mounted('Communication', 'p/c')).toBeUndefined();
});
it('drops navigation when its context or version is replaced', () => {
    const navigation = new ClusterNavigation(); const intent = { context: 'p/c', event: 'locateCommunication', body: {} };
    navigation.defer('Communication', intent); expect(navigation.mounted('Communication', 'other')).toBeUndefined();
    navigation.begin('Communication'); navigation.defer('Communication', intent); navigation.reset();
    expect(navigation.mounted('Communication', 'p/c')).toBeUndefined();
});
