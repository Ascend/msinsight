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
import React from 'react';
import * as testingLibrary from '@testing-library/react';
import { renderWithProviders } from '../../../../testing/render';
import { light } from '../../../../lib/src/theme/light';
import { coreData } from '../__mock__/data.mock';
import CoreChart from '../detail/CoreOccupancy/CoreChart';

const { screen } = testingLibrary;

function getCore(name: string): SVGSVGElement {
    const core = screen.getByText(name).closest('svg');
    if (core === null) throw new Error(`Missing core: ${name}`);
    return core;
}

it('renders the populated core with its reported levels and values', () => {
    renderWithProviders(<CoreChart condition={{ showAs: 'cycles', isCompared: false }} data={coreData} />);
    const [cube, vector] = getCore('Core15').querySelectorAll('.subCore');
    expect(cube.querySelector('text')).toHaveTextContent('Cube0');
    expect(cube?.querySelector('rect')).toHaveAttribute('fill', '#679405');
    expect(cube).toHaveTextContent('38');
    expect(vector.querySelector('text')).toHaveTextContent('Vector0');
    expect(vector?.querySelector('rect')).toHaveAttribute('fill', '#81BA06');
});

it('fills unused hardware cores with the no-data color', () => {
    renderWithProviders(<CoreChart condition={{ showAs: 'cycles', isCompared: false }} data={coreData} />);
    const cells = getCore('Core19').querySelectorAll('.subCore rect');
    expect(cells).toHaveLength(6);
    cells.forEach(cell => expect(cell).toHaveAttribute('fill', light.bgColorGrey));
});

it('includes the baseline value in comparison mode', () => {
    renderWithProviders(<CoreChart condition={{ showAs: 'cycles', isCompared: true }} data={coreData} />);
    expect(getCore('Core15').querySelector('.subCore')).toHaveTextContent('38(0)');
});
