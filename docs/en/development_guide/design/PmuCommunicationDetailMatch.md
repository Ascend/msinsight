# Matching PMU and Communication Detail Timestamps in the DB Scenario

## Core Data Invariants

```text
globalTaskId + TASK time range [startNs, endNs]
```

uniquely determine a TASK and one `COMMUNICATION_TASK_INFO` / `TASK_PMU_INFO` data set.

Specifically:

- Different TASK invocations of the same `globalTaskId` do not overlap in their time ranges [startNs, endNs].
- The `timestampNs` of each `COMMUNICATION_TASK_INFO` record falls within and only within the [startNs, endNs] of one TASK.
- A TASK corresponds to 0 or 1 `COMMUNICATION_TASK_INFO` record.
- One timestamp can correspond to multiple `TASK_PMU_INFO` metric rows (different names of the same metric group). These rows form the complete PMU metric group of the current TASK.

Therefore, using `globalTaskId` + the closed interval `[TASK.startNs, TASK.endNs]` as the matching key makes the exact queries of Communication and PMU unique and deterministic.

## Background

In the DB scenario, multiple Device TASKs may reuse the same `globalTaskId`. Therefore, `globalTaskId` is only used to divide the candidate set,
and cannot by itself determine which TASK invocation a detail belongs to. Both `COMMUNICATION_TASK_INFO` and `TASK_PMU_INFO` add only one
`timestampNs` field; the data model does not introduce a second time field.

## Communication Matching Model

New tables with `timestampNs` are paired by invocation sequence number, and a `TASK.rowid -> COMMUNICATION_TASK_INFO` mapping is constructed:

- TASKs in the new table are sorted by `(startNs, ROWID)`, communication rows are sorted by `(timestampNs, ROWID)`, and the nth row is paired with the nth TASK.
- After pairing in the new table, `TASK.startNs <= timestampNs <= TASK.endNs` must also be verified, with both ends of the interval closed; a verification failure does not change the pairing sequence number.
- The old table does not infer the correspondence by ROWID. A single detail restores the original logic and uses the first result of a query with the same `globalTaskId`.
- A single slice detail uses `TASK.rowid` to look up the exact mapping instead of linearly scanning candidate rows for each TASK.
- Batch slice names read only `taskType`. The service guarantees that all Communication details under the same `globalTaskId` have the same
  `taskType`, so the original `globalTaskId -> taskType` hash map is kept and no sequence pairing is performed.
- A Plane simple slice does not read Communication details. It only obtains the `globalTaskId` set by the target `groupName/planeId` and then queries
  the corresponding TASK. This path keeps the original logic and performs no sequence pairing; the service must guarantee that repeated identical `globalTaskId` values do not cross different planes.
- bandwidth directly takes the paired detail row instead of querying separately, ensuring that the detail fields come from the same communication invocation.
- When pairing fails in the old or new table, detail fields use the first fallback result. Only when multiple candidate details are queried is
  `_ambiguousKeys` (those field names) written into args, so that the frontend can indicate on the corresponding fields that the value may not belong to the current TASK; a single candidate means no ambiguity.

## PMU Matching Model

The code is organized in fixed steps for readability:

1. The new table directly queries PMU with the `globalTaskId` and `[startNs, endNs]` of the passed-in TASK.
2. The old table directly calls `QueryAllPmuInfo`, restoring the original full globalTaskId query.
3. When the exact query of the new table is empty, `QueryAllPmuInfo` is also called as a fallback.
4. `AppendPmuInfoToArgs` writes the detail JSON uniformly; when multiple candidate values exist for a metric with the same name, that metric name is marked with `_ambiguousKeys`.

### New Table

The target TASK passed in already contains the complete matching condition, so it is queried directly:

```text
the globalTaskId is the same, and TASK.startNs <= PMU.timestampNs <= TASK.endNs
```

The query result is the complete PMU metrics of that TASK. When the result is empty, the original globalTaskId query is restored to avoid blank details;
in the fallback result, the metric name is listed in `_ambiguousKeys` only when a metric with the same name appears multiple times.

### Old Table

Old PMU has no timestamp and cannot reliably confirm which TASK invocation a detail belongs to, so ROWID is not used to infer the correspondence. The original logic is kept,
returning all PMU records with the same `globalTaskId`; the metric name is listed in `_ambiguousKeys` only when a metric with the same name appears multiple times.

## Field Uncertainty Marker

Keys prefixed with `_` in `args` are internal markers and are not rendered as parameter rows by the frontend. Currently there is only `_ambiguousKeys`: its value is an array of
uncertain field names collected by the backend according to the field names actually written when writing the detail JSON, so the frontend and backend do not need to maintain a static field list.

## Query Boundary

The entry point uses `CheckColumnExist(table, "timestampNs")` to determine the data capability:

- Pure query functions differ only slightly in SELECT columns and sort fields, so they are merged into one helper with a `withTimestamp` parameter;
  when the parameter is false, the `timestampNs` that does not exist in the old table is not referenced.
- The new PMU table queries directly by time range; the old table queries all metrics only by globalTaskId.

When support for old data is stopped, the branch without a timestamp in the entry point can be deleted directly.

All queries that require exact pairing explicitly select and sort `ROWID`. A single detail query reads all TASKs and details of the same `globalTaskId` to
avoid querying only the target TASK and mistakenly treating it as the first occurrence.

## Complexity

Let the number of TASKs be T, the number of Communication rows be C, and the number of PMU rows be P:

- Communication bucketing and mapping construction are `O(T + C)`, sorting each bucket is `O(T log T + C log C)` in total, and consuming the mapping is `O(1)`.
- The new PMU is filtered directly by SQLite by globalTaskId and the closed time interval, without constructing timestamp groups or sorting TASKs in C++.
- The old PMU is a single globalTaskId query without TASK/PMU pairing or C++ sorting.

## Exception Handling

- A warning is recorded when the number of Communication TASKs and details is inconsistent or the sequence pairing time verification fails. The new table then recovers from
  the unused details by the closed TASK time interval and builds a mapping only when exactly one row matches; zero or multiple matches build no mapping. The old table performs no
  sequence matching, and a single detail uses the first result of the query with the same `globalTaskId`.
- When the exact query of the new PMU is empty, the original globalTaskId query is restored; when multiple candidate values exist for a metric with the same name, `_ambiguousKeys` marks it.
- The old PMU directly returns all metrics with the same `globalTaskId`; when multiple candidate values exist for a metric with the same name, `_ambiguousKeys` marks it.
- The table handler keeps the default value 0 when `timestampNs` is not selected in the query, and the old table SQL does not reference that column.
