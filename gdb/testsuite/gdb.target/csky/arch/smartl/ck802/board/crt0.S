/*
 * crt0.S -- startup codes for CSKY.
 *
 * Define a exception vector table
 *
 * Initial PSR register.
 * Initial VBR register.
 * Initial sp(stack pointer) register.
 * Initial MGU (for csky having MGU)
 * Initial Cache.
 * Initial SPM (for csky having SPM).
 * Initial BSS section to "0"
 * Then goto "firstc".
 *
 * Copyright (C) 2014  Hangzhou C-SKY Microsystems Co., Ltd
 */

#include "board.h"

.file "crt0.S"

/*
 * From "board.h"
 */
	/* MGU & CACHE */
/* CONFIG_CKCPU_MGU_BLOCKS	---- MGU block priority setting value */
/* CONFIG_CKCPU_MGU_REGION(1-4)	---- MGU (1-4) block base address and size. */
/* CONFIG_CKCPU_ICACHE		---- Instruction cache enable */
/* CONFIG_CKCPU_DCACHE		---- Data cache enable */
	/* SPM  */
/* CONFIG_CKCPU_SPM		---- CSKY having SPM ? */
/* CONFIG_CKCPU_SPM_ENABLE	---- SPM Enable */
/* CONFIG_CKCPU_ISPM_MODE	---- Instruction SPM Mode(cache/local memory) */
/* CONFIG_CKCPU_DSPM_MODE	---- Data SPM Mode (cache/local memory) */
/* CONFIG_CKCPU_ISPM_ADDRESS	---- Instruction SPM external memory base address,
				     Just for Local Memory Mode */
/* CONFIG_CKCPU_DSPM_ADDRESS	---- Data SPM external memory base address, Just
				     for Local Memory Mode */
/* CONFIG_CKCPU_ISPM_TRANSFER	---- The content in Instruction SPM must be 
				     initialed, Just for Local Memory Mode */
/* CONFIG_CKCPU_DSPM_TRANSFER	---- The content in Data SPM must be initialed,
				     Just for Local Memory Mode */

/*
 * For importing variable or functions from script file "csky.ld".
 */
.import __stack		/* The stack pointer from linking script file */
.import	__sbss		/* The end of bss from linking script file */
.import __ebss		/* The end of bss from linking script file */
/*
 * For importing variable or functions from other c or assemble files.
 */
.import firstc	/* The c codes entry */


/* For exporting varialbe or functions */
.export	csky_vsr_table	/* Vector table base address. */
.export __start		/* The system entry. */


.data
/* Vector table space. */
.align 10
csky_vsr_table:
.long   __start
.rept 127
.long default_exception_handler
.endr

/* The csky startup codes. */
.text
	/*
	 *	
	 * This is the codes first entry point. This is where it all
	 * begins...
	 */
__start:
	/* Init psr value */
	lrw	r1, 0x80000100
	mtcr	r1, psr
	
	/*
	 * Setup initial vector base table for interrupts and exceptions
	 */
	lrw	r1, csky_vsr_table
	mtcr	r1, vbr
	
	/* Initialize the stack pointer from the linker definition. */
	lrw	r7, __stack
	mov	sp, r7 

	/*****************************
	 * Initial CSKY MGU Module ***
	 *****************************/

	/* priority setting, ff -- read and write */
	lrw	r7, CONFIG_CKCPU_MGU_BLOCKS	/* MGU block priority setting value for second block */
	mtcr	r7, cr19

   	/* The first block (0x00000000 ~ 0xFFFFFFFF) */
	movi	r10, 0
	mtcr	r10, cr21
#if CONFIG_CKCPU_MGU_REGION1
	lrw	r7, CONFIG_CKCPU_MGU_REGION1	/* MGU (1) block base address(0x0) and size(4G) */
	mtcr	r7, cr20
#else
	mtcr	r10, cr20
#endif //CONFIG_CKCPU_MGU_REGION1

	/* The second block */
	movi	r10, 1
	mtcr	r10, cr21
