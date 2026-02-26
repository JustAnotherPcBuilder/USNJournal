# WinNotify

WinNotify is a modular, low‑overhead file‑change detection system built directly on the Windows NTFS Update Sequence Number (USN) Journal. 
It is designed to monitor specific directories or volumes with minimal CPU usage, making it suitable for background services, sync tools, and automation workflows that require precise and efficient change detection.
The goal is to make this similar to Linux inotify.
---

## Purpose

The project focuses on:

- **Modularity** — Components for reading, filtering, and processing USN records will be cleanly separated so they can be extended or replaced without touching the core reader.
- **Minimal overhead** — The plan is to avoid unnecessary polling and high‑level APIs, relying instead on direct USN Journal reads that wake only when NTFS produces new events.
- **Targeted monitoring** — The program will focus on specific paths, sets of paths, and file types, reducing noise and improving performance.
- **Action on Detection** — Detected changes will trigger custom actions, such as with file copying/mirroring, custom file processing, or running scripts.
  
---

## Origin

The initial implementation is based on Microsoft’s official USN Journal example code, specifically the sample demonstrating how to walk a buffer of change‑journal records:

https://learn.microsoft.com/en-us/windows/win32/fileio/walking-a-buffer-of-change-journal-records
