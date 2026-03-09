# LangDFS: Distributed Network File System

This is a robust, distributed document collaboration system designed to simulate the functionality of platforms like Google Docs. Built entirely in C using POSIX standards, it features a centralized Name Server (NM), multiple Storage Servers (SS), and concurrent Clients, all communicating via TCP sockets.

The system is unique for its **sentence-level concurrency control**, allowing multiple users to edit different parts of the same text file simultaneously without locking the entire document.

##  Project Structure

```text
Distributed-Network-File-System-NFS/
├── client/                  # Client-side logic
│   ├── src/                 # Source code (client.c, function.c)
│   ├── inc/                 # Header files (client_funcs.h)
│   └── Makefile             # Build configuration for client
├── name_server/             # Central Name Server logic
│   ├── src/                 # Source code (ns.c, heap.c, functions.c)
│   ├── inc/                 # Header files (ns.h, heap.h, ip.h)
│   └── Makefile             # Build configuration for Name Server
├── storage/                 # Storage Server logic
│   ├── src/                 # Source code (storage.c, write_helpers.c)
│   ├── inc/                 # Header files (storage.h, locks.h)
│   ├── data/                # Current state of the file is stored here
│   ├── tmp/                 # Previous state of the file used in undo
│   └── Makefile             # Build configuration for Storage Server
└── cmn_inc.h                # Common definitions used across all modules
```

## Key Features

### 1. Distributed Architecture

- **Name Server (NM)**: The central brain. Maps filenames to storage locations, handles directory lookups, and manages the registry of available Storage Servers.
- **Storage Servers (SS)**: Persistent data stores. They handle file I/O and synchronize with the NM.
- **Clients**: The user interface for issuing commands (READ, WRITE, STREAM, etc.).

### 2. Advanced Concurrency

- **Sentence-Level Locking**: Unlike standard file locking, LangDFS parses files into sentences (delimited by `.`, `?`, `!`). Users can edit Sentence A while another user edits Sentence B concurrently.
- **Reader-Writer Locks**: Multiple users can read a file simultaneously, but writing to a specific sentence requires an exclusive lock.

### 3. User Functionalities

- **Streaming**: `STREAM <file>` fetches content word-by-word with simulated network latency (0.1s delay).
- **Access Control**: Owners can grant READ or WRITE permissions to specific users.
- **Remote Execution**: `EXEC <file>` executes shell commands stored within a file on the server.
- **Undo**: `UNDO <file>` reverts the last change made to a file.

## Build & Run Instructions

**Prerequisites**: Linux/Unix environment with GCC and Make.

### 1. Build All Components

Navigate to each directory and run make:
```bash
cd name_server && make
cd ../storage && make
cd ../client && make
```

### 2. Start the Name Server (NM)

The Name Server must be started first. It listens for Storage Servers and Clients.
```bash
./name_server/ns
```

### 3. Start Storage Servers (SS)

Start one or more Storage Servers. They will register themselves with the NM.
```bash
./storage/storage_server
```

### 4. Start Clients

Launch the client application. You will be prompted for a username.
```bash
./client/client
```
### 5. Future Improvements

Implement the replication logic for the storage servers so that if one breaks the other will be used

##  Command Reference

| Command | Description | Example |
|---------|-------------|---------|
| CREATE | Creates a new empty file. | `CREATE doc.txt` |
| READ | Reads the entire content of a file. | `READ doc.txt` |
| WRITE | Updates specific sentences/words. | `WRITE doc.txt 0 1 Hello` |
| STREAM | Streams file content word-by-word. | `STREAM doc.txt` |
| DELETE | Deletes a file. | `DELETE doc.txt` |
| INFO | Shows file metadata (size, owner). | `INFO doc.txt` |
| LIST | Lists all active users. | `LIST` |
| ADDACCESS | Grants permission to a user. | `ADDACCESS -R doc.txt alice` |
| REMACCESS | Removes permission to a user. | `REMACCESS doc.txt alice` |
| EXEC | Executes file content as shell script. | `EXEC script.txt` |
| UNDO | Reverts the last modification. | `UNDO doc.txt` |

##  Technical Details

- **Communication**: Custom Application Layer Protocol over TCP/IP.
- **Data Structures**: Tries/Hashmaps (in NM) for O(1) file lookups.
- **Synchronization**: POSIX Threads (pthread), Mutexes, and Condition Variables.

