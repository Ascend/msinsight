export type InitialZoomDomainInput = {
    blockMinTimestamp: number;
    blockMaxTimestamp: number;
    allocationMinTimestamp: number;
    allocationMaxTimestamp: number;
    funcMinTimestamp: number;
    funcMaxTimestamp: number;
};

export type InitialZoomDomain = {
    minTime: number;
    maxTime: number;
};

export const getInitialZoomDomain = ({
    blockMinTimestamp,
    blockMaxTimestamp,
    allocationMinTimestamp,
    allocationMaxTimestamp,
    funcMinTimestamp,
    funcMaxTimestamp,
}: InitialZoomDomainInput): InitialZoomDomain => {
    const hasBlockRange = blockMaxTimestamp > blockMinTimestamp;
    let minTime = hasBlockRange ? blockMinTimestamp : allocationMinTimestamp;
    let maxTime = Math.max(blockMaxTimestamp, allocationMaxTimestamp);

    if (funcMaxTimestamp > 0) {
        minTime = Math.min(minTime, funcMinTimestamp);
        maxTime = Math.max(maxTime, funcMaxTimestamp);
    }

    return { minTime, maxTime };
};
