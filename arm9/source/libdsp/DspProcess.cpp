#include <nds.h>
#include <string.h>
#include "dsp.h"
#include "dsp_pipe.h"
#include "dsp_coff.h"
#include "dsp_dsp1.h"
#include "DspProcess.h"

static DspProcess* sDspProcess = NULL;

void DspProcess::DspIrqHandler()
{
    if(sDspProcess)
        sDspProcess->HandleDspIrq();
}

void DspProcess::HandleDspIrq()
{
    while(true)
    {
        u32 sources = (REG_DSP_SEM | (((REG_DSP_PSTS >> DSP_PCFG_IE_REP_SHIFT) & 7) << 16)) & _callbackSources;
        if(!sources)
            break;
        while(sources)
        {
            int idx = __builtin_ctz(sources);
            sources &= ~_callbackGroups[idx];
            _callbackFuncs[idx](_callbackArgs[idx]);
        }
    }
}

DspProcess::DspProcess(bool forceDspSyncLoad)
    : _slotB(0), _slotC(0), _codeSegs(0), _dataSegs(0), _flags(forceDspSyncLoad ? DSP_PROCESS_FLAG_SYNC_LOAD : 0),
    _callbackSources(0)
{
    for(int i = 0; i < TWR_WRAM_BC_SLOT_COUNT; i++)
    {
        _codeSlots[i] = 0xFF;
        _dataSlots[i] = 0xFF;
    }
    for(int i = 0; i < DSP_PROCESS_CALLBACK_COUNT; i++)
    {
        _callbackFuncs[i] = NULL;
        _callbackArgs[i] = NULL;
        _callbackGroups[i] = 0;
    }
}

bool DspProcess::SetMemoryMapping(bool isCode, u32 addr, u32 len, bool toDsp)
{
    addr = DSP_MEM_ADDR_TO_CPU(addr);
    len = DSP_MEM_ADDR_TO_CPU(len < 1 ? 1 : len);
    int segBits = isCode ? _codeSegs : _dataSegs;
    int start = addr >> TWR_WRAM_BC_SLOT_SHIFT;
    int end = (addr + len - 1) >> TWR_WRAM_BC_SLOT_SHIFT;
    for(int i = start; i <= end; i++)
    {
        if(!(segBits & (1 << i)))
            continue;
        int slot = isCode ? _codeSlots[i] : _dataSlots[i];
        if(isCode)
            twr_mapWramBSlot(slot, toDsp ? TWR_WRAM_B_SLOT_MASTER_DSP_CODE : TWR_WRAM_B_SLOT_MASTER_ARM9, toDsp ? i : slot, true);
        else
            twr_mapWramCSlot(slot, toDsp ? TWR_WRAM_C_SLOT_MASTER_DSP_DATA : TWR_WRAM_C_SLOT_MASTER_ARM9, toDsp ? i : slot, true);
    }
    return true;
}

bool DspProcess::EnumerateSectionsCoff(dsp_process_sec_callback_t callback)
{
    if(!callback)
        return false;
    dsp_coff_header_t coffHeader;
    fseek(_dspFile, 0, SEEK_SET);
    if(fread(&coffHeader, 1, sizeof(dsp_coff_header_t), _dspFile) != sizeof(dsp_coff_header_t))
        return false;
    int sectabOffset = sizeof(dsp_coff_header_t) + coffHeader.optHdrLength;
    dsp_coff_section_t section;
    for(int i = 0; i < coffHeader.nrSections; i++)
    {
        fseek(_dspFile, sectabOffset + i * sizeof(dsp_coff_section_t), SEEK_SET);
        if(fread(&section, 1, sizeof(dsp_coff_section_t), _dspFile) != sizeof(dsp_coff_section_t))
            return false;
        if((section.flags & DSP_COFF_SECT_FLAG_BLK_HDR) || section.size == 0)
            continue;
        if(!callback(this, &coffHeader, &section))
            return false;
    }
    return true;
}

