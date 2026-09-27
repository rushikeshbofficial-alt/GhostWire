<div align="center">

# ⚡ GHOSTWIRE // P2P
### High-Performance Direct Peer-to-Peer File Transfer Protocol in Pure C
*A 2nd-Year B.Tech Data Structures & Networking Course Project*

<br/>

[![C11 Standard](https://img.shields.io/badge/Language-C11-00599C?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-E95420?style=for-the-badge&logo=linux&logoColor=white)](https://ubuntu.com/)
[![Networking](https://img.shields.io/badge/Sockets-POSIX%20%2F%20WinSock2-0078D4?style=for-the-badge)](https://en.wikipedia.org/wiki/Berkeley_sockets)
[![Dependencies](https://img.shields.io/badge/Dependencies-Zero%20(Pure%20C)-success?style=for-the-badge)]()
[![Tests](https://img.shields.io/badge/Tests-36%2F36%20Passed-brightgreen?style=for-the-badge)]()
[![License](https://img.shields.io/badge/License-Academic%20Use-blueviolet?style=for-the-badge)]()

<br/>

```text
 ┌─────────────────────┐                          ┌─────────────────────┐
 │  PEER A (Provider)  │                          │ PEER B (Requester)  │
 │  Port: 5000         │                          │ Downloads Folder    │
 └──────────┬──────────┘                          └──────────▲──────────┘
            │               1. TCP Handshake                 │
            │ ◄════════════════════════════════════════════► │
            │               2. REQUEST "report.pdf"          │
            │ ◄───────────────────────────────────────────── │
            │               3. STATUS_OK + [File Size]       │
            │ ─────────────────────────────────────────────► │
            │               4. Binary Stream (8KB Chunks)    │
            │ ═════════════════════════════════════════════► │
            │               5. SHA-256 Match Verified        │
```

<p align="center">
  <b>No central trackers. No third-party dependencies. No bloat. Just pure sockets and linked lists.</b>
</p>

---

</div>

<br/>

## 🎯 Executive Overview

**GhostWire** is an ultra-lean, direct peer-to-peer file transfer engine written strictly in **C11** using POSIX network sockets and standard C libraries. Designed for a 2nd-year B.Tech Data Structures course, it demonstrates the real-world convergence of **abstract data structures** and **low-level network protocols**.

Every instance is a dual-role peer: one peer serves files directly from its filesystem catalog, while another downloads them simultaneously over an atomic TCP byte stream.

### 🌟 Key Highlights
- 🔗 **Linked-List File Catalog**: Complete singly linked list data structure storing file metadata in RAM with $O(1)$ insertion and $O(n)$ linear traversal.
- 📦 **Atomic Binary Wire Protocol**: Custom binary header structure with explicit 64-bit network byte-order endianness converters (`hton64_compat` / `ntoh64_compat`) eliminating TCP fragmentation and Nagle's algorithm stalls.
- 🚀 **Streaming $O(1)$ Memory I/O**: Files are read, packetized, and written in discrete **8192-byte chunks** (`TRANSFER_BUFFER_SIZE`), allowing gigabyte-scale transfers with virtually zero RAM footprint.
- 🛡️ **Hardened Stream Sockets**: Custom `send_all()` and `recv_all()` loops handling partial stream delivery, `EINTR` signal interrupts, and graceful peer teardowns.
- 🛑 **Crash & Injection Proof**: Ignores `SIGPIPE` with `MSG_NOSIGNAL` on Linux, automatically wipes corrupted partial downloads on abort, and strips directory-traversal attempts (`../`).

---

## 🛠️ Architecture & Data Structures

### 1. Singly Linked List (`catalog.h` / `catalog.c`)
At startup, the provider peer reads its shared folder and maps all regular files into dynamic memory:

```c
typedef struct FileEntry {
    char name[256];             /* Remote file name query */
    char path[512];             /* Full local disk path */
    long long size;             /* File size in bytes */
    struct FileEntry *next;     /* Pointer to next node */
} FileEntry;

typedef struct {
    FileEntry *head;            /* LIFO List Head Pointer */
    int count;                  /* Total Cataloged Files */
} FileCatalog;
```

```
Head ──► [ report.pdf | 1.0 MB ] ──► [ image.png | 450 KB ] ──► [ hello.txt | 18 B ] ──► NULL
```

### 2. Time & Space Complexity

| Operation | Component | Complexity | Practical Impact |
| :--- | :--- | :---: | :--- |
| **Catalog Insertion** | `catalog_add()` | **$O(1)$** | Instant LIFO head-insertion at provider startup. |
| **Catalog Search** | `catalog_find()` | **$O(n)$** | Linear scan across catalog upon client request. |
| **Catalog Cleanup** | `catalog_free()` | **$O(n)$** | Zero memory leaks on shutdown; all nodes freed. |
| **File Transfer** | `send_file()` / `receive_file()` | **$O(F)$** | Strictly proportional to file size $F$. |
| **RAM Footprint** | Memory Buffering | **$O(1)$** | **8192-byte bounded buffer** independent of file size. |

---

## 📡 Wire Protocol Specification

GhostWire enforces a deterministic binary header for every frame:

### 📥 1. Client Request Packet
```text
+---------------+------------------------+------------------------------------+
| Field         | Type                   | Description                        |
+---------------+------------------------+------------------------------------+
| type          | uint8_t                | 1 = MSG_REQUEST_FILE               |
| name_len      | uint32_t (big-endian)  | Length of requested filename       |
| filename      | char[name_len]         | Name bytes (no null terminator)    |
+---------------+------------------------+------------------------------------+
```

### 📤 2. Server Response Packet
```text
+---------------+------------------------+------------------------------------+
| Field         | Type                   | Description                        |
+---------------+------------------------+------------------------------------+
| status        | uint8_t                | 0 = STATUS_NOT_FOUND, 1 = OK       |
| file_size     | uint64_t (big-endian)  | Announced byte count               |
+---------------+------------------------+------------------------------------+
```
- If `status == 1 (OK)`: Exactly `file_size` raw binary bytes stream immediately after the header.
- If `status == 0 (NOT_FOUND)`: Socket terminates; client prints clean rejection.

---

## 🚀 Quickstart Guide

### 1. Build from Source
Compile with full optimization and strict warnings (`-Wall -Wextra -std=c11`):
```bash
make
```

### 2. Launch Provider Peer (Terminal 1)
```bash
./p2p server 5000 ./shared
```
```text
Server started on port 5000
Shared files:
  1. large.dat (1048576 bytes)
  2. hello.txt (18 bytes)
Waiting for incoming peer connections... (Ctrl+C to quit)
```

### 3. Request File (Terminal 2)
```bash
./p2p get 127.0.0.1 5000 hello.txt downloads/hello.txt
```
```text
Connected to 127.0.0.1:5000
Requesting: hello.txt
File size: 18 bytes
Transfer complete
Saved to: downloads/hello.txt
Bytes received: 18
Time: 0.001 seconds
Speed: 24.12 KB/s
```

### 4. Inspect Local Catalog Without Sockets
```bash
./p2p list ./shared
```

---

## 🧪 Comprehensive Verification Suite

The repository includes both unit test harnesses and full integration pipelines:

```bash
# 1. Run Unit Tests (Data Structures & Memory)
make test
```
```text
========================================
   P2P File Catalog Data Structure Tests 
========================================
  [PASS] Initial catalog head must be NULL
  [PASS] catalog_add returned success (0) for 3 items
  [PASS] Last added node is at head of list (O(1) insertion)
  [PASS] catalog_find returns NULL for nonexistent file
  [PASS] Path does not contain duplicate slashes
  [PASS] Added 50 items successfully
  ...
Summary: 36 / 36 tests passed.
========================================
```

```bash
# 2. Run End-to-End Integration Suite (Linux)
bash run_integration_tests.sh

# Or on Windows (PowerShell)
powershell -ExecutionPolicy Bypass -File .\run_integration_tests.ps1
```
*Tests automated transmission of text files, raw binaries (JPG/PDF), 1MB chunks, empty files, and path traversal rejection with SHA-256 validation.*

---

## 📂 Project Structure

```
p2p-file-transfer/
├── Makefile                      # Cross-platform build configuration
├── README.md                     # Project documentation & presentation
├── run_integration_tests.sh      # Linux bash test pipeline
├── run_integration_tests.ps1     # Windows PowerShell test pipeline
├── include/
│   ├── common.h                  # High-resolution timers & diagnostic logging
│   ├── peer.h                    # Peer endpoint structure
│   ├── catalog.h                 # FileEntry linked list & FileCatalog definitions
│   ├── protocol.h                # Protocol constants & 64-bit endian converters
│   ├── network.h                 # POSIX socket wrappers & send_all / recv_all
│   └── transfer.h                # Chunked binary stream send_file & receive_file
├── src/
│   ├── main.c                    # CLI entrypoint, argument parsing, signal hooks
│   ├── peer.c                    # Peer metadata utilities
│   ├── catalog.c                 # Singly linked list operations & directory scanner
│   ├── protocol.c                # Packet serialization & endian conversion
│   ├── network.c                 # Socket lifecycle, stream loop, & platform abstractions
│   ├── transfer.c                # Streaming binary chunk I/O engine
│   └── common.c                  # Time formatting and error handlers
├── shared/                       # Sample files shared by provider
└── downloads/                    # Directory where requester outputs files
```

---

## 👥 Team Module Ownership

| Member | Focus Area | Deliverable Modules |
| :--- | :--- | :--- |
| **Member 1** | **Data Structures** | `catalog.h`, `catalog.c`, linked-list unit tests |
| **Member 2** | **Networking Engine** | `network.h`, `network.c`, `send_all()`, `recv_all()` |
| **Member 3** | **Wire Protocol** | `protocol.h`, `protocol.c`, 64-bit endian serialization |
| **Member 4** | **File Transfer** | `transfer.h`, `transfer.c`, chunked binary I/O, speed metrics |
| **Member 5** | **Integration & QA** | `main.c`, `Makefile`, `README.md`, integration test scripts |

---

<details>
<summary><b>🎓 Viva & Exam Review Cheat Sheet (Click to expand)</b></summary>
<br/>

| Question | Core Concept Answer |
| :--- | :--- |
| **What is the primary data structure?** | A singly linked list of `FileEntry` nodes stored within `FileCatalog`. |
| **Where is it actively used in networking?** | When a request arrives, the server executes `catalog_find()`. It only transmits if the linked list node exists. |
| **Why not an array or hash table?** | Singly linked lists provide dynamic sizing without fixed upper bounds or resizing overhead, perfectly illustrating foundational node-pointer mechanics. |
| **Why use TCP instead of UDP?** | File transfer demands strict, loss-free, byte-ordered reliability. TCP guarantees packet sequencing and checksum validation. |
| **Why are `send_all()` and `recv_all()` required?** | TCP is an arbitrary stream of bytes, not a message protocol. Sockets may send or receive fewer bytes than requested due to MTU and OS kernel buffer limits. |
| **How is memory consumption minimized?** | By streaming disk data in fixed **8192-byte** buffers (`O(1)` memory), avoiding caching entire files in RAM. |

</details>

---

<div align="center">
  <sub>Built with passion for Data Structures & Network Systems Engineering.</sub>
</div>
