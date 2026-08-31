"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

    http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

from base import Block, DeviceSnapshot, Segment


def is_valid_sub_block(block: Block, addr: int, size: int) -> bool:
    """Return whether the given range is fully contained in a block."""
    return block.address <= addr and addr + size <= block.address + block.size


def find_block(segment: Segment, block_addr: int) -> tuple[int, Block | None]:
    """Find the block containing an address inside a segment.

    Returns `(idx, block)` when matched, otherwise `(-1, None)`.
    """
    left = 0
    right = len(segment.blocks) - 1
    while left <= right:
        mid = (left + right) // 2
        if block_addr < segment.blocks[mid].address:
            right = mid - 1
        elif block_addr >= segment.blocks[mid].address + segment.blocks[mid].size:
            left = mid + 1
        else:
            return mid, segment.blocks[mid]
    return -1, None


def _scan_overlapping_segments_for_stream(
    segments: list[Segment], mid: int, addr: int, stream: int
) -> tuple[int, Segment | None]:
    """Scan neighboring overlapping segments to find a stream match."""
    for i in range(mid - 1, -1, -1):
        if addr < segments[i].address:
            break
        if addr < segments[i].address + segments[i].total_size and segments[i].stream == stream:
            return i, segments[i]
    for i in range(mid + 1, len(segments)):
        if addr < segments[i].address:
            break
        if addr < segments[i].address + segments[i].total_size and segments[i].stream == stream:
            return i, segments[i]
    return -1, None


def find_overlapping_segment(
    snapshot: DeviceSnapshot, addr: int, stream: int | None = None
) -> tuple[int, Segment | None]:
    """Find the segment whose range overlaps the given address.

    Returns `(idx, segment)` for a containing-range match, otherwise
    `(-1, None)`. When `stream` is provided, the matched segment must also
    share that stream.
    """
    left = 0
    segments = snapshot.segments
    right = len(segments) - 1
    while left <= right:
        mid = (left + right) // 2
        if addr < segments[mid].address:
            right = mid - 1
        elif addr >= segments[mid].address + segments[mid].total_size:
            left = mid + 1
        else:
            if stream is not None and segments[mid].stream != stream:
                return _scan_overlapping_segments_for_stream(segments, mid, addr, stream)
            return mid, segments[mid]
    return -1, None


def find_segment(snapshot: DeviceSnapshot, addr: int, stream: int) -> tuple[int, Segment | None]:
    """Find the segment whose start address exactly matches the given address.

    Returns `(idx, segment)` for an exact start-address and stream match,
    otherwise `(-1, None)`.
    """
    seg_idx, seg = find_overlapping_segment(snapshot, addr, stream)
    if seg is not None and seg.address == addr and seg.stream == stream:
        return seg_idx, seg
    return -1, None


def find_gap_for_alloc_block(
    snapshot: DeviceSnapshot, event_addr: int, event_size: int, stream: int = None
) -> tuple[Segment, int] | None:
    """Find the insertion gap for a block allocation inside a segment.

    Returns `(segment, insert_idx)` when a valid gap exists, otherwise `None`.
    The returned index is the position where the new block should be inserted.
    """
    seg_idx, segment = find_overlapping_segment(snapshot, event_addr, stream)
    if seg_idx == -1 or segment is None:
        return None
    blocks = segment.blocks
    event_end = event_addr + event_size
    seg_start = segment.address
    seg_end = seg_start + segment.total_size
    if len(blocks) == 0:
        if seg_start <= event_addr and event_end <= seg_end:
            return segment, 0
        return None

    if blocks[0].address >= event_end:
        if seg_start <= event_addr:
            return segment, 0
        return None

    left, right = 0, len(blocks) - 1
    while left < right:
        mid = (left + right + 1) // 2
        if blocks[mid].address <= event_addr:
            left = mid
        else:
            right = mid - 1

    gap_start = blocks[left].address + blocks[left].size
    if left + 1 < len(blocks):
        gap_end = blocks[left + 1].address
        if gap_start <= event_addr and event_end <= gap_end:
            return segment, left + 1
    else:
        if gap_start <= event_addr and event_end <= seg_end:
            return segment, len(blocks)

    return None
