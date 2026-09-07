## Features

### 1. Sequential Zero Write

Creates a temporary `zero.bin` file and writes a 4 MiB buffer containing zeroes sequentially.

Measures:

```text
Sequential Write Speed = Total Bytes / Elapsed Time

A lightweight Linux C++ disk I/O benchmark for measuring **sequential write throughput** and **4 KiB random write performance**.

The program uses POSIX low-level file APIs, `O_DIRECT`, aligned memory buffers, `fsync()`, and `pwrite()` to perform storage tests with reduced page-cache involvement.

## Benchmark Overview

The program runs three tests:

| Test | I/O Size | Access Pattern | Data | Result |
|---|---:|---|---|---|
| Sequential Zero Write | 4 MiB | Sequential | Zero-filled | MB/s, GB/s |
| Sequential Random Write | 4 MiB | Sequential | `/dev/urandom` data | MB/s, GB/s |
| 4K Random Write | 4 KiB | Random | `/dev/urandom` data | IOPS, MB/s |

Temporary files are created in the directory supplied on the command line and removed after each test.

---

## Features

- 4 MiB sequential write buffer
- 4 KiB random-write workload
- Linux `O_DIRECT` support
- 4 KiB-aligned memory allocation with `posix_memalign()`
- Monotonic high-resolution timing
- `fsync()` before final throughput calculation
- Deterministic random block ordering
- Progress display during tests
- Configurable benchmark size
- Automatic cleanup of temporary benchmark files
- 64-bit file-offset support

---

## Requirements

### Operating System

The current implementation is **Linux-specific**.

It uses Linux/POSIX facilities including:

```text
/dev/zero
/dev/urandom
O_DIRECT
fallocate()
clock_gettime()
posix_memalign()
unistd.h
dirent.h
