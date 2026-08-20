/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { Painter as NativePainter } from '../../../leaksWorker/blockWorker/nativeCanvas/Painter';
import { Painter as WebGLPainter } from '../../../leaksWorker/blockWorker/webgl/Painter';

jest.mock('@/leaksWorker/tools/color', () => ({
    getColorStringByAddr: jest.fn(() => '#000'),
    getDimmedColorStringByAddr: jest.fn(() => '#999'),
}), { virtual: true });
jest.mock('../../../leaksWorker/tools/BlockDataOPFS', () => ({
    BlockDataOPFS: class { readonly mocked = true; },
    getPointFromPathData: jest.fn(),
}));
jest.mock('../../../leaksWorker/blockWorker/webgl/programs/MemoryBlockProgram', () => ({
    MemoryBlockProgram: class { readonly mocked = true; },
}));
jest.mock('../../../leaksWorker/blockWorker/webgl/programs/MemoryBlockBorderProgram', () => ({
    MemoryBlockBorderProgram: class { readonly mocked = true; },
}));
jest.mock('../../../leaksWorker/blockWorker/webgl/programs/ReservedLineProgram', () => ({
    ReservedLineProgram: class { readonly mocked = true; },
}));

const OPTIONS: RenderOptions = {
    transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
    viewport: { width: 320, height: 180 },
    zoom: { x: 1, y: 1, offset: 0 },
};

describe('lifecycle graph layer rendering', () => {
    it('renders native block and overview layers independently', async () => {
        const painter = new NativePainter({} as HTMLCanvasElement, 1);
        (painter as any).context = {
            resetTransform: jest.fn(),
            clearRect: jest.fn(),
            translate: jest.fn(),
            scale: jest.fn(),
            save: jest.fn(),
            restore: jest.fn(),
        };
        const renderData = jest.spyOn(painter as any, 'renderData').mockImplementation(() => undefined);
        const renderOverview = jest.spyOn(painter, 'renderReservedLine').mockImplementation(() => undefined);

        await painter.render(OPTIONS, () => false, { blocks: false, overview: true });
        expect(renderData).not.toHaveBeenCalled();
        expect(renderOverview).toHaveBeenCalledTimes(1);

        renderData.mockClear();
        renderOverview.mockClear();
        await painter.render(OPTIONS, () => false, { blocks: true, overview: false });
        expect(renderData).toHaveBeenCalledTimes(3);
        expect(renderOverview).not.toHaveBeenCalled();
    });

    it('renders WebGL programs only for visible layers', () => {
        const painter = new WebGLPainter({} as OffscreenCanvas, 'test-runtime');
        (painter as any).gl = {
            BLEND: 1,
            SRC_ALPHA: 2,
            ONE_MINUS_SRC_ALPHA: 3,
            ONE: 4,
            enable: jest.fn(),
            disable: jest.fn(),
            blendFuncSeparate: jest.fn(),
        };
        jest.spyOn(painter, 'clear').mockImplementation(() => undefined);
        const blocks = { render: jest.fn() };
        const highlighted = { render: jest.fn() };
        const outlined = { render: jest.fn() };
        const overview = { render: jest.fn() };
        painter.memoryBlockProgram = blocks as any;
        painter.memoryBlockHighlightProgram = highlighted as any;
        painter.memoryBlockBorderHightlightProgram = outlined as any;
        painter.reservedLineProgram = overview as any;

        painter.render(OPTIONS, { blocks: false, overview: true });
        expect(blocks.render).not.toHaveBeenCalled();
        expect(highlighted.render).not.toHaveBeenCalled();
        expect(outlined.render).not.toHaveBeenCalled();
        expect(overview.render).toHaveBeenCalledTimes(1);

        overview.render.mockClear();
        painter.render(OPTIONS, { blocks: true, overview: false });
        expect(blocks.render).toHaveBeenCalledTimes(1);
        expect(highlighted.render).toHaveBeenCalledTimes(1);
        expect(outlined.render).toHaveBeenCalledTimes(1);
        expect(overview.render).not.toHaveBeenCalled();
    });
});
