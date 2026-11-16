# **Comparison Between MemScope Data Collection and PyTorch Snapshot Data Collection**

**Data Source Selection Guide**

| Data Source | Applicable Scenarios | Advantages | Limitations |
| --- | --- | --- | --- |
| msMemScope | Analyze all memory events (including memory outside the framework), detect memory leaks, and perform memory breakdown analysis. | Complete data covering all memory events; supports Memory Block Timeline Graph, Memory Details Disassembly Diagram, and Call Stack Flame Graph. | Collection has a certain impact on runtime performance. |
| PyTorch Snapshot | Only analyze the memory pool managed by the PyTorch framework, perform memory fragmentation analysis, and optimize memory peaks. | Low collection performance overhead; memory pool status data included | Covering only memory managed by the PyTorch framework; large-scale data analysis depends on visualization tools. |

- Compared with the memory tuning data collected by MemScope, memory snapshot data has the following advantages:

  1. Collection performance overhead: During the collection of memory snapshot data, memory events are recorded at the host level only after `record_memory_history` is enabled, and the PTA allocator segments are saved at the dump time. This has little impact on the model running performance.
  2. Tuning data size: The core data consists of PTA memory events and segments at the collection end time. The data density is high, and the data size is smaller than that collected by MemScope.
  3. Special memory pool status data: Compared with the memory tuning data collected by MemScope, the memory snapshot data also includes the status data of the PTA memory pool, such as the memory pool size and memory pool usage. This data helps users comprehensively analyze the memory usage. Especially in the case of memory fragmentation, the memory pool status data helps users intuitively analyze the memory fragmentation.

- Compared with the memory tuning data collected by MemScope, memory snapshot data has the following disadvantages:

  1. Tuning data analysis: Memory snapshot data is stored in Python pickle data files, which are difficult to use. The analysis relies heavily on visualization capabilities. To parse and analyze raw data, you need to have a basic understanding of memory events and data collection principles.
  2. Limited capabilities/performance of community visualization tools: Although community-based online visualization enables analysis of memory snapshot data, notable deficiencies remain in stability, performance, and interactivity.
      - Stability: Online web pages may be unavailable due to client environment issues (such as restricted to an intranet) or network problems (such as CDN resource exceptions).
      - Performance: When processing large-scale memory snapshots (with more than 15,000 events and a snapshot size exceeding 10 MB), the online visualization web page may experience performance issues, such as long loading times and delayed interaction responses.
      - Interactivity: During the analysis process on the online visualization web page, the only way to associate the Memory Block Timeline Graph with the memory pool status graph is to manually copy the memory address and search for it on the web page. Additionally, it is not possible to reverse the association from the memory pool status graph to the Memory Block Timeline Graph.
