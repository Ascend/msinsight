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
import * as React from 'react';
import { createRoot } from 'react-dom/client';
import { flushSync } from 'react-dom';
import styled from '@emotion/styled';
import Index from '../components/BaseContainer';
import { MIDescriptions, MIDescriptionsItem } from '../components/MIDescriptions';
import COLOR from './Color';
import {
    chartVisbilityListener,
    disposeAdaptiveEchart,
    getAdaptiveEchart,
    getDefaultChartOptions,
    getLegendStyle,
} from './EchartUtils';
import { Empty, message } from '../components/index';
import { type Theme, useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import { t as i18nextT } from 'i18next';
import { AlarmIcon, BulbIcon } from '../icon/Icon';
import ResizeObserver from 'resize-observer-polyfill';
import { getShortcutKey, isMac, KEYS } from './key';

export { customConsole } from './Console';
export { Decimal } from './Decimal';
export {
    Index,
    MIDescriptions,
    MIDescriptionsItem,
    COLOR,
    chartVisbilityListener,
    getAdaptiveEchart,
    disposeAdaptiveEchart,
    getDefaultChartOptions,
    getLegendStyle,
    KEYS,
    getShortcutKey,
    isMac,
};

const BREAK_LINE_REGEXP = /\r\n|\r|\n/g;
export const StyledEmpty = ({ descriptor, style }:
{ descriptor?: string; style?: object; translation?: any }): JSX.Element => {
    const theme = useTheme();
    const { t } = useTranslation();
    return (
        <Empty
            image={Empty.PRESENTED_IMAGE_SIMPLE}
            style={style}
            imageStyle={{ height: 30 }}
            description={
                <span style={{ color: theme.fontColor }}>
                    {t(descriptor ?? 'No Data')}
                </span>}>
        </Empty>
    );
};

const StyleDiv = styled.div`
display: flex;
align-items: flex-start;
`;

const StyledAdvice = styled.div`
    color: ${(p): string => p.theme.textColorPrimary};
    background-color: ${(p): string => p.theme.bgColorLight};
    font-size: 14px;
    padding: 8px 16px;
    margin-bottom: 5px;
    display: flex;
    flex-direction: row;
    & > div:first-child {
        flex: 0 0 auto;
        vertical-align: top;
        & > span,div {
            margin-right: 5px;
            font-weight: bold;
        }
    }
    & > div:nth-child(2) {
        flex: auto;
        word-break: break-all;
    }
`;

interface IHitProps {
    title?: React.ReactNode;
    text: React.ReactNode | React.ReactNode[];
    style?: React.CSSProperties;
    type?: string;
}
export function Hit(props: IHitProps): JSX.Element {
    const { type, title, text, style, ...restProps } = props;
    const icon = type === 'alarm' ? <AlarmIcon /> : <BulbIcon />;
    const splitText = (str: React.ReactNode): React.ReactNode => {
        if (typeof str !== 'string') {
            return str;
        }
        const list = str.split(BREAK_LINE_REGEXP).filter(v => v !== '');
        return list.map((item) =>
            <>
                { item } <br/>
            </>);
    };
    return <StyledAdvice style={style} {...restProps}>
        <div>
            <StyleDiv>
                { icon }
                { (title !== undefined && title !== null)
                    ? (
                        <span className={'lh-20'}>{ title }</span>)
                    : <></> }
            </StyleDiv>
        </div>
        <div>{ Array.isArray(text)
            ? text.map((item, index) =>
                <div className={'lh-20'} key={ index }>{ splitText(item) }</div>)
            : <div className={'lh-20'}>
                {splitText(text)}
            </div> }
        </div>
    </StyledAdvice>;
}
export function Advice(props: IHitProps): JSX.Element {
    const { t } = useTranslation();
    return <Hit type={'advice'} title={`${t('Advice')}:`} {...props} />;
}

export function limitInput(maxlength?: string): void {
    setTimeout(() => {
        const inputs = document.querySelectorAll('input');
        inputs.forEach(input => {
            if (input.maxLength < 0) {
                input.setAttribute('maxlength', maxlength ?? '200');
            }
        });
    });
}

let logindex: number = 0;
const logRecord: Record<number, unknown> = {};
export function log(...param: unknown[]): void {
    logRecord[logindex++ % 1000] = param;
}

export function Label({ name, style }: { name: React.ReactNode; style?: object }): JSX.Element {
    return <span style={{ marginRight: 10, ...(style ?? {}) }}>{name}{name !== undefined && ':'} </span>;
};

export function getSet<T extends object>(list: T[], field: keyof T): unknown[] {
    return Array.from(new Set(list.map(item => item[field])));
}

export function firstLetterUpper(word: string): string {
    const list = word.split(/\s+/);
    return list.map(item => item.charAt(0).toUpperCase() + item.slice(1)).join(' ');
}

export function getUsableVal<T>(val: T, options: Array<{ value: T }>, defaultVal: T, func?: (inputArray: Array<{ value: T }>) => T): T {
    if (options.length === 0) {
        return defaultVal;
    }
    if (options.find(item => item.value === val)) {
        return val;
    }
    if (func !== undefined) {
        return func(options);
    }
    return options[0].value;
};

export const delayExecute = (doFunc: () => void, timeout = 500): void => {
    setTimeout(() => {
        doFunc();
    }, timeout);
};

export function sortFunc<T>(a: T, b: T, sorter = 'asc'): number {
    const aNum = Number(a);
    const bNum = Number(b);
    if (isNaN(aNum) && isNaN(bNum)) {
        // a、b不是数字，不变
        return 0;
    } else if (isNaN(aNum)) {
        // a不是数字，b是数字，a放后面
        return 1;
    } else if (isNaN(bNum)) {
        // a是数字，b不是数字，不变
        return 0;
    } else {
        return sorter === 'asc' ? (aNum - bNum) : (bNum - aNum);
    }
}
export function formatDecimal(val: number | string, fixed = 2): number {
    const num = Number(val);
    if (isNaN(num) || num === 0) {
        return num;
    } else {
        const numStr = String(num);
        let decimal = 0;
        const match = numStr.match(/\.0*(?<notZero>[1-9])/);
        if (match) {
            // 小数点后第一个非零数字位置
            decimal = match[0].indexOf(match[1]);
        }
        return Number(num.toFixed(decimal <= fixed ? fixed : decimal - 1 + fixed));
    }
}

const htmlEscapeMap = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    '\'': '&#39;',
    '/': '&#x2F;',
};