#if CONFIG_CKCPU_MGU_REGION2
	lrw	r7, CONFIG_CKCPU_MGU_REGION2	/* MGU (2) block base address and size */
	mtcr	r7, cr20
#else
	movi	r7, 0
	mtcr	r7, cr20
#endif //CONFIG_CKCPU_MGU_REGION2

	/* The third block */
	movi	r10, 2
	mtcr	r10, cr21
#if CONFIG_CKCPU_MGU_REGION3
	lrw	r7, CONFIG_CKCPU_MGU_REGION3	/* MGU (3) block base address and size */
	mtcr	r7, cr20
#else
	movi	r7, 0
	mtcr	r7, cr20
#endif //CONFIG_CKCPU_MGU_REGION3

	/* The forth block  */
	movi	r10, 3
	mtcr	r10, cr21
#if CONFIG_CKCPU_MGU_REGION4
	lrw	r7, CONFIG_CKCPU_MGU_REGION4	/* MGU (4) block base address and size */
	mtcr	r7, cr20
#else
	movi	r7, 0
	mtcr	r7, cr20
#endif //CONFIG_CKCPU_MGU_REGION4

	/* Enable MGU */
	mfcr	r7, cr18
	bclri	r7, 0		/* Enable MGU */
	bclri	r7, 1
	bclri	r7, 2		/* Disable instruction cache */
	bclri	r7, 3		/* Disable Data cache */
	bseti   r7, 4           /* WB: write back */
	bseti   r7, 5           /* RS: return stack */
	bseti   r7, 6		/* Enable Branch prediction */
	mtcr	r7, cr18


	/*
	 * Set CPU internal cache.
	 */
#if CONFIG_CKCPU_ICACHE
        movi    r7, 0x11
        mtcr    r7, cr17	/* Invalid Instruction/Data cache content */

        /* Enable Instruction/Data cache  */
        mfcr    r7, cr18
        bseti   r7, 2     	/* Enable Instruction cache  */
	//bclri	r7, 2		/* Disable instruction cache */
        mtcr    r7, cr18
#endif // CONFIG_CKCPU_ICACHE

#if CONFIG_CKCPU_DCACHE
        movi    r7, 0x12
        mtcr    r7, cr17	/* Invalid Instruction/Data cache content */

        mfcr    r7, cr18
        bseti   r7, 3     	/* Enable Data cache  */
	//bclri	r7, 3		/* Disable Data cache */
        mtcr    r7, cr18
#endif // CONFIG_CKCPU_DCACHE

	/*
	 * Initial CSKY FPU Module
	 */
#ifdef CONFIG_CKCPU_FPU
        cpseti 1
        movi   r7, 0x7
        mtcr   r7, cr15
        movi   r7, 0  	//FPC control register initial value,not care all fpu exception
        cpwcr  r7, cpcr1
#endif //CONFIG_CKCPU_FPU

	/*
	 * Zero out the bss region.
	 */
	lrw	r1, __sbss	/* Get start of bss from linking script file */
	lrw	r2, __ebss	/* Get end of bss from linking script file */
	subu	r2, r1		/* Calculate size of bss */
	lsri	r2, 2		/* Size of whole words */
	cmpnei	r2, 0
	bf	__goto_c
	movi	r3, 0		/* Set zero value to write */
__clear_bss:
	stw	r3, (r1)	/* Zero next word */
	addi	r1, 4		/* Increase bss pointer */
	decne	r2		/* Decrease counter */
	bt	__clear_bss	/* Repeat for all bss */

	/*
	 * Assember start up done, start code proper.
	 * C codes start here!
	 */
__goto_c:
#ifdef CONFIG_CKCPU_SIGN
	jbsr	__sign__firstc
#else
	jbsr	main
#endif
	
	/* Should never get here. */
__exit:
#ifdef CONFIG_CKCPU_QEMU
	lrw     r4, 0x10002000
	mov     r5, a0
	stw     r5, (r4, 0)
#else
	bkpt
#endif
	br	__exit

default_exception_handler:
        br      default_exception_handler
        rte
