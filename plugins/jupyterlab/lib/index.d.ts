import { JupyterFrontEndPlugin } from '@jupyterlab/application';
import { IWidgetTracker, MainAreaWidget } from '@jupyterlab/apputils';
import { MindStudioReactWidget } from './biz/widget';
/**
 * Initialization data for the mindstudio_insight_jupyterlab extension.
 */
declare const plugin: JupyterFrontEndPlugin<IWidgetTracker<MainAreaWidget<MindStudioReactWidget>>>;
export default plugin;
