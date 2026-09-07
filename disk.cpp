#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <limits.h>
#include <numeric>
#include <random>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace std;

#define BUFF_SIZE (4ULL * 1024ULL * 1024ULL)
#define IOPS_BLOCK_SIZE (4096ULL)
#define IOPS_TOTAL_OPS (100000ULL)
#define DIRECT_IO_ALIGNMENT (4096ULL)

unsigned long long total_bytes;

double get_time(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int write_all(int fd, const char *buffer, size_t count) {
  size_t total_written = 0;
  while (total_written < count) {
    ssize_t n = write(fd, buffer + total_written, count - total_written);
    if (n == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    total_written += (size_t)n;
  }
  return 0;
}

void print_progress(const char *name, unsigned long long current,
                    unsigned long long total) {
  static int dots = 0;
  int percentage = 0;
  if (total > 0)
    percentage = (int)((current * 100ULL) / total);
  dots = (dots + 1) % 4;

  printf("\rWriting %s", name);
  for (int i = 0; i < dots; i++)
    printf(".");
  for (int i = dots; i < 3; i++)
    printf(" ");
  printf(" %3d%%", percentage);
  fflush(stdout);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <size_in_GB> <path_to_folder>\n", argv[0]);
    return 1;
  }

  DIR *dir = opendir(argv[2]);
  if (dir == NULL) {
    perror("opendir");
    return 1;
  }
  closedir(dir);

  char *endptr;
  errno = 0;
  double gb_input = strtod(argv[1], &endptr);
  if (errno != 0 || *endptr != '\0' || gb_input <= 0.0) {
    fprintf(stderr, "Error: Invalid size in GB specified.\n");
    return 1;
  }

  unsigned long long total_bytes =
      static_cast<unsigned long long>(gb_input * 1024.0 * 1024.0 * 1024.0);

  filesystem::space_info info = filesystem::space("/");

  printf("Debug - Requested: %llu bytes\n", total_bytes);
  printf("Debug - Available: %llu bytes\n", (unsigned long long)info.available);

  if (total_bytes > (unsigned long long)info.available) {
    printf("Not enough space available\n");
    return -1;
  }

  unsigned long long raw_bytes =
      (unsigned long long)(gb_input * 1024.0 * 1024.0 * 1024.0);

  total_bytes =
      (raw_bytes + DIRECT_IO_ALIGNMENT - 1ULL) & ~(DIRECT_IO_ALIGNMENT - 1ULL);

  unsigned long long chunks = total_bytes / BUFF_SIZE;
  unsigned long long remainder = total_bytes % BUFF_SIZE;

  printf("\n");
  printf("=====================================\n");
  printf("         DISK WRITE BENCHMARK\n");
  printf("=====================================\n");
  printf("Input Size : %.3f GB\n", gb_input);
  printf("Total Bytes: %llu bytes (Aligned for O_DIRECT)\n", total_bytes);
  printf("Buffer Size: %llu KB\n", BUFF_SIZE / 1024ULL);
  printf("Directory  : %s\n", argv[2]);
  printf("Chunks     : %llu (Remainder: %llu bytes)\n", chunks, remainder);
  printf("=====================================\n\n");

  int fd_zero = open("/dev/zero", O_RDONLY);
  int fd_random = open("/dev/urandom", O_RDONLY);

  if (fd_zero == -1 || fd_random == -1) {
    perror("open /dev/zero or /dev/urandom");
    return 1;
  }

  char zero_path[PATH_MAX];
  char random_path[PATH_MAX];
  char iops_path[PATH_MAX];

  snprintf(zero_path, sizeof(zero_path), "%s/zero.bin", argv[2]);
  snprintf(random_path, sizeof(random_path), "%s/random.bin", argv[2]);
  snprintf(iops_path, sizeof(iops_path), "%s/iops.bin", argv[2]);

  char *zero_buffer = NULL;
  char *random_buffer = NULL;

  if (posix_memalign((void **)&zero_buffer, DIRECT_IO_ALIGNMENT, BUFF_SIZE) !=
          0 ||
      posix_memalign((void **)&random_buffer, DIRECT_IO_ALIGNMENT, BUFF_SIZE) !=
          0) {
    perror("posix_memalign");
    return 1;
  }

  read(fd_random, random_buffer, BUFF_SIZE);

  printf("Starting sequential zero write test...\n");
  int fd_zero_new =
      open(zero_path, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
  if (fd_zero_new == -1) {
    perror("open zero.bin (O_DIRECT)");
    return 1;
  }

  double zero_start = get_time();
  read(fd_zero, zero_buffer, BUFF_SIZE);

  for (unsigned long long i = 0; i < chunks; i++) {
    if (write_all(fd_zero_new, zero_buffer, BUFF_SIZE) == -1) {
      perror("write zero.bin");
      break;
    }
    if (i % (chunks / 100ULL + 1ULL) == 0)
      print_progress("zeroes", i + 1, chunks);
  }

  if (remainder > 0) {
    if (write_all(fd_zero_new, zero_buffer, (size_t)remainder) == -1) {
      perror("write zero remainder");
    }
  }

  fsync(fd_zero_new);
  double zero_time = get_time() - zero_start;
  double zero_mb = (double)total_bytes / (1024.0 * 1024.0);
  double zero_speed = zero_mb / zero_time;

  close(fd_zero_new);
  unlink(zero_path);
  printf("\rWriting zeroes... 100%%\nZero write complete. Speed: %.2f MB/s "
         "(%.2f GB/s)\n\n",
         zero_speed, zero_speed / 1024.0);

  printf("Starting sequential random write test...\n");
  int fd_random_new =
      open(random_path, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
  if (fd_random_new == -1) {
    perror("open random.bin (O_DIRECT)");
    return 1;
  }

  double random_start = get_time();
  for (unsigned long long i = 0; i < chunks; i++) {
    if (write_all(fd_random_new, random_buffer, BUFF_SIZE) == -1) {
      perror("write random.bin");
      break;
    }
    if (i % (chunks / 100ULL + 1ULL) == 0)
      print_progress("random data", i + 1, chunks);
  }

  if (remainder > 0) {
    if (write_all(fd_random_new, random_buffer, (size_t)remainder) == -1) {
      perror("write random remainder");
    }
  }

  fsync(fd_random_new);
  double random_time = get_time() - random_start;
  double random_mb = (double)total_bytes / (1024.0 * 1024.0);
  double random_speed = random_mb / random_time;

  close(fd_random_new);
  unlink(random_path);
  printf("\rWriting random data... 100%%\nRandom write complete. Speed: %.2f "
         "MB/s (%.2f GB/s)\n\n",
         random_speed, random_speed / 1024.0);

  printf("Starting 4KB Random Write IOPS test...\n");

  char *iops_buffer = NULL;
  if (posix_memalign((void **)&iops_buffer, DIRECT_IO_ALIGNMENT,
                     IOPS_BLOCK_SIZE) != 0) {
    perror("posix_memalign IOPS");
    return 1;
  }
  read(fd_random, iops_buffer, IOPS_BLOCK_SIZE);

  int fd_iops = open(iops_path, O_RDWR | O_CREAT | O_TRUNC | O_DIRECT, 0644);

  if (fd_iops == -1) {
    perror("open iops.bin (O_DIRECT)");
    return 1;
  }

  if (fallocate(fd_iops, 0, 0, (off_t)total_bytes) != 0) {
    perror("fallocate iops.bin");
  }

  unsigned long long max_4k_blocks = total_bytes / IOPS_BLOCK_SIZE;

  if (max_4k_blocks > 0) {
    vector<unsigned long long> block_indices(max_4k_blocks);
    iota(block_indices.begin(), block_indices.end(), 0ULL);

    mt19937_64 rng(1337);
    shuffle(block_indices.begin(), block_indices.end(), rng);

    unsigned long long total_iops_to_run =
        min((unsigned long long)IOPS_TOTAL_OPS, max_4k_blocks);

    double iops_start = get_time();
    for (unsigned long long i = 0; i < total_iops_to_run; i++) {
      off_t offset = (off_t)block_indices[i] * IOPS_BLOCK_SIZE;

      if (pwrite(fd_iops, iops_buffer, IOPS_BLOCK_SIZE, offset) !=
          (ssize_t)IOPS_BLOCK_SIZE) {
        perror("pwrite IOPS");
        break;
      }

      if (i % (total_iops_to_run / 100ULL + 1ULL) == 0)
        print_progress("4K IOPS", i + 1, total_iops_to_run);
    }

    fsync(fd_iops);
    double iops_time = get_time() - iops_start;

    double calculated_iops = (double)total_iops_to_run / iops_time;
    double iops_mbps = (calculated_iops * IOPS_BLOCK_SIZE) / (1024.0 * 1024.0);

    printf("\r4KB Random Writes... 100%%\n\n");

    printf("=====================================\n");
    printf("               RESULTS\n");
    printf("=====================================\n");
    printf("Seq Zeroes Write   : %.2f MB/s (%.2f GB/s)\n", zero_speed,
           zero_speed / 1024.0);
    printf("Seq Random Write   : %.2f MB/s (%.2f GB/s)\n", random_speed,
           random_speed / 1024.0);
    printf("-------------------------------------\n");
    printf("4K Random IOPS     : %.0f IOPS\n", calculated_iops);
    printf("4K Random Bandwidth: %.2f MB/s\n", iops_mbps);
    printf("=====================================\n");
  } else {
    printf("\nFile size too small (< 4096 bytes) for 4KB IOPS test.\n");
  }

  close(fd_iops);
  unlink(iops_path);

  close(fd_zero);
  close(fd_random);
  free(zero_buffer);
  free(random_buffer);
  free(iops_buffer);

  return 0;
}
