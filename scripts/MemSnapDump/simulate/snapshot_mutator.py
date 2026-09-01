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

import bisect

from base import Block, BlockState, DeviceSnapshot, Segment
from util import get_logger

snapshot_mutator_logger = get_logger("ALLOCATOR")


def _segment_sort_key(segment: Segment) -> tuple[int, int]:
    return segment.address, segment.stream


# Public API: block mutation helpers


def attach_block(snapshot: DeviceSnapshot, segment: Segment, block: Block, insert_idx: int):
    """Attach a block to a segment and update allocation totals.

    Mutates the block, the owning segment, and snapshot totals in place.
    The block is inserted into `segment.blocks` at `insert_idx`.
    """
    block.segment_ptr = segment
    segment.blocks.insert(insert_idx, block)
    segment.active_size += block.size
    snapshot.total_activated += block.size
    if block.state == BlockState.ACTIVE_ALLOCATED:
        segment.allocated_size += block.size
        snapshot.total_allocated += block.size


def detach_block(snapshot: DeviceSnapshot, block: Block, block_idx: int | None = None) -> bool:
    """Detach a block from its owning segment.

    Mutates the block, its segment, and snapshot totals in place.
    Returns `True` on success, otherwise `False` when the block has no segment.
    ``block_idx`` can be supplied by callers that already searched for the block
    to avoid a second linear lookup.
    """
    segment = block.segment_ptr
    if segment is None:
        return False
    segment.active_size -= block.size
    snapshot.total_activated -= block.size
    if block.state == BlockState.ACTIVE_ALLOCATED:
        segment.allocated_size -= block.size
        snapshot.total_allocated -= block.size
    if block_idx is None:
        block_idx = segment.blocks.index(block)
    del segment.blocks[block_idx]
    block.segment_ptr = None
    return True


def promote_pending_free_block(snapshot: DeviceSnapshot, block: Block) -> bool:
    """Promote a pending-free block back to allocated state.

    Mutates the block state plus the owning segment and snapshot allocation
    totals. Returns `False` when the block has no owning segment.
    """
    segment = block.segment_ptr
    if segment is None:
        return False
    block.state = BlockState.ACTIVE_ALLOCATED
    segment.allocated_size += block.size
    snapshot.total_allocated += block.size
    return True


# Public API: segment collection mutation


def insert_segment(snapshot: DeviceSnapshot, segment: Segment):
    """Insert a segment into the snapshot and increase reserved memory.

    Preserves segment ordering and mutates `snapshot.total_reserved` in place.
    """
    _insert_segment_sorted(snapshot, segment)
    snapshot.total_reserved += segment.total_size


def remove_segment(snapshot: DeviceSnapshot, segment: Segment, segment_idx: int | None = None):
    """Remove a segment from the snapshot and decrease reserved memory.

    Clears every block's `segment_ptr` and mutates `snapshot.total_reserved`
    in place.
    """
    snapshot.total_reserved -= segment.total_size
    if segment_idx is None:
        segment_idx = snapshot.segments.index(segment)
    del snapshot.segments[segment_idx]
    for block in segment.blocks:
        block.segment_ptr = None


# Public API: segment shape mutation


def merge_mapped_segment(
    snapshot: DeviceSnapshot,
    new_segment: Segment,
    left_adjacent_idx: int,
    right_adjacent_idx: int,
) -> bool:
    """Merge a newly mapped segment with adjacent segments.

    Supports left-only, right-only, or both-side adjacency. Mutates
    `snapshot.segments` in place and returns `False` when neither adjacent
    index is valid.
    """
    _error = "Failed to merge mapped segment"
    if left_adjacent_idx == -1 and right_adjacent_idx == -1:
        snapshot_mutator_logger.error("%s: both adjacent segment indices are missing", _error)
        return False

    segments = snapshot.segments
    if left_adjacent_idx != -1:
        left_seg = segments[left_adjacent_idx]
        left_seg.total_size += new_segment.total_size
        left_seg.allocated_size += new_segment.allocated_size
        left_seg.active_size += new_segment.active_size
        for block in new_segment.blocks:
            block.segment_ptr = left_seg
            left_seg.blocks.append(block)
        if right_adjacent_idx != -1:
            return _merge_segments(snapshot, left_adjacent_idx, right_adjacent_idx)
        return True

    new_idx = _insert_segment_sorted(snapshot, new_segment)
    corrected_right_idx = right_adjacent_idx + (new_idx <= right_adjacent_idx)
    if corrected_right_idx >= len(segments):
        snapshot_mutator_logger.error("%s: right adjacent segment missing after insert", _error)
        segments.remove(new_segment)
        return False
    if corrected_right_idx != right_adjacent_idx + 1:
        right_segment = segments[corrected_right_idx]
        if right_segment.address != new_segment.address + new_segment.total_size:
            snapshot_mutator_logger.error("%s: inserted segment is not adjacent to the expected right segment", _error)
            segments.remove(new_segment)
            return False
    if not _merge_segments(snapshot, new_idx, corrected_right_idx):
        segments.remove(new_segment)
        return False
    return True


