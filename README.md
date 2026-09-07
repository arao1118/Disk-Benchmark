# Disk Benchmark

A lightweight **Linux C++ disk I/O benchmark** that measures sequential write throughput and 4 KiB random-write performance using low-level POSIX APIs and direct I/O.

The benchmark performs three tests:

1. Sequential zero-filled writes
2. Sequential random-data writes
3. 4 KiB random writes with IOPS measurement

---

## Features

- Sequential write throughput measurement
- Sequential zero-data and random-data comparison
- 4 KiB random write IOPS test
- `O_DIRECT` direct-I/O mode
- 4 KiB-aligned memory buffers
- `fsync()` before final throughput calculation
- Monotonic timing with `clock_gettime()`
- Deterministic random block ordering
- Configurable benchmark size
- Progress indicator
- Temporary-file cleanup
- 64-bit file-offset support

---

## Requirements

### Operating system

The current implementation is **Linux-specific**.

It uses Linux/POSIX functionality including:

- `/dev/zero`
- `/dev/urandom`
- `O_DIRECT`
- `fallocate()`
- `clock_gettime()`
- `posix_memalign()`
- `unistd.h`
- `dirent.h`

The source currently includes `<windows.h>`, but the benchmark itself does not use the Windows API and will not compile as a native Windows application without modification.

### Compiler

A C++17-compatible compiler is recommended.

Tested/expected toolchains include:

- GCC
- Clang

---

## Building

### Using Make

If the repository contains the provided `Makefile`:

```bash
make
```

### Using GCC directly

```bash
g++ -std=c++17 -O2 -Wall -Wextra disk.cpp -o disk
```

For debugging:

```bash
g++ -std=c++17 -g -Wall -Wextra disk.cpp -o disk
```

---

## Usage

```text
./disk <size_in_GB> <path_to_folder>
```

Example:

```bash
./disk 10 /mnt/ssd-test
```

You can also use a relative path:

```bash
./disk 5 .
```

Decimal sizes are accepted:

```bash
./disk 1.5 /mnt/ssd-test
```

The program interprets the requested size using binary units:

```text
1 GB input = 1024 × 1024 × 1024 bytes
```

Technically, this is a GiB-style conversion even though the program labels the input as GB.

---

# Benchmark Tests

## 1. Sequential Zero Write

The program creates a temporary:

```text
zero.bin
```

A 4 MiB buffer is allocated and filled with zeroes using `/dev/zero`.

The same buffer is then written sequentially until the requested amount of data has been written.

The main sequential buffer size is:

```cpp
#define BUFF_SIZE (4ULL * 1024ULL * 1024ULL)
```

which is:

```text
4 MiB
```

After the writes finish, `fsync()` is called and the elapsed time is used to calculate throughput.

### Result

```text
Sequential Zeroes Write : XXXX MB/s
```

This test measures sustained sequential writing of zero-filled data.

---

## 2. Sequential Random-Data Write

The program creates:

```text
random.bin
```

A 4 MiB buffer is filled using:

```text
/dev/urandom
```

The buffer is then written sequentially to the file.

The test measures:

```text
Sequential Random Write : XXXX MB/s
```

### Important detail

The program reads random data into the 4 MiB buffer once and repeatedly writes that buffer.

Therefore:

- The **data pattern is random-looking**
- The **write locations are sequential**
- New random data is **not generated for every 4 MiB write**

---

## 3. 4 KiB Random Write IOPS

The third test creates:

```text
iops.bin
```

Each operation writes exactly:

```text
4096 bytes = 4 KiB
```

The maximum number of operations is:

```cpp
#define IOPS_TOTAL_OPS (100000ULL)
```

Therefore, up to 100,000 random 4 KiB writes are performed.

---

# Direct I/O

The benchmark opens its test files using:

```cpp
O_DIRECT
```

For example:

```cpp
open(
    zero_path,
    O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT,
    0644
);
```

`O_DIRECT` requests direct I/O and reduces reliance on the normal filesystem page cache.

Direct I/O generally has alignment requirements. The program therefore allocates its buffers using:

```cpp
posix_memalign()
```

with:

```cpp
#define DIRECT_IO_ALIGNMENT (4096ULL)
```

This provides buffers aligned to 4096 bytes.

