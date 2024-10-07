#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <libelf.h>
#include <gelf.h>

#include "esp2elf.h"
#include "bootrom.h"
#include "bootrom_symbols.h"

Elf *e = 0;
char shstrtab[1024] = {0};
int shstrtab_off = 1;
int current_phdr = 0;
int dram_count = 0;
int iram_count = 0;
int unkn_count = 0;

Elf_Scn *AddElfSection(char *name, Elf32_Word sec_type, Elf32_Word sec_flags, int sec_addr, void *data, int data_len, Elf_Type data_type) {

    Elf_Scn *scn = elf_newscn(e);
    Elf32_Shdr *shdr = elf32_getshdr(scn);
    shdr->sh_name = shstrtab_off;
    shdr->sh_type = sec_type;
    shdr->sh_flags = sec_flags;
    shdr->sh_entsize = 0;
    shdr->sh_addr = sec_addr;

    // add this section's name to .shstrtab
    strcpy(shstrtab + shstrtab_off, name);
    shstrtab_off += strlen(name) + 1;

    Elf_Data *d = elf_newdata(scn);
    d->d_align = 4;
    d->d_buf = data;
    d->d_size = data_len;
    d->d_off = 0;
    d->d_type = data_type;
    d->d_version = EV_CURRENT;

    elf_update(e, ELF_C_NULL);

    return scn;
}

Elf_Scn *AddElfProgSection(uint8_t *buf, int len, int addr) {
    
    // work out name and flags for the section
    char name[30] = {0};
    Elf32_Word sflags = SHF_EXECINSTR | SHF_ALLOC;
    Elf32_Word pflags = PF_R | PF_X;
    if ((addr >= 0x3FFE8000) && (addr < 0x3FFFC000)) {
        sprintf(name, ".dram0_%d.data", dram_count++);
        sflags = SHF_WRITE | SHF_ALLOC;
        pflags = PF_R | PF_W;
    } else if ((addr >= 0x40000000) && (addr < 0x40100000)) {
        strcpy(name, ".bootrom.text");
    } else if ((addr >= 0x40100000) && (addr < 0x40200000)) {
        sprintf(name, ".iram1_%d.text", iram_count++);
    } else if ((addr >= 0x40200000) && (addr < 0x40300000)) {
        strcpy(name, ".irom0.text");
    } else {
        sprintf(name, ".unknown_%d.text", unkn_count++);
    }

    Elf_Scn *scn = AddElfSection(name, SHT_PROGBITS, sflags, addr, buf, len, ELF_T_BYTE);

    // get a program header for this section and update it
    Elf32_Shdr *shdr = elf32_getshdr(scn);
    GElf_Phdr gphdr;
    gelf_getphdr(e, current_phdr, &gphdr);
    gphdr.p_type = PT_LOAD;
    gphdr.p_align = 4;
    gphdr.p_offset = shdr->sh_offset;
    gphdr.p_vaddr = gphdr.p_paddr = shdr->sh_addr;
    gphdr.p_filesz = gphdr.p_memsz = shdr->sh_size;
    gphdr.p_flags = pflags;
    gelf_update_phdr(e, current_phdr, &gphdr);

    elf_flagphdr(e, ELF_C_SET, ELF_F_DIRTY);
    elf_update(e, ELF_C_NULL);

    current_phdr++;

    return scn;
}

Elf *CreateElf(FILE *elffile, uint32_t entry, int sect_count) {

    elf_version(EV_CURRENT);
    e = elf_begin(fileno(elffile), ELF_C_WRITE, NULL);

    Elf32_Ehdr *ehdr = elf32_newehdr(e);
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr->e_machine = EM_XTENSA;
    ehdr->e_type = ET_EXEC;
    ehdr->e_flags = 0x300; // unknown flags, copied from sdk compiled elf files
    ehdr->e_entry = entry;

    // create enough program headers for user sections, irom0 and bootrom
    elf32_newphdr(e, sect_count);

    return e;
}

int FinishElf(FILE *fd) {

    // finally add the shstrtab, length needs to account for it's own name which will be added by AddElfSection
    Elf_Scn *scn = AddElfSection(".shstrtab", SHT_STRTAB, SHF_STRINGS, 0, shstrtab, shstrtab_off + 10, ELF_T_BYTE);

    // link string table to elf header
    elf32_getehdr(e)->e_shstrndx = elf_ndxscn(scn);

    // write elf
    elf_update(e, ELF_C_WRITE);
    elf_end(e);

    return 1;
}

Elf_Scn *AddElfSymbolTable(symbol_def *symbol_list, int symbol_count, int code_sect, Elf32_Sym **symbols, char **symbol_strs) {

    // allocate memory for symbol table and strings, caller should free when finished
    int symtab_sz = sizeof(Elf32_Sym) * (symbol_count + 1);
    Elf32_Sym *sym_tab = (Elf32_Sym *)malloc(symtab_sz);
    memset(sym_tab, 0, symtab_sz);

    char *names = (char*)malloc(10240);
    names[0] = 0;
    int names_off = 1;

    // define default symbol
    Elf32_Sym *s = &sym_tab[0];
    s->st_info = ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE);

    for (int i = 0; i < symbol_count; i++) {
        // define this symbol
        s = &sym_tab[i+1];
        s->st_name = names_off;
        s->st_info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
        s->st_shndx = code_sect;
        s->st_value = symbol_list[i].addr;

        // add symbol name to string table
        strcpy(names + names_off, symbol_list[i].name);
        names_off += strlen(symbol_list[i].name) + 1;
    }

    Elf_Scn *scn = AddElfSection(".symtab", SHT_SYMTAB, 0, 0, (void*)sym_tab, symtab_sz, ELF_T_SYM);
    Elf32_Shdr *shdr = elf32_getshdr(scn);

    scn = AddElfSection(".strtab", SHT_STRTAB, SHF_STRINGS, 0, names, names_off, ELF_T_BYTE);
    shdr->sh_link = elf_ndxscn(scn);

    *symbols = sym_tab;
    *symbol_strs = names;
    return scn;
}

