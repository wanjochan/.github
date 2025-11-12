/* cosmo_elf_parser.c - ELF Parser Implementation */

#include "cosmo_elf_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== ELF Magic Numbers ========== */

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define EI_CLASS 4
#define ELFCLASS32 1
#define ELFCLASS64 2

/* ========== Format Detection ========== */

int elf_is_valid(const uint8_t *data, size_t size) {
    if (!data || size < 16) return 0;

    return (data[0] == ELFMAG0 &&
            data[1] == ELFMAG1 &&
            data[2] == ELFMAG2 &&
            data[3] == ELFMAG3);
}

elf_class_t elf_detect_class(const uint8_t *data, size_t size) {
    if (!elf_is_valid(data, size)) return ELF_CLASS_NONE;

    switch (data[EI_CLASS]) {
        case ELFCLASS32: return ELF_CLASS_32;
        case ELFCLASS64: return ELF_CLASS_64;
        default: return ELF_CLASS_NONE;
    }
}

/* ========== ELF64 Parsing ========== */

static int elf64_parse_dynamic(elf_parser_t *parser) {
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)parser->file_data;
    const Elf64_Phdr *phdr;
    const Elf64_Dyn *dyn;
    int i, count;

    /* Validate program header */
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        fprintf(stderr, "No program headers found\n");
        return -1;
    }

    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf64_Phdr) > parser->file_size) {
        fprintf(stderr, "Invalid program header offset/size\n");
        return -1;
    }

    /* Find PT_DYNAMIC segment */
    phdr = (const Elf64_Phdr *)(parser->file_data + ehdr->e_phoff);
    const Elf64_Phdr *dynamic_phdr = NULL;

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            dynamic_phdr = &phdr[i];
            break;
        }
    }

    if (!dynamic_phdr) {
        /* Not an error - static binaries don't have PT_DYNAMIC */
        parser->num_dynamic = 0;
        return 0;
    }

    /* Validate dynamic segment */
    if (dynamic_phdr->p_offset + dynamic_phdr->p_filesz > parser->file_size) {
        fprintf(stderr, "Invalid PT_DYNAMIC segment\n");
        return -1;
    }

    /* Parse dynamic entries */
    dyn = (const Elf64_Dyn *)(parser->file_data + dynamic_phdr->p_offset);
    count = dynamic_phdr->p_filesz / sizeof(Elf64_Dyn);

    parser->dynamic = malloc(count * sizeof(elf_dynamic_entry_t));
    if (!parser->dynamic) {
        fprintf(stderr, "Failed to allocate dynamic entries\n");
        return -1;
    }

    parser->num_dynamic = 0;
    for (i = 0; i < count && dyn[i].d_tag != DT_NULL; i++) {
        parser->dynamic[i].tag = dyn[i].d_tag;
        parser->dynamic[i].value = dyn[i].d_un.d_val;
        parser->num_dynamic++;

        /* Find string table */
        if (dyn[i].d_tag == DT_STRTAB) {
            /* Find string table in segments */
            for (int j = 0; j < ehdr->e_phnum; j++) {
                if (phdr[j].p_type == PT_LOAD &&
                    dyn[i].d_un.d_ptr >= phdr[j].p_vaddr &&
                    dyn[i].d_un.d_ptr < phdr[j].p_vaddr + phdr[j].p_filesz) {

                    uint64_t offset = phdr[j].p_offset + (dyn[i].d_un.d_ptr - phdr[j].p_vaddr);
                    if (offset < parser->file_size) {
                        parser->strtab = (const char *)(parser->file_data + offset);
                        parser->strtab_size = parser->file_size - offset;
                    }
                    break;
                }
            }
        }
    }

    return 0;
}

/* ========== ELF32 Parsing ========== */