> `O_DIRECT` does not mean that every layer of the storage stack is guaranteed to completely bypass every form of caching. Exact behavior depends on the kernel, filesystem, storage device, and configuration.

---

# Memory Buffers

The sequential tests use:

```text
4 MiB
```

buffers.

The IOPS test uses:

```text
4 KiB
```

buffers.

The relevant definitions are:

```cpp
#define BUFF_SIZE (4ULL * 1024ULL * 1024ULL)
#define IOPS_BLOCK_SIZE (4096ULL)
#define DIRECT_IO_ALIGNMENT (4096ULL)
```

The buffers are allocated with:

```cpp
posix_memalign()
```

rather than ordinary `malloc()` so that their addresses satisfy direct-I/O alignment requirements.

---

# Timing

The benchmark uses:

```cpp
clock_gettime(CLOCK_MONOTONIC, &ts);
```

for elapsed-time measurement.

`CLOCK_MONOTONIC` is used because it is intended for measuring elapsed time and is not affected by normal wall-clock changes.

The helper function returns the time as seconds:

```text
seconds + nanoseconds / 1,000,000,000
```

---

# Sequential Throughput Calculation

The sequential tests calculate throughput approximately as:

```text
Throughput = Total Bytes / Elapsed Seconds
```

The result is converted to MiB/s using:

```text
MiB/s = Total Bytes / Elapsed Seconds / 1024²
```

The program then displays:

```text
MB/s
GB/s
```

Although the labels say MB/GB, the conversion uses powers of 1024.

---

# 4 KiB IOPS Calculation

The random-write test calculates:

```text
IOPS = Number of Operations / Elapsed Seconds
```

For example:

```text
100,000 operations
------------------
2 seconds

= 50,000 IOPS
```

The equivalent bandwidth is:

```text
Bandwidth = IOPS × 4096 / 1024²
```

For example:

```text
100,000 IOPS × 4096 bytes
≈ 390.63 MiB/s
```

---

# Random Block Generation

The IOPS test creates an array containing the available 4 KiB block indexes:

```cpp
iota(block_indices.begin(), block_indices.end(), 0ULL);
```

The indexes are then shuffled:

```cpp
shuffle(block_indices.begin(), block_indices.end(), rng);
```

The random generator is initialized with:

```cpp
mt19937_64 rng(1337);
```

The fixed seed means that repeated runs use the same randomized block order.

This makes testing more reproducible.

---

# Random Write Operation

For every selected block, the program calculates an aligned file offset:

```cpp
off_t offset =
    (off_t)block_indices[i] * IOPS_BLOCK_SIZE;
```

The write is then performed using:

```cpp
pwrite(
    fd_iops,
    iops_buffer,
    IOPS_BLOCK_SIZE,
    offset
);
```

Because `pwrite()` accepts an explicit offset, each operation can write to a different random location without changing a shared file position.

---

# File Preallocation

Before the random-write test begins, the program attempts to allocate the requested file size using:

```cpp
fallocate()
```

This creates the space needed for the random-write workload before the individual operations are performed.

If `fallocate()` fails, the program reports the error but continues.

---

# Temporary Files

The program creates three temporary files:

```text
zero.bin
random.bin
iops.bin
```

They are created inside the directory supplied to the program.

Each file is removed after its corresponding test:

```cpp
unlink(...)
```

If the program is interrupted before cleanup, a temporary file may remain.

You can remove leftover files manually:

```bash
rm zero.bin random.bin iops.bin
```

Be sure to run the command in the correct directory.

---

# Example Output

A typical run has output similar to:

```text
=====================================
         DISK WRITE BENCHMARK
=====================================
Input Size : 10.000 GB
Total Bytes: 10737418240 bytes (Aligned for O_DIRECT)
Buffer Size: 4096 KB
Directory  : /mnt/ssd-test
Chunks     : 2560 (Remainder: 0 bytes)
=====================================

Starting sequential zero write test...
Writing zeroes... 100%
Zero write complete. Speed: 2500.00 MB/s (2.44 GB/s)

Starting sequential random write test...
Writing random data... 100%
Random write complete. Speed: 2400.00 MB/s (2.34 GB/s)

Starting 4KB Random Write IOPS test...
4KB Random Writes... 100%

=====================================
               RESULTS
=====================================
Seq Zeroes Write   : 2500.00 MB/s (2.44 GB/s)
Seq Random Write   : 2400.00 MB/s (2.34 GB/s)
-------------------------------------
4K Random IOPS     : 85000 IOPS
4K Random Bandwidth: 332.03 MB/s
=====================================
```

