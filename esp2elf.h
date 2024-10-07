
#ifndef  ESP2ELF_H
#define ESP2ELF_H

// esp8266 rom structures
typedef struct {
    uint8_t  magic;
    uint8_t  sect_count;
    uint8_t  flags1;
    uint8_t  flags2;
    uint32_t entry_addr;
} rom_header;

typedef struct {
    uint8_t  magic1;
    uint8_t  magic2;
    uint8_t  config[2];
    uint32_t entry_addr;
    uint8_t  unused[4];
    uint32_t irom_len;
} ota_rom_header;

typedef struct {
    uint32_t address;
    uint32_t length;
} sect_header;

typedef union {
    rom_header rom;
    ota_rom_header ota;
} header;

// structure for holding symbols
typedef struct {
    char name[30];
    uint32_t addr;
} symbol_def;

#endif

