#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "procmaps.h"

#ifdef PROCMAPS_TEST
char* procmaps_test_snapshot;
#endif

struct memorymap_vma {
  memorymap_address start;
  memorymap_address end;
  const char* permissions;
  memorymap_offset offset;
  const char* file;
};

struct memorymap {
  char* snapshot;
  size_t nr_maps;
  struct memorymap_vma vma[];
};

/**
 * If a + b does not overflow, SATADD(r, a, b) performs *r = a + b and
 * evaluates to false.  If a + b does overflow, *r is set to its
 * maximum value and SATADD evalutes to true.  All types must be
 * unsigned.
 */
#define SATADD(r, a, b)                         \
    ({                                          \
        typedef __typeof((*(r))) xT;            \
        xT xa = (a);                            \
        xT xb = (b);                            \
        int overflow;                           \
        if (((xT) -1) - xa < xb) {              \
            overflow = 1;                       \
            *(r) = ((xT) -1);                   \
        } else {                                \
            overflow = 0;                       \
            *(r) = xa + xb;                     \
        }                                       \
                                                \
        overflow;                               \
    })

/**
 * Read up to MAXIMUM bytes from FD into BUF.  On failure, return -1.
 * On success, return 0 with the total number of bytes read in
 * *ACTUAL.  *ACTUAL is less than MAXIMUM if we saw EOF before reading
 * MAXIMUM bytes.
 */
static
int
xread(int fd, void* buf, size_t maximum, size_t* actual)
{
  size_t total_read = 0;
  ssize_t actually_read = 0;
  size_t to_read;

  do {
    total_read += actually_read;
    to_read = maximum - total_read;
    if (SSIZE_MAX < to_read) {
      to_read = SSIZE_MAX;
    }

    do {
      actually_read = read(fd, (char*) buf + total_read, to_read);
    } while (actually_read < 0 && errno == EINTR);
  } while (actually_read > 0);

  *actual = total_read;
  return actually_read < 0 ? -1 : 0;
}

/**
 * Read file descriptor FD until we read EOF.  On success, *TOTAL_READ
 * contains the number of bytes we read.  Fail with EFBIG if read(2)
 * fails or if *TOTAL_READ would overflow.
 */
static
int
read_to_eof(int fd, size_t* total_read)
{
  size_t l_total_read = 0;
  size_t this_read = 0;
  char sink[1024];

  // No, we can't just lseek: procfs files don't support SEEK_END.

  do {
    if (SATADD(&l_total_read, l_total_read, this_read)) {
      errno = EFBIG;
      return -1;
    }

    if (xread(fd, sink, sizeof (sink), &this_read) < 0) {
      return -1;
    }
  } while (this_read > 0);

  *total_read = l_total_read;
  return 0;
}

/**
 * Read from a file descriptor.  Write up to the first BUFSZ bytes we
 * read into BUF, but keep reading until we see EOF.  Write the total
 * number of bytes we read (including the ones we copied into BUF)
 * into *TOTAL_READ.
 *
 * Return 0 on success or -1 on failure with errno set.  Fail with
 * EFBIG if *TOTAL_READ would overflow.
 */
static
int
read_and_drain(int fd, void* buf, size_t bufsz, size_t* total_read)
{
  size_t normal_read = 0;
  size_t eof_read = 0;
  size_t l_total_read;

  if (bufsz > 0) {
    if (xread(fd, buf, bufsz, &normal_read) < 0) {
      return -1;
    }
  }

  if (normal_read == bufsz) {
    if (read_to_eof(fd, &eof_read) < 0) {
      return -1;
    }
  }

  if (SATADD(&l_total_read, normal_read, eof_read)) {
    errno = EFBIG;
    return -1;
  }

  *total_read = l_total_read;
  return 0;
}

/**
 * Read the entire contents of FD into an allocated buffer using one
 * read(2) call, backing up to the start of the file and retrying
 * until we have a buffer large enough to hold the file's entire
 * contents.
 *
 * Use this routine to read files from /proc in order to get an atomic
 * snapshot of their contents.
 *
 * On success, return 0 with *CONTENTS receiving a pointer to the
 * buffer we allocated and *SIZE receiving the number of bytes in the
 * buffer.
 *
 * If *SIZE is less than SIZE_MAX, *CONTENTS is followed by a trailing
 * NUL byte, making it safe (when *SIZE is less than SIZE_MAX) to
 * treat the returned buffer as a C string.
 *
 * GUESS is the size of the initial buffer we allocate.
 *
 * On failure, return -1.
 */
static
int
atomic_read_fd(int fd, size_t guess, void** contents, size_t* size)
{
  size_t total_read = guess;
  char* buf = NULL;
  char* new_buf;
  int ret = -1;
  size_t bufsz;

  do {
    bufsz = total_read;
    new_buf = realloc(buf, bufsz < SIZE_MAX ? bufsz + 1 : bufsz);
    if (new_buf == NULL) {
      goto out;
    }

    buf = new_buf;
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1) {
      goto out;
    }

    if (read_and_drain(fd, buf, bufsz, &total_read) < 0) {
      goto out;
    }
  } while (bufsz < total_read);

  if (total_read < SIZE_MAX) {
    buf[total_read] = '\0';
    // Return to the heap allocator any memory we didn't use
    new_buf = realloc(buf, total_read + 1);
    if (new_buf == NULL) { // How ?!
      goto out;
    }

    buf = new_buf;
  }

  *contents = buf;
  *size = total_read;
  buf = NULL;
  ret = 0;

  out:

  free(buf);
  return ret;
}

/**
 * Atomically read the maps file for process PID; on success, return
 * malloc-allocated buffer containing the snapshot; on failure, return
 * NULL with errno set.
 */
