/*
 * crt0.S -- startup codes for CKCORE.
 *
 * Define a exception vector table
 *
 * Initial PSR register.
 * Initial VBR register.
 * Initial r0(stack pointer) register.
 * Initial MGU (for ckcore having MGU)
 * Initial Cache.
 * Initial SPM (for ckcore having SPM).
 * Initial BSS section to "0"
 * Then goto "start_main".
 *
 * Copyright (C) 2008  Hangzhou C-SKY Microsystems Co., Ltd
 */

//#include "tests/arch/ckconfig.h"

.file "crt0.S"

/*
 * From "ckconfig.h"
 */
	/* MGU & CACHE */
/* CONFIG_CK510_MGU_BLOCKS	---- MGU block priority setting value */
/* CONFIG_CK510_MGU_REGION(1-4)	---- MGU (1-4) block base address and size. */
/* CONFIG_CK510_ICACHE		---- Instruction cache enable */
/* CONFIG_CK510_DCACHE		---- Data cache enable */
	/* SPM  */
/* CONFIG_CKCORE_SPM		---- CKCORE having SPM ? */
/* CONFIG_CKCORE_SPM_ENABLE	---- SPM Enable */
/* CONFIG_CKCORE_ISPM_MODE	---- Instruction SPM Mode(cache/local memory) */
/* CONFIG_CKCORE_DSPM_MODE	---- Data SPM Mode (cache/local memory) */
/* CONFIG_CKCORE_ISPM_ADDRESS	---- Instruction SPM external memory base address,
				     Just for Local Memory Mode */
/* CONFIG_CKCORE_DSPM_ADDRESS	---- Data SPM external memory base address, Just
				     for Local Memory Mode */
/* CONFIG_CKCORE_ISPM_TRANSFER	---- The content in Instruction SPM must be 
				     initialed, Just for Local Memory Mode */
/* CONFIG_CKCORE_DSPM_TRANSFER	---- The content in Data SPM must be initialed,
				     Just for Local Memory Mode */

/*
 * For importing variable or functions from script file "ckcore.ld".
 */
.import __stack		/* The stack pointer from linking script file */
.import	__sbss		/* The end of bss from linking script file */
.import __ebss		/* The end of bss from linking script file */
/*
 * For importing variable or functions from other c or assemble files.
 */
.import start_main	/* The c codes entry */
.import spm_init        /* Initial ckcore SPM */


/* For exporting varialbe or functions */
.export	__vbrbase	/* Vector table base address. */
.export __start		/* The system entry. */


.data
/* Vector table space. */
.align 10
__vbrbase:
.rept 128
.long __default_exception_handler
.endr

/* The ckcore startup codes. */
.text
	/*
	 *	
	 * This is the codes first entry point. This is where it all
	 * begins...
	 */
#if CONFIG_SYS_SPARK
.long	__start
#endif
__start:
	/* Init psr value */
	lrw	r1, 0x80000100
	mtcr	r1, psr
	
	/*
	 * Setup initial vector base table for interrupts and exceptions
	 */
	lrw	r1, __vbrbase
	mtcr	r1, vbr
	
	/* Initialize the stack pointer from the linker definition. */
	lrw	r7, __stack
	mov	sp, r7 

	/*
	 * Disable MMU & Cache.
	 */
//	mfcr	r7, cr<18, 0>
//	bclri	r7, 0
//	bclri	r7, 1
//	bclri	r7, 2
//	bclri	r7, 3
//	bclri	r7, 6
//	mtcr	r7, cr<18, 0>

    /* Enable MMU */
    //invalid mmu jtlb
    movi    r1, 0
    bseti   r1, 26
    mtcr    r1, cr8
 
    //creat a mapping between VPN:0 and PFN:0
    lrw     r1, 0x1e
    mtcr    r1, cr2           //MELO
    lrw     r1, 0x101e
    mtcr    r1, cr3           //MEL1
    lrw     r1, 0
    mtcr    r1, cr4           //MEH
    mtcr    r1, cr6           //MPR
    bseti   r1, 28
    mtcr    r1, cr8           //MCIR	

	/* Enable MGU */
	mfcr	r7, cr18
	bseti	r7, 0
	bclri	r7, 1
	mtcr	r7, cr18
	jmpi	MMU_OK
MMU_OK:
	/* Enable jumping predict. */
	mfcr	r7, cr18
	bseti	r7, 6
	bseti	r7, 5
	bseti	r7, 4
	mtcr	r7, cr18

	/*
	 * Set CPU internal cache.
	 */
	/* Invalid Instruction/Data cache content */
	mfcr	r7, cr17
	bseti	r7, 0
	bseti	r7, 1
	bclri	r7, 2
	bseti	r7, 4
	bseti	r7, 16
	bseti	r7, 17
	mtcr	r7, cr17
	/* Enable instruction cache */
	mfcr	r7, cr18
	bseti	r7, 2
	mtcr	r7, cr18

	/* 
	 * Set Data cache 
	 */
	/* Enable Data cache */
	mtcr	r7, cr17
	mfcr	r7, cr18
	bseti	r7, 3
	mtcr	r7, cr18
//#ifdef CONFIG_CSKY_FPU
//        cpseti 1
//        movi   r7, 0x7
//        mtcr   r7, cr15
//        movi   r7, 0  /*FPC control register initial value,not care all fpu exception*/
//        cpwcr  r7, cpcr1
//#endif


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
	lrw		r29, __stext
	lrw 	r28, __sdata
	jbsr	main
/*	jbsr	firstc */
/*	jsri	firstc  */
	
	/* Should never get here. */

__exit:
#ifdef CONFIG_CKCPU_QEMU
        lrw     r4, 0x10002000
        mov     r5, a0
        stw     r5, (r4, 0)
#else
        bkpt
#endif
        br      __exit

//__exit:
//#if CONFIG_SYS_SPARK
//	lrw	r4, 0x8007c50
//	lrw	r5, 0x2002
//	stw	r5, (r4, 0)
//	stw	r5, (r4, 0)
//#else
//    bkpt
//#endif
//	br	__exit

abort:
__GI_abort:
_abort:
    movi  r2, 1
    rsubi r2, 0
    bkpt
    br _abort

__default_exception_handler:
    bkpt
	br	__default_exception_handler
	rte
