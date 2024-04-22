/***************************************************************************************
*  This file is create by YYH
***************************************************************************************/


#include <common.h>

#include <elf.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#ifdef CONFIG_FTRACE

#define FUNC_NAME_MAX 64
#define FUNC_NUM_MAX 128 
typedef struct {
    uint32_t addr;
    uint32_t size;
    char func_name[FUNC_NAME_MAX];
}FTRACE_FUNC;
FTRACE_FUNC ftrace_func[FUNC_NUM_MAX];
static uint32_t func_num = 0;

void parse_elf(const char *elf_file) {
    int fd;
    struct stat file_stat;
    Elf32_Ehdr *ehdr;
    Elf32_Shdr *shdr;
    Elf32_Sym *symtab;
    char *strtab;
    size_t shnum, i;

    // Open the ELF file
    if ((fd = open(elf_file, O_RDONLY, 0)) < 0) {
        perror("open");
        exit(1);
    }

    // Get file size
    if (fstat(fd, &file_stat) < 0) {
        perror("fstat");
        exit(1);
    }

    // Memory map the file
    void *map_start = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_start == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // ELF header
    ehdr = (Elf32_Ehdr *)map_start;
    // Section header table
    shdr = (Elf32_Shdr *)((char *)map_start + ehdr->e_shoff);
    // Number of sections
    shnum = ehdr->e_shnum;
    // Symbol table section
    symtab = NULL;
    for (i = 0; i < shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (Elf32_Sym *)((char *)map_start + shdr[i].sh_offset);
            break;
        }
    }
    if (symtab == NULL) {
        fprintf(stderr, "Symbol table not found.\n");
        munmap(map_start, file_stat.st_size);
        close(fd);
        exit(1);
    }

    // String table section
    strtab = (char *)((char *)map_start + shdr[shdr[i].sh_link].sh_offset);

    // Iterate over symbols
    for (int j = 0; j < shdr[i].sh_size / sizeof(Elf32_Sym); j++) {
        if (ELF32_ST_TYPE(symtab[j].st_info) == STT_FUNC) {
            if (func_num >= FUNC_NUM_MAX) {
                fprintf(stderr, "Function amount out of range!\n");
                exit(1);
            }
            printf("Symbol Name: %s, Value: 0x%x, Info: 0x%x, Size: %d\n", &strtab[symtab[j].st_name], symtab[j].st_value, symtab[j].st_info, symtab[j].st_size);
            sprintf(ftrace_func[func_num].func_name, "%s",  &strtab[symtab[j].st_name]);
            ftrace_func[func_num].size= symtab[j].st_size;
            ftrace_func[func_num].addr = symtab[j].st_value;
            func_num++;
        }
    }
    // Clean up
    munmap(map_start, file_stat.st_size);
    close(fd);
}

#define DOUBLE_SPACE "  "
#define FTRACE_DEPTH_MAX  32
static int ftrace_dep = 0;

void ftrace_call_print(uint32_t inst_pc, uint32_t inst_des, bool is_tail_call) {
  ftrace_dep++;

  char name[FUNC_NAME_MAX] = {'\0'};
  char space[FTRACE_DEPTH_MAX * 2 + 1] = {'\0'};

  for(int i = 0; i < func_num; i++){
    if(inst_des == ftrace_func[i].addr){
      strcpy(name, ftrace_func[i].func_name);
      break;
    }
  }
  for(int i = 0; i < ftrace_dep; i++){
      strcat(space, DOUBLE_SPACE);
  }
  printf("0x%x: %scall [%s@0x%08x]\n", inst_pc, space, name, inst_des);

  // usleep(500000);
}

void ftrace_ret_print(uint32_t inst_pc, uint32_t inst_des) {

  char name[FUNC_NAME_MAX] = {'\0'};
  char space[FTRACE_DEPTH_MAX * 2 + 1] = {'\0'};
  for(int i = 0; i < func_num; i++){
    if(inst_des == ftrace_func[i].addr){
      strcpy(name, ftrace_func[i].func_name);
      break;
    }
  }
  for(int i = 0; i < ftrace_dep; i++){
      strcat(space, DOUBLE_SPACE);
  }
  printf("0x%x: %sret [\33[1;31m%s@0x%08x\33[0m]\n",inst_pc, space, strlen(name) == 0 ? "???" : name ,inst_des);
  ftrace_dep--;
  /*
   *  TODO : realize tail-recursive function name print
   */
}
#endif


/*
 * datastruct for itrace
 */
#ifdef CONFIG_ITRACE_COND
#define MAX_IRINGBUF 8
typedef struct {
    char logbuf[64];
} ItraceNode;
ItraceNode iringbuf[MAX_IRINGBUF];
static uint32_t inode = 0;

void write_iring(char *buf) {
    int idx = inode++ % MAX_IRINGBUF;
    strcpy(iringbuf[idx].logbuf, buf);
}

void iring_display() {
    for(int i = 0; i< MAX_IRINGBUF; i++){
        if(unlikely(i == ((inode % MAX_IRINGBUF == 0) ? MAX_IRINGBUF - 1 : inode % MAX_IRINGBUF - 1)))
        printf("\33[1;31m --> %s\n\33[0m", iringbuf[i].logbuf);
    else
        printf("     %s\n", iringbuf[i].logbuf);
    }
}
#endif