bool DspProcess::LoadSectionCoff(const dsp_coff_header_t* header, const dsp_coff_section_t* section)
{
    const char* name = (const char*)section->name.name;
    char fullName[128];
    if(section->name.zeroIfLong == 0)
    {
        fseek(_dspFile, header->symTabPtr + 0x12 * header->nrSyms + section->name.longNameOffset, SEEK_SET);
        fread(fullName, 1, sizeof(fullName), _dspFile);
        name = fullName;
    }

    struct
    {
        bool isCode;
        bool noLoad;
        int address;
    } placements[4];

    int nrPlacements = 0;

    if(!strcmp(name, "SDK_USING_OS@d0"))
        _flags |= DSP_PROCESS_FLAG_SYNC_LOAD;

    if(section->flags & DSP_COFF_SECT_FLAG_MAP_IN_CODE_MEM)
    {
        bool noLoad = section->flags & DSP_COFF_SECT_FLAG_NOLOAD_CODE_MEM;
        if(strstr(name, "@c0"))
        {
            placements[nrPlacements].isCode = true;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->codeAddr);
            nrPlacements++;
        }
        if(strstr(name, "@c1"))
        {
            placements[nrPlacements].isCode = true;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->codeAddr) + TWR_WRAM_BC_SLOT_SIZE * 4;
            nrPlacements++;
        }
    }

    if(section->flags & DSP_COFF_SECT_FLAG_MAP_IN_DATA_MEM)
    {
        bool noLoad = section->flags & DSP_COFF_SECT_FLAG_NOLOAD_DATA_MEM;
        if(strstr(name, "@d0"))
        {
            placements[nrPlacements].isCode = false;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->dataAddr);
            nrPlacements++;
        }
        if(strstr(name, "@d1"))
        {
            placements[nrPlacements].isCode = false;
            placements[nrPlacements].noLoad = noLoad;
            placements[nrPlacements].address = DSP_MEM_ADDR_TO_CPU(section->dataAddr) + TWR_WRAM_BC_SLOT_SIZE * 4;
            nrPlacements++;
        }
    }

    for(int i = 0; i < nrPlacements; i++)
    {
        bool isCode = placements[i].isCode;
        bool noLoad = placements[i].noLoad;
        if(!noLoad)
            fseek(_dspFile, section->sectionPtr, SEEK_SET);
        int dst = placements[i].address;
        int left = section->size;
        while(left > 0)
        {
            int blockEnd = (dst + TWR_WRAM_BC_SLOT_SIZE) & ~(TWR_WRAM_BC_SLOT_SIZE - 1);
            int blockLeft = blockEnd - dst;
            int len = left < blockLeft ? left : blockLeft;
            int seg = dst >> TWR_WRAM_BC_SLOT_SHIFT;
            int slot = isCode ? _codeSlots[seg] : _dataSlots[seg];
            if(slot == 0xFF)
            {
                u16* slots = isCode ? &_slotB : &_slotC;
                int* segs = isCode ? &_codeSegs : &_dataSegs;
                u8* slotMap = isCode ? _codeSlots : _dataSlots;
                slot = __builtin_ctz(*slots);
                if(slot >= TWR_WRAM_BC_SLOT_COUNT)
                    return false;
                slotMap[seg] = slot;
                *slots &= ~(1 << slot);
                *segs |= 1 << seg;
                if(isCode)
                    twr_mapWramBSlot(slot, TWR_WRAM_B_SLOT_MASTER_ARM9, slot, true);
                else
                    twr_mapWramCSlot(slot, TWR_WRAM_C_SLOT_MASTER_ARM9, slot, true);
                memset(DspToArm9Address(isCode, DSP_MEM_ADDR_TO_DSP(seg << TWR_WRAM_BC_SLOT_SHIFT)), 0, TWR_WRAM_BC_SLOT_SIZE);
            }
            if(!noLoad && fread(DspToArm9Address(isCode, DSP_MEM_ADDR_TO_DSP(dst)), 1, len, _dspFile) != len)
                return false;
            left -= len;
            dst += len;
        }
    }
    return true;
}

bool DspProcess::LoadProcessCoff(const char* path, u16 slotB, u16 slotC)
{    
    _slotB = slotB;
    _slotC = slotC;
    _dspFile = fopen(path, "rb");
    if(!_dspFile)
        return false;

    bool ok = EnumerateSectionsCoff(DspProcess::LoadSectionCoff);

    fclose(_dspFile);
    _dspFile = NULL;

    if(!ok)
        return false;

    SetMemoryMapping(true, 0, (TWR_WRAM_BC_SLOT_SIZE * TWR_WRAM_BC_SLOT_COUNT) >> 1, true);
    SetMemoryMapping(false, 0, (TWR_WRAM_BC_SLOT_SIZE * TWR_WRAM_BC_SLOT_COUNT) >> 1, true);
    return true;
}

bool DspProcess::Execute()
{
    int irq = enterCriticalSection();
	{
        sDspProcess = this;
        dsp_initPipe();
        irqSet(IRQ_WIFI, DspProcess::DspIrqHandler);
        SetCallback(DSP_PROCESS_CALLBACK_SEMAPHORE(15) | DSP_PROCESS_CALLBACK_REPLY(DSP_PIPE_CMD_REG), dsp_pipeIrqCallback, NULL);
        irqEnable(IRQ_WIFI);
		dsp_powerOn();
		dsp_setCoreResetOff(_callbackSources >> 16);
		dsp_setSemaphoreMask(~(_callbackSources & 0xFFFF));
        SetupCallbacks();
		//needed for some modules
		if(_flags & DSP_PROCESS_FLAG_SYNC_LOAD)
			for(int i = 0; i < 3; i++)
				while(dsp_receiveData(i) != 1);
        DspProcess::DspIrqHandler();
	}
	leaveCriticalSection(irq);
    return true;
}

