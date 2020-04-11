#pragma once

enum DspCoffSectFlag : u32
{
    //DSP_COFF_SECT_FLAG_DATASECT        = 0,
    DSP_COFF_SECT_FLAG_CODESECT        = (1 << 0),
    DSP_COFF_SECT_FLAG_MAP_IN_CODE_MEM = (1 << 3),
    DSP_COFF_SECT_FLAG_MAP_IN_DATA_MEM = (1 << 4),
    DSP_COFF_SECT_FLAG_NOLOAD_CODE_MEM = (1 << 5),
    DSP_COFF_SECT_FLAG_NOLOAD_DATA_MEM = (1 << 6),
    DSP_COFF_SECT_FLAG_BLK_HDR         = (1 << 7),
    DSP_COFF_SECT_FLAG_LINE_DEBUG      = (1 << 9)
};

// enum DspCoffSymNum
// {
//     DSP_COFF_SYM_NUM_UNDEFINED = 0,
//     DSP_COFF_SYM_NUM_DEBUG     = 0xFFFE,
//     DSP_COFF_SYM_NUM_ABSOLUTE  = 0xFFFF
// };

// enum DspCoffSymType : u16
// {
//     DSP_COFF_SYM_TYPE_NULL = 0,
//     DSP_COFF_SYM_TYPE_VOID,
//     DSP_COFF_SYM_TYPE_CHAR,
//     DSP_COFF_SYM_TYPE_SHORT,
//     DSP_COFF_SYM_TYPE_INT,
//     DSP_COFF_SYM_TYPE_LONG,
//     DSP_COFF_SYM_TYPE_FLOAT,
//     DSP_COFF_SYM_TYPE_DOUBLE,
//     DSP_COFF_SYM_TYPE_STRUCT,
//     DSP_COFF_SYM_TYPE_UNION,
//     DSP_COFF_SYM_TYPE_ENUM,
//     DSP_COFF_SYM_TYPE_MOE,
//     DSP_COFF_SYM_TYPE_BYTE,
//     DSP_COFF_SYM_TYPE_WORD,
//     DSP_COFF_SYM_TYPE_UINT,
//     DSP_COFF_SYM_TYPE_DWORD
// };

// enum DspCoffSymDType
// {
//     DSP_COFF_SYM_DTYPE_NULL = 0,
//     DSP_COFF_SYM_DTYPE_POINTER,
//     DSP_COFF_SYM_DTYPE_FUNCTION,
//     DSP_COFF_SYM_DTYPE_ARRAY
// };

struct dsp_coff_header_t
{
    u16 magic;
    u16 nrSections;
    u32 timeStamp;
    u32 symTabPtr;
    u32 nrSyms;
    u16 optHdrLength;
    u16 flags;
};

static_assert(sizeof(dsp_coff_header_t) == 20);

union dsp_coff_name_t
{
    u8 name[8];
    struct
    {
        u32 zeroIfLong;
        u32 longNameOffset;
    };    
};

static_assert(sizeof(dsp_coff_name_t) == 8);

struct dsp_coff_section_t
{
    dsp_coff_name_t name;
    u32 codeAddr;
    u32 dataAddr;
    u32 size;
    u32 sectionPtr;
    u32 relocationPtr;
    u32 lineDebugPtr;
    u16 nrRelocs;
    u16 nrLines;
    DspCoffSectFlag flags;
};

static_assert(sizeof(dsp_coff_section_t) == 40);

// struct dsp_coff_symbol_t
// {
//     dsp_coff_symbol_name_t name;
//     u32 value;
//     u16 sectionId;
//     DspCoffSymType type;
//     u8 storage;
//     u8 followCount;
//     u16 padding;
// };

// static_assert(sizeof(dsp_coff_symbol_t) == 18);