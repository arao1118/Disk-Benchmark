from pathlib import Path

readme = r'''# Disk Write Benchmark

A Linux-oriented C++ disk I/O benchmark that measures:

- **Sequential zero-data write throughput**
- **Sequential random-data write throughput**
- **4 KiB random write IOPS and bandwidth**

The benchmark uses **direct I/O (`O_DIRECT`)**, aligned memory buffers, `fsync()`, and `pwrite()` to reduce the influence of the page cache and provide more representative storage-device measurements.

> **Important:** This program is currently written for Linux/POSIX environments. Although `<windows.h>` is included, the implementation uses Linux-specific APIs such as `/dev/zero`, `/dev/urandom`, `O_DIRECT`, `fallocate()`, `clock_gettime()`, `posix_memalign()`, `DIR`, and `unistd.h`.

---

## Features

### 1. Sequential Zero Write

Creates a temporary `zero.bin` file and writes a 4 MiB buffer containing zeroes sequentially.

Measures:

```text
Sequential Write Speed = Total Bytes / Elapsed Time
