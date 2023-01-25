#include "rebootex.h"

#define PTR_ALIGN_64(p) (((u32)p & (~63)) + 64)

int (*pspemuLfatOpen)(char** filename, int unk) = NULL;
void (*SetMemoryPartitionTable)(void *sysmem_config, SceSysmemPartTable *table) = NULL;
extern int UnpackBootConfigPatched(char **p_buffer, int length);
extern int (* UnpackBootConfig)(char * buffer, int length);


typedef struct{
    u8 filesize[4];
    char namelen;
    char name[1];
} FlashFile;

int findFlashFile(BootFile* file, const char* path){
    void* flashfs = reboot_conf->flashfs;
    if (flashfs == NULL) return -1;
    u32 nfiles = *(u32*)(flashfs);
    FlashFile* cur = (FlashFile*)((size_t)(flashfs)+4);

    for (int i=0; i<nfiles; i++){
        size_t filesize = (cur->filesize[0]) + (cur->filesize[1]<<8) + (cur->filesize[2]<<16) + (cur->filesize[3]<<24);
        if (strncmp(path, cur->name, cur->namelen) == 0){
            file->buffer = (void*)((size_t)(&(cur->name[0])) + cur->namelen);
            file->size = filesize;
            return 0;
        }
        cur = (FlashFile*)((size_t)(cur)+filesize+cur->namelen+5);
    }
    return -1;
}

void relocateFlashFile(BootFile* file){
    static u8* curbuf = (u8*)PTR_ALIGN_64(FLASH_SONY+(12*1024*1024));
    memcpy((void *)curbuf, file->buffer, file->size);
    file->buffer = (void *)curbuf;
    curbuf += file->size;
    curbuf = PTR_ALIGN_64(curbuf);
}

// Load Core module_start Hook
int loadcoreModuleStartVita(unsigned int args, void* argp, int (* start)(SceSize, void *))
{
    loadCoreModuleStartCommon(start);
    flushCache();
    return start(args, argp);
}

int _pspemuLfatOpen(BootFile* file, int unk)
{
    char* p = file->name;
    int is_bootfile = 0;
    
    if (strcmp(p, "pspbtcnf.bin") == 0){
        is_bootfile = 1;
        int ret = -1;
        switch(reboot_conf->iso_mode) {
            case MODE_NP9660:
            case MODE_MARCH33:
            case MODE_INFERNO:
                ret = findFlashFile(file, "psvbtknf.bin"); // use inferno ISO mode (psvbtknf.bin)
                break;
            default:
                ret = findFlashFile(file, "psvbtjnf.bin"); // normal mode (psvbtjnf.bin)
                break;
        }
        if (ret == 0){
            relocateFlashFile(file);
            return ret;
        }
    }
    else if (strncmp(p, "/kd/ark_", 8) == 0){ // ARK module
        int ret = findFlashFile(file, p);
        if (ret == 0){
            relocateFlashFile(file);
            return ret;
        }
    }
    else if (strcmp(p, REBOOT_MODULE) == 0){
        file->buffer = (void *)0x89000000;
		file->size = reboot_conf->rtm_mod.size;
		memcpy(file->buffer, reboot_conf->rtm_mod.buffer, file->size);
		reboot_conf->rtm_mod.buffer = NULL;
        reboot_conf->rtm_mod.size = 0;
		return 0;
    }
    int res = pspemuLfatOpen(file, unk);
    return 0;
}

int UnpackBootConfigVita(char **p_buffer, int length){
    int res = (*UnpackBootConfig)(*p_buffer, length);
    if(reboot_conf->rtm_mod.before && reboot_conf->rtm_mod.buffer && reboot_conf->rtm_mod.size)
    {
        //add reboot prx entry
        res = AddPRX(*p_buffer, reboot_conf->rtm_mod.before, REBOOT_MODULE, reboot_conf->rtm_mod.flags);
    }
    return res;
}

//extra ram through flash0 ramfs on Vita
void SetMemoryPartitionTablePatched(void *sysmem_config, SceSysmemPartTable *table)
{
    // Add flash0 ramfs as partition 11
    SetMemoryPartitionTable(sysmem_config, table);
    table->extVshell.addr = VITA_EXTRA_RAM;
    table->extVshell.size = 32 * 1024 * 1024;
}

int PatchSysMem(void *a0, void *sysmem_config)
{
    int (* module_bootstart)(SceSize args, void *sysmem_config) = (void *)_lw((u32)a0 + 0x28);
    u32 text_addr = SYSMEM_TEXT;
    u32 top_addr = text_addr+0x14000;
    int patches = 2;
    for (u32 addr = text_addr; addr<top_addr && patches; addr += 4) {
        u32 data = _lw(addr);
        if (data == 0x247300FF){
            SetMemoryPartitionTable = K_EXTRACT_CALL(addr-20);
            _sw(JAL(SetMemoryPartitionTablePatched), addr-20);
            patches--;
        }
        else if (data == 0x8E86004C){
            _sw(0x2405000F, addr+16);
            patches--;
        }
    }

    flushCache();

    return module_bootstart(4, sysmem_config);
}


// patch reboot on ps vita
void patchRebootBuffer(){
    // hijack UnpackBootConfig to insert modules at runtime
    _sw(0x27A40004, UnpackBootConfigArg); // addiu $a0, $sp, 4
    _sw(JAL(UnpackBootConfigVita), UnpackBootConfigCall); // Hook UnpackBootConfig

    for (u32 addr = reboot_start; addr<reboot_end; addr+=4){
        u32 data = _lw(addr);
        if (data == JAL(pspemuLfatOpen)){
            _sw(JAL(_pspemuLfatOpen), addr); // Hook pspemuLfatOpen
        }
        else if (data == 0x3A230001){ // found pspemuLfatOpen
            u32 a = addr;
            do {a-=4;} while (_lw(a) != 0x27BDFFF0);
            pspemuLfatOpen = (void*)a;
        }
        else if (data == 0x00600008){ // found loadcore jump on Vita
            // Move LoadCore module_start Address into third Argument
            _sw(0x00603021, addr); // move a2, v1
            // Hook LoadCore module_start Call
            _sw(JUMP(loadcoreModuleStartVita), addr+8);
        }
        else if (data == 0x24040004) {
            _sw(0x02402021, addr); //move $a0, $s2
            _sw(JAL(PatchSysMem), addr + 0x64); // Patch call to SysMem module_bootstart
        }
    }
    // Flush Cache
    flushCache();
}