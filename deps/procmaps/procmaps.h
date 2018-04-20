// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
#define MEMORYMAP_EXTERN_C_START extern "C" {
#define MEMORYMAP_EXTERN_C_END }
#else
#define MEMORYMAP_EXTERN_C_START
#define MEMORYMAP_EXTERN_C_END
#endif

MEMORYMAP_EXTERN_C_START;

struct memorymap;
struct memorymap_vma;

typedef uint64_t memorymap_address;

/**
 * Read a consistent snapshot of a processes's memory mappings.  PID
 * is the ID of the process to examine; on success, return a memory
 * mapping structure for that process.  The snapshot is immutable and
 * multiple threads may access it.  The caller is responsible for
 * destroying the snapshot with memorymap_destroy.
 *
 * On failure, return NULL with errno set.
 */
struct memorymap* memorymap_snapshot(pid_t pid);

/**
 * Find the memory map containing ADDR.  Return NULL if no map
 * contains ADDR.
 */
const struct memorymap_vma* memorymap_find(
  const struct memorymap* map,
  memorymap_address addr);

/**
 * Return the first VMA in the map, or NULL if the map contains no
 * VMAs.
 */
const struct memorymap_vma* memorymap_first_vma(
  const struct memorymap* map);

/**
 * Return the VMA that occurs in memory ordering after VMA or NULL if
 * there is no next VMA.
 */
const struct memorymap_vma* memorymap_vma_next(
  const struct memorymap_vma* vma);

/**
 * Return START. This VMA covers addresses in the range [START, END).
 */
memorymap_address memorymap_vma_start(const struct memorymap_vma* vma);

/**
 * Return END. This VMA covers addresses in the range [START, END).
 */
memorymap_address memorymap_vma_end(const struct memorymap_vma* vma);

/**
 * Return the permissions for this VMA.  Is is a NUL-terminated four
 * character string in the same format used in /proc/pid/maps, e.g.,
 * "rw-p".  The memory map object owns the string, which is valid
 * until the memorymap object is destroyed.
 */
const char* memorymap_vma_permissions(const struct memorymap_vma* vma);

/**
 * Return the name of the file backing VMA as a NUL-terminated string;
 * return NULL if this information is unavailable.  The memory map
 * object owns the string, which is valid until the memorymap object
 * is destroyed.
 */
const char* memorymap_vma_file(const struct memorymap_vma* vma);

/**
 * Destroy a memory map object MAP.  MAP may not be NULL.
 */
void memorymap_destroy(struct memorymap* map);

/**
 * Correctly convert a pointer to a memorymap_address.
 */
memorymap_address memorymap_pointer_to_address(const volatile void* x);

MEMORYMAP_EXTERN_C_END;
