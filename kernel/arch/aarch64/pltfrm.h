#ifndef KERNEL_AARCH64_PLATFORM_H_
#define KERNEL_AARCH64_PLATFORM_H_

#define LOG_PAGE_SIZE 12
#define PAGE_SIZE (1 << LOG_PAGE_SIZE)

#define KERNEL_VIRT_BEGIN 0xffff000000000000

// 192 gives access to tables via 0xffff600000000000 through 0xffff607fffffffff
#define RECURSIVE_INDEX 192

#endif
