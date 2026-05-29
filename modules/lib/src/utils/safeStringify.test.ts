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

import { safeStringify } from './safeStringify';

describe('safeStringify - 黑盒行为验证', () => {
  // ✅ 修复：使用 ownKeys 陷阱 + 类型安全写法
  test('访问触发 ownKeys 异常的 Proxy 应返回 [Unserializable]', () => {
    // 方案1：类型安全写法（推荐）
    const handler: ProxyHandler<Record<string, never>> = {
      ownKeys() {
        throw new Error('Blocked by ownKeys trap');
      }
    };
    const unsafe = new Proxy({}, handler);

    const result = safeStringify({ risky: unsafe });
    const parsed = JSON.parse(result);
    expect(parsed.risky).toBe('[Unserializable]');

    // 验证无 console.error（异常被 replacer 内部 catch 捕获）
    const errorSpy = jest.spyOn(console, 'error');
    safeStringify({ test: unsafe });
    expect(errorSpy).not.toHaveBeenCalled();
    errorSpy.mockRestore();
  });

  // ✅ 其他关键测试（精简核心用例）
  test('危险属性名应替换为 [Filtered]', () => {
    const input = { safe: 'keep', window: {}, _context: null };
    const parsed = JSON.parse(safeStringify(input));
    expect(parsed.window).toBe('[Filtered]');
    expect(parsed._context).toBe('[Filtered]');
    expect(parsed.safe).toBe('keep');
  });

  test('含自身 __proto__ 属性的对象应标记为 [Complex Object]', () => {
    // ⚠️ 关键：必须用 defineProperty 创建 *自身* __proto__ 属性（非原型链）
    const obj = Object.defineProperty({}, '__proto__', {
      value: 'fake proto property',
      enumerable: true,
      configurable: true
    });
    const parsed = JSON.parse(safeStringify({ data: obj }));
    expect(parsed.data).toBe('[Complex Object]');
  });

  test('循环引用应触发外层 catch，返回字符串 [Serialization Failed]', () => {
    const circular: any = { name: 'loop' };
    circular.self = circular;

    const result = safeStringify(circular);
    expect(result).toBe('[Serialization Failed]'); // 字符串，非 JSON

    // 验证 console.error 被触发（外层 catch 行为）
    const errorSpy = jest.spyOn(console, 'error').mockImplementation();
    safeStringify(circular);
    expect(errorSpy).toHaveBeenCalledWith(
      'Safe stringify failed:',
      expect.any(TypeError)
    );
    errorSpy.mockRestore();
  });

  test('数组元素应正确替换且长度不变', () => {
    const input = [
      1,
      { $$typeof: Symbol.for('react.element') },
      'safe',
      { tag: 1, key: 'x', stateNode: {} }
    ];
    const result = JSON.parse(safeStringify(input));
    expect(result).toEqual([1, '[React Element]', 'safe', '[React Fiber Node]']);
    expect(result.length).toBe(4); // 长度不变 = replacer 有 return value
  });

  test('普通数据应完整保留', () => {
    const input = { str: 'ok', num: 42, arr: [1, null, true] };
    const parsed = JSON.parse(safeStringify(input));
    expect(parsed).toEqual(input);
  });
});
