# Case Study on Analyzing Memory Issues Based on PyTorch Snapshot Data

<!-- md-trans-meta sourceCommit=cef2f118d66b4336ea74f9576a816aafd4de8d01 translatedAt=2026-08-12T11:36:47.870Z pushedAt=2026-08-12T11:57:31.052Z -->

## Background

In training scenarios, a model may run normally under a small batch size, but an OOM error suddenly occurs when the batch size is increased to a certain threshold. Such issues are often more complex than simply "insufficient video memory." During actual analysis, in addition to monitoring the overall peak video memory usage, it is also necessary to examine profile data to determine whether there are abnormal operator workspace allocations, shape amplifications, or temporary memory at a certain stage being multiplied several times over.

This case uses an issue where OOM is triggered after the batch size is increased from 2 to 3 as an example, and describes how to first observe the video memory usage trend through profile data, and then pinpoint the specific root cause by combining information about abnormal operators and call stacks.

## Analysis Methodology

For this type of "OOM after increasing batch size" issue, you are advised to analyze in the following order:

1. **Examine the overall peak video memory**: Determine whether the issue is caused by an excessively high single peak or by continuous accumulation leading to OOM.

2. **Examine the static video memory usage**: Distinguish among base usage, PTA statistical usage, and temporary operator allocations to determine whether there is any obvious exception.

3. **Focus on the difference between Reserved and Allocated**: If there is a large gap between the two, further investigate workspace, fragmentation, or unaccounted component usage.

4. **Return to the exception operator and timeline**: Locate the specific operator and its corresponding shape through the large memory allocation events in the profiler.

5. **Confirm the root cause in combination with the source code**: Finally, examine the shape changes and type conversion order before and after the operator to determine whether unnecessary large memory amplification exists.

This type of issue is usually not that "the model is too large," but rather that a certain intermediate tensor is amplified under a specific order, causing the workspace to increase exponentially.

## Problem Symptoms

In a certain training scenario, training proceeded normally at `bs=2`, but an OOM occurred when the batch size was increased to `bs=3`. Based on the model scale and historical experience, this configuration should theoretically be able to accommodate `bs=3`, so further analysis is required to determine whether there is any abnormal video memory allocation.

The OOM log is as follows:

```text
File "<path-to-py-file>/transformer.py", line xx, in forward
    attn_mask = attn_mask.bool()
RuntimeError: NPU out of memory. Tried to allocate 48.78 GiB (NPU 0; 60.96 GiB total capacity; 13.22 GiB already allocated; 13.22 GiB current active; 40.54 GiB free; 54.86 GiB allowed; 13.86 GiB reserved in total by PyTorch) If reserved memory is >> allocated memory try setting max_split_size_mb to avoid fragmentation.
```

Based on the error information, the OOM occurred during a type conversion operation such as `attn_mask.bool()`, and the allocation reached 48 GiB, which is clearly abnormal.

## Information Collection

Before proceeding with the analysis, two key pieces of information need to be supplemented:

1. The training task should have `task_queue_enable=2` enabled.

2. It is necessary to compare the profile data under the two scenarios of `bs=2` and `bs=3`.

The reason for paying special attention to `task_queue_enable=2` is that in this mode, the PTA reserved/allocated statistics in profiling may not include the workspace, and it is necessary to further confirm whether an independent workspace allocation exists by combining other views.

## Profile Data Analysis

### `bs=1` and `bs=2` Baselines

Starting with the profile data from smaller batch sizes helps confirm that the issue is not caused by continuous growth in the model's baseline usage.

When `bs=1`, the observed peak allocated memory in profiling is approximately 21 GB, of which the static video memory usage accounts for about 11 GB.

When `bs=2`, the static video memory usage remains at approximately 11 GB, but the peak allocated memory rises to about 26 GB. This result indicates that the model's baseline usage shows no obvious anomaly, yet the peak video memory begins to increase rapidly as the batch size grows.

At this point, another noteworthy phenomenon can be observed in the profiling chart: there is a significant gap between `APP Reserved` and `Operator Reserved`, with the peak approaching 20 GB or more. This typically suggests that, in addition to known operator allocations, there are other exceptional usage sources that require further investigation.

