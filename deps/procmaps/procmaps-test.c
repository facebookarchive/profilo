#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdbool.h>
#include "procmaps.h"

extern char* procmaps_test_snapshot;

static void
print_vma(FILE* to, const struct memorymap_vma* vma)
{
  if (vma == NULL) {
    fprintf(to, "[NULL vma]\n");
  } else {
    fprintf(to,
            "%016llx - %016llx [%s] [%s]\n",
            (unsigned long long) memorymap_vma_start(vma),
            (unsigned long long) memorymap_vma_end(vma),
            memorymap_vma_permissions(vma),
            memorymap_vma_file(vma));
  }
}

int
main(int argc, char** argv)
{
  pid_t pid = getpid();
  char** arg = &argv[1];
  memorymap_address addr;
  bool have_addr = false;
  char* prgname = basename(argv[0]);
  struct memorymap* mm = NULL;
  const struct memorymap_vma* vma;
  unsigned i;

  if (*arg) {
    pid = atoi(argv[1]);
    ++arg;
  }

  if (*arg) {
    if (strcmp(*arg, "live") != 0) {
      static char buf[32768] = { 0 };
      if (read(open(*arg, O_RDONLY), buf, sizeof (buf)-1) == -1) {
        fprintf(stderr, "%s: could not read \"%s\": %s\n",
                prgname, *arg, strerror(errno));
        return 1;
      }

      procmaps_test_snapshot = buf;
    }
    ++arg;
  }

  if (*arg) {
    char* endptr = NULL;
    errno = 0;
    addr = strtoull(*arg, &endptr, 16);
    if (errno != 0 || *endptr != '\0') {
      fprintf(stderr, "%s: invalid address \"%s\"\n", prgname, *arg);
      return 1;
    }
    have_addr = true;
    ++arg;
  }

  if (*arg) {
    fprintf(stderr, "%s: too many arguments\n", prgname);
    return 1;
  }

  mm = memorymap_snapshot(pid);
  if (mm == NULL) {
    fprintf(stderr, "%s: memorymap_snapshot(%d) failed: %s",
            prgname,
            (int) pid,
            strerror(errno));
    return 1;
  }

  if (have_addr) {
    printf("%016llx: ", (unsigned long long) addr);
    print_vma(stdout, memorymap_find(mm, addr));
  } else {
    for (i = 0, vma = memorymap_first_vma(mm);
         vma != NULL;
         vma = memorymap_vma_next(vma), ++i)
    {
      printf("%02u: ", i);
      print_vma(stdout, vma);
    }
  }

  memorymap_destroy(mm);
  return 0;
}
