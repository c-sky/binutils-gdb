/*
 * Description: ck5a6.h - Define the system configuration, memory & IO base
 * address, flash size & address, interrupt resource for ck5a6 soc.
 *
 * Copyright (C) : 2008 Hangzhou C-SKY Microsystems Co.,LTD.
 * Author(s): Liu Bing (bing_liu@c-sky.com)
 * Contributors: Liu Bing
 * Date:  2010-06-26
 * Modify by liu jirang  on 2012-09-11
 */
#ifndef __INCLUDE_BOARD_H
#define __INCLUDE_BOARD_H

//#include "config.h"

/**************************************
 * MCU & Borads.
 *************************************/

/* PLL input ckock(crystal frequency) */
#define CONFIG_PLL_INPUT_CLK   48000000   /* HZ */
/* CPU frequence definition */
#define CPU_DEFAULT_FREQ       48000000  /* Hz */
/* AHB frequence definition */
#define AHB_DEFAULT_FREQ       48000000   /* Hz */
/* APB frequence definition */
#define APB_DEFAULT_FREQ       48000000   /* Hz */


#define APB_FREQ APB_DEFAULT_FREQ
#define CPU_FREQ CPU_DEFAULT_FREQ

#define SYS_FREQ CPU_FREQ
/**********************************************
 * Config CPU cache
 *********************************************/
/* 0 - rw; 1 - rwc; 2 - rwc; 3 - rw */
#define CONFIG_CKCPU_MGU_BLOCKS         0xff06

/* 0 - baseaddr: 0x0; size: 4G */
#define CONFIG_CKCPU_MGU_REGION1  	0x3f
/* 1- baseaddr: 0x8000000; size: 8M */
#define CONFIG_CKCPU_MGU_REGION2	0x00000025
/* 2- baseaddr: 0x8600000; size:  */
#define CONFIG_CKCPU_MGU_REGION3	0x20000025
/* 3- Disable */
#undef CONFIG_CKCPU_MGU_REGION4

/*******************************
 * Config CPU cache
 ******************************/
#define CONFIG_CKCPU_ICACHE             0
#define CONFIG_CKCPU_DCACHE             0

/************************************************
 * perpheral module baseaddress and irq number
 ***********************************************/
/**** AHB BUS ****/
#define CK_AHBBUS_BASE			((volatile CK_UINT32*)(0x40001000))


/***** Intc  ******/
#undef CK_INTC_BASEADDRESS
/***** VIC ******/
#define CK_VIC_BASEADDRESS		(0xe000e100)  // CKS_INTC 

/*
 * define irq number of perpheral modules
 */

#define  CK_INTC_UART0          0
#define  CK_INTC_CORETIM        1
#define  CK_INTC_TIM1     	2
#define  CK_INTC_TIM2     	3
#define  CK_INTC_TIM3     	4
#define  CK_INTC_TIM4     	5

#define CK_UART0_IRQID      CK_INTC_UART0
#define CK_CORETIM_IRQID    CK_INTC_CORETIM
#define CK_TIMER0_IRQ       CK_INTC_TIM1
#define CK_TIMER1_IRQ       CK_INTC_TIM2
#define CK_TIMER2_IRQ       CK_INTC_TIM3
#define CK_TIMER3_IRQ       CK_INTC_TIM4

/***** GPIO *****/
#undef 	CK_GPIO_ADDRBASE 

/**** MAC *******/

/***** Uart *******/
#define CK_UART0_ADDRBASE    		(volatile CK_UINT32 *)(0x40015000)

/**** CoreTimer  ****/
#define  CK_CORETIM_BASEADDR          	(volatile CK_UINT32 *)(0xe000e010)
#define  CK_CORETIM_CSR			(CK_CORETIM_BASEADDR)
#define  CK_CORETIM_CURRENTREG		(CK_CORETIM_BASEADDR + 2)

#define  CK_CORETIM_LOADCOUNT          	0xffffff

/**** Timer ****/
#define  CK_TIMER0_BASSADDR		(volatile CK_UINT32 *)(0x40011000)
#define  CK_TIMER1_BASSADDR		(volatile CK_UINT32 *)(0x40011014)
#define  CK_TIMER2_BASSADDR		(volatile CK_UINT32 *)(0x40011028)
#define  CK_TIMER3_BASSADDR		(volatile CK_UINT32 *)(0x4001103c)

#define  CK_SYS_TIMER                   TIMER0
#define  CK_SYSTIME_LOADCOUNTREG	(CK_TIMER0_BASSADDR)
#define  CK_SYSTIME_CURRENTREG		(CK_TIMER0_BASSADDR + 1)
#define  CK_SYSTIME_CTRREG		(CK_TIMER0_BASSADDR + 2)
#define  CK_SYSTIME_EOIREG		(CK_TIMER0_BASSADDR + 3)                 
#define  CK_SYSTIME_LOADCOUNT          	0xffffff

/****** DMA *****/

/****** IIC *************/

/****** SPI *************/
#undef 	CK_SPI_ADDRBASE0	

/****** WDT *************/

/****** RTC *************/ 

/****** PWM  *************/
#undef 	CK_PWM_ADDRBASE0

/****** LCDC  *************/

/****** POWM  *************/

/****** NFC  *************/

/****** AC97  *************/

/****** SDHC  *************/

/****** USBD  *************/

/****** USBH  *************/

/**** Nor FLASH ****/


#endif /* __INCLUDE_BOARD_H */
