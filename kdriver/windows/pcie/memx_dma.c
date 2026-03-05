/**
 * @file memx_dma.c
 * @author Rock Lin (rock.lin@memryx.com)
 * @brief This file contains the dma transition
 * @version 0.1
 * @date 2025
 *
 * @copyright Copyright (c) 2025 MemryX. Inc. All Rights Reserved.
 *
 */

#include "memx_dma.h"

#define DMAC_CH_SETREG(devContext, chip_id, reg_offset, dma_ch, value) MemxUtilXflowWrite(devContext, chip_id, (ARM_DMA_BASE+(0x100*ch)+(reg_offset)), 0, value, TO_CONFIG_OUTPUT)
#define DMAC_CH_GETREG(devContext, chip_id, reg_offset, dma_ch)        MemxUtilXflowRead(devContext,  chip_id, (ARM_DMA_BASE+(0x100*ch)+(reg_offset)), 0, TO_CONFIG_OUTPUT)
#define DMAC_SETREG(devContext, chip_id, reg_offset, value)            MemxUtilXflowWrite(devContext, chip_id, (ARM_DMA_BASE+(reg_offset)), 0, value, TO_CONFIG_OUTPUT)
#define DMAC_GETREG(devContext, chip_id, reg_offset)                   MemxUtilXflowRead(devContext,  chip_id, (ARM_DMA_BASE+(reg_offset)), 0, TO_CONFIG_OUTPUT)

void memx_dma_transfer_mb(PDEVICE_CONTEXT devContext, UCHAR chip_id, enum ARMDMA_CH ch, ULONG src_addr, ULONG dst_addr, ULONG size_bytes)
{
	struct ARMDMA_LL_MEM *pArmLlp;
	ULONG tmp, i;
	ULONGLONG chx_ctl_reg_val;
	ULONG no,slice,done, descriptor_block_offset;

	descriptor_block_offset = MEMX_DMA_CHIP0_DESC0_OFFSET + (ch * MEXM_DMA_DESC_BLKSZ) + (chip_id * MEMX_DMA_DESC_DESC_COUNT_PER_CHIP * MEXM_DMA_DESC_BLKSZ);

	pArmLlp = (struct ARMDMA_LL_MEM *)(devContext->CommonBufferBaseDriver[MEMX_DMA_DESC_BUFFER_BLOCK_NUMBER(descriptor_block_offset)] + MEMX_DMA_DESC_BUFFER_BLOCK_OFFSET(descriptor_block_offset));

	DMAC_CH_SETREG(devContext, chip_id, DMAC_CHx_INTSIGNAL_ENABLE0_REG_OFS, ch, 0xFFFFFFFE);
	DMAC_SETREG(devContext, chip_id, DMAC_CFG_REG_OFS, (ARM_DMAC_EN|ARM_INT_EN));

	/* [1:0]SRC_MULTBLK_TYPE = 3, [3:2]DST_MULTBLK_TYPE=3 */
	tmp = 0xF;
	DMAC_CH_SETREG(devContext, chip_id, DMAC_CHx_CFG0_REG_OFS, ch, tmp);

	/* [3]HS_SEL_SRC=1 [4]HS_SEL_DST=1 [19:17]CH_PRIOR=1 [26:23]SRC_OSR_LMT=15 [30:27]DST_OSR_LMT=15 */
	tmp = 0x7F820018;
	DMAC_CH_SETREG(devContext, chip_id, DMAC_CHx_CFG1_REG_OFS, ch, tmp);

	DMAC_CH_SETREG(devContext, chip_id, DMAC_CHx_LLP0_REG_OFS, ch, MEMX_DMA_DESC_CHIPADDR(ch, 0, chip_id));

	/* [0]SMS=1 [2]DMS=1 [4]SINC=0 [6]DINC=0 [10:8]SRC_TR_WIDTH=2 [13:11]DST_TR_WIDTH=2 [17:14]SRC_MSIZE=3 [21:18]DST_MSIZE=3
	   [38][6]ARLEN_EN=1 [46:39][14:7]ARLEN=15 [47][15]AWLEN_EN=1 [55:48][23:16]AWLEN=15 [58][26]IOC_BlkTfr=1 [63][31]SHADOWREG_OR_LLI_VALID=1 */
	chx_ctl_reg_val = 0x840F87C0000CD205ull;

	no   = (size_bytes+SB_SIZE-1)/SB_SIZE;
	done = 0;
	for (i = 0; i < no; i++) {

		if((size_bytes - done) > SB_SIZE)
			slice = SB_SIZE;
		else
			slice = (size_bytes - done);
		
		pArmLlp[i].CHx_SAR 		 = (ULONGLONG)(src_addr + done);
		pArmLlp[i].CHx_DAR 		 = (ULONGLONG)(dst_addr + done);
		pArmLlp[i].CHx_BLOCK_TS  = (slice>>2)-1;
		pArmLlp[i].CHx_CTL 		 = chx_ctl_reg_val;
		pArmLlp[i].CHx_SSTAT 	 = 0;
		pArmLlp[i].CHx_DSTAT 	 = 0;
		pArmLlp[i].CHx_LLP_STATUS= 0;	
		if ( i== (no-1)) {
			pArmLlp[i].CHx_LLP   = 0;
			pArmLlp[i].CHx_CTL  |= (0x1ull << 62);
		} else {
			pArmLlp[i].CHx_LLP   = MEMX_DMA_DESC_CHIPADDR(ch, (i+1), chip_id);
		}
		done += slice;
	}

	DMAC_SETREG(devContext, chip_id, DMAC_CHEN0_REG_OFS, (0x101<< ch));
}
