/*****************************************************************************
 *                                                                          *
 * C-Sky Microsystems Confidential                                          *
 * -------------------------------                                          *
 * This file and all its contents are properties of C-Sky Microsystems. The *
 * information contained herein is confidential and proprietary and is not  *
 * to be disclosed outside of C-Sky Microsystems except under a             *
 * Non-Disclosure Agreement (NDA).                                          *
 *                                                                          *
 ****************************************************************************/ 



/* File name: csky-djp.h
   function description: proxy structure and error definition. 
                       It is the same in GDB JTAG definition.  */

#ifndef __CKCORE_DEBUGGER_SERVER_DJP_H__
#define __CKCORE_DEBUGGER_SERVER_DJP_H__

#ifndef __CKDATATYPE__
#define __CKDATATYPE__
typedef unsigned int   U32;
typedef int            S32;
typedef unsigned short U16;
typedef short          S16;
typedef unsigned char  U8;
typedef char           S8;
#endif

/* For arch info.  */
#define CSKY_BIG_ENDIAN      1
#define CSKY_LITTLE_ENDIAN   0

#define CSKY_SOCKET_PACKAGE_SIZE 4096

/* Possible djp errors are listed here.  */
enum enum_errors 
{
  /* Codes > 0 are for system errors.  */
  ERR_NONE = 0,
  ERR_CRC = -1,
  ERR_MEM = -2,
  JTAG_PROXY_INVALID_COMMAND = -3,
  JTAG_PROXY_SERVER_TERMINATED = -4,
  JTAG_PROXY_NO_CONNECTION = -5,
  JTAG_PROXY_PROTOCOL_ERROR = -6,
  JTAG_PROXY_COMMAND_NOT_IMPLEMENTED = -7,
  JTAG_PROXY_INVALID_CHAIN = -8,
  JTAG_PROXY_INVALID_ADDRESS = -9,
  JTAG_PROXY_ACCESS_EXCEPTION = -10, /* Write to ROM.  */
  JTAG_PROXY_INVALID_LENGTH = -11,
  JTAG_PROXY_OUT_OF_MEMORY = -12,
  /* CPU in non-debug region, cpu can't enter debug.  */
  JTAG_NON_DEBUG_REGION = -13,
};

/* All JTAG chains.  */
enum jtag_chains
{ 
  SC_GLOBAL,      /* 0 Global BS Chain.  */
  SC_RISC_DEBUG,  /* 1 RISC Debug Interface chain.  */
  SC_RISC_TEST,   /* 2 RISC Test Chain.  */
  SC_TRACE,       /* 3 Trace Chain.  */
  SC_REGISTER,    /* 4 Register Chain.  */
  SC_WISHBONE,    /* 5 Wisbone Chain.  */
  SC_BLOCK        /* Block Chains.  */
};


/* Command definition of Proxy Function.  */
typedef enum   
{
  DBGCMD_REG_READ               = 1,
  DBGCMD_REG_WRITE              = 2,
  DBGCMD_BLOCK_READ_OLD         = 3,
  DBGCMD_BLOCK_WRITE_OLD        = 4,
  DBGCMD_ENTER_DEBUG            = 5,
  DBGCMD_CHECK_DEBUG            = 6,
  DBGCMD_SINGLESTEP             = 7,
  DBGCMD_RUN                    = 8,
  DBGCMD_SYSTEM_RESET           = 9,
  DBGCMD_BLOCK_LOAD_OLD         = 10,
  DBGCMD_INSERT_WATCHPOINT_OLD  = 11,
  DBGCMD_REMOVE_WATCHPOINT_OLD  = 12,
  DBGCMD_MEM_READ               = 13,
  DBGCMD_MEM_WRITE              = 14,
  DBGCMD_INSERT_HW_BREAKPOINT   = 16,
  DBGCMD_REMOVE_HW_BREAKPOINT   = 17,
  DBGCMD_SEND_LOADINFO_OLD      = 18,
  DBGCMD_TRACE_PC               = 19,
  DBGCMD_ENDIAN_INFO            = 20,
  DBGCMD_INSERT_WATCHPOINT      = 21,
  DBGCMD_REMOVE_WATCHPOINT      = 22,
  DBGCMD_SET_PROFILING_INFO     = 23,
  DBGCMD_GET_PROFILING_INFO     = 24,
  DBGCMD_VERSION_INFO           = 25,
  DBGCMD_XREG_READ              = 26,
  DBGCMD_XREG_WRITE             = 27,
  DBGCMD_HAD_REG_READ           = 28,
  DBGCMD_HAD_REG_WRITE          = 29,
  DBGCMD_SYSTEM_SOFT_RESET      = 30,
  DBGCMD_HW_BKPT_NUM            = 31,
  DBGCMD_XML_TDESC_READ         = 32,
  DBGCMD_DEL_ALL_HWBKPT         = 33
} DBG_CMD;