static int elf32_parse_dynamic(elf_parser_t *parser) {
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)parser->file_data;
    const Elf32_Phdr *phdr;
    const Elf32_Dyn *dyn;
    int i, count;

    /* Validate program header */
    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        fprintf(stderr, "No program headers found\n");
        return -1;
    }

    if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf32_Phdr) > parser->file_size) {
        fprintf(stderr, "Invalid program header offset/size\n");
        return -1;
    }

    /* Find PT_DYNAMIC segment */
    phdr = (const Elf32_Phdr *)(parser->file_data + ehdr->e_phoff);
    const Elf32_Phdr *dynamic_phdr = NULL;

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            dynamic_phdr = &phdr[i];
            break;
        }
    }

    if (!dynamic_phdr) {
        /* Not an error - static binaries don't have PT_DYNAMIC */
        parser->num_dynamic = 0;
        return 0;
    }

    /* Validate dynamic segment */
    if (dynamic_phdr->p_offset + dynamic_phdr->p_filesz > parser->file_size) {
        fprintf(stderr, "Invalid PT_DYNAMIC segment\n");
        return -1;
    }

    /* Parse dynamic entries */
    dyn = (const Elf32_Dyn *)(parser->file_data + dynamic_phdr->p_offset);
    count = dynamic_phdr->p_filesz / sizeof(Elf32_Dyn);

    parser->dynamic = malloc(count * sizeof(elf_dynamic_entry_t));
    if (!parser->dynamic) {
        fprintf(stderr, "Failed to allocate dynamic entries\n");
        return -1;
    }

    parser->num_dynamic = 0;
    for (i = 0; i < count && dyn[i].d_tag != DT_NULL; i++) {
        parser->dynamic[i].tag = dyn[i].d_tag;
        parser->dynamic[i].value = dyn[i].d_un.d_val;
        parser->num_dynamic++;

        /* Find string table */
        if (dyn[i].d_tag == DT_STRTAB) {
            /* Find string table in segments */
            for (int j = 0; j < ehdr->e_phnum; j++) {
                if (phdr[j].p_type == PT_LOAD &&
                    dyn[i].d_un.d_ptr >= phdr[j].p_vaddr &&
                    dyn[i].d_un.d_ptr < phdr[j].p_vaddr + phdr[j].p_filesz) {

                    uint32_t offset = phdr[j].p_offset + (dyn[i].d_un.d_ptr - phdr[j].p_vaddr);
                    if (offset < parser->file_size) {
                        parser->strtab = (const char *)(parser->file_data + offset);
                        parser->strtab_size = parser->file_size - offset;
                    }
                    break;
                }
            }
        }
    }

    return 0;
}

/* ========== Parser Lifecycle ========== */

int elf_parser_init(elf_parser_t *parser, const uint8_t *data, size_t size) {
    if (!parser || !data || size < 64) {
        return -1;
    }

    memset(parser, 0, sizeof(elf_parser_t));
    parser->file_data = data;
    parser->file_size = size;
    parser->elf_class = elf_detect_class(data, size);

    if (parser->elf_class == ELF_CLASS_NONE) {
        fprintf(stderr, "Invalid ELF file\n");
        return -1;
    }

    parser->valid = 1;
    return 0;
}

void elf_parser_free(elf_parser_t *parser) {
    if (!parser) return;

    if (parser->dynamic) {
        free(parser->dynamic);
        parser->dynamic = NULL;
    }

    parser->num_dynamic = 0;
    parser->strtab = NULL;
    parser->strtab_size = 0;
    parser->valid = 0;
}

/* ========== Dynamic Section Parsing ========== */

int elf_parse_dynamic(elf_parser_t *parser) {
    if (!parser || !parser->valid) return -1;

    if (parser->elf_class == ELF_CLASS_64) {
        return elf64_parse_dynamic(parser);
    } else if (parser->elf_class == ELF_CLASS_32) {
        return elf32_parse_dynamic(parser);
    }

    return -1;
}

const char* elf_get_string(const elf_parser_t *parser, uint64_t offset) {
    if (!parser || !parser->strtab || offset >= parser->strtab_size) {
        return NULL;
    }
    return parser->strtab + offset;
}

int elf_get_needed_libs(const elf_parser_t *parser, char ***libs, int *num_libs) {
    if (!parser || !libs || !num_libs) return -1;

    *libs = NULL;
    *num_libs = 0;

    /* Count DT_NEEDED entries */
    int count = 0;
    for (int i = 0; i < parser->num_dynamic; i++) {
        if (parser->dynamic[i].tag == DT_NEEDED) {
            count++;
        }
    }

    if (count == 0) return 0;

    /* Allocate array */
    *libs = malloc(count * sizeof(char *));
    if (!*libs) return -1;

    /* Extract library names */
    int idx = 0;
    for (int i = 0; i < parser->num_dynamic; i++) {
        if (parser->dynamic[i].tag == DT_NEEDED) {
            const char *name = elf_get_string(parser, parser->dynamic[i].value);
            if (name) {
                (*libs)[idx++] = strdup(name);
            }
        }
    }

    *num_libs = idx;
    return 0;
}

const char* elf_get_rpath(const elf_parser_t *parser) {
    if (!parser) return NULL;

    for (int i = 0; i < parser->num_dynamic; i++) {
        if (parser->dynamic[i].tag == DT_RPATH) {
            return elf_get_string(parser, parser->dynamic[i].value);
        }
    }
    return NULL;
}

const char* elf_get_runpath(const elf_parser_t *parser) {
    if (!parser) return NULL;

    for (int i = 0; i < parser->num_dynamic; i++) {
        if (parser->dynamic[i].tag == DT_RUNPATH) {
            return elf_get_string(parser, parser->dynamic[i].value);
        }
    }
    return NULL;
}