def split_or_shrink_segment(snapshot: DeviceSnapshot, seg_idx: int, seg_addr: int, unmap_size: int) -> bool:
    """Shrink or split a segment to remove an unmapped range.

    Chooses the appropriate internal mutation strategy based on whether the
    unmapped range touches the segment edges or lies in the middle.
    Returns `True` on success and `False` when the mutation is invalid.
    """
    exist_seg = snapshot.segments[seg_idx]
    seg_start = exist_seg.address
    unmap_end = seg_addr + unmap_size
    if seg_addr == seg_start:
        return _shrink_segment(snapshot, seg_idx, seg_addr, unmap_size, "left")
    if unmap_end == seg_start + exist_seg.total_size:
        return _shrink_segment(snapshot, seg_idx, seg_addr, unmap_size, "right")
    return _split_segment_at(snapshot, seg_idx, seg_addr, unmap_size)


# Public API: snapshot total mutation


def increase_reserved(snapshot: DeviceSnapshot, size: int):
    """Increase the snapshot reserved-memory total by `size`."""
    snapshot.total_reserved += size


def decrease_reserved(snapshot: DeviceSnapshot, size: int):
    """Decrease the snapshot reserved-memory total by `size`."""
    snapshot.total_reserved -= size


# Private helpers


def _insert_segment_sorted(snapshot: DeviceSnapshot, segment: Segment) -> int:
    """Insert a segment into `snapshot.segments` while preserving sort order."""
    segments = snapshot.segments
    idx = bisect.bisect_left(
        segments,
        (segment.address, segment.stream),
        key=_segment_sort_key,
    )
    segments.insert(idx, segment)
    return idx


def _merge_segments(snapshot: DeviceSnapshot, target_idx: int, source_idx: int) -> bool:
    """Merge two adjacent segments with the same stream.

    Mutates `snapshot.segments` in place. Returns `True` on success, otherwise
    `False` when indices, adjacency, or stream constraints are not satisfied.
    """
    _error = "Failed to merge segments"
    segments = snapshot.segments
    if target_idx < 0 or target_idx >= len(segments):
        snapshot_mutator_logger.error("%s: invalid target segment index %s", _error, target_idx)
        return False
    if source_idx < 0 or source_idx >= len(segments):
        snapshot_mutator_logger.error("%s: invalid source segment index %s", _error, source_idx)
        return False
    if target_idx == source_idx:
        snapshot_mutator_logger.error("%s: target and source are the same segment", _error)
        return False
    target = segments[target_idx]
    source = segments[source_idx]
    if target.stream != source.stream:
        snapshot_mutator_logger.error(
            "%s: segments have different streams (target: %s, source: %s)",
            _error,
            target.stream,
            source.stream,
        )
        return False
    are_adjacent = (
        target.address + target.total_size == source.address or source.address + source.total_size == target.address
    )
    if not are_adjacent:
        snapshot_mutator_logger.error(
            "%s: segments are not adjacent (target: [%s, %s), source: [%s, %s))",
            _error,
            target.address,
            target.address + target.total_size,
            source.address,
            source.address + source.total_size,
        )
        return False
    if target.address > source.address:
        target, source = source, target
        target_idx, source_idx = source_idx, target_idx
    target.total_size += source.total_size
    target.allocated_size += source.allocated_size
    target.active_size += source.active_size
    for block in source.blocks:
        block.segment_ptr = target
        target.blocks.append(block)
    del segments[source_idx]
    return True


