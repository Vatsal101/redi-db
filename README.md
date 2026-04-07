This is a custom-built, log-structured key-value database written entirely from scratch in C. I started this project because I wanted to really understand how databases work under the hood; the magic behind systems like Redis, Bitcask, and LevelDB.

## Features 

* **Log-Structured Storage Engine**: All writes (puts and deletes) are append-only. This means writes are fast since we don't have to seek around the disk.
* **In-Memory Hash Index**: A hash table maps keys to their offset in the file, making lookups incredibly fast (O(1)).
* **Write-Ahead Logging (WAL)**: Transactions are written to a WAL before the main database. This gives us crash recovery, atomic transactions (`BEGIN`, `COMMIT`), and partial-write detection if the system suddenly loses power.
* **Data Compaction**: Since append-only logs grow forever, I implemented a compaction process that merges logs, throws away overwritten data, and cleans up "tombstones" (deleted keys) to reclaim disk space.
* **Data Integrity**: Built-in checksums (CRC) to detect corrupted records and partial writes.

## How to Build & Run

You'll need `gcc` and `make` installed.

```bash
# Build the project
make

# Run the index and basic operations test suite
make test_index

# Run the Write-Ahead Log (WAL) and crash recovery test suite
make test_wal
```

## What I've Learned

Building this has taught me a ton about systems programming:
- How hard it actually is to guarantee a file is written to disk.
- Handling endianness and binary serialization.
- Creating test harnesses for simulated crashes.
- Dealing with memory leaks and building hash tables efficiently in C.

## What's Next

I'm still actively building and adding to this! Here are a few things on my roadmap that I plan to implement soon:
* **Client-Server Architecture**: Wrapping the database engine in a TCP network layer so I can connect and query it remotely (to get that true Redis-like experience).
* **Range Queries**: Moving beyond a simple hash index to something like a B-Tree or Skip List so the database can support sequential scans and range-based queries.
* **Concurrency Layer**: Adding multithreading support with proper locks or lock-free data structures to handle multiple readers and writers safely.
* **LRU Caching**: Adding an in-memory cache for frequently accessed values to skip the disk reads entirely.