These numbers are **examples only**. Actual performance depends on the hardware and system configuration.

---

# Understanding the Results

## Sequential Write

Sequential throughput is useful for workloads such as:

- Large file transfers
- Video recording
- Disk imaging
- Backups
- Large dataset writes

Higher throughput generally means the storage system can sustain larger contiguous writes at a higher rate.

## 4 KiB Random IOPS

Random IOPS measures how many small 4 KiB write operations can be completed per second.

It is more representative of small-block workloads than sequential MB/s.

Possible workloads include:

- Databases
- Metadata-heavy applications
- Virtual machines
- Operating-system workloads
- Applications that frequently update small files

---

# Benchmark Limitations

This project is intended primarily as a **learning project and lightweight storage benchmark**.

It is not intended to replace professional storage benchmarking tools.

## Queue Depth

The 4 KiB random-write test performs one `pwrite()` and waits for it to complete before starting the next one.

Conceptually:

```text
pwrite()
   ↓
wait
   ↓
pwrite()
   ↓
wait
   ↓
pwrite()
```

This is approximately a:

```text
Queue Depth = 1
```

workload.

Modern NVMe SSDs can achieve much higher IOPS with multiple outstanding operations.

Therefore, this benchmark should not be compared directly with manufacturer specifications that were measured at higher queue depths and/or multiple threads.

## Single Thread

The benchmark is single-threaded.

It does not currently implement:

- Multiple I/O threads
- Multiple processes
- Asynchronous I/O
- `io_uring`
- Configurable queue depth

These features can significantly change storage performance.

## Short Random Workload

The IOPS test performs a maximum of:

```text
100,000 operations
```

This may be too short to characterize long-term performance on some SSDs.

## SSD SLC Cache

Many consumer SSDs use an SLC cache.

A short benchmark may therefore measure cached write performance rather than sustained NAND write performance.

For sustained testing, use a larger benchmark size and multiple runs.

## Thermal Throttling

SSD performance can decrease when the drive becomes hot.

For more consistent measurements:

- Avoid heavy background disk activity
- Keep system conditions similar between runs
- Allow the drive to cool between repeated tests

---

# Filesystem and Storage Effects

Results can be affected by:

- Filesystem type
- Mount options
- Linux kernel version
- SSD firmware
- Storage controller
- NVMe/SATA interface
- Encryption
- Virtualization
- Background I/O
- Drive temperature
- Drive capacity and free space

For meaningful comparisons, keep the testing environment consistent.

---

# Free-Space Check

The current implementation checks available space using:

```cpp
filesystem::space("/");
```

This checks the filesystem represented by `/`.

If the benchmark directory is on another mounted filesystem, the reported available space may not correspond to the actual target filesystem.

For example:

```text
/
└── mnt/
    └── ssd-test/
```

If `/mnt/ssd-test` is a separate filesystem, the program should ideally query:

```cpp
filesystem::space(argv[2]);
```

instead.

This is a potential improvement for a future version.

---

# Data Units

The program uses binary units internally:

```text
1 KiB = 1024 bytes
1 MiB = 1024² bytes
1 GiB = 1024³ bytes
```

However, some output labels use:

```text
KB
MB/s
GB/s
```

For strict technical terminology, these could be renamed to:

```text
KiB
MiB/s
GiB/s
```

This is important when comparing the output with SSD specifications, which commonly use decimal units:

```text
1 KB = 1000 bytes
1 MB = 1000² bytes
1 GB = 1000³ bytes
```

---

# Benchmark Parameters

The main workload parameters are defined near the top of `disk.cpp`:

```cpp
#define BUFF_SIZE (4ULL * 1024ULL * 1024ULL)
#define IOPS_BLOCK_SIZE (4096ULL)
#define IOPS_TOTAL_OPS (100000ULL)
#define DIRECT_IO_ALIGNMENT (4096ULL)
```

