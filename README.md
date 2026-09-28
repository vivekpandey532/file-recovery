# How to Build & Run File Recovery Tool using Visual Studio

## What You Need to Install

| Software | Version | Download Link |
|---|---|---|
| Visual Studio 2022 | Community (Free) | https://visualstudio.microsoft.com/downloads/ |
| Qt6 | 6.x (MSVC build) | https://www.qt.io/download-open-source |
| CMake | 3.16+ (bundled in VS) | Comes with Visual Studio |

---

## Step 1 — Install Visual Studio 2022

1. Go to https://visualstudio.microsoft.com/downloads/
2. Download **Visual Studio 2022 Community** (free)
3. Run the installer
4. In the **Workloads** screen, check:
   - ✅ **Desktop development with C++**
5. On the right side under **Individual components**, make sure these are checked:
   - ✅ MSVC v143 - VS 2022 C++ x64/x86 build tools
   - ✅ CMake tools for Windows
   - ✅ Windows 10/11 SDK
6. Click **Install** and wait (this may take 10–20 minutes)

> After install, CMake is automatically available — no separate CMake install needed.

---

## Step 2 — Install Qt6

1. Go to https://www.qt.io/download-open-source
2. Click **Download the Qt Online Installer**
3. Run the installer — it will ask you to create a free Qt account (required)
4. Sign in or create an account and proceed
5. In the **Select Components** screen:
   - Expand **Qt → Qt 6.x.x**
   - Check ✅ **MSVC 2019 64-bit** (works with VS 2022 too)
   - Check ✅ **Qt Concurrent** (required by this project)
6. Click **Next** and complete the installation
7. Default install path is usually:
   ```
   C:\Qt\6.x.x\msvc2019_64\
   ```
   Note this path — you will need it in Step 4.

---

## Step 3 — Open the Project in Visual Studio

1. Open **Visual Studio 2022**
2. On the start screen click **Open a local folder**

   ![Open Folder](https://i.imgur.com/placeholder.png)

3. Browse to your project folder:
   ```
   C:\Users\vivek.pandey\Documents\Project\file-recovery
   ```
4. Click **Select Folder**
5. Visual Studio will detect `CMakeLists.txt` automatically and start configuring the project
6. Wait for the **CMake configuration** to finish (watch the Output window at the bottom)

---

## Step 4 — Tell CMake Where Qt6 Is Installed

CMake needs to know where Qt6 is on your machine. Do this:

1. In Visual Studio, go to **Project → CMake Settings for FileRecovery**

   OR open the file `CMakeSettings.json` if it appears in Solution Explorer

2. Find the **CMake command arguments** field (or **CMake variables**)

3. Add this line (replace the path with your actual Qt install path from Step 2):
   ```
   -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2019_64
   ```

   **Example** (if Qt 6.7.0 is installed):
   ```
   -DCMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64
   ```

4. Save the settings — Visual Studio will re-run CMake automatically

5. Check the **Output** window — you should see:
   ```
   -- Found Qt6: C:/Qt/6.7.0/msvc2019_64/lib/cmake/Qt6
   -- Configuring done
   -- Build files have been written to ...
   ```
   If you see errors about Qt6 not found, double-check the path in step 3.

---

## Step 5 — Build the Project

1. In the top toolbar, make sure the configuration is set to **x64-Debug** or **x64-Release**
2. Go to **Build → Build All**  (or press `Ctrl + Shift + B`)
3. Watch the **Output** window — a successful build ends with:
   ```
   Build All succeeded.
   ```

### Common Build Errors & Fixes

| Error | Fix |
|---|---|
| `Qt6 not found` | Re-check `CMAKE_PREFIX_PATH` in Step 4 |
| `MSVC not found` | Re-run VS installer and ensure "Desktop development with C++" workload is installed |
| `windows.h not found` | Make sure Windows SDK is installed (Step 1, Individual components) |
| `setupapi.lib not found` | Included in Windows SDK — reinstall Windows SDK component |

---

## Step 6 — Run as Administrator (Required)

This application reads raw disk data and **must run as Administrator**.

### Option A — Run from Visual Studio as Admin

1. Close Visual Studio completely
2. Right-click the **Visual Studio 2022** shortcut
3. Select **Run as administrator**
4. Re-open the project folder
5. Press **F5** or go to **Debug → Start Debugging**

### Option B — Run the built .exe as Admin

1. After building, find the output `.exe` file. It will be in a path like:
   ```
   C:\Users\vivek.pandey\Documents\Project\file-recovery\out\build\x64-Debug\FileRecovery.exe
   ```
2. Right-click `FileRecovery.exe`
3. Select **Run as administrator**

> If you run without admin rights, the app will show a warning dialog and exit immediately — this is by design.

---

## Step 7 — Using the Application

Once running:

1. The main window opens
2. Go to **File → New Scan** (or press `Ctrl + N`)
3. A dialog appears:
   - **Select Drive** — choose the drive you want to scan from the dropdown
   - **Custom path** (optional) — check "Use custom disk path" to type a raw path like `\\.\PhysicalDrive0`
   - **Scan Method** — choose one:
     - Quick Scan — fast, uses filesystem metadata (MFT/FAT)
     - Deep Scan — slow, scans raw disk bytes for file signatures
     - Full Scan — both methods combined (recommended)
4. Click **Start Scan**
5. Wait for the scan to complete — progress shows in the status bar
6. Results appear in the table — filter by filename or extension using the search box
7. Select files you want to recover (or click **Select All**)
8. Click **Recover Selected**
9. Choose a destination folder — recovered files are saved there

---

## Project Folder Structure (for reference)

```
file-recovery/
├── core/
│   ├── DiskScanner.cpp / .h       — Lists drives, opens raw disk handles
│   ├── NTFSParser.cpp / .h        — Scans NTFS MFT for deleted files
│   ├── FATParser.cpp / .h         — Scans FAT32 directories for deleted files
│   ├── FileCarver.cpp / .h        — Raw disk scan using file signatures
│   └── FileRecovery.cpp / .h      — Writes recovered file data to disk
├── ui/
│   ├── MainWindow.cpp / .h        — Main application window
│   ├── ScanDialog.cpp / .h        — Drive selection & scan options dialog
│   └── ResultsView.cpp / .h       — Results table & recover button
├── main.cpp                       — Entry point, admin check
├── CMakeLists.txt                 — Build configuration
└── SETUP_VISUAL_STUDIO.md         — This file
```

---

## Quick Checklist Before Running

- [ ] Visual Studio 2022 installed with **Desktop development with C++** workload
- [ ] Qt6 installed with **MSVC 64-bit** component
- [ ] `CMAKE_PREFIX_PATH` set to your Qt6 MSVC folder
- [ ] Build succeeded with no errors
- [ ] Running Visual Studio or the `.exe` **as Administrator**
