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
import type { Theme } from '@emotion/react';
import type { Session } from '../../entity/session';
import type { InsightUnit } from '../../entity/insight';
import {
    getTimeOffset as getCategoryTimeOffset,
    type CardIdIndex,
} from './offset';

export { buildCardIdIndex } from './offset';
export type { CardIdIndex } from './offset';

export const colorPalette: Array<keyof Theme['colorPalette']> = [
    'deepBlue',
    'coralRed',
    'tealGreen',
    'aquaBlue',
    'raspberryPink',
    'vividBlue',
    'vividRed',
    'royalPurple',
    'skyBlue',
    'sunsetOrange',
    'amethystPurple',
    'limeGreen',
];
export function getTimeOffset(session: Session, metaData: { cardId?: string; processId?: string; metaType?: string }, units: InsightUnit[] = [],
    timestampOffset?: Record<string, number>, cardIdIndex?: CardIdIndex): number {
    return getCategoryTimeOffset(session, metaData, units, timestampOffset, cardIdIndex);
}

function parseDecimal(str: string): {
    sign: bigint;
    int: string;
    frac: string;
} {
    let sign = BigInt(1);
    if (str.startsWith('-')) {
        sign = BigInt(-1);
        str = str.slice(1);
    } else if (str.startsWith('+')) {
        str = str.slice(1);
    }
    const [intPart, fracPart = ''] = str.split('.');
    return { sign, int: intPart || '0', frac: fracPart };
}

export function bigSubtract(a: number | string, b: number | string): string {
    if (typeof BigInt === 'undefined') {
        return '-';
    }
    // 转为字符串，避免 number 精度丢失
    const aStr = String(a);
    const bStr = String(b);

    // 拆分整数/小数部分
    const aParser = parseDecimal(aStr);
    const bParser = parseDecimal(bStr);

    // 统一小数长度（取较长的小数位数）
    const maxFracLen = Math.max(aParser.frac.length, bParser.frac.length);
    const aFracPadded = aParser.frac.padEnd(maxFracLen, '0');
    const bFracPadded = bParser.frac.padEnd(maxFracLen, '0');

    // 拼接整数+小数 → 大整数
    const aBig = aParser.sign * BigInt(aParser.int + aFracPadded);
    const bBig = bParser.sign * BigInt(bParser.int + bFracPadded);

    // 执行减法
    let resultBig = aBig - bBig;

    // 结果符号
    let signStr = '';
    if (resultBig < 0) {
        signStr = '-';
        resultBig = -resultBig;
    }

    // 转字符串并插入小数点
    let resultStr = resultBig.toString().padStart(maxFracLen + 1, '0');
    if (maxFracLen > 0) {
        const intPart = resultStr.slice(0, -maxFracLen);
        const fracPart = resultStr.slice(-maxFracLen);
        resultStr = intPart + '.' + fracPart;
    }

    // 去掉前导零 & 末尾多余的零
    resultStr = resultStr.replace(/^0+(?=\d)/, ''); // 去掉多余前导零
    if (resultStr.includes('.')) {
        // 先去掉小数点后多余的 0
        resultStr = resultStr.replace(/(\.\d*?[1-9])0+$/, '$1');
        // 再去掉类似 "123." 的末尾小数点
        resultStr = resultStr.replace(/\.$/, '');
    }

    return signStr + resultStr;
}
