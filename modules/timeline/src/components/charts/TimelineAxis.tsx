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
import { useTheme } from '@emotion/react';
import styled from '@emotion/styled';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import * as React from 'react';
import { useRenderEngine } from '../../context/context';
import type { SizePx } from '../../entity/chart';
import type { Session } from '../../entity/session';
import type { TimeUnit } from '../../utils/adaptTimeForLength';
import { getDuration, getTimestamp } from '../../utils/humanReadable';
import { useWatchResize } from '../../utils/useWatchDomResize';
import { RangeMarkerButtonCanvas } from '../TimeMakerButton';
import { adaptDpr } from '@insight/lib/utils';
import { MAX_ZOOM_DURATION } from '../../entity/domain';

export const BASE_TICKS_SPACE_PX = 50;
export const BASE_DOMAIN_STEP = 1;

const DEFAULT_DRAW_TIMELINE_AXIS_OPTIONS = {
    domain: [0, 2e4] as Domain,
    spaceX: 30,
    spaceY: 25,
    ticksLength: -11,
    fontSize: 11,
    fontFamily: 'microsoft yahei',
    letterLineSpace: -20,
    fontColor: 'black',
    lineColor: 'black',
    lineWidth: 2,
    textParser: (text: string | number): string => text.toString(),
    timePerPx: 1,
};
export const TIME_LINE_AXIS_CLASSNAME = 'timelineAxis';

const drawText = (ctx: CanvasRenderingContext2D, {
    beginX,
    beginY,
    text,
    color = 'black',
    fontSize = 16,
    fontFamily = 'microsoft yahei',
}: {
    beginX: number;
    beginY: number;
    text: string;
    color?: React.CSSProperties['color'];
    fontSize?: number;
    fontFamily?: React.CSSProperties['fontFamily'];
}): void => {
    ctx.font = `${fontSize}px ${fontFamily}`;
    ctx.fillStyle = color;
    ctx.fillText(text, beginX, beginY);
};

interface LineOptions {
    beginX: number;
    beginY: number;
    length: number;
    lineWidth?: number;
    color?: React.CSSProperties['color'];
};

const drawVerticalLine = (ctx: CanvasRenderingContext2D, { beginX, beginY, length, lineWidth = 1, color = 'black' }: LineOptions): void => {
    ctx.beginPath();
    ctx.lineWidth = lineWidth;
    ctx.moveTo(beginX, beginY);
    ctx.lineTo(beginX, beginY + length);
    ctx.closePath();
    ctx.strokeStyle = color;
    ctx.stroke();
};

type Domain = [ number, number ];

interface GetTimestamper {
    originX: number;
    domain: Domain;
    timeStep: number;
    tickSpace: number;
    index: number;
};
// get timestamp and its position for each tick
export const getTimestamper = ({ timeStep, tickSpace, domain: [timeStart], index, originX }: GetTimestamper): { timestamp: number; beginX: number } => {
    if (timeStep === 0) {
        return { timestamp: timeStart, beginX: originX + (index * tickSpace) };
    }
    const integerStart = timeStart - (timeStart % timeStep);
    const timeOffset = timeStart - integerStart;
    const stepOffset = tickSpace * timeOffset / timeStep;
    const timestamp = integerStart + (timeStep * index);
    return { timestamp, beginX: originX + (index * tickSpace) - stepOffset };
};

interface CommonTickArgs {
    beginX: number;
    originX: number;
    distanceX: number;
    spaceInterval: number;
    timestamp: number;
    timeStep: number;
};

type GetTickText = CommonTickArgs & { textualCenterOffset: number; text: string };
export const getTickText = ({ beginX, originX, distanceX, spaceInterval, timestamp, timeStep, textualCenterOffset, text }: GetTickText): string | null => {
    const isOverflow = beginX + textualCenterOffset < originX || beginX - textualCenterOffset > distanceX;
    const isTextTick = (timestamp + timeStep) % (timeStep * spaceInterval) === 0;
    if (!isOverflow && isTextTick) {
        return text;
    } else {
        return null;
    }
};