def _split_segment_at(snapshot: DeviceSnapshot, seg_idx: int, cut_addr: int, cut_size: int) -> bool:
    """Split a segment around a cut range and keep non-overlapping blocks.

    Mutates `snapshot.segments` in place. Returns `True` on success, otherwise
    `False` when the cut range is invalid.
    """
    _error = "Failed to split segment"
    segments = snapshot.segments
    if seg_idx < 0 or seg_idx >= len(segments):
        snapshot_mutator_logger.error("%s: invalid segment index %s", _error, seg_idx)
        return False
    original_segment = segments[seg_idx]
    seg_start = original_segment.address
    seg_end = seg_start + original_segment.total_size
    cut_end = cut_addr + cut_size
    if cut_addr < seg_start or cut_end > seg_end:
        snapshot_mutator_logger.error(
            "%s: cut range [%s, %s) is outside segment [%s, %s)",
            _error,
            cut_addr,
            cut_end,
            seg_start,
            seg_end,
        )
        return False
    if cut_addr == seg_start and cut_end == seg_end:
        snapshot_mutator_logger.warning("Split Seg: cut range covers entire segment, nothing to split, just remove it")
        del snapshot.segments[seg_idx]
        return True
    left_segment = Segment(
        address=seg_start,
        total_size=cut_addr - seg_start,
        stream=original_segment.stream,
        segment_type=original_segment.segment_type,
        allocated_size=0,
        active_size=0,
        blocks=[],
        device=original_segment.device,
        frames=original_segment.frames,
        _raw_frames=original_segment._raw_frames,
        is_expandable=original_segment.is_expandable,
        free_or_unmap_event_idx=original_segment.free_or_unmap_event_idx,
        alloc_or_map_event_idx=original_segment.alloc_or_map_event_idx,
    )
    right_segment = Segment(
        address=cut_end,
        total_size=seg_end - cut_end,
        stream=original_segment.stream,
        segment_type=original_segment.segment_type,
        allocated_size=0,
        active_size=0,
        blocks=[],
        device=original_segment.device,
        frames=original_segment.frames,
        _raw_frames=original_segment._raw_frames,
        is_expandable=original_segment.is_expandable,
        free_or_unmap_event_idx=original_segment.free_or_unmap_event_idx,
        alloc_or_map_event_idx=original_segment.alloc_or_map_event_idx,
    )
    for block in original_segment.blocks:
        block_start = block.address
        block_end = block_start + block.size
        if block_end <= cut_addr:
            block.segment_ptr = left_segment
            left_segment.blocks.append(block)
            left_segment.active_size += block.size
            if block.state == BlockState.ACTIVE_ALLOCATED:
                left_segment.allocated_size += block.size
        elif block_start >= cut_end:
            block.segment_ptr = right_segment
            right_segment.blocks.append(block)
            right_segment.active_size += block.size
            if block.state == BlockState.ACTIVE_ALLOCATED:
                right_segment.allocated_size += block.size
        else:
            snapshot_mutator_logger.warning(
                "%s: active block [%s, %s) overlaps with cut range [%s, %s), just drop it.",
                _error,
                block_start,
                block_end,
                cut_addr,
                cut_end,
            )
    del segments[seg_idx]
    if left_segment.total_size > 0:
        _insert_segment_sorted(snapshot, left_segment)
    if right_segment.total_size > 0:
        _insert_segment_sorted(snapshot, right_segment)
    return True


def _shrink_segment(
    snapshot: DeviceSnapshot,
    seg_idx: int,
    shrink_addr: int,
    shrink_size: int,
    direction: str,
) -> bool:
    """Shrink a segment from the left or right side.

    Mutates the target segment in place and recomputes its size counters.
    Returns `True` on success, otherwise `False` when the shrink is invalid.
    """
    _error = "Failed to shrink segment"
    segments = snapshot.segments
    if seg_idx < 0 or seg_idx >= len(segments):
        snapshot_mutator_logger.error("%s: invalid segment index %s", _error, seg_idx)
        return False
    if direction not in ["left", "right"]:
        snapshot_mutator_logger.error("%s: invalid direction '%s', must be 'left' or 'right'", _error, direction)
        return False
    segment = segments[seg_idx]
    repositioned = False
    seg_start = segment.address
    seg_end = seg_start + segment.total_size
    shrink_end = shrink_addr + shrink_size
    if direction == "left":
        if shrink_addr < seg_start or shrink_end > seg_end:
            snapshot_mutator_logger.error(
                "%s: shrink range [%s, %s) is outside segment [%s, %s)",
                _error,
                shrink_addr,
                shrink_end,
                seg_start,
                seg_end,
            )
            return False
        new_start = shrink_end
        new_size = seg_end - new_start
        if new_size < 0:
            snapshot_mutator_logger.error("%s: shrink results in negative segment size", _error)
            return False
        for block in segment.blocks:
            block_start = block.address
            block_end = block_start + block.size
            if block_end <= shrink_end:
                snapshot_mutator_logger.error(
                    "%s: active block [%s, %s) in shrink range [%s, %s)",
                    _error,
                    block_start,
                    block_end,
                    shrink_addr,
                    shrink_end,
                )
                return False
        del segments[seg_idx]
        repositioned = True
        segment.address = new_start
        segment.total_size = new_size
        segment.blocks = [block for block in segment.blocks if block.address >= new_start]
    else:
        if shrink_addr < seg_start or shrink_end > seg_end:
            snapshot_mutator_logger.error(
                "%s: shrink range [%s, %s) is outside segment [%s, %s)",
                _error,
                shrink_addr,
                shrink_end,
                seg_start,
                seg_end,
            )
            return False
        new_size = shrink_addr - seg_start
        if new_size < 0:
            snapshot_mutator_logger.error("%s: shrink results in negative segment size", _error)
            return False
        for block in segment.blocks:
            block_start = block.address
            block_end = block_start + block.size
            if block_start >= shrink_addr:
                snapshot_mutator_logger.error(
                    "%s: active block [%s, %s) in shrink range [%s, %s)",
                    _error,
                    block_start,
                    block_end,
                    shrink_addr,
                    shrink_end,
                )
                return False
        segment.blocks = [block for block in segment.blocks if block.address + block.size <= shrink_addr]
        segment.total_size = new_size
    segment.allocated_size = sum(b.size for b in segment.blocks if b.state == BlockState.ACTIVE_ALLOCATED)
    segment.active_size = sum(b.size for b in segment.blocks)
    if segment.total_size == 0:
        if not repositioned:
            del segments[seg_idx]
    elif repositioned:
        _insert_segment_sorted(snapshot, segment)
    return True