const escapeHTML = (input: string): string => {
    return input.replace(/[&<>"'/]/g, match => htmlEscapeMap[match as keyof typeof htmlEscapeMap]);
};

export const safeStr = (val: string | number, ignore?: string): string => {
    if (typeof val === 'number') {
        return val.toString();
    }
    if (typeof val !== 'string') {
        return '';
    }
    if (ignore) {
        return val.split(ignore).map(item => escapeHTML(item)).join(ignore);
    }
    return escapeHTML(val);
};

export function CustomFormItem({ name, style, content, nameStyle }:
{ name: React.ReactNode; nameStyle?: React.CSSProperties; style?: React.CSSProperties; content: React.ReactElement }): JSX.Element {
    return (<div style={{
        display: 'inline-block',
        height: '30px',
        lineHeight: '30px',
        margin: '0 20px 10px 0',
        ...style ?? {},
    }}>
        <Label name={name} style={{ width: '80px', display: 'inline-block', ...nameStyle ?? {} }} />
        {content}
    </div>);
};

export function getIndexByRankNameAndDeviceId(rankName: string, deviceId: string): number {
    if (rankName === '' || isNaN(Number(rankName))) {
        return isNaN(Number(deviceId)) ? 0 : Number(deviceId);
    } else {
        return Number(rankName);
    }
}

/**
 * 将 cardId 转化成 cardIdInfo
 * @param cardId 有两种可能：1 - rankName; 2 - host+deviceId;
 */
export function transformCardIdInfo(cardId: string): { host: string; rankName: string; deviceId: string; index: number } {
    const list = cardId.split(' ');
    const len = list.length;
    const result = { host: '', rankName: '', deviceId: '', index: 0 };
    switch (len) {
        case 1: // rankId
            result.rankName = list[0];
            break;
        case 2: // host+deviceId
            result.host = list[0];
            result.deviceId = list[1];
            break;
        default: break;
    }
    result.index = getIndexByRankNameAndDeviceId(result.rankName, result.deviceId);
    return result;
}

export function GroupCardInfosByHost<T extends { cardId: string; dbPath: string } = { cardId: string; dbPath: string; index: number }>(
    rankInfos: T[]): { hosts: string[]; cardsMap: Map<string, T[]> } {
    const host = new Set<string>();
    const cardsMap = new Map<string, T[]>();
    rankInfos.forEach((item): void => {
        const cardIdInfo = transformCardIdInfo(item.cardId);
        if (cardIdInfo.host !== '') {
            host.add(cardIdInfo.host);
        }
        cardsMap.set(cardIdInfo.host, [
            ...cardsMap.get(cardIdInfo.host) ?? [],
            { ...item, index: getIndexByRankNameAndDeviceId(cardIdInfo.rankName, cardIdInfo.deviceId) },
        ]);
    });
    return { hosts: Array.from(host), cardsMap };
}

interface RankInfo {
    clusterId: string;
    host: string;
    rankName: string;
    rankId: string;
    deviceId: string;
}

export function getRankInfoKey({ clusterId, host, rankName, deviceId }: RankInfo): string {
    return `${clusterId}_${host}_${rankName}_${deviceId}`;
}

export function getRankInfoLabel({ clusterId, rankName, deviceId }: RankInfo): string {
    return `${clusterId} ${rankName !== '' ? rankName : deviceId}`.trim();
}

function transformRankInfo(rankInfo: RankInfo): { clusterId: string; host: string; rankName: string; deviceId: string } {
    return { ...rankInfo, host: rankInfo.host.trim() };
}

export function GroupCardRankInfosByHost<T extends { rankInfo: RankInfo; dbPath: string } = { rankInfo: RankInfo; dbPath: string }>(
    cardRankInfos: T[]): { hosts: string[]; cardsMap: Map<string, Array<T & { index: number }>> } {
    const host = new Set<string>();
    const cardsMap = new Map<string, Array<T & { index: number }>>();
    cardRankInfos.forEach((item): void => {
        const info = transformRankInfo(item.rankInfo);
        if (info.host !== '') {
            host.add(info.host);
        }
        cardsMap.set(info.host, [
            ...cardsMap.get(info.host) ?? [],
            { ...item, index: getIndexByRankNameAndDeviceId(info.rankName, info.deviceId) },
        ]);
    });
    cardsMap.forEach((v) => v.sort((a, b) => a.index - b.index));
    return { hosts: Array.from(host), cardsMap };
}

export function GroupRankIdsByHost(rankIds: string[]): { hosts: string[]; ranks: Map<string, string[]> } {
    const host = new Set<string>();
    const ranks = new Map<string, string[]>();
    rankIds.forEach(item => {
        const list = item.split(' ');
        if (list.length > 1) {
            host.add(list[0]);
            ranks.set(list[0], [...ranks.get(list[0]) ?? [], item]);
        } else {
            ranks.set('', [...ranks.get('') ?? [], item]);
        }
    });
    return { hosts: Array.from(host), ranks };
};

interface AdaptDprResult {
    canvasWidth: number;
    canvasHeight: number;
}
export const adaptDpr = (canvas: HTMLCanvasElement, ctx: CanvasRenderingContext2D): AdaptDprResult => {
    const dpr = window.devicePixelRatio;
    const canvasWidth = canvas.clientWidth;
    const canvasHeight = canvas.clientHeight;
    canvas.width = canvasWidth * dpr;
    canvas.height = canvasHeight * dpr;
    ctx.scale(dpr, dpr);
    return { canvasWidth, canvasHeight };
};

export type SizeProp = 'width' | 'height';
export function useWatchDomResize<T extends Element>(prop: SizeProp): [number, React.RefObject<T>] {
    const [rect, setRect] = React.useState<DOMRectReadOnly | null>(null);
    const ref = React.useRef<T>(null);
    React.useEffect(() => {
        const observer = new ResizeObserver(([entry]) => {
            window.requestAnimationFrame(() => {
                setRect(entry.contentRect);
            });
        });
        if (ref.current) {
            observer.observe(ref.current);
        }
        return () => {
            observer.disconnect();
        };
    }, []);
    return [rect?.[prop] ?? 0, ref];
}

const removePrototypePollution = (obj: any, loopIndex = 0): void => {
    const MaxLoop = 200;
    if (loopIndex > MaxLoop) {
        return;
    }
    if (obj && typeof obj === 'object') {
        for (const key in obj) {
            if (key === '__proto__' || key === 'constructor') {
                delete obj[key];
            } else if (typeof obj[key] === 'object') {
                removePrototypePollution(obj[key], loopIndex++);
            }
        }
    }
};

export const safeJSONParse = (str: any, defaultValue: any = null): any => {
    try {
        const res = JSON.parse(str);
        removePrototypePollution(res);
        return res;
    } catch (error) {
        return defaultValue;
    }
};

interface KeydownInfo {
    hasCtrl: boolean;
    hasCommand: boolean;
    key: string;
    isMac: boolean;
}

export const disableShortcuts = (forbiddenComboKeys = [], forbiddenSingleKeys = [], specialHandler?: (key: KeydownInfo) => void): void => {
    document.addEventListener('keydown', (e) => {
        const defaultForbiddenComboKeys = ['f', 'p', 'g', 'j', 'r'];
        const defaultForbiddenSingleKeys = ['F3', 'F5', 'F7'];
        const comboKeys = forbiddenComboKeys.length ? forbiddenComboKeys : defaultForbiddenComboKeys;
        const singleKeys = forbiddenSingleKeys.length ? forbiddenSingleKeys : defaultForbiddenSingleKeys;
        const isCtrlCombo = (e.ctrlKey || e.metaKey) && comboKeys.includes(e.key.toLowerCase());

        if (isCtrlCombo || singleKeys.includes(e.key)) {
            e.preventDefault();
        }
        specialHandler?.({ hasCtrl: e.ctrlKey, hasCommand: e.metaKey, key: e.key, isMac });
    });
};

export const chartColors = [
    '#0062DC',
    '#279C6E',
    '#34B1B9',
    '#6037DB',
    '#FF5432',
    '#0077FF',
    '#FD2F2F',
    '#D53F78',
    '#2F9CE0',
    '#EE891D',
    '#AA38CE',
    '#75A105',
    '#E44222',
    '#0158C5',
    '#248B62',
    '#2F9EA6',
    '#026AE5',
    '#E32A2A',
    '#BF376C',
    '#2A8CC9',
    '#5631C4',
    '#D57B18',
    '#9831B9',
    '#699006',
    '#CB2F0F',
    '#004EB0',
    '#1F7C58',
    '#2A8D93',
    '#005FCC',
    '#CA2425',
    '#AA3260',
    '#247CB3',
    '#4C2CAF',
    '#BE6D17',
    '#882CA4',
    '#5D8004',
    '#FE6E50',
    '#2679E1',
    '#48AA82',
    '#51BCC3',
    '#278AFF',
    '#FD4444',
    '#DB5B8C',
    '#4CAAE4',
    '#7754E0',
    '#F09A3D',
    '#B655D4',
    '#89AF2A',
    '#FF876F',
    '#4D91E5',
    '#67B999',
    '#71C8CE',
    '#4DA0FF',
    '#FD5857',
    '#E178A0',
    '#6DB9E8',
    '#9073E5',
    '#F3AC60',
    '#C374DC',
    '#9EBD51',
];

export function clamp(value: number, min: number, max: number): number {
    return Math.min(Math.max(value, min), max);
}

export function formateMicrosecond(t: number): string {
    if (isNaN(t)) {
        return '';
    }
    let leftTime = t;
    // 小时级
    if (t >= 1000 * 1000 * 60 * 60) {
        const h = Math.floor(leftTime / (1000 * 1000 * 60 * 60));
        leftTime = leftTime % (1000 * 1000 * 60 * 60);
        const m = Math.floor(leftTime / (1000 * 1000 * 60));
        leftTime = leftTime % (1000 * 1000 * 60);
        const s = Number((leftTime / (1000 * 1000)).toFixed(2));
        return `${h}h${m}m${s}s`;
    }
    // 分钟级
    if (t >= 1000 * 1000 * 60) {
        const m = Math.floor(leftTime / (1000 * 1000 * 60));
        leftTime = leftTime % (1000 * 1000 * 60);
        const s = Number((leftTime / (1000 * 1000)).toFixed(2));
        return `${m}m${s}s`;
    }
    // 秒级
    if (t >= 1000 * 1000) {
        const s = Number((leftTime / (1000 * 1000)).toFixed(2));
        return `${s}s`;
    }
    // 毫秒级
    if (t >= 1000) {
        const s = Number((leftTime / (1000)).toFixed(2));
        return `${s}ms`;
    }
    return `${t}μs`;
}

const CompareDiv = styled.div`
    width: 100%;
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
    word-break: keep-all;
    &.positive {
        color: ${(props): string => props.theme.successColor};
    }
    &.negative {
        color: ${(props): string => props.theme.dangerColor};
    }
`;

export const CompareNumber = ({ data }: { data: string | number }): JSX.Element => {
    const dataNum = Number(data);
    if (isNaN(dataNum)) {
        return <>{data}</>;
    }
    return <CompareDiv className={dataNum >= 0 ? 'positive' : 'negative'} title={`${data}`}>{data}</CompareDiv>;
};

export const FONT_FAMILY = '\'Inter\', -apple-system, BlinkMacSystemFont, \'Segoe UI\', Roboto, Oxygen, Ubuntu, Cantarell, \'Fira Sans\', \'Droid Sans\', sans-serif';

export const disableIframePointerEvent = (): void => {
    if (window.document.querySelectorAll('iframe').length !== 0) {
        window.document.querySelectorAll('iframe').forEach((item: HTMLIFrameElement) => {
            (item.style as any)['pointer-events'] = 'none';
        });
    }
};
export const recoverIframePointerEvent = (): void => {
    if (window.document.querySelectorAll('iframe').length !== 0) {
        window.document.querySelectorAll('iframe').forEach((item: HTMLIFrameElement) => {
            (item.style as any)['pointer-events'] = 'auto';
        });
    }
};

export class HandleSingleDoubleClick {
    static timerMap: Record<string, number> = {};
    static click(action: () => void, key: string = 'default'): void {
        clearTimeout(this.timerMap[key]);
        this.timerMap[key] = window.setTimeout(() => { action(); }, 300);
    }

    static doubleClick(action: () => void, key: string = 'default'): void {
        clearTimeout(this.timerMap[key]);
        action();
    }
}

export const notNull = (val: any): boolean => {
    return val !== undefined && val !== null && val !== '';
};

export const hexToRgb = (hex: string): [number, number, number] | null => {
    if (!/^#(?<hexCode>[0-9A-Fa-f]{6})$/.test(hex)) {
        return null;
    }

    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);

    return [r, g, b];
};

export const isArray = (val: any): boolean => {
    if (!Array.isArray) {
        return Object.prototype.toString.call(val) === '[object Array]';
    } else {
        return Array.isArray(val);
    }
};

export const getUpdateObject = (data: Record<string, any>, obj: Record<string, any>, ignoreNull: boolean = true): Record<string, any> => {
    if (data === undefined || data === null) {
        return {};
    }
    const dataPropKeys = Object.keys(data);
    const objPropKeys = Object.keys(obj);
    const updateState: Record<string, any> = {};
    for (const key of dataPropKeys) {
        // 1.data的字段key在obj中存在
        // 2.data[key]的类型（例如string、boolean)obj[key]相同。如果ignoreNull，session[key]等于null也可以更新。
        const valid = objPropKeys.includes(key) &&
            (Object.prototype.toString.call(data[key]) === Object.prototype.toString.call(obj[key]) ||
                (ignoreNull && obj[key] === null));
        if (valid) {
            Object.assign(updateState, { [key]: data[key] });
        }
    }
    return updateState;
};

// 创建可取消的异步请求, 解决异步请求竞态问题
type AsyncFunction<TArgs extends any[], TResult> = (...args: TArgs) => Promise<TResult>;
export const createCancelableApi = <TArgs extends any[], TResult>(
    asyncApi: AsyncFunction<TArgs, TResult>,
): {
    invoke: (...args: TArgs) => Promise<TResult>;
    cancel: () => void;
} => {
    let cancel: () => void = () => {};

    function invoke(...args: TArgs): Promise<TResult> {
        cancel(); // 先取消上一次请求

        let isCanceled = false;

        return new Promise<TResult>((resolve, reject) => {
            cancel = (): void => {
                isCanceled = true;
            };

            asyncApi(...args).then(
                (res) => !isCanceled && resolve(res),
                (err) => !isCanceled && reject(err),
            );
        });
    }

    return { invoke, cancel };
};

interface TableHeaderAndData {
    header: Array<{ title: string; key: string }>;
    data: Array<{ [key: string]: number | string }>;
};

interface ObjectKeyString {
    [key: string]: any;
};

export const copyToClipboard = async (text: string): Promise<void> => {
    if (navigator.clipboard === undefined && document.execCommand === undefined) {
        message.warning(i18nextT('NotSupportCopy'));
        return;
    }

    try {
        if (navigator.clipboard !== undefined) {
            await navigator.clipboard.writeText(text);
        } else {
            const input = document.createElement('textarea');
            input.value = text;
            document.body.appendChild(input);
            input.select();
            document.execCommand('copy');
            document.body.removeChild(input);
        }
        message.success({ content: i18nextT('CopySuccessful'), key: 'copyToClipboard' });
    } catch (err) {
        message.error({ content: i18nextT('CopyFailed', { err }), key: 'copyToClipboard' });
    }
};

export const copyTableToClipboard = async (columns: any[], dataSource: any[]): Promise<void> => {
    const header = getTableHeader(columns);
    const data = getTableData(columns, dataSource);
    const formatStr = dataFormat({ header, data });
    await copyToClipboard(formatStr);
};

/**
 * 格式化并复制对象到剪贴板
 * @param obj 任意可序列化对象
 * @param pretty 是否美化输出（默认 true）
 */
export const copyObjectToClipboard = async (
    obj: unknown,
    pretty: boolean = true,
): Promise<void> => {
    if (obj === undefined || obj === null) {
        message.warning('Copy content is empty');
        return;
    }

    try {
        const text = JSON.stringify(obj, null, pretty ? 2 : 0);
        await copyToClipboard(text);
    } catch (err) {
        message.error({
            content: i18nextT('CopyFailed', { err: (err as Error).message }),
            key: 'copyToClipboard',
        });
    }
};

const getTableHeader = (columns: any[]): TableHeaderAndData['header'] => {
    const header = columns.map(col => {
        return {
            title: React.isValidElement(col.title) ? getTextFromReactElement(col.title) : col.title,
            key: col.dataIndex ?? col.key,
        };
    });
    return header;
};

const getTableData = (columns: any[], dataSource: any[]): TableHeaderAndData['data'] => {
    const data = dataSource?.map((item, index) => {
        const obj: ObjectKeyString = {};

        columns.forEach(col => {
            getColData(obj, item, col, index);
        });
        return obj;
    });
    return data;
};

const getTextFromReactElement = (element: React.ReactElement): string => {
    const container = document.createElement('div');
    container.style.display = 'none';
    document.body.appendChild(container);

    let root: ReturnType<typeof createRoot> | null = null;
    try {
        root = createRoot(container);
        flushSync(() => {
            root?.render(element);
        });
        const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT, null);
        const textParts: string[] = [];
        for (let node = walker.nextNode(); node; node = walker.nextNode()) {
            const text = node.textContent?.trim();
            if (text) {
                textParts.push(text);
            }
        }
        return textParts.join(' ');
    } finally {
        root?.unmount();
        container.remove();
    }
};