### Characteristics of OOM at `bs=3`

When the batch size is increased to `bs=3`, training immediately encounters OOM, and the error occurs at cast operations such as `attn_mask.bool()`. Since this location is neither a large model parameter loading path nor a long-term cache path, but rather an intermediate tensor type conversion point, it is more likely a temporary workspace allocation failure.

By combining the profiling results for `bs=2` with the OOM log for `bs=3`, a preliminary assessment can be made:

- The issue does not stem from model weights or static video memory exhaustion;

- Nor does it appear to be caused by CANN components reserving excessive memory;

- It is more likely that a particular operator requests an excessively large workspace under certain shape configurations.

### Non-PTA Occupancy

To confirm whether the exception occupancy originates from components outside PTA, continue to examine `npu_module_mem`. The results show that CANN components occupy less than 4 GB, which is not exceptional overall.

Next, examining the operator allocation table also reveals no significant large allocation that is clearly "not counted in PTA." Therefore, the scope can be further narrowed: the issue is more likely to lie in the temporary allocation of a certain operator, rather than long-term occupancy by external components.

### Workspace Statistics

There is another key clue here: when `task_queue_enable=2`, the PTA reserved/allocated values in profiling may not cover all workspace behaviors, and a separate workspace statistics item should exist. If no significant workspace usage is visible in the chart, you need to suspect that a very large temporary allocation occurred inside a certain operator but is not intuitively reflected in the regular view.

The significance of this step is: do not focus solely on the overall reserved/allocated counts; instead, combine specific operators and call stacks to identify "who is allocating this memory."

### Abnormal Allocations Identification by Operator Sorting and Timeline

To further confirm the issue, first set `task_queue_enable` to `1` and re-collect profile data for `bs=2`. This allows for a clearer observation of individual operator allocations and peak spikes.

In the zoomed-in profiling chart, after sorting operators by allocation size in descending order, an abnormal `cast` operator can be identified, with its total memory allocation reaching 32.5 GB. Then, by navigating to the timeline to inspect the corresponding shape, it can be observed that the allocation amount is highly correlated with the input shape.

According to the calculation formula:

```text
2 * 24 * 9536 * 9536 * 8 / 1024 / 1024 / 1024 = 32.5G
```

When the batch size is increased to 3, this value further rises to approximately 48 GB, which is largely consistent with the allocation amount recorded in the OOM log.

This indicates that the issue is not a random fluctuation, but rather that the shape of an intermediate tensor had already been amplified to an extremely large scale before the cast operation.

## Analysis Conclusion

By combining the profile data and OOM logs, the following conclusion can be drawn: during the forward pass, the `attention mask` first underwent `expand`, which enlarged its shape to a very large dimension, and then the `bool()` conversion was performed, causing the cast stage to request an enormous workspace. As the batch size increased, this workspace expanded linearly with the shape, eventually triggering an OOM at `bs=3`.

In other words, the root cause of the problem is not that the model parameters are too large, but rather that the **intermediate tensor was amplified in the wrong order**, turning what should have been a lightweight type conversion operation into a massive memory allocation.

## Fix Suggestion

The fix suggestion is straightforward: adjust the operation order by executing `bool()` first, then `expand()`.

Original logic:

```python
attn_mask = attn_mask.expand(batch_size, self.num_attention_heads, total_seq_len, -1)
attn_mask = attn_mask.bool()
```

Suggested adjustments:

```python
attn_mask = attn_mask.bool()
attn_mask = attn_mask.expand(batch_size, self.num_attention_heads, total_seq_len, -1)
```

This avoids performing a cast on an oversized shape, thereby significantly reducing the workspace usage.

## Summary

The key to locating such issues lies in the following approach: first, identify peak anomalies through profiling; then, combine operator ordering and the timeline to pinpoint where large memory allocations occur; finally, return to the source code to examine the order of shape expansion. For scenarios where OOM occurs suddenly after increasing the batch size, you are advised to first check whether intermediate tensors are expanded before operations such as type casting, reshape, expand, or broadcast, which often leads to faster root cause identification.
