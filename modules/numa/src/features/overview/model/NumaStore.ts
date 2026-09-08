import { makeAutoObservable, runInAction } from 'mobx';
import type { NumaOverviewParams } from '@/features/overview/api/getNumaOverview';
import type { DirectoryState, Locale, NumaOverview, SelectedItem } from '@/entities/numa/types';
import { FULL_TIME_RANGE, normalizeTimeAnalysisRange } from './timeRange';
import type { MetricTimeRange } from './timeRange';

export const DEFAULT_ZOOM = 1;
export const MIN_ZOOM = 0.45;
export const MAX_ZOOM = 1.5;
export const ZOOM_STEP = 0.1;

export type NumaOverviewLoader = (params: NumaOverviewParams) => Promise<NumaOverview>;

export class NumaStore {
    locale: Locale = 'zhCN';
    directory: DirectoryState = { rankId: '', selectedFilePath: '' };
    timeRange: MetricTimeRange = FULL_TIME_RANGE;
    analysisRange: MetricTimeRange = FULL_TIME_RANGE;
    data: NumaOverview | null = null;
    selected: SelectedItem | null = null;
    loading = false;
    error = '';
    zoom = DEFAULT_ZOOM;
    showConnectionMetrics = true;
    private requestSequence = 0;
    private lastRequestKey = '';

    constructor(private readonly overviewLoader: NumaOverviewLoader) {
        makeAutoObservable(this, {}, { autoBind: true });
    }

    setLocale(value: unknown): void {
        this.locale = value === 'enUS' ? 'enUS' : 'zhCN';
    }

    switchDirectory(directory: DirectoryState): Promise<void> {
        this.directory = directory;
        return this.load();
    }

    setTimeAnalysisRange(value: unknown): Promise<void> {
        const range = normalizeTimeAnalysisRange(value);
        this.timeRange = range;
        this.analysisRange = range;
        return this.load();
    }

    activate(): Promise<void> {
        return this.load();
    }

    select(selected: SelectedItem | null): void {
        this.selected = selected;
    }

    changeZoom(value: number): void {
        this.zoom = Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, Number(value.toFixed(2))));
    }

    toggleConnectionMetrics(): void {
        this.showConnectionMetrics = !this.showConnectionMetrics;
    }

    reset(): void {
        this.requestSequence++;
        this.lastRequestKey = '';
        this.timeRange = FULL_TIME_RANGE;
        this.analysisRange = FULL_TIME_RANGE;
        this.data = null;
        this.selected = null;
        this.loading = false;
        this.error = '';
    }

    async load(): Promise<void> {
        const { rankId, selectedFilePath } = this.directory;
        if (!rankId || !selectedFilePath) {
            this.requestSequence++;
            this.lastRequestKey = '';
            this.data = null;
            this.selected = null;
            this.loading = false;
            this.error = '';
            return;
        }

        const { startTime, endTime } = this.timeRange;
        const requestKey = `${rankId}\n${selectedFilePath}\n${startTime}\n${endTime}`;
        if (this.lastRequestKey === requestKey) return;

        const requestSequence = ++this.requestSequence;
        this.lastRequestKey = requestKey;
        this.loading = true;
        this.error = '';
        try {
            const data = await this.overviewLoader({
                rankId,
                dbPath: selectedFilePath,
                startTime,
                endTime,
            });
            if (requestSequence !== this.requestSequence) return;
            runInAction(() => {
                this.data = data;
                this.selected = null;
            });
        } catch (error) {
            if (requestSequence !== this.requestSequence) return;
            runInAction(() => {
                this.lastRequestKey = '';
                this.data = null;
                this.error = error instanceof Error ? error.message : String(error);
            });
        } finally {
            if (requestSequence === this.requestSequence) {
                runInAction(() => {
                    this.loading = false;
                });
            }
        }
    }
}
