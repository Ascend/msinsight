import { ClientConnector } from '@insight/lib/connection';

export const connector = new ClientConnector({
    getTargetWindow: (): Window[] => [window.parent],
    module: 'numa',
});
