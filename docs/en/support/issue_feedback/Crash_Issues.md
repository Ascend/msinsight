# Crash Issues

<!-- md-trans-meta sourceCommit=81c78c7bc5b57be8952a8eb0685833246e436fe7 translatedAt=2026-08-12T11:44:53.431Z pushedAt=2026-08-12T11:57:31.130Z -->

## MindStudio Insight Crashes Immediately on Startup

### Problem Description

When opening MindStudio Insight, it crashes immediately. How can I resolve this issue?

![image](figures/Crash_Issues/webview2-install-error.png)

[Environment Information]

Tool version: MindStudio Insight 8.2.RC1

Operating system: Windows 11

### Solution

**Symptom**

After msInsight is installed, it crashes on startup and cannot be used normally. Attempting to install the dependency WebView2 Runtime also reports the error "already installed."

**Scoping  Progress**

1. When the crash on startup occurred, no `profiler_server_x_x.log` was generated, scoping the issue to an OS-related dependency problem at startup.

2. Communication with the user revealed that some files on the C drive had been cleaned, which may have accidentally deleted WebView2 Runtime or compromised dependency integrity.

[Workaround/Fix]

1. It is difficult to directly locate the missing dependencies and perform targeted repairs. It is recommended to try reinstalling MSVC and then reinstalling WebView2 Runtime.

2. If the environment cannot be repaired, the tool can be temporarily used through browser access (users need to ensure environment security on their own).

## MindStudio Insight Keeps Crashing on Startup, and Reinstallation Does Not Resolve the Issue

### Problem Description

MindStudio Insight crashes on startup continuously, and reinstallation fails to resolve the issue.

[Solution]

In a restricted network zone, contact the customer service 12345 to obtain a proxy for downloading the Edge WebView2 Runtime tool.

Download the startup package from the official Windows website.

### Problem Description

[Problem Analysis]

It was found that MindStudio Insight crashed immediately upon startup.

The user was using Windows 11. The .mindstudio_insight folder was missing from the installation directory, and the Edge browser was also found to be unable to open. It was initially suspected that the Edge browser was corrupted, preventing Insight from launching.

The Insight frontend depends on the Edge browser to launch. If the Edge browser is corrupted, Insight cannot start properly.