type GetTickLength = CommonTickArgs & { ticksLength: number };
export const getTickLength = ({ beginX, originX, distanceX, timestamp, timeStep, spaceInterval, ticksLength }: GetTickLength): number => {
    // tick overflow start bound
    if ((beginX - originX) < 0 ||
    // tick overflow end bound
    (distanceX - beginX) < 0) {
        return 0;
    // short tick with timestamp
    } else if ((timestamp + timeStep) % (timeStep * spaceInterval) !== 0) {
        return ticksLength / 3;
    } else {
    // long tick
        return ticksLength;
    }
};

interface RenderNode {
    text: {
        text: string | null;
        beginX: number;
        beginY: number;
    };
    line: {
        beginX: number;
        beginY: number;
        length: number;
    };
};

const getAdaptableTickSpaceAndNum = ([start, end]: Domain, timePerPx: number): { timeStep: number; tickSpace: number; tickNum: number } => {
    const duration = end - start;
    /**
     * if duration's first digit over 5, timeStep is 5 * (duration.length - 1)
     * else timeStep is 2.5 * (duration.length - 1)
     * so the tickNum's bound is (4, 20)
     */
    const getTimeStep = (time: number): number => {
        const INTEGER_ONE = 2.5;
        const INTEGER_TWO = 1;
        const timeString = time.toString();
        const expotential = timeString.length - 2;
        const base = Number(timeString[0]) > INTEGER_ONE ? INTEGER_ONE : INTEGER_TWO;
        return base * Math.pow(10, expotential) / 2;
    };
    const timeStep = getTimeStep(Math.ceil(duration));
    const tickSpace = timePerPx === 0 ? 0 : timeStep / timePerPx;
    const tickNum = timeStep === 0 ? 0 : Math.floor(duration / timeStep) + 1;
    return { timeStep, tickSpace, tickNum };
};

const getResFromStrategies = (target: number, strategies: Record<number, number>, defaultValue: number = 10): number => {
    const keys = Object.keys(strategies).map(item => Number(item));
    for (const key of keys) {
        if (target <= key) {
            return strategies[key];
        }
    }
    return defaultValue;
};
interface CreateRenderNodes {
    domain: Domain;
    ticksLength: number;
    distanceX: number;
    originX: number;
    originY: number;
    letterLineSpace: number;
    fontSize: number;
    textParser: (text: number, domain: Domain) => string;
    timePerPx: number;
};
export const createRenderNodes = ({
    domain,
    distanceX,
    originX,
    originY,
    ticksLength,
    letterLineSpace,
    fontSize,
    textParser,
    timePerPx,
}: CreateRenderNodes): RenderNode[] => {
    const { timeStep, tickSpace, tickNum } = getAdaptableTickSpaceAndNum(domain, timePerPx);
    const renderNodes: RenderNode[] = [];
    /**
     * space interval strategies
     * [count of space]: [count of space interval]
     * eg:
     * 30: 4 -> when timeline has 30 spaces, display text & long ticke line every 4 spaces.
     */
    const spaceIntervalStrategies = {
        20: 4,
        15: 3,
        10: 2,
    };
    const spaceInterval = getResFromStrategies(tickNum, spaceIntervalStrategies);
    for (let index = -1; index < tickNum + 3; index++) {
        const { timestamp, beginX } = getTimestamper({ timeStep, tickSpace, domain, index, originX });
        const text = textParser(timestamp, domain);
        const textualCenterOffset = text !== null ? text.length * fontSize / 3.5 : 0;
        renderNodes.push({
            text: {
                text: getTickText({ beginX, originX, distanceX, timestamp, timeStep, textualCenterOffset, text, spaceInterval }),
                beginX: beginX - textualCenterOffset,
                beginY: originY - letterLineSpace,
            },
            line: {
                length: getTickLength({ beginX, originX, distanceX, timestamp, timeStep, ticksLength, spaceInterval }),
                beginY: originY,
                beginX,
            },
        });
    }
    return renderNodes;
};