/* Error definition of Proxy Function.  */
typedef enum   
{
  DJP_ERR_NONE = 0,                  /* Success.  */
  DJP_PROTOCOL_ERROR = -6,           /* Command actual length is error.  */
  DJP_COMMAND_NOT_IMPLEMENTED = -7,  /* Command is not implemented.  */
  DJP_OUT_OF_MEMORY = -12,           /* Fail in mallocing memory.  */
  DJP_OPERATION_TIME_OUT = -13,      /* Time out when entering debug mode.  */
  DJP_COMMUNICATION_TIME_OUT = -14,  /* Communication(ICE) time out.  */
  DJP_TARGET_NOT_CONNECTED = -15,    /* No connect with target board.  */
  DJP_REG_NOT_DEFINED = -16,         /* Register ID is not defined.  */
  DJP_ADDRESS_MISALIGNED = -17,      /* Address is not matched with mode.  */
  DJP_DATA_LENGTH_INCONSISTENT = -18,/* Data length error.  */
  DJP_INVALID_BKPT = -19,            /* Third BKPT is not allowed.  */
  DJP_DEL_NONEXISTENT_BKPT = -20,    /* No BKPT in specified address.  */
  DJP_MASK_MISALIGNED = -21,         /* Address isn't cosistent with mask.  */
  DJP_NO_PROFILING = -22,            /* No profilling function in target.  */
  DJP_CPU_ACCESS = -23,              /* Reserved (CPU access right).  */
  DJP_ADDRESS_ACCESS = -24,          /* Reserved (address access right).  */
  DJP_COMMAND_FORMAT = -25,          /* Command format error.  */
  DJP_COMMAND_EXECUTE = -26,         /* Command is not finished.  */
  DJP_NON_DEBUG_REGION = -27,        /* CPU in non-debug region.  */
} PROXY_ERROR;


/*
 * Data Structure Of Proxy Communication
 * Including : 1. receive massage structure
 *             2. send response structure
 */

/* DBGCMD_REG_READ.  */
struct ReadRegMsgStruct
{
  U32 command;  /* Read reg.  */
  U32 length;   /* Length of frame.  */
  U32 address;  /* No. of reg.  */
};
typedef struct ReadRegMsgStruct ReadRegMsg;

struct ReadRegRspStruct
{
  S32 status;
  U32 data_H;
  U32 data_L;
};
typedef struct ReadRegRspStruct ReadRegRsp;

/* DBGCMD_HAD_REG_READ.  */
struct ReadHadRegMsgStruct
{
  U32 command;    /* Read reg.  */
  U32 length;     /* Length of frame.  */
  U32 address;    /* No. of reg.  */
};
typedef struct ReadHadRegMsgStruct ReadHadRegMsg;

struct ReadHadRegRspStruct
{
  S32 status;
  U32 data;
};
typedef struct ReadHadRegRspStruct ReadHadRegRsp;

/* DBGCMD_HW_BKPT_NUM.  */
struct HwBkptNumMsgStruct
{
  U32 command;
  U32 length;
};
typedef struct HwBkptNumMsgStruct HwBkptNumMsg;

struct HwBkptNumRspStruct
{
  S32 status;
  U32 data1;
  U32 data2;
};
typedef struct HwBkptNumRspStruct HwBkptNumRsp;

/* DBGCMD_HAD_REG_WRITE.  */
struct WriteHadRegMsgStruct
{
  U32 command;    /* Write reg.  */
  U32 length;     /* Length of frame.  */
  U32 address;    /* No. of reg.  */
  U32 data;
};
typedef struct WriteHadRegMsgStruct WriteHadRegMsg;

struct WriteHadRegRspStruct
{
  S32 status;
};
typedef struct WriteHadRegRspStruct WriteHadRegRsp;

