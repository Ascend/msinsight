/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { parseActionMarkup, type ActionMarkupSegment } from '../../components/actionMarkup';

const actionXml = (blockId = 123): string => `<insight-action>
{
  "label": "Block #${blockId}",
  "description": "Highlight block #${blockId}.",
  "command": "MemScope.lifecycleGraph.selectBlock",
  "args": { "blockId": ${blockId} }
}
</insight-action>`;

test('parses interleaved markdown and one action per XML block', () => {
    const segments = parseActionMarkup(`Before\n${actionXml(1)}\nMiddle\n${actionXml(2)}\nAfter`, { keyPrefix: 'message-1' });

    expect(segments.map(segment => segment.type)).toEqual(['markdown', 'action', 'markdown', 'action', 'markdown']);
    const actions = segments.filter((segment): segment is Extract<ActionMarkupSegment, { type: 'action' }> => segment.type === 'action');
    expect(actions.map(segment => segment.action.args.blockId)).toEqual([1, 2]);
    expect(actions.map(segment => segment.key)).toEqual([
        expect.stringMatching(/^message-1:action:/),
        expect.stringMatching(/^message-1:action:/),
    ]);
});

test('parses multiple single-line actions separated only by spaces', () => {
    const source = '<insight-action> { "label": "定位泄露块 #1 (40MB)", "description": "定位并高亮泄露块 #1。", "command": "MemScope.lifecycleGraph.selectBlock", "args": { "blockId": 1 } } </insight-action> <insight-action> { "label": "定位泄露块 #2932 (40MB)", "description": "定位并高亮泄露块 #2932。", "command": "MemScope.lifecycleGraph.selectBlock", "args": { "blockId": 2932 } } </insight-action> <insight-action> { "label": "定位泄露块 #5515 (40MB)", "description": "定位并高亮泄露块 #5515。", "command": "MemScope.lifecycleGraph.selectBlock", "args": { "blockId": 5515 } } </insight-action>';
    const segments = parseActionMarkup(source);
    const actions = segments.filter((segment): segment is Extract<ActionMarkupSegment, { type: 'action' }> => segment.type === 'action');

    expect(actions.map(segment => segment.action.args.blockId)).toEqual([1, 2932, 5515]);
});

test('hides an unclosed candidate while streaming and restores it as text after completion', () => {
    const source = 'Before\n<insight-action>\n{"label":"Block';

    expect(parseActionMarkup(source, { streaming: true })).toEqual([{ key: 'content:markdown:0', type: 'markdown', text: 'Before\n' }]);
    expect(parseActionMarkup(source, { streaming: false })).toEqual([
        { key: 'content:markdown:0', type: 'markdown', text: 'Before\n' },
        { key: 'content:literal:7', type: 'literal', text: '<insight-action>\n{"label":"Block' },
    ]);
});

test('hides a trailing partial opening tag while streaming', () => {
    expect(parseActionMarkup('Before\n<insight-act', { streaming: true })).toEqual([
        { key: 'content:markdown:0', type: 'markdown', text: 'Before\n' },
    ]);
    expect(parseActionMarkup('Before\n<insight-act', { streaming: false })).toEqual([
        { key: 'content:markdown:0', type: 'markdown', text: 'Before\n<insight-act' },
    ]);
});

test('keeps closed invalid payloads as ordinary text', () => {
    const invalid = '<insight-action>{"label":"Missing fields"}</insight-action>';
    const segments = parseActionMarkup(`Before${invalid}After`);

    expect(segments).toEqual([
        { key: 'content:markdown:0', type: 'markdown', text: 'Before' },
        { key: 'content:literal:6', type: 'literal', text: invalid },
        { key: `content:markdown:${6 + invalid.length}`, type: 'markdown', text: 'After' },
    ]);
});

test('does not parse action tags in fenced or inline code', () => {
    const fenced = `\`\`\`xml\n${actionXml(1)}\n\`\`\``;
    const inline = `Example: \`${actionXml(2)}\``;
    const segments = parseActionMarkup(`${fenced}\n${inline}`);

    expect(segments).toHaveLength(1);
    expect(segments[0]).toMatchObject({ type: 'markdown', text: expect.stringContaining('<insight-action>') });
});

test('does not parse action tags in quoted or list-nested fenced code', () => {
    const quoted = `> \`\`\`xml\n> ${actionXml(3)}\n> \`\`\``;
    const listed = `- \`\`\`xml\n  ${actionXml(4)}\n  \`\`\``;
    const segments = parseActionMarkup(`${quoted}\n${listed}`);

    expect(segments).toHaveLength(1);
    expect(segments[0]).toMatchObject({ type: 'markdown', text: expect.stringContaining('<insight-action>') });
});

test('does not hide a partial opening tag inside a streaming code span', () => {
    const source = 'Example: `<insight-act`';
    expect(parseActionMarkup(source, { streaming: true })).toEqual([
        { key: 'content:markdown:0', type: 'markdown', text: source },
    ]);
});

test('keeps an orphan closing tag as literal text', () => {
    const source = `Before</insight-action>After`;
    expect(parseActionMarkup(source)).toEqual([
        { key: 'content:markdown:0', type: 'markdown', text: 'Before' },
        { key: 'content:literal:6', type: 'literal', text: '</insight-action>' },
        { key: 'content:markdown:23', type: 'markdown', text: 'After' },
    ]);
});

test('does not combine nested action tags into one action', () => {
    const source = `<insight-action>{"label":"outer"}${actionXml(2)}</insight-action>`;
    const segments = parseActionMarkup(source);
    const actions = segments.filter((segment): segment is Extract<ActionMarkupSegment, { type: 'action' }> => segment.type === 'action');

    expect(actions).toHaveLength(1);
    expect(actions[0].action.args.blockId).toBe(2);
});

test('rejects arrays, unknown fields, non-JSON args, and oversized labels', () => {
    const invalidPayloads = [
        [{ label: 'A', description: 'B', command: 'observe', args: {} }],
        { label: 'A', description: 'B', command: 'observe', args: {}, extra: true },
        { label: 'A', description: 'B', command: 'observe', args: [] },
        { label: 'A'.repeat(201), description: 'B', command: 'observe', args: {} },
        { label: 'A', description: 'B', command: 'observe', args: { values: Array.from({ length: 2001 }, (_, index) => index) } },
    ];

    for (const payload of invalidPayloads) {
        const source = `<insight-action>${JSON.stringify(payload)}</insight-action>`;
        expect(parseActionMarkup(source)).toEqual([{ key: 'content:literal:0', type: 'literal', text: source }]);
    }
});