interface DrawAxisOptions {
    spaceX?: number;
    spaceY?: number;
    ticksLength?: number;
    domain: Domain;
    fontSize?: number;
    fontFamily?: React.CSSProperties['fontFamily'];
    letterLineSpace?: number;
    fontColor?: React.CSSProperties['color'];
    lineColor?: React.CSSProperties['color'];
    lineWidth?: number;
    textParser?: (text: number, domain: Domain) => string;
    timePerPx: number;
}
const drawTimelineAxis = (canvas: HTMLCanvasElement, {
    domain,
    spaceX = 30,
    spaceY = 2,
    ticksLength = -12,
    fontSize = 11,
    fontFamily = 'microsoft yahei',
    letterLineSpace = 16,
    fontColor = 'black',
    lineColor = 'black',
    lineWidth = 1,
    textParser = (text: number): string => text.toString(),
    timePerPx = 1,
}: DrawAxisOptions = DEFAULT_DRAW_TIMELINE_AXIS_OPTIONS): void => {
    const ctx = canvas.getContext('2d');
    if (!ctx) { return; }
    const { canvasWidth, canvasHeight } = adaptDpr(canvas, ctx);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const originX = spaceX;
    const originY = canvasHeight - spaceY;
    const distanceX = canvasWidth - (2 * spaceX);

    // draw stable line, includes a horizontal main axis(if needed), and vertical ticks for both terminal point
    drawVerticalLine(ctx, { beginX: originX, beginY: originY, length: ticksLength, lineWidth, color: lineColor });
    drawVerticalLine(ctx, { beginX: originX + distanceX, beginY: originY, length: ticksLength, lineWidth, color: lineColor });

    const renderNodes = createRenderNodes({
        domain,
        distanceX,
        originX,
        originY,
        ticksLength,
        letterLineSpace,
        fontSize,
        textParser,
        timePerPx,
    });
    renderNodes.forEach(({ text: { text, ...rest }, line }) => {
        if (line.beginY !== 0) {
            drawVerticalLine(ctx, { color: lineColor, lineWidth, ...line });
        }
        if (text !== null && rest.beginX > 0) {
            drawText(ctx, { fontFamily, fontSize, color: fontColor, text, ...rest });
        }
    });
};

const CanvasContainer = styled.div`
    width: 100%;
`;

interface TimelineAxisProps {
    session: Session;
    margin: SizePx;
    timelineHeight: number;
};

interface TimelineAxisDrawState {
    canvasWidth: number;
    canvasHeight: number;
    devicePixelRatio: number;
    domainStart: number;
    domainEnd: number;
    timePerPx: number;
    isNsMode: boolean;
    margin: SizePx;
    fontColor: React.CSSProperties['color'];
    fontFamily: React.CSSProperties['fontFamily'];
    lineColor: React.CSSProperties['color'];
}

const isSameDrawState = (previous: TimelineAxisDrawState | undefined, current: TimelineAxisDrawState): boolean =>
    previous !== undefined &&
    previous.canvasWidth === current.canvasWidth &&
    previous.canvasHeight === current.canvasHeight &&
    previous.devicePixelRatio === current.devicePixelRatio &&
    previous.domainStart === current.domainStart &&
    previous.domainEnd === current.domainEnd &&
    previous.timePerPx === current.timePerPx &&
    previous.isNsMode === current.isNsMode &&
    previous.margin === current.margin &&
    previous.fontColor === current.fontColor &&
    previous.fontFamily === current.fontFamily &&
    previous.lineColor === current.lineColor;

type TextParser = (time: number, domain: Domain) => string;
export const getTextParser = (isNsMode: boolean): TextParser => {
    return (time: number, [start, end]: Domain): string => {
        const precision: TimeUnit = isNsMode ? 'ns' : 'ms';
        const duration = end - start;
        /**
         * segments
         * [time duration]: segments
         * eg:
         * 1e4: 6 -> when time duration below 1e4, segements is 6. The formatted timestamp is [hh:mm:]ss:ms.μs.ns
         * [hh:mm] display optionally, just show with non-empty value.
         */
        const segmentsStrategies = {
            1e4: 6,
            1e6: 5,
            1e8: 4,
        };
        const segments = isNsMode ? getResFromStrategies(duration, segmentsStrategies, 3) : 3;
        return getTimestamp(time, { precision, segments });
    };
};