/* DBGCMD_REG_WRITE.  */
struct WriteRegMsgStruct
{
  U32 command;  /* Write reg.  */
  U32 length;   /* Length of frame.  */
  U32 address;  /* No. of reg.  */
  U32 data_H;
  U32 data_L;
};
typedef struct WriteRegMsgStruct WriteRegMsg;

struct WriteRegRspStruct
{
  S32 status;
};
typedef struct WriteRegRspStruct WriteRegRsp;

/* DBGCMD_MEM_READ.  */
struct ReadMemMsgStruct
{
  U32 command;  /* Read memory: 13.  */
  U32 length;   /* Length of frame.  */
  U32 address;  /* Write address.  */
  U32 nLength;  /* Length of data.  */
  U32 nSize;    /* Data Unit.  */
};
typedef struct ReadMemMsgStruct ReadMemMsg;

struct ReadMemRspStruct
{
  S32 status;
  U32 nLength;  /* Length of data.  */
  U32 nSize;    /* Data Unit.  */
  U8 data[0];   /* Write data pointer.  */
};
typedef struct ReadMemRspStruct ReadMemRsp;

/* DBGCMD_MEM_WRITE.  */
struct WriteMemMsgStruct
{
  U32 command;  /* Write memory: 14.  */
  U32 length;   /* Length of frame.  */
  U32 address;  /* Write address.  */
  U32 nLength;  /* Length of data.  */
  U32 nSize;    /* Data Unit.  */
  U8  data[0];  /* Write data pointer.  */
};
typedef struct WriteMemMsgStruct WriteMemMsg;

struct WriteMemRspStruct
{
  S32 status;
};
typedef struct WriteMemRspStruct WriteMemRsp;

/* DBGCMD_ENTER_DEBUG.  */
struct EnterDebugMsgStruct
{
  U32 command; 	/* Enter debug mode: 5.  */
  U32 length;   /* Length of frame.  */
  U32 chain;    /* Enter mode.  */
};
typedef struct EnterDebugMsgStruct EnterDebugMsg;

struct EnterDebugRspStruct
{
  S32 status;
};
typedef struct EnterDebugRspStruct EnterDebugRsp;

/* DBGCMD_CHECK_DEBUG.  */
struct CheckDebugMsgStruct
{
  U32 command; 	/* Check debug mode 6.  */
  U32 length;   /* Length of frame.  */
};
typedef struct CheckDebugMsgStruct CheckDebugMsg;

struct CheckDebugRspStruct
{
  S32 status;
  U32 debugmode;
};
typedef struct CheckDebugRspStruct CheckDebugRsp;

/* DBGCMD_SINGLESTEP.  */
struct SingleStepMsgStruct
{
  U32 command;  /* Single step: 7.  */
  U32 length;   /* Length of frame.  */
};
typedef struct SingleStepMsgStruct SingleStepMsg;

struct SingleStepRspStruct
{
  S32 status;
};
typedef struct SingleStepRspStruct SingleStepRsp;

/* DBGCMD_RUN.  */
struct CmdRunMsgStruct
{
  U32 command;  /* Run: 8.  */
  U32 length;   /* Length of frame.  */
};
typedef struct CmdRunMsgStruct CmdRunMsg;

struct CmdRunRspStruct
{
  S32 status;
};
typedef struct CmdRunRspStruct CmdRunRsp;

/* DBGCMD_INSERT_HW_BREAKPOINT.  */
/* DBGCMD_REMOVE_HW_BREAKPOINT.  */
struct HwBkptMsgStruct
{
  U32 command;  /* Insert: 16; remove: 17.  */
  U32 length;   /* Length of frame.  */
  U32 address;
};
typedef struct HwBkptMsgStruct HwBkptMsg;

struct HwBkptRspStruct
{
  S32 status;
};
typedef struct HwBkptRspStruct HwBkptRsp;

#define HW_WRITE   0x5   /* Enable all data write memory insn.  */
#define HW_READ    0x6   /* Enable all data read memory insn.  */
#define HW_ACCESS  0x1   /* Enable all data r/w memory insn.  */
#define HW_EXECUTE 0x2   /* Enable all text fetch insn.  */
#define HW_NONE    0     /* Disable all hareware breakpoint/watchpoint.  */