int ParseBin(uint8_t *userrom, int rom_offset, char *elffile, int irom_start, int irom_len) {
    uint32_t entry = 0;
    int sect_count = 0;
    Elf32_Sym *symbols = 0;
    char *symbol_strs = 0;

    FILE *fd = fopen(elffile, "wb");
    if (!fd) {
        printf("Unable to open %s for writing.", elffile);
        return 0;
    }

    header *hdr = (header*)(userrom+rom_offset);
    if (hdr->rom.magic == 0xe9) {
        entry = hdr->rom.entry_addr;
        sect_count = hdr->rom.sect_count;
        rom_offset+=8;
    } else if ((hdr->ota.magic1 == 0xea) && (hdr->ota.magic2 == 0x04)) {
        entry = hdr->ota.entry_addr;
        irom_start = rom_offset + 16;
        irom_len = hdr->ota.irom_len;
        rom_offset += 16;

        rom_offset += irom_len;
        hdr = (header*)(userrom+rom_offset);
        if (hdr->rom.magic  != 0xe9) return 0;
        sect_count = hdr->rom.sect_count;
        rom_offset += 8;
    } else {
        printf("unknown magic 0x%02x\n", hdr->rom.magic );
        return 0;
    }

    printf("entry point %08x\n", entry);
    printf("irom_start  %08x\n", irom_start);
    printf("irom_len    %08x\n", irom_len);
    printf("sect count  %d\n", sect_count);

    CreateElf(fd, entry, sect_count + 2);

    // add bootrom
    Elf_Scn *bootrom_scn = AddElfProgSection(bootrom_bin, bootrom_bin_len, 0x40000000);

    // add irom0
    AddElfProgSection(userrom+irom_start, irom_len, 0x40200000 + irom_start);

    // add other code/data sections
    for (int i = 0; i < sect_count; i++) {
        sect_header *sect = (sect_header*)(userrom+rom_offset);
        rom_offset += 8;
        printf("section %d:\n", i);
        printf("  address   %08x\n", sect->address);
        printf("  length    %08x\n", sect->length);
        AddElfProgSection(userrom+rom_offset, sect->length, sect->address);
        rom_offset += sect->length;
    }

    // add a symbol table for known bootrom functions
    AddElfSymbolTable(bootrom_symbols, sizeof(bootrom_symbols) / sizeof(symbol_def), elf_ndxscn(bootrom_scn), &symbols, &symbol_strs);

    FinishElf(fd);
    fclose(fd);

    if (symbols) free(symbols);
    if (symbol_strs) free(symbol_strs);

    return 1;
}

uint8_t *read_file(char *path) {
    int success = 0;
    uint8_t *data = 0;

    FILE *fd = fopen(path, "rb");
    if (fd) {
        fseek(fd, 0, SEEK_END);
        int s = ftell(fd);
        fseek(fd, 0, SEEK_SET);
        data = (uint8_t*)malloc(s);
        int len = fread(data, 1, s, fd);
        if (len == s) success = 1;
        fclose(fd);
        return data;
    }
    return 0;
}

int main(int argc, char *argv[]){
    uint8_t *userrom = 0;
    int ret = 0;
    FILE *fd = 0;

    printf("esp2elf v1.00 richardaburton@gmail.com\n\n");

    if (argc < 3 || argc > 6) {
        printf("usage: esp2elf <binfile> <elffile> [offset] [iromoff] [iromlen]\n\n");
        printf("  binfile - input esp8266 rom dump\n");
        printf("  elffile - output elf file\n");
        printf("  offset  - offset into rom to start from (defaults to 0)\n");
        printf("  iromoff - irom start offset (non-ota only, defaults to 0x40000)\n");
        printf("  iromlen - irom length (non-ota only, defaults to 0x3e000)\n");
        printf("\nSee readme.txt for notes and example usage.\n");
        return 1;
    }

    int offset = 0;
    if (argc >= 4) offset = (int)strtoul(argv[3], NULL, 0);

    int irom_start = 0x40000;
    if (argc >= 5) irom_start = (int)strtoul(argv[4], NULL, 0);

    int irom_len = 0x3e000;
    if (argc >= 6) irom_len = (int)strtoul(argv[5], NULL, 0);

    if (userrom = read_file(argv[1])) {
        ret = ParseBin(userrom, offset, argv[2], irom_start, irom_len);
    } else {
        printf("Error: unable to read '%s'\n", argv[1]);
        ret = 1;
    }

    if (fd) fclose(fd);
    if (userrom) free(userrom);

    return ret;
}

