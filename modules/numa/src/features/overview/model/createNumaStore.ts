import { getNumaOverview } from '@/features/overview/api/getNumaOverview';
import { NumaStore } from './NumaStore';

export const numaStore = new NumaStore(getNumaOverview);