/* DBGCMD_INSERT_WATCHPOINT.  */
/* DBGCMD_REMOVE_WATCHPOINT.  */
struct WatchpointMsgStruct
{
  U32 command;  /* Set: 21; remove:22.  */
  U32 length;   /* Length of frame.  */
  U32 address;  /* Start address of watchpoint.  */
  U32 mode;     /* Tragger mode.  */
  U32 mask;     /* Address mask.  */
  U32 counter;  /* Tragger after <counter>.  */
};
typedef struct WatchpointMsgStruct WatchpointMsg;

struct WatchpointRspStruct
{
  S32 status;
};
typedef struct WatchpointRspStruct WatchpointRsp;

/* DBGCMD_SYSTEM_RESET.  */
struct SystemResetMsgStruct
{
  U32 command;  /* System reset 9.  */
  U32 length;   /* Length of frame.  */
};
typedef struct SystemResetMsgStruct SystemResetMsg;

struct SystemResetRspStruct
{
  S32 status;
};
typedef struct SystemResetRspStruct SystemResetRsp;

/* DBGCMD_SYSTEM_SOFT_RESET.  */
struct SystemSoftResetMsgStruct
{
  U32 command;   /* System reset 30.  */
  U32 length;    /* Length of frame.  */
  U32 insn;
};
typedef struct SystemSoftResetMsgStruct SystemSoftResetMsg;

struct SystemSoftResetRspStruct
{
  S32 status;
};
typedef struct SystemSoftResetRspStruct SystemSoftResetRsp;

/* FIXME: it must be 8, even if the real pcfifo depth is not 8.  */
#define PCFIFO_DEPTH  8

/* DBGCMD_TRACE_PC.  */
struct PcJumpMsgStruct
{
  U32 command;  /* Jump pc trace : 19.  */
  U32 length;   /* Length of frame.  */
};
typedef struct PcJumpMsgStruct PcJumpMsg;

struct PcJumpRspStruct
{
  S32 status;
  U32 pc[PCFIFO_DEPTH];
};
typedef struct PcJumpRspStruct PcJumpRsp;

/* DBGCMD_SET_PROFILING_INFO.  */
struct SetProfilingMsgStruct
{
  U32 command; 	/* Set profiling info: 23.  */
  U32 length;	/* Length of frame.  */
  U8 data[0];	/* Set information.  */
};
typedef struct SetProfilingMsgStruct SetProfilingMsg;

struct SetProfilingRspStruct
{
  S32 status;
};
typedef struct SetProfilingRspStruct SetProfilingRsp;

/* DBGCMD_GET_PROFILING_INFO.  */
struct GetProfilingMsgStruct
{
  U32 command;  /* Get profiling info : 24.  */
  U32 length;   /* Length of frame.  */
};
typedef struct GetProfilingMsgStruct GetProfilingMsg;

struct GetProfilingRspStruct
{
  S32 status;
  U32 length;   /* Length of data.  */
  U8 data[0];   /* Data.  */
};
typedef struct GetProfilingRspStruct GetProfilingRsp;

/* DBGCMD_VERSION_INFO.  */
struct VersionMsgStruct
{
  U32 command;  /* Version info: 25.  */
  U32 length;   /* Length of frame.  */
};
typedef struct VersionMsgStruct VersionMsg;

struct VersionRspStruct
{
  S32 status;
  U32 srv_version;
  U32 had_version[4];
};
typedef struct VersionRspStruct VersionRsp;

/* DBGCMD_ENDIAN_INFO.  */
struct EndianMsgStruct
{
  U32 command; 	/* Version info: 20.  */
  U32 length;	/* Length of frame.  */
};
typedef struct EndianMsgStruct EndianMsg;

struct EndianRspStruct
{
  S32 status;
  U32 endian;
};
typedef struct EndianRspStruct EndianRsp;

/* ----------------------------------------------------------  */
/* For compatibility (reserve the command in old version).  */


/* DBGCMD_BLOCK_WRITE_OLD.  */
struct WriteMemMsg_oldStruct
{
  U32 command;    /* Write memory: 4(old version).  */
  U32 length;     /* Length of frame.  */
  U32 address;    /* Write address.  */
  U32 nRegisters; /* Write Byte.  */
  U8 data[0];     /* Write data pointer.  */
};
typedef struct WriteMemMsg_oldStruct WriteMemMsg_old;

