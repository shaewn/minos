#ifndef KERNEL_FDT_H_
#define KERNEL_FDT_H_

#include "types.h"

struct fdt_header {
    /* This field shall contain the value 0xd00dfeed (big-endian). */
    uint32_t magic;

    /* This field shall contain the total size in bytes of the devicetree data structure. This size
     * shall encompass all sections of the structure: the header, the memory reservation block,
     * structure block and strings block, as well as any free space gaps between the blocks or after
     * the final block. */
    uint32_t totalsize;

    /* This field shall contain the offset in bytes of the structure block (see section 5.4) from
     * the beginning of the header. */
    uint32_t off_dt_struct;

    /* This field shall contain the offset in bytes of the strings block (see section 5.5) from the
     * beginning of the header. */
    uint32_t off_dt_strings;

    /* This field shall contain the offset in bytes of the memory reservation block (see
     * section 5.3) from the beginning of the header. */
    uint32_t off_mem_rsvmap;

    /* This field shall contain the version of the devicetree data structure. The version is 17 if
     * using the structure as defined in this document. An DTSpec boot program may provide the
     * devicetree of a later version, in which case this field shall contain the version number
     * defined in whichever later document gives the details of that version. */
    uint32_t version;

    /* This field shall contain the lowest version of the devicetree data structure with which the
     * version used is backwards compatible. So, for the structure as defined in this document
     * (version 17), this field shall contain 16 because version 17 is backwards compatible with
     * version 16, but not earlier versions. As per section 5.1, a DTSpec boot program should
     * provide a devicetree in a format which is backwards compatible with version 16, and thus this
     * field shall always contain 16. */
    uint32_t last_comp_version;

    /* This field shall contain the physical ID of the system’s boot CPU. It shall be identical to
     * the physical ID given in the reg property of that CPU node within the devicetree. */
    uint32_t boot_cpuid_phys;

    /* This field shall contain the length in bytes of the strings block section of the devicetree
     * blob. */
    uint32_t size_dt_strings;

    /* This field shall contain the length in bytes of the structure block section of the devicetree
     * blob. */
    uint32_t size_dt_struct;
};

/* reserved memory region */
struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

#define FDT_MAGIC 0xd00dfeed
#define FDT_MAGIC_BE 0xd00dfeed
#define FDT_MAGIC_LE 0xedfe0dd0

#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

#endif
