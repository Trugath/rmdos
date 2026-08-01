/* Fixed Disk option ROM equates (C800:) */
.equ HDC_DATA, 0x320
.equ HDC_STATUS, 0x321
.equ HDC_SELECT, 0x322
.equ HDC_MASK, 0x323

.equ HDC_STA_READY, 0x01
.equ HDC_STA_INPUT, 0x02
.equ HDC_STA_CMD, 0x04
.equ HDC_STA_REQ, 0x10
.equ HDC_STA_IRQ, 0x20

.equ PORT_PIC_CMD, 0x20
.equ PORT_PIC_DATA, 0x21

.equ PORT_DMA_CH3_ADDR, 0x06
.equ PORT_DMA_CH3_CNT, 0x07
.equ PORT_DMA_FF, 0x0C
.equ PORT_DMA_MODE, 0x0B
.equ PORT_DMA_MASK, 0x0A
.equ PORT_DMA_PAGE_CH3, 0x82

.equ BDA_SEG, 0x0040
.equ BDA_HF_STATUS, 0x74
.equ BDA_HF_COUNT, 0x75

/* Scratch in ROM data segment (patched by host after map). */
.equ GEO_OFF, 0x700
/* Per drive at GEO_OFF: cyl_lo, cyl_hi, heads, spt (4 bytes × 2) */
