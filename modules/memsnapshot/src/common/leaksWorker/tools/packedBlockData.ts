/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

export const isPackedRenderData = (data: RenderData | PackedRenderData): data is PackedRenderData =>
    'ids' in data && data.ids instanceof Float64Array;

export const packRenderData = (data: RenderData): PackedRenderData => {
    const blocks = data.blocks ?? [];
    const blockCount = blocks.length;
    const ids = new Float64Array(blockCount);
    const startTimestamps = new Float64Array(blockCount);
    const endTimestamps = new Float64Array(blockCount);
    const sizes = new Float64Array(blockCount);
    const addressIndices = new Uint32Array(blockCount);
    const addresses: string[] = [];
    const addressIndexByValue = new Map<string, number>();

    for (let i = 0; i < blockCount; i++) {
        const block = blocks[i];
        ids[i] = block.id;
        startTimestamps[i] = block._startTimestamp;
        endTimestamps[i] = block._endTimestamp;
        sizes[i] = block.size;
        let addressIndex = addressIndexByValue.get(block.addr);
        if (addressIndex === undefined) {
            addressIndex = addresses.length;
            addresses.push(block.addr);
            addressIndexByValue.set(block.addr, addressIndex);
        }
        addressIndices[i] = addressIndex;
    }

    const transferBytes = ids.byteLength + startTimestamps.byteLength + endTimestamps.byteLength +
        sizes.byteLength + addressIndices.byteLength;
    return {
        maxTimestamp: data.maxTimestamp,
        minTimestamp: data.minTimestamp,
        maxSize: data.maxSize,
        minSize: data.minSize,
        ids,
        startTimestamps,
        endTimestamps,
        sizes,
        addressIndices,
        addresses,
        reservedLine: data.reservedLine,
        reservedSizeMax: data.reservedSizeMax,
        transferBytes,
    };
};

export const getPackedRenderDataTransferList = (data: PackedRenderData): Transferable[] => [
    data.ids.buffer,
    data.startTimestamps.buffer,
    data.endTimestamps.buffer,
    data.sizes.buffer,
    data.addressIndices.buffer,
];

export const unpackRenderData = (data: PackedRenderData): RenderData => {
    const blocks = new Array<Block>(data.ids.length);
    for (let i = 0; i < data.ids.length; i++) {
        blocks[i] = {
            id: data.ids[i],
            addr: data.addresses[data.addressIndices[i]] ?? '',
            _startTimestamp: data.startTimestamps[i],
            _endTimestamp: data.endTimestamps[i],
            size: data.sizes[i],
            path: [],
        };
    }
    return {
        maxTimestamp: data.maxTimestamp,
        minTimestamp: data.minTimestamp,
        maxSize: data.maxSize,
        minSize: data.minSize,
        blocks,
        reservedLine: data.reservedLine,
        reservedSizeMax: data.reservedSizeMax,
    };
};