struct WriteMemRsp_oldStruct
{
  S32 status;
};
typedef struct WriteMemRsp_oldStruct WriteMemRsp_old;

/* DBGCMD_BLOCK_READ_OLD.  */
struct ReadMemMsg_oldStruct
{
  U32 command;    /* Read memory: 3 (old version).  */
  U32 length;     /* Length of frame.  */
  U32 address;    /* Write address.  */
  U32 nRegisters; /* Read Byte.  */
};
typedef struct ReadMemMsg_oldStruct ReadMemMsg_old;

struct ReadMemRsp_oldStruct
{
  S32 status;
  U32 nRegisters; /* Read Byte.  */
  U8 data[0];     /* Write data pointer.  */
};
typedef struct ReadMemRsp_oldStruct ReadMemRsp_old;

/* DBGCMD_BLOCK_LOAD_OLD.  */
struct BlockLoadMsg_oldStruct
{
  U32 command;    /* Block load: 10 (old version).  */
  U32 length;     /* Length of frame.  */
  U32 address;    /* Write address.  */
  U32 nRegisters; /* Write Byte.  */
  U8 data[0];     /* Write data pointer.  */
};
typedef struct BlockLoadMsg_oldStruct BlockLoadMsg_old;

struct BlockLoadRsp_oldStruct
{
  S32 status;
};
typedef struct BlockLoadRsp_oldStruct BlockLoadRsp_old;

/* DBGCMD_INSERT_WATCHPOINT_OLD.  */
/* DBGCMD_REMOVE_WATCHPOINT_OLD.  */
struct WatchpointMsg_oldStruct
{
  U32 command;      /* Set : 11 Remove: 12 (old version).  */
  U32 length;       /* Length of frame.  */
  U32 address;
  U32 mode;         /* Tragger mode.  */
};
typedef struct WatchpointMsg_oldStruct WatchpointMsg_old;

struct WatchpointRsp_oldStruct
{
  S32 status;
};
typedef struct WatchpointRsp_oldStruct WatchpointRsp_old;

/* DBGCMD_SEND_LOADINFO_OLD.  */
struct StartAddrMsg_oldStruct
{
  U32 command;   /* Command: 18 (old version).  */
  U32 length;    /* Length of frame.  */
  U32 address;   /* Start address.  */
};
typedef struct StartAddrMsg_oldStruct StartAddrMsg_old;

struct StartAddrRsp_oldStruct
{
  S32 status;
};
typedef struct StartAddrRsp_oldStruct StartAddrRsp_old;

/* ----------------- Old command version end -------------------  */


/* DBGCMD_XREG_READ.  */
struct ReadXRegMsgStruct
{
  U32 command;  /* Command: 26.  */
  U32 length;   /* Length of frame.  */
  U32 address;  /* New regnum.  */
  U32 size;     /* Length of reg value by byte.  */
};
typedef struct ReadXRegMsgStruct ReadXRegMsg;

struct ReadXRegRspStruct
{
  U32 status;
  U8 data[0];
};
typedef struct ReadXRegRspStruct ReadXRegRsp;

/* DBGCMD_XREG_WRITE.  */
struct WriteXRegMsgStruct
{
  U32 command;  /* Command: 27.  */
  U32 length;   /* Length of frame.  */
  U32 address;  /* New regnum.  */
  U32 size;     /* Length of reg value by byte.  */
  U8 data[0];
};
typedef struct WriteXRegMsgStruct WriteXRegMsg;

struct WriteXRegRspStruct
{
  U32 status;
};
typedef struct WriteXRegRspStruct WriteXRegRsp;

/* DBGCMD_XML_TDESC_READ.  */
struct ReadXmlTdescMsgStruct
{
  U32 command;    /* Read xml-tdesc: 32.  */
  U32 length;     /* Length of frame.  */
  U32 address;    /* Start address.  */
  U32 nLength;    /* Length of data buffer.  */
};
typedef struct ReadXmlTdescMsgStruct ReadXmlTdescMsg;

struct ReadXmlTdescRspStruct
{
  S32 status;
  U32 rlen;      /* Length of data actually get.  */
  U8 data[0];
};
typedef struct ReadXmlTdescRspStruct ReadXmlTdescRsp;

#endif /* __CKCORE_DEBUGGER_SERVER_DJP_H__  */
