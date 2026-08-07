import { MindStudioManager } from '../manager';
import * as MindStudio from '../mindstudio';
interface MindStudioInsightProps {
    setWidgetName?: (name: string) => void;
    createdModelName?: string;
    mindstudioManager: MindStudioManager;
    closeWidget: () => void;
    openMindStudio: (modelName: string) => void;
    updateCurrentModel: (model: MindStudio.IModel | null) => void;
    startNew: (name: string, options?: MindStudio.IOptions) => Promise<MindStudio.IMindStudio>;
    startIFrame: () => Promise<string>;
}
export declare const MindStudioInsightTab: (props: MindStudioInsightProps) => JSX.Element;
export {};
