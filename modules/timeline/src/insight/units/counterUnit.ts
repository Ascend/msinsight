import type { CounterMetaData } from '../../entity/data';

const UNIT_ONLY_COUNTER_TYPES = new Set([
    '%',
    'B',
    'Byte',
    'B/s',
    'Byte/s',
    'Packet',
    'Packet/s',
    'Pkt',
    'Pkt/s',
    'Retry',
    'Rty',
    'Cycle',
    'Level',
    'Hz',
    'KHz',
    'Mhz',
    'MHz',
    'GHz',
    'ns',
    'us',
    'ms',
    's',
]);

const UNIT_IN_BRACKETS_REGEXP = /\(([^()]+)\)\s*$/;

const extractUnitFromDataType = (dataType: string): string | undefined => {
    const trimmedDataType = dataType.trim();
    const bracketMatch = trimmedDataType.match(UNIT_IN_BRACKETS_REGEXP);
    if (bracketMatch?.[1] !== undefined) {
        return bracketMatch[1].trim();
    }
    return UNIT_ONLY_COUNTER_TYPES.has(trimmedDataType) ? trimmedDataType : undefined;
};

const getCounterUnit = (dataType: string[]): string | undefined => {
    const units = dataType.map(extractUnitFromDataType);
    if (units.length === 0 || units.some(unit => unit === undefined)) {
        return undefined;
    }
    const [firstUnit] = units;
    return units.every(unit => unit === firstUnit) ? firstUnit : undefined;
};

const hasUnitSuffix = (name: string, unit: string): boolean => {
    const trimmedName = name.trim();
    if (trimmedName.endsWith(`(${unit})`)) {
        return true;
    }
    return UNIT_IN_BRACKETS_REGEXP.test(trimmedName);
};

export const getCounterLaneDisplayName = (metadata: CounterMetaData): string => {
    const unit = getCounterUnit(metadata.dataType);
    if (unit === undefined || hasUnitSuffix(metadata.threadName, unit)) {
        return metadata.threadName;
    }
    return `${metadata.threadName} (${unit})`;
};