bool DspProcess::ExecuteCoff(const char* path, u16 slotB, u16 slotC)
{
    if(sDspProcess)
        return false;
    
    if(!LoadProcessCoff(path, slotB, slotC))
        return false;

    return Execute();
}

bool DspProcess::ExecuteDsp1(const char* path)
{
    if(sDspProcess)
        return false;

    _dspFile = fopen(path, "rb");
    if(!_dspFile)
    {
        printf("Failed opening file\n");
        return false;
    }
    
    dsp_dsp1_t* dsp1 = new dsp_dsp1_t;
    fseek(_dspFile, 0, SEEK_SET);
    if(fread(dsp1, 1, sizeof(dsp_dsp1_t), _dspFile) != sizeof(dsp_dsp1_t))
    {
        printf("Failed reading header\n");
        fclose(_dspFile);
        _dspFile = NULL;
        delete dsp1;
        return false;
    }

    if(dsp1->header.magic != DSP_DSP1_MAGIC)
    {
        printf("Invalid magic!");
        fclose(_dspFile);
        _dspFile = NULL;
        delete dsp1;
        return false;
    }

    _slotB = 0xFF;//dsp1->header.memoryLayout & 0xFF;
    _slotC = 0xFF;//(dsp1->header.memoryLayout >> 8) & 0xFF;

    _codeSegs = 0xFF;
    _dataSegs = 0xFF;

    for(int i = 0; i < TWR_WRAM_BC_SLOT_COUNT; i++)
    {
        _codeSlots[i] = i;
        _dataSlots[i] = i;
        twr_mapWramBSlot(i, TWR_WRAM_B_SLOT_MASTER_ARM9, i, true);
        memset((void*)(twr_getBlockAddress(TWR_WRAM_BLOCK_B) + i * TWR_WRAM_BC_SLOT_SIZE), 0, TWR_WRAM_BC_SLOT_SIZE);
        twr_mapWramCSlot(i, TWR_WRAM_C_SLOT_MASTER_ARM9, i, true);
        memset((void*)(twr_getBlockAddress(TWR_WRAM_BLOCK_C) + i * TWR_WRAM_BC_SLOT_SIZE), 0, TWR_WRAM_BC_SLOT_SIZE);
    }

    if(dsp1->header.flags & DSP_DSP1_FLAG_SYNC_LOAD)
        _flags |= DSP_PROCESS_FLAG_SYNC_LOAD;

    for(int i = 0; i < dsp1->header.nrSegments; i++)
    {
        bool isCode = dsp1->segments[i].segmentType != DSP_DSP1_SEG_TYPE_DATA;
        fseek(_dspFile, dsp1->segments[i].offset, SEEK_SET);
        if(fread(DspToArm9Address(isCode, dsp1->segments[i].address), 1, dsp1->segments[i].size, _dspFile) != dsp1->segments[i].size)
        {
            printf("Failed reading segment\n");
            fclose(_dspFile);
            _dspFile = NULL;
            delete dsp1;
            return false;
        }
    }

    fclose(_dspFile);
    _dspFile = NULL;

    SetMemoryMapping(true, 0, (TWR_WRAM_BC_SLOT_SIZE * TWR_WRAM_BC_SLOT_COUNT) >> 1, true);
    SetMemoryMapping(false, 0, (TWR_WRAM_BC_SLOT_SIZE * TWR_WRAM_BC_SLOT_COUNT) >> 1, true);

    delete dsp1;

    return Execute();
}

void DspProcess::SetCallback(u32 sources, dsp_process_irq_callback_t func, void* arg)
{
    int irq = enterCriticalSection();
	{
        for(int i = 0; i < DSP_PROCESS_CALLBACK_COUNT; i++)
        {
            if(!(sources & (1 << i)))
                continue;
            _callbackFuncs[i] = func;
            _callbackArgs[i] = arg;
            _callbackGroups[i] = sources;
        }
        if(func)
        {
            REG_DSP_PCFG |= ((sources >> 16) & 7) << DSP_PCFG_IE_REP_SHIFT;
            REG_DSP_PMASK &= ~(sources & 0xFFFF);
            _callbackSources |= sources;
        }
        else
        {
            REG_DSP_PCFG &= ~(((sources >> 16) & 7) << DSP_PCFG_IE_REP_SHIFT);
            REG_DSP_PMASK |= sources & 0xFFFF;
            _callbackSources &= ~sources;
        }        
	}
	leaveCriticalSection(irq);
}