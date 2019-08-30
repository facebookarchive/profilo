#if !defined(ANDROID) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE  1 // needed for mremap on host linux
#endif
#include <sys/mman.h>

#include <sys/stat.h>
#include <errno.h>

#include <procmaps.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

char* procmaps_test_snapshot; // testing only

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

// we really only need sizeof(size_t) extra space, but 16-byte alignment is sometimes a thing, so
// let's say the heck with it and cache-line-align the returned pointer. we're operating on PAGESIZE
// chunks anyway; the odds of us saving a page on the end by squeezing our padding is low.
#define XALLOC_PADDING  64

static
int
xfree(void* mem)
{
  if (mem) {
    void* raw = (char*)mem - XALLOC_PADDING;
    return munmap(raw, *(size_t*)raw);
  } else {
    return 0;
  }
}

static
void*
xrealloc(void* orig, size_t sz)
{
  size_t map_sz;
  if (SATADD(&map_sz, sz, XALLOC_PADDING)) {
    return NULL;
  }

  if (sz == 0) {
    xfree(orig);
    return NULL;
  }

  char* raw;

  if (orig) {
    raw = (char*)orig - XALLOC_PADDING;
    raw = (char*)mremap(raw, *(size_t*)raw, map_sz, MREMAP_MAYMOVE);
  } else {
    raw = (char*)mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }

  if (raw == MAP_FAILED) {
    return NULL;
  } else {
    *(size_t*)raw = map_sz;
    return raw + XALLOC_PADDING;
  }
}

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
    new_buf = (char*)xrealloc(buf, bufsz < SIZE_MAX ? bufsz + 1 : bufsz);
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
    // Return to the kernel any memory we didn't use
    new_buf = (char*)xrealloc(buf, total_read + 1);
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

  xfree(buf);
  return ret;
}

static
size_t
build_proc_path(char* path, size_t max_len, pid_t pid)
{
  size_t pos = 0;
  const char pre[] = {'/', 'p', 'r', 'o', 'c', '/'};
  const char post[] = {'/', 'm', 'a', 'p', 's'};

  int digits = 1;
  pid_t tmp = pid;
  while (tmp > 9) {
    tmp /= 10;
    digits++;
  }

  // NOTE: sizeof() here is a C trick - sizeof(char*) == <pointer size>, but sizeof(char[10]) == 10.
  // this will _only_ work as long as pre and post are defined as char[] instead of char*
  if (sizeof(pre) + sizeof(post) + digits + 1 <= max_len) {
    for (int i = 0; i < sizeof(pre); i++, pos++) {
      path[pos] = pre[i];
    }

    for (int i = digits - 1; i >= 0; i--) {
      path[pos + i] = (pid % 10) + '0';
      pid /= 10;
    }
    pos += digits;

    for (int i = 0; i < sizeof(post); i++, pos++) {
      path[pos] = post[i];
    }

    path[pos] = '\0';
  }

  return pos;
}

/**
 * Atomically read the maps file for process PID; on success, return
 * allocated buffer containing the snapshot; on failure, return NULL
 * with errno set.
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
  static const size_t bufsz_guess = 32 * 1024 * 1024 - XALLOC_PADDING; // leave room for internal sz

  if (procmaps_test_snapshot) { // testing only
    snapshot_length = 0;
    while (procmaps_test_snapshot[snapshot_length] != '\0') {
      snapshot_length++;
    }
    snapshot = (char*)xrealloc(NULL, snapshot_length + 1);
    for (size_t i = 0; i < snapshot_length; i++) {
      snapshot[i] = procmaps_test_snapshot[i];
    }
    snapshot[snapshot_length] = '\0';
    return snapshot;
  }

  if (!build_proc_path(mapsname, sizeof (mapsname), pid)) {
    goto out;
  }

  mapsfd = open(mapsname, O_RDONLY | O_CLOEXEC, 0);
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

  xfree(snapshot);
  return ret;
}

/**
 * Returns the first occurrence of c in str, or NULL
 */
