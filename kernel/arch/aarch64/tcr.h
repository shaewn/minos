#ifndef AARCH64_TCR_H_
#define AARCH64_TCR_H_

#define TCR_T0SZ_START 0
#define TCR_T0SZ_END 7
#define TCR_IRGN0_START 8
#define TCR_IRGN0_END 9
#define TCR_ORGN0_START 10
#define TCR_ORGN0_END 11
#define TCR_SH0_START 12
#define TCR_SH0_END 13
#define TCR_TG0_START 14
#define TCR_TG0_END 15

#define TCR_T1SZ_START (TCR_T0SZ_START + 16)
#define TCR_T1SZ_END (TCR_T0SZ_END + 16)
#define TCR_IRGN1_START (TCR_IRGN0_START + 16)
#define TCR_IRGN1_END (TCR_IRGN0_END + 16)
#define TCR_ORGN1_START (TCR_ORGN0_START + 16)
#define TCR_ORGN1_END (TCR_ORGN0_END + 16)
#define TCR_SH1_START (TCR_SH0_START + 16)
#define TCR_SH1_END (TCR_SH0_END + 16)
#define TCR_TG1_START (TCR_TG0_START + 16)
#define TCR_TG1_END (TCR_TG0_END + 16)

#define TCR_RGN_NO_CACHE 0

/* write-back, read-allocate, write-allocate */
#define TCR_RGN_WB_RA_WA 1

/* write-through, read-allocate, no write-allocate */
#define TCR_RGN_WT_RA_NWA 2

/* write-back, read-allcoate, no write-allocate */
#define TCR_RGN_WB_RA_NWA 3

/* shift by TCR_SH0_START or TCR_SH1_START */

#define TCR_SH_NON_SHAREABLE 0
#define TCR_SH_OUTER_SHAREABLE 2
#define TCR_SH_INNER_SHAREABLE 3

#define TCR_TG_4KB 0
#define TCR_TG_64KB 1
#define TCR_TG_16KB 2

#define TCR_HA (1ULL << 39)
#define TCR_HD (1ULL << 40)

#endif
