/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { CommandDefinition } from '@insight/lib/FrontendAgentCommand';
import { CommandCatalog } from './CommandCatalog';

describe('CommandCatalog', () => {
    test('lists built-ins, globals, and only the active Module snapshot', () => {
        const catalog = new CommandCatalog();
        catalog.registerGlobal(definition('framework.openModule'), jest.fn());
        catalog.replaceModule('MemScope', [definition('MemScope.table.refresh')]);
        catalog.replaceModule('Timeline', [definition('Timeline.operator.locate')]);

        expect(catalog.listVisible('MemScope').map(({ name }) => name)).toEqual([
            'help',
            'observe',
            'framework.openModule',
            'MemScope.table.refresh',
        ]);

        catalog.replaceModule('MemScope', []);
        expect(catalog.getVisible('MemScope.table.refresh', 'MemScope')).toBeUndefined();
        expect(catalog.getVisible('Timeline.operator.locate', 'Timeline')).toBeDefined();
    });

    test('rejects commands outside the owner namespace', () => {
        const catalog = new CommandCatalog();

        expect(() => catalog.registerGlobal(definition('MemScope.global'), jest.fn()))
            .toThrow("must use the 'framework.' namespace");
        expect(() => catalog.replaceModule('MemScope', [definition('Timeline.table.refresh')]))
            .toThrow("must use the 'MemScope.' namespace");
    });
});

const definition = (name: string): CommandDefinition => ({
    name,
    title: name,
    description: name,
    inputSchema: { type: 'object', properties: {}, additionalProperties: false },
});
