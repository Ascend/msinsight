/* Browser APIs used by command transports are absent in the Jest/jsdom runtime. */
import { randomUUID } from 'crypto';

if (!globalThis.crypto) {
    Object.defineProperty(globalThis, 'crypto', { configurable: true, value: {} });
}
if (!globalThis.crypto.randomUUID) {
    Object.defineProperty(globalThis.crypto, 'randomUUID', { configurable: true, value: randomUUID });
}