const getColData = (obj: ObjectKeyString, data: ObjectKeyString, col: any, index: number): void => {
    const dataKey = col.dataIndex ?? col.key;
    if (col.render === undefined || col.render.name === 'useTextColor') {
        obj[dataKey] = data[dataKey];
    } else {
        const itemData = col.render(data[dataKey], data, index);
        if (React.isValidElement(itemData)) {
            obj[dataKey] = getTextFromReactElement(itemData);
        } else if (typeof itemData !== 'object') {
            obj[dataKey] = itemData;
        } else {
            const { content = undefined, children = [] } = { ...itemData, ...(itemData.props ?? {}) };
            obj[dataKey] = content ?? (
                Array.isArray(children) ? children.map(colData => typeof colData !== 'object' ? colData : JSON.stringify(colData)).join('') : children);
        }
    }
};

const dataFormat = ({ header, data }: TableHeaderAndData): string => {
    let result: string = '';
    header.forEach(headerItem => {
        result = `${result}${headerItem.title}\t`;
    });
    result = `${result}\n`;
    data.forEach(item => {
        header.forEach(headerItem => {
            result = `${result}${wrapValue(item[headerItem.key])}\t`;
        });
        result = `${result}\n`;
    });
    return result;
};

const wrapValue = (value: string | number): string | number => {
    if (typeof value !== 'string') {
        return value;
    }
    if (value.includes('\n') || value.includes('\t') || value.includes('"')) {
        return `"${value.replace(/"/g, '""')}"`;
    }
    return value;
};

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

// 判断对象不为空
export const isNotEmpty = (obj: any): boolean =>
    obj && typeof obj === 'object' && !Array.isArray(obj) && Object.keys(obj).length > 0;