const StyledTotalDuration = styled.div`
    position: absolute;
    right: 0;
    bottom: 1px;
    padding: 0 4px;
    background: ${(props): string => props.theme.bgColorLight};
    border-radius: 4px;
    border: 1px solid ${(props): string => props.theme.borderColor};
    font-size: 12px;
`;

const TotalDuration = observer(({ session }: { session: Session }): JSX.Element => {
    if (session.endTimeAll === undefined || session.endTimeAll < MAX_ZOOM_DURATION) {
        return <></>;
    }

    const duration = getDuration(session.endTimeAll, { maxChars: 4 });

    return <StyledTotalDuration>
        {duration}
    </StyledTotalDuration>;
});

const TimelineAxis = observer(({ session, margin, timelineHeight }: TimelineAxisProps): JSX.Element => {
    const canvas = React.useRef<HTMLCanvasElement>(null);
    const previousDrawState = React.useRef<TimelineAxisDrawState>();
    const [width, ref] = useWatchResize<HTMLDivElement>('width');
    const theme = useTheme();
    React.useLayoutEffect(() => {
        // React 更新 Canvas 尺寸属性时会清空画布；即使恢复为原尺寸，也必须使绘制缓存失效。
        previousDrawState.current = undefined;
    }, [width, timelineHeight]);
    const draw = React.useMemo(() => () => {
        const canvasElement = canvas.current;
        const container = ref.current;
        if (canvasElement === null || container === null ||
            canvasElement.clientWidth === 0 || canvasElement.clientHeight === 0) {
            return;
        }
        runInAction(() => {
            if (session.zoom !== undefined) {
                session.domain.zoom = {
                    zoomCount: session.zoom.zoomCount,
                    zoomPoint: session.zoom.zoomPoint ?? ((session.selectedRange) && ((session.selectedRange[0] + session.selectedRange[1]) / 2)),
                };
                session.zoom = undefined;
            }
            session.domain.chartViewWidth = container.clientWidth;
        });
        const { domainStart, domainEnd } = session.domainRange;
        const currentDrawState: TimelineAxisDrawState = {
            canvasWidth: canvasElement.clientWidth,
            canvasHeight: canvasElement.clientHeight,
            devicePixelRatio: window.devicePixelRatio,
            domainStart,
            domainEnd,
            timePerPx: session.domain.timePerPx,
            isNsMode: session.isNsMode,
            margin,
            fontColor: theme.textColor,
            fontFamily: theme.fontFamily,
            lineColor: theme.timelineAxisColor,
        };
        if (isSameDrawState(previousDrawState.current, currentDrawState)) {
            return;
        }
        drawTimelineAxis(canvasElement, {
            domain: [domainStart, domainEnd],
            spaceX: margin,
            fontColor: theme.textColor,
            fontFamily: theme.fontFamily,
            lineColor: theme.timelineAxisColor,
            textParser: getTextParser(session.isNsMode),
            timePerPx: session.domain.timePerPx,
        });
        // 仅在绘制完成后记录参数，异常时下一帧仍可重试，避免保留未更新的画面。
        previousDrawState.current = currentDrawState;
    }, [session, margin, timelineHeight, theme]);
    const renderEngine = useRenderEngine();
    React.useEffect(() => {
        const renderID = renderEngine.addTask(draw);
        return () => {
            renderEngine.deleteTask(renderID);
        };
    }, [renderEngine, draw]);
    return <CanvasContainer ref={ref} className={TIME_LINE_AXIS_CLASSNAME}>
        <canvas
            ref={canvas}
            width={width}
            height={timelineHeight}
            style={{ width, height: timelineHeight }}
        />
        <RangeMarkerButtonCanvas session={ session } timelineHeight={timelineHeight}/>
        <TotalDuration session={session}></TotalDuration>
    </CanvasContainer>;
});

export default TimelineAxis;
