/**
 * Copyright (c) Jupyter Development Team.
 * Distributed under the terms of the BSD-3-Clause License.
 *
 * This file is based on code from the @lumino/signaling package:
 * https://github.com/jupyterlab/lumino/tree/master/packages/signaling
 *
 * Modifications made by Huawei Technologies Co., Ltd., 2025.
 */
import { ArrayExt, find } from "./algorithm";
export class Signal {
    constructor(sender) {
        this.sender = sender;
    }
    connect(slot, thisArg) {
        return Private.connect(this, slot, thisArg);
    }
    disconnect(slot, thisArg) {
        return Private.disconnect(this, slot, thisArg);
    }
    emit(args) {
        Private.emit(this, args);
    }
}
/**
 * The namespace for the `Signal` class statics.
 */
(function (Signal) {
    function disconnectBetween(sender, receiver) {
        Private.disconnectBetween(sender, receiver);
    }
    Signal.disconnectBetween = disconnectBetween;
    function disconnectSender(sender) {
        Private.disconnectSender(sender);
    }
    Signal.disconnectSender = disconnectSender;
    function disconnectReceiver(receiver) {
        Private.disconnectReceiver(receiver);
    }
    Signal.disconnectReceiver = disconnectReceiver;
    function disconnectAll(object) {
        Private.disconnectAll(object);
    }
    Signal.disconnectAll = disconnectAll;
    function clearData(object) {
        Private.disconnectAll(object);
    }
    Signal.clearData = clearData;
})(Signal || (Signal = {}));
var Private;
(function (Private) {
    const exceptionHandler = (err) => {
        console.error(err);
    };
    function connect(signal, slot, thisArg) {
        thisArg = thisArg || undefined;
        let receivers = receiversForSender.get(signal.sender);
        if (!receivers) {
            receivers = [];
            receiversForSender.set(signal.sender, receivers);
        }
        if (findConnection(receivers, signal, slot, thisArg)) {
            return false;
        }
        let receiver = thisArg || slot;
        let senders = sendersForReceiver.get(receiver);
        if (!senders) {
            senders = [];
            sendersForReceiver.set(receiver, senders);
        }
        let connection = { signal, slot, thisArg };
        receivers.push(connection);
        senders.push(connection);
        return true;
    }
    Private.connect = connect;
    function disconnect(signal, slot, thisArg) {
        thisArg = thisArg || undefined;
        let receivers = receiversForSender.get(signal.sender);
        if (!receivers || receivers.length === 0) {
            return false;
        }
        let connection = findConnection(receivers, signal, slot, thisArg);
        if (!connection) {
            return false;
        }
        let receiver = thisArg || slot;
        let senders = sendersForReceiver.get(receiver);
        connection.signal = null;
        scheduleCleanup(receivers);
        scheduleCleanup(senders);
        return true;
    }
    Private.disconnect = disconnect;
    function disconnectBetween(sender, receiver) {
        let receivers = receiversForSender.get(sender);
        if (!receivers || receivers.length === 0) {
            return;
        }
        let senders = sendersForReceiver.get(receiver);
        if (!senders || senders.length === 0) {
            return;
        }
        for (const connection of senders) {
            if (!connection.signal) {
                continue;
            }
            if (connection.signal.sender === sender) {
                connection.signal = null;
            }
        }
        scheduleCleanup(receivers);
        scheduleCleanup(senders);
    }
    Private.disconnectBetween = disconnectBetween;
    function disconnectSender(sender) {
        let receivers = receiversForSender.get(sender);
        if (!receivers || receivers.length === 0) {
            return;
        }
        for (const connection of receivers) {
            if (!connection.signal) {
                continue;
            }
            let receiver = connection.thisArg || connection.slot;
            connection.signal = null;
            scheduleCleanup(sendersForReceiver.get(receiver));
        }
        scheduleCleanup(receivers);
    }
    Private.disconnectSender = disconnectSender;
    function disconnectReceiver(receiver) {
        let senders = sendersForReceiver.get(receiver);
        if (!senders || senders.length === 0) {
            return;
        }
        for (const connection of senders) {
            if (!connection.signal) {
                continue;
            }
            let sender = connection.signal.sender;
            connection.signal = null;
            scheduleCleanup(receiversForSender.get(sender));
        }
        scheduleCleanup(senders);
    }
    Private.disconnectReceiver = disconnectReceiver;
    function disconnectAll(object) {
        disconnectSender(object);
        disconnectReceiver(object);
    }
    Private.disconnectAll = disconnectAll;
    function emit(signal, args) {
        let receivers = receiversForSender.get(signal.sender);
        if (!receivers || receivers.length === 0) {
            return;
        }
        for (let i = 0, n = receivers.length; i < n; ++i) {
            let connection = receivers[i];
            if (connection.signal === signal) {
                invokeSlot(connection, args);
            }
        }
    }
    Private.emit = emit;
    const dirtySet = new Set();
    const sendersForReceiver = new WeakMap();
    const receiversForSender = new WeakMap();
    const schedule = (() => {
        let ok = typeof requestAnimationFrame === 'function';
        return ok ? requestAnimationFrame : (cb) => setTimeout(cb, 0);
    })();
    function invokeSlot(connection, args) {
        let { signal, slot, thisArg } = connection;
        try {
            slot.call(thisArg, signal.sender, args);
        }
        catch (err) {
            exceptionHandler(new Error(String(err)));
        }
    }
    function findConnection(connections, signal, slot, thisArg) {
        return find(connections, (connection) => connection.signal === signal &&
            connection.slot === slot &&
            connection.thisArg === thisArg);
    }
    function cleanupDirtySet() {
        dirtySet.forEach(cleanupConnections);
        dirtySet.clear();
    }
    function scheduleCleanup(array) {
        if (dirtySet.size === 0) {
            schedule(cleanupDirtySet);
        }
        dirtySet.add(array);
    }
    function isDeadConnection(connection) {
        return connection.signal === null;
    }
    function cleanupConnections(connections) {
        ArrayExt.removeAllWhere(connections, isDeadConnection);
    }
})(Private || (Private = {}));
//# sourceMappingURL=signaling.js.map