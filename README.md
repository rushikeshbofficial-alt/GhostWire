# Peer-to-Peer (P2P) File Transfer System (Mid-Semester Version)

## 1. Project Overview
A modular, direct peer-to-peer file transfer prototype written in C using POSIX sockets and the standard C library. Designed as a second-year B.Tech Data Structures course project, the system demonstrates how foundational data structures integrate with computer networking primitives. Every running instance of the application functions as a peer: one peer provides files while another peer requests them over a TCP byte stream without relying on any central tracker or permanent server. The provider peer indexes its shared directory at startup into an in-memory singly linked list file catalog, which is linearly searched to validate incoming peer requests before streaming binary file data in fixed-size 8192-byte chunks.

## 2. Features Implemented for Mid-Semester
- **Dual-Role CLI**: The same executable operates in either provider mode (`server`), requester mode (`get`), or catalog inspection mode (`list`).
- **Linked-List File Catalog**: Complete singly linked list data structure implementation (`FileCatalog` and `FileEntry`) managing shared file metadata in RAM.
- **Directory Ingestion**: Startup scanning of local directory using POSIX directory APIs (`opendir`, `readdir`, `closedir`, `stat`) to dynamically populate the catalog, with trailing slash normalization and buffer overflow protection.
- **Atomic Binary Protocol**: Network-byte-order binary messaging protocol with packed headers to prevent TCP fragmentation and Nagle delayed-ACK stalls.
- **64-bit Network Endianness**: Transparent 64-bit integer byte-swapping helpers (`hton64_compat`, `ntoh64_compat`) ensuring seamless file size exchange across big-endian and little-endian architectures.
- **Reliable Stream I/O**: Robust `send_all()` and `recv_all()` stream helpers handling partial socket transfers, signal interruptions (`EINTR`), and premature socket closures.
- **Bounded Memory Streaming**: Binary file chunking via fixed 8192-byte buffers (`TRANSFER_BUFFER_SIZE`) preventing high memory usage even when transferring large multi-megabyte files.
- **Transfer Statistics & Integrity**: Real-time elapsed time calculation (microsecond resolution) and human-readable transfer speed computation (B/s, KB/s, MB/s) with disk byte count verification.
- **Sequential Connection Handling**: Provider server loops sequentially to serve multiple client download requests without restarting.
- **Signal & Fault Resilience**: Ignores `SIGPIPE` and uses `MSG_NOSIGNAL` to prevent server crashes on Linux when clients disconnect mid-transfer; verifies file accessibility before announcing `STATUS_OK`.
- **Security & Integrity**: Filename validation and sanitization rejecting directory traversal attempts (`..`, `/`, `\`) and embedded null bytes; automatically purges incomplete/corrupt download files if transfers abort.
- **Strict Input Parsing**: Rejects non-numeric, out-of-range, and malformed port strings via `strtol`.

## 3. Requirements
- **Compiler**: GCC with C11 support (`-Wall -Wextra -std=c11`).
- **Platform**: Linux / Ubuntu (POSIX sockets: `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<unistd.h>`, `<dirent.h>`).
- *Note for Windows*: The codebase also natively supports MinGW GCC on Windows (using `-lws2_32`) for convenient cross-platform development and local testing.
- **Build System**: GNU Make.

## 4. Build Instructions
To build the project executable and test suite:
```bash
# Build the main peer executable (creates ./p2p)
make

# Clean build artifacts (object files and executables)
make clean
```

## 5. How to Start Server (Provider)
To start a peer in provider mode:
```bash
./p2p server <port> <shared_directory>
```
**Example**:
```bash
# Prepare shared directory
mkdir -p shared
echo "Sample file content" > shared/sample.txt

# Start provider listening on port 5000
./p2p server 5000 ./shared
```
**Expected Server Console Output**:
```
Server started on port 5000
Shared files:
1. sample.txt (20 bytes)
Waiting for incoming peer connections... (Ctrl+C to quit)
```

## 6. How to Request a File (Requester)
To request and download a file from a running provider:
```bash
./p2p get <server_ip> <server_port> <filename> <output_path>
```
**Example**:
```bash
# Request sample.txt and save into downloads folder
mkdir -p downloads
./p2p get 127.0.0.1 5000 sample.txt downloads/sample.txt
```
**Expected Client Console Output**:
```
Connected to 127.0.0.1:5000
Requesting: sample.txt
File size: 20 bytes
Transfer complete
Saved to: downloads/sample.txt
Bytes received: 20
Time: 0.001 seconds
Speed: 19.53 KB/s
```

**Negative Test (Nonexistent File)**:
```bash
./p2p get 127.0.0.1 5000 missing.txt downloads/missing.txt
```
**Expected Output**:
```
Connected to 127.0.0.1:5000
Requesting: missing.txt
File not found on peer
```

**Optional Local Catalog Inspection**:
```bash
./p2p list ./shared
```

## 7. How to Run Tests

### Unit Tests
The project includes a dedicated unit test suite for the linked-list catalog data structure:
```bash
make test
```
This runs `tests/test_catalog.c`, executing tests T01 through T04 plus boundary and edge cases (empty catalog, insertion ordering, O(n) search hits and misses, directory ingestion, and memory freeing).

### Automated Integration Tests
Automated end-to-end integration test scripts verify server startup, sequential transfers, binary integrity, 1MB buffer chunking, and negative rejection:
- **On Linux / Ubuntu**:
  ```bash
  bash run_integration_tests.sh
  ```
- **On Windows (PowerShell)**:
  ```powershell
  powershell -ExecutionPolicy Bypass -File .\run_integration_tests.ps1
  ```

## 8. Project Directory Structure
```
p2p-file-transfer/
├── Makefile                     # Cross-platform build script
├── README.md                    # Project specification & documentation
├── run_integration_tests.sh     # Linux bash integration test script
├── run_integration_tests.ps1    # Windows PowerShell integration test script
├── include/                     # Modular header files
│   ├── common.h                 # Logging, timing, and formatting helpers
│   ├── peer.h                   # Peer endpoint metadata struct
│   ├── catalog.h                # Linked list data structure prototypes
│   ├── protocol.h               # Wire protocol constants and headers
│   ├── network.h                # Socket creation, send_all, and recv_all
│   └── transfer.h               # Binary chunked file transmission
├── src/                         # Module implementations
│   ├── main.c                   # CLI router and high-level client/server flow
│   ├── peer.c                   # Peer metadata initialization
│   ├── catalog.c                # Singly linked list catalog and directory scanner
│   ├── protocol.c               # Binary header serialization and endian conversions
│   ├── network.c                # POSIX/Winsock network abstraction and stream helpers
│   ├── transfer.c               # Chunked binary send_file and receive_file
│   └── common.c                 # Utility, formatting, and high-resolution timer
├── shared/                      # Directory for provider files
│   └── README.txt
├── downloads/                   # Directory for requester downloaded files
│   └── .gitkeep
└── tests/                       # Unit test suites
    └── test_catalog.c           # Catalog linked list test harness
```

## 9. Data Structures Used

### `FileEntry`
A node in a singly linked list representing an individual file available in the provider's shared directory:
```c
typedef struct FileEntry {
    char name[256];             /* Filename exposed to remote peers */
    char path[512];             /* Full path on local disk */
    long long size;             /* File size in bytes */
    struct FileEntry *next;     /* Pointer to the next node in the catalog list */
} FileEntry;
```

### `FileCatalog`
The list container storing the head pointer and total file count:
```c
typedef struct {
    FileEntry *head;            /* Head pointer to linked list */
    int count;                  /* Total number of items in catalog */
} FileCatalog;
```

### `PeerInfo`
Remote peer endpoint representation:
```c
typedef struct {
    char ip[INET_ADDRSTRLEN];   /* IPv4 address string (e.g., "127.0.0.1") */
    int port;                   /* TCP port number */
} PeerInfo;
```

### Why a Linked List is Mandatory
In a Data Structures course project, abstract data types must be demonstrably functional. The catalog linked list is directly used on every network transaction: when a requester transmits a filename, the provider executes `catalog_find()`, traversing the linked list nodes sequentially to find the file's path and size. If found, `entry->size` and `entry->path` are used to serve the file; if `NULL`, a `STATUS_NOT_FOUND` message is sent.

## 10. Complexity Table

| Operation | Function | Complexity | Explanation |
| :--- | :--- | :--- | :--- |
| **Catalog Init** | `catalog_init()` | **O(1)** | Initializes head pointer to `NULL` and count to 0. |
| **Catalog Insertion** | `catalog_add()` | **O(1)** | Inserts new `FileEntry` at the head of the linked list. |
| **Catalog Search** | `catalog_find()` | **O(n)** | Traverses singly linked list comparing filenames sequentially (`n` files). |
| **Catalog Print** | `catalog_print()` | **O(n)** | Iterates through all nodes in the linked list to print metadata. |
| **Catalog Deallocation** | `catalog_free()` | **O(n)** | Frees each dynamically allocated `FileEntry` node individually. |
| **Directory Ingestion** | `catalog_scan_directory()` | **O(n)** | Scans filesystem directory entries and calls `catalog_add()`. |
| **Sending File** | `send_file()` | **O(F)** | Transmits `F` bytes over socket in chunks. |
| **Receiving File** | `receive_file()` | **O(F)** | Reads `F` bytes from socket and writes to disk. |
| **Memory for Payload** | Buffering | **O(B)** | Uses fixed 8192-byte buffer (`B = 8192`), avoiding `O(F)` RAM usage. |

## 11. Protocol Summary
Communication occurs over TCP using explicit binary packet headers. All integer quantities are transmitted in standard **network byte order (big-endian)**.

### Request Message (Requester -> Provider)
```
+---------------+------------------------+------------------------------------+
| Field         | Type                   | Description                        |
+---------------+------------------------+------------------------------------+
| type          | uint8_t                | 1 = MSG_REQUEST_FILE               |
| name_len      | uint32_t (big-endian)  | Length of filename (1..255)        |
| name          | char[name_len]         | Filename ASCII bytes (no null byte)|
+---------------+------------------------+------------------------------------+
```

### Response Message (Provider -> Requester)
```
+---------------+------------------------+------------------------------------+
| Field         | Type                   | Description                        |
+---------------+------------------------+------------------------------------+
| status        | uint8_t                | 0 = STATUS_NOT_FOUND, 1 = STATUS_OK|
| file_size     | uint64_t (big-endian)  | Size of file in bytes              |
+---------------+------------------------+------------------------------------+
```
- If `status == STATUS_OK (1)`: exactly `file_size` payload bytes follow immediately as raw binary data.
- If `status == STATUS_NOT_FOUND (0)`: `file_size` is 0 and connection terminates.

## 12. Known Limitations (Mid-Semester Scope)
- **Sequential Connections**: The provider serves one requester at a time (blocking sequential requests, no concurrent multithreading/select).
- **Single File Transfer**: Only one file can be requested per command invocation.
- **No Transfer Resume**: Interrupted transfers must be restarted from byte 0.
- **No Directory Recursion**: Subdirectories inside `shared/` and hidden files are ignored during directory scanning.
- **No Peer Discovery**: Remote peer IP address and port must be specified manually.

## 13. End-Semester Planned Features
- Multi-peer active tracking and peer discovery mechanism.
- File-to-peers mapping using a hash table / inverted index.
- Splitting files into fixed-size chunks (e.g., 256 KB) with bitset/bitmap tracking.
- Parallel chunk downloading across multiple peer providers simultaneously.
- Non-blocking I/O using `select()` / `poll()` or POSIX threads (`pthreads`).
- Cryptographic chunk verification (SHA-256) and transfer resumption for interrupted downloads.

## 14. Team-of-5 Module Ownership
| Team Member | Area | Primary Responsibility | Concrete Deliverables |
| :--- | :--- | :--- | :--- |
| **Member 1** | Data Structures | Linked list catalog management and unit tests | `catalog.h`, `catalog.c`, `test_catalog.c` |
| **Member 2** | Networking | Sockets, client/server lifecycle, stream I/O | `network.h`, `network.c`, `send_all`, `recv_all` |
| **Member 3** | Protocol | Binary headers, serialization, endian conversion | `protocol.h`, `protocol.c`, 64-bit converters |
| **Member 4** | File Transfer | Binary chunked file I/O and transfer metrics | `transfer.h`, `transfer.c`, timing & speed |
| **Member 5** | Integration & Testing | Top-level CLI flow, Makefile, integration tests | `main.c`, `Makefile`, `README.md`, test scripts |

## 15. Review & Viva Cheat Sheet

| Viva Question | Concise Answer |
| :--- | :--- |
| **What is the primary data structure?** | A singly linked list of `FileEntry` nodes contained inside `FileCatalog`. |
| **Why is it needed?** | The provider must store, index, and query metadata of available shared files in memory. |
| **Where is it used during networking?** | Upon receiving a request header, the provider calls `catalog_find()` to verify if the file exists before sending headers or file bytes. |
| **What is its time complexity?** | `O(1)` head insertion, `O(n)` linear search. |
| **Why not a hash table?** | A singly linked list is clean, demonstrable, directly meets the course syllabus requirements, and avoids hash collisions/resizing overhead for small-to-medium directories. |
| **Why TCP instead of UDP?** | File transfer requires guaranteed, in-order, error-checked delivery of byte streams. |
| **Why `send_all()` and `recv_all()`?** | TCP is a stream protocol; a single `send()` or `recv()` call is not guaranteed to transfer all requested bytes due to OS buffer limits and MTU packet segmentation. |
| **How much file data is kept in RAM?** | Only a fixed 8192-byte buffer (`O(1)` relative to file size), streaming directly from disk to network and network to disk. |
| **Is there a central server?** | No. Any peer can act as a provider (`server`) or a requester (`get`). |