| Parameter | Value | Purpose |
|---|---:|---|
| `BUFF_SIZE` | 4 MiB | Sequential I/O buffer |
| `IOPS_BLOCK_SIZE` | 4 KiB | Random I/O block size |
| `IOPS_TOTAL_OPS` | 100,000 | Maximum random operations |
| `DIRECT_IO_ALIGNMENT` | 4 KiB | Direct-I/O alignment |

These constants can be modified to experiment with different workloads.

---

# Project Structure

Recommended repository structure:

```text
Disk-Benchmark/
├── disk.cpp
├── Makefile
├── README.md
└── .gitignore
```

The compiled executable should normally not be committed to Git.

Example `.gitignore`:

```gitignore
disk
*.o
```

---

# Recommended Testing Procedure

For more consistent results:

1. Close applications performing heavy disk I/O.
2. Choose the filesystem you actually want to test.
3. Make sure enough free space is available.
4. Run the benchmark multiple times.
5. Compare repeated measurements.
6. Monitor SSD temperature if possible.
7. Allow the drive to cool between tests when necessary.
8. Keep the benchmark size and system configuration consistent.

Example:

```bash
./disk 10 /mnt/ssd-test
./disk 10 /mnt/ssd-test
./disk 10 /mnt/ssd-test
```

Do not treat a single run as an absolute specification of the storage device.

---

# Safety and Usage Notes

This program performs real writes to the selected filesystem.

A large benchmark can:

- Generate substantial disk writes
- Consume SSD endurance
- Generate heat
- Temporarily occupy a large amount of storage space
- Affect other applications using the same storage device

Use a dedicated test directory when possible.

Do not run the program against a location containing important data if you are unsure about the workload.

The benchmark removes its temporary files after each test, but an interrupted process can leave files behind.

---

# Possible Future Improvements

Possible extensions include:

- Sequential read benchmark
- Random read benchmark
- Mixed read/write workloads
- Configurable block size
- Configurable queue depth
- Multiple worker threads
- Asynchronous I/O
- `io_uring` support
- Latency measurement
- p50/p95/p99 latency reporting
- Multiple iterations
- Warm-up phase
- Runtime-based testing
- Configurable random seed
- JSON output
- CSV output
- SSD temperature monitoring
- SMART information
- Better target-filesystem space detection
- Device identification
- Optional raw block-device testing

For serious storage benchmarking, results should be compared against a mature tool such as `fio`.

---

# Technical API Overview

The project demonstrates several low-level Linux/POSIX APIs.

| API | Purpose |
|---|---|
| `open()` | Open files and device pseudo-files |
| `close()` | Close file descriptors |
| `read()` | Read zero/random source data |
| `write()` | Sequential writes |
| `pwrite()` | Random-position writes |
| `fsync()` | Synchronize pending file data |
| `fallocate()` | Preallocate file space |
| `unlink()` | Remove temporary files |
| `posix_memalign()` | Allocate aligned buffers |
| `clock_gettime()` | Measure elapsed time |
| `opendir()` | Validate the target directory |

The program also uses C++ standard-library components such as:

```text
std::filesystem
std::vector
std::iota
std::shuffle
std::mt19937_64
```

---

# Why This Project?

This project demonstrates how storage benchmarking can be implemented using low-level system programming instead of relying entirely on high-level libraries.

It provides practical experience with:

- Linux system calls
- File descriptors
- Direct I/O
- Memory alignment
- Filesystems
- Sequential I/O
- Random I/O
- IOPS
- Throughput measurement
- File preallocation
- C++ standard-library containers and algorithms

---

# License

No license is currently specified for this project.

If the project is distributed publicly, add an appropriate open-source license such as MIT, BSD-3-Clause, or GPL-3.0.

---

# Summary

The benchmark measures three storage workloads:

```text
┌─────────────────────────────────────┐
│       Sequential Zero Write         │
│             ↓                       │
│         MB/s / GB/s                 │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│    Sequential Random-Data Write     │
│             ↓                       │
│         MB/s / GB/s                 │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│        4 KiB Random Write           │
│             ↓                       │
│        IOPS / MB/s                  │
└─────────────────────────────────────┘
```

It is a compact example of implementing a Linux storage benchmark with C++ and low-level POSIX I/O.
