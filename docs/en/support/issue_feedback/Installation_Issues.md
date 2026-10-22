# Installation Issues

## MindStudio Insight Tool Cannot Be Installed

### Problem Description

The MindStudio Insight tool versions 8.2 and 8.1 downloaded from the community cannot be installed properly.

Neither regular users nor administrator users can launch the installer by double-clicking.

### Solution

1. Check compatibility settings
Right-click the .exe file and select **Properties**.
Switch to the **Compatibility** tab.
Under **Privilege Level**, check **Run this program as an administrator**.
Click **Apply** and then **OK**.
2. Check system policies (advanced operation)
Press **Win + R**, type **gpedit.msc**, and press **Enter** to open the **Local Group Policy Editor**.
Navigate to **Computer Configuration** > **Administrative Templates** > **Windows Components** > **Windows Installer**.
Locate the **Prohibit User Installs** policy and double-click it.
Ensure that this policy is not enabled (if it is enabled, select **Not Configured** or **Disabled**).
3. Re-download the file.
If none of the above methods work, the file may have been corrupted during download.
Try downloading the installation package again from the official website and repeat the preceding steps.
4. Check the antivirus software.
Some antivirus software may prevent the installer from running.
Temporarily disable the antivirus software, and then try the installation again.
Remember to re-enable the antivirus software after the installation is complete.

---

## MindStudio Insight Cannot Be Used After Offline Installation

### Problem Description

Error: missing dependencies, install from `https://developer.microsoft.com/en-US/microsoft-edge/webview2/#download-section`

### Solution

[Cause]

The Edge browser is not installed on the customer's Windows system. The WebView2 component is not present.

[Resolution]

Download and install the Edge browser, or download the browser directly from the link above.
