#include "memtable/allocator.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include <memory>
#include <fmt/core.h>

#include "../slab_allocator/allocator.h"

#if SCONE || 1
#ifndef SYS_untrusted_mmap
#define SYS_untrusted_mmap 1025
#endif

static void * scone_kernel_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  return (void *)syscall(SYS_untrusted_mmap, addr, length, prot, flags, fd, offset);
}

static void * mmap_helper(size_t size) {
  return scone_kernel_mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

#else
static void * mmap_helper(size_t size) {
  return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}
#endif


namespace avocado {

static const size_t pagesize = getpagesize();

std::unique_ptr<slab::UniquePtrWrap<slab::DynamicLockLessAllocator>, AllocatorDeleter> create_allocator(size_t size, size_t element_size, bool warmup) {
  using namespace slab;

  auto remainder = size % pagesize;

  if (remainder != 0) size = size + (size - remainder);

  auto * ptr = (std::byte*)mmap_helper(size);
  if (ptr == MAP_FAILED)
    return nullptr;

  if (warmup) {
    memset(ptr, 0, size);
  }

  auto slab = std::unique_ptr<slab::UniquePtrWrap<slab::DynamicLockLessAllocator>, AllocatorDeleter>(
        new (mmap_helper(pagesize))
          UniquePtrWrap<DynamicLockLessAllocator>(ptr, ptr + size, element_size)
      );
  if (slab == nullptr) {
    munmap(ptr, size);
  }
  return slab;
}

void AllocatorDeleter::operator()(slab::UniquePtrWrap<slab::DynamicLockLessAllocator> * ptr) {
  munmap(ptr->get_region_start(), ptr->get_region_end() - ptr->get_region_start());
  munmap(ptr, pagesize);
}

} //avocado