static
char*
find_first(char* str, char c)
{
  while (*str) {
    if (*str == c) {
      return str;
    }
    str++;
  }

  return NULL;
}

/**
 * Count the number of times character C appears in STR.
 */
static
size_t
count_char(char* str, char c)
{
  size_t count = 0;
  while ((str = find_first(str, c))) {
    count += 1;
    str = str + 1;
  }

  return count;
}

static
char*
parse_hex(char* str, char delim, uint64_t *out_val)
{
  uint64_t val = 0;
  int i = 0;
  #define MAX_HEX_DIGITS (sizeof(val) * 2) /* each hex digit is half a byte */

  while (*str != delim) {
    int digit;

    if (++i > MAX_HEX_DIGITS) {
      return NULL;
    }

    if (*str >= '0' && *str <= '9') {
      digit = *str - '0';
    } else if (*str >= 'a' && *str <= 'f') {
      digit = *str - 'a' + 0xA;
    } else if (*str >= 'A' && *str <= 'F') {
      digit = *str - 'A' + 0xA;
    } else {
      return NULL;
    }

    val = (val << 4) + digit;
    ++str;
  }

  *out_val = val;
  return str;
}

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
  return (struct memorymap*)xrealloc(NULL, alloc_sz);
}

struct memorymap*
memorymap_snapshot(pid_t pid)
{
  struct memorymap* map = NULL;
  struct memorymap* ret = NULL;
  struct memorymap_vma* vma;
  char* snapshot = NULL;
  size_t nr_maps;
  char* line;

  snapshot = read_proc_maps_snapshot(pid);
  if (snapshot == NULL) {
    goto out;
  }

  nr_maps = count_char(snapshot, '\n') + 1; // three \n means four lines total
  map = memorymap_allocate(nr_maps + 1);
  if (map == NULL) {
    goto out;
  }

  _Static_assert(sizeof (vma->start) == sizeof (uint64_t), "!");
  _Static_assert(sizeof (vma->end) == sizeof (uint64_t), "!");
  _Static_assert(sizeof (vma->offset) == sizeof (uint64_t), "!");

  vma = &map->vma[0];
  line = snapshot;
  while (line)
  {
    if (*line == '\0') {
      break;
    }
    line = parse_hex(line, '-', &vma->start);
    if (!line) {
      goto out;
    }
    ++line;

    line = parse_hex(line, ' ', &vma->end);
    if (!line) {
      goto out;
    }
    ++line;

    vma->permissions = line;
    line = find_first(line, ' ');
    if (!line) {
      goto out;
    }
    *line = '\0';
    ++line;

    line = parse_hex(line, ' ', &vma->offset);
    if (!line) {
      goto out;
    }
    ++line;

    // skip past inode
    line = find_first(line, ' ');
    if (!line) {
      goto out;
    }
    ++line;

    // skip past size
    line = find_first(line, ' ');
    if (!line) {
      goto out;
    }
    ++line;

    // skip past whitespace padding before filename
    while (*line == ' ') {
      ++line;
    }
    vma->file = line;
    line = find_first(line, '\n');
    if (line) {
      *line = '\0';
      ++line; // don't blindly increment! if we don't find \n, we're at the real terminator.
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

  xfree(snapshot);
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
memorymap_get_vma(const struct memorymap* map, size_t index) {
  return index < memorymap_size(map) ? &map->vma[index] : NULL;
}

const struct memorymap_vma*
memorymap_vma_next(const struct memorymap_vma* vma)
{
  ++vma;
  return vma->permissions ? vma : NULL;
}

size_t
memorymap_size(const struct memorymap* map) {
  return map->nr_maps;
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
  if (map) {
    xfree(map->snapshot);
    xfree(map);
  }
}

memorymap_address
memorymap_pointer_to_address(const volatile void* x)
{
  return (memorymap_address) (uintptr_t) x;
}