static
char*
read_proc_maps_snapshot(pid_t pid)
{
  char* ret = NULL;
  char* snapshot = NULL;
  size_t snapshot_length;
  int mapsfd = -1;
  char mapsname[sizeof ("/proc/18446744073709551615/maps")];
  static const size_t bufsz_guess = 32 * 1024 * 1024;

#ifdef PROCMAPS_TEST
  if (procmaps_test_snapshot) {
    return strdup(procmaps_test_snapshot);
  }
#endif

  snprintf(mapsname,
           sizeof (mapsname),
           "/proc/%lu/maps",
           (unsigned long) pid);

  mapsfd = open(mapsname, O_RDONLY | O_CLOEXEC);
  if (mapsfd < 0) {
    goto out;
  }

  if (atomic_read_fd(mapsfd,
                     bufsz_guess,
                     (void**) &snapshot,
                     &snapshot_length))
  {
    goto out;
  }

  if (snapshot_length == SIZE_MAX) {
    errno = EFBIG;
    goto out;
  }

  ret = snapshot;
  snapshot = NULL;

  out:

  if (mapsfd != -1) {
    close(mapsfd);
  }

  free(snapshot);
  return ret;
}

/**
 * Count the number of times character C appears in STR.
 */
static
size_t
count_char(const char* str, char c)
{
  size_t count = 0;
  while ((str = strchr(str, c))) {
    count += 1;
    str = str + 1;
  }

  return count;
}

/**
 * Allocate a memorymap large enough to hold NR_MAPS maps; return NULL
 * on failure with errno set.
 */
static
struct memorymap*
memorymap_allocate(size_t nr_maps)
{
  struct memorymap* map;
  size_t vma_sz = sizeof (struct memorymap_vma);
  size_t alloc_sz;

  alloc_sz = sizeof (*map);
  if (nr_maps > (SIZE_MAX - alloc_sz) / vma_sz) {
    errno = EFBIG;
    return NULL;
  }

  alloc_sz += nr_maps * vma_sz;
  return calloc(1, alloc_sz);
}

struct memorymap*
memorymap_snapshot(pid_t pid)
{
  struct memorymap* map = NULL;
  struct memorymap* ret = NULL;
  struct memorymap_vma* vma;
  char* snapshot = NULL;
  size_t nr_maps;
  char* line_saveptr;
  char* line;

  snapshot = read_proc_maps_snapshot(pid);
  if (snapshot == NULL) {
    goto out;
  }

  nr_maps = count_char(snapshot, '\n');
  map = memorymap_allocate(nr_maps + 1);
  if (map == NULL) {
    goto out;
  }

  _Static_assert(sizeof (vma->start) == sizeof (unsigned long long), "!");
  _Static_assert(sizeof (vma->end) == sizeof (unsigned long long), "!");
  _Static_assert(sizeof (vma->offset) == sizeof (unsigned long long), "!");

  vma = &map->vma[0];
  for (line = strtok_r(snapshot, "\n", &line_saveptr);
       line != NULL;
       line = strtok_r(NULL, "\n", &line_saveptr))
  {
    int permissions_start_offset = -1;
    int permissions_end_offset = -1;
    int file_start_offset = -1;
    char* file;
    sscanf(line, "%llx-%llx %n%*s%n %llx %*s %*s %n",
           (unsigned long long*) &vma->start,
           (unsigned long long*) &vma->end,
           &permissions_start_offset,
           &permissions_end_offset,
           (unsigned long long*) &vma->offset,
           /* &inode, */
           /* &size, */
           &file_start_offset);

    if (file_start_offset == -1) {
      errno = EINVAL;
      goto out;
    }

    vma->permissions = line + permissions_start_offset;
    line[permissions_end_offset] = '\0';
    file = line + file_start_offset;
    if (*file != '\0') {
      vma->file = file;
    }

    ++vma;
  }

  map->nr_maps = nr_maps;
  map->snapshot = snapshot;
  snapshot = NULL;
  ret = map;
  map = NULL;

  out:

  if (map != NULL) {
    memorymap_destroy(map);
  }

  free(snapshot);
  return ret;
}

const struct memorymap_vma*
memorymap_find(const struct memorymap* map,
                   memorymap_address addr)
{
  const struct memorymap_vma* found = NULL;
  const struct memorymap_vma* vma = &map->vma[0];
  size_t imin = 0, imid = 0, imax = map->nr_maps;
  while (imin < imax) {
    imid = imin + (imax - imin - 1) / 2;
    if (addr < vma[imid].start) {
      imax = imid;
    } else if (vma[imid].end <= addr) {
      imin = imid + 1;
    } else {
      found = &vma[imid];
      break;
    }
  }

  return found;
}

const struct memorymap_vma*
memorymap_first_vma(const struct memorymap* map)
{
  return map->vma[0].permissions ? &map->vma[0] : NULL;
}

const struct memorymap_vma*
memorymap_vma_next(const struct memorymap_vma* vma)
{
  ++vma;
  return vma->permissions ? vma : NULL;
}

memorymap_address
memorymap_vma_start(const struct memorymap_vma* vma)
{
  return vma->start;
}

memorymap_address
memorymap_vma_end(const struct memorymap_vma* vma)
{
  return vma->end;
}

const char*
memorymap_vma_permissions(const struct memorymap_vma* vma)
{
  return vma->permissions;
}

memorymap_offset
memorymap_vma_offset(const struct memorymap_vma* vma)
{
  return vma->offset;
}

const char*
memorymap_vma_file(const struct memorymap_vma* vma)
{
  return vma->file;
}

void
memorymap_destroy(struct memorymap* map)
{
  free(map->snapshot);
  free(map);
}

memorymap_address
memorymap_pointer_to_address(const volatile void* x)
{
  return (memorymap_address) (uintptr_t) x;
}
