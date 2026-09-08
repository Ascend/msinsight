import { createRequest } from '@insight/lib/utils';
import type { NumaOverview } from '@/entities/numa/types';
import { connector } from '@/infrastructure/connection/client';

const request = createRequest(connector);

export interface NumaOverviewParams {
    rankId: string;
    dbPath: string;
    startTime?: number;
    endTime?: number;
}

export const getNumaOverview = async (params: NumaOverviewParams): Promise<NumaOverview> => {
    return request('numa/overview', {
        ...params,
        startTime: params.startTime ?? 0,
        endTime: params.endTime ?? 0,
    });
};
