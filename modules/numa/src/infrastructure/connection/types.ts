export type NotificationBody = Record<string, unknown>;

export interface NotificationMessage {
    event: string;
    body: NotificationBody;
}

export type NotificationHandler = (body: NotificationBody) => void | Promise<void>;
