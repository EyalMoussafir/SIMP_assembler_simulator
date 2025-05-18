#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*Constants*/

#define MEM_DEPTH 4096
#define CPU_REG_NUM 16
#define IO_REG_NUM 22
#define MAX_LINE 500
#define TRUE 1
#define FALSE 0
#define READ "READ"
#define WRITE "WRITE"
#define PIXELS 256
#define DISK_CYCLES 1024
#define DISK_SECTORS 128

/*Function Prototypes*/

/*Functions that initialize arrays at the beginning of the program.*/

void strip_newline(char *s);
int first_init(int argc, char *argv[], int **interrupts, FILE **imemin_fp, FILE **trace_fp, FILE **hwregtrace_fp, FILE **leds_fp, FILE **display7seg_fp);
int init_memory(const char *dmemin_file);
int init_disk(const char *diskin_file);
int create_interrupts_array(const char *irq2in_file, int **interrupts);

/*Functions preformed in each cycle.*/

/*Functions that are part of the fetch-decode-execute process.*/

void decode_instruction(char *instruction, int *opcode, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void execute_instruction(FILE *hwregtrace_fp, FILE *leds_fp, FILE *display7seg_fp, int *opcode, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
int set_register(int *reg, int *imm1, int *imm2);

/*Functions that are responsible for writing to output file during the fetch-decode-execute process.*/

void write_to_trace(FILE *fp, char *instruction, int imm1, int imm2);
void write_to_hwregtrace(FILE *fp, int cycle, char *action, int reg_num, int data, FILE *leds_fp, FILE *display7seg_fp);
void write_to_leds_and_display(FILE *fp, int cycle, int status);
char *find_io_reg(int reg_num);

/*Functions that are responsible for handling interrupts in the program.*/

int handle_interrupts(int *interrupts);
void check_interrupts(int *interrupts);
void handle_clock_cycles();
void update_timer();
void check_irq2in(int *interrupts);

/*Functions that are responsible for handling the disk.*/

void handle_disk();
void read_sector();
void write_sector();

/*Function implemetaions of the cpu registers.*/

void add(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void sub(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void mac(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void and_func(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void or_func(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void xor_func(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void sll(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void sra(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void srl(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void beq(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void bne(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void blt(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void bgt(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void ble(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void bge(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void jal(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void lw(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void sw(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void reti(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void in(FILE *hwregtrace_fp, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void out(FILE *hwregtrace_fp, FILE *leds_fp, FILE *display7seg_fp, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2);
void halt();

/*Functions that write to the output files at the end of the program.*/

int end_of_run(char *argv[], FILE *imemin_fp, FILE *trace_fp, FILE *hwregtrace_fp, FILE *leds_fp, FILE *display7seg_fp, int **interrupts);
int write_output_files(char *argv[], int cycles);
int write_to_regout(const char *regout_file);
int write_to_dmemout(const char *dmemout_file);
int write_to_cycles(const char *cycles_file, int cycles);
int write_to_diskout(const char *diskout_file);
int write_to_monitor(const char *monitor_txt_file, const char *monitor_yuv_file);

/*Global static variables*/

static int pc = 0;                          /*Program counter*/
static int cont = TRUE;                     /*The program continues running as long as cont is TRUE (1)*/
static int irq = FALSE;                     /*The program jumps to interrupt service if irq is TRUE (1)*/
static int in_isr = FALSE;                  /*Flag that indicates if the program is currently in interupt service*/
static int max_monitor_offset = 0;          /*The maximum offset in which a pixel was written to the monitor*/
static int interrupt_index = 0;             /*Index of the next clock cycle in which interrupt 2 is triggered*/
static int max_interrupts = 0;              /*Maximum number of cpu interrupts*/
static int disk_cycles = 0;                 /*Number of disk cycles the disk has performed*/
int depth = 0;                              /*The maximum depth of used memory*/
int disk_offset = 0;                        /*Maximum offset of disk*/
int memory[MEM_DEPTH] = {0};                /*Memory of the program*/
int cpu_registers[CPU_REG_NUM] = {0};       /*Cpu registers and their contents*/
int io_registers[IO_REG_NUM] = {0};         /*IO registers and their contents*/
int monitor[PIXELS][PIXELS] = {0};          /*Monitor and the pixel values of each pixel*/
int disk[DISK_SECTORS][DISK_SECTORS] = {0}; /*Disk and its contents*/

int main(int argc, char *argv[])
{
    FILE *imemin_fp = NULL, *dmemin_fp = NULL, *trace_fp = NULL, *hwregtrace_fp = NULL;
    FILE *irq2in_fp = NULL, *leds_fp = NULL, *display7seg_fp = NULL, *monitor_txt_fp = NULL;
    FILE *monitor_yuv_fp = NULL, *diskin_fp = NULL, *diskout_fp = NULL;
    char instruction[MAX_LINE], mem_line[9];
    int opcode = 0, rd = 0, rs = 0, rt = 0, rm = 0, imm1 = 0, imm2 = 0;
    int i, j;
    int *interrupts = NULL;

    /*First initialization of all array and file pointers use in the program.*/
    if (first_init(argc, argv, &interrupts, &imemin_fp, &trace_fp, &hwregtrace_fp, &leds_fp, &display7seg_fp))
    {
        return 1;
    }

    /*Running the asmbler code in a fetch-decode-execute loop and handleing interrupts.*/
    while (cont)
    {
        /*Update program counter.*/
        if (fseek(imemin_fp, pc * 13, SEEK_SET) != 0)
        {
            break;
        }

        /*Fetch instruction.*/
        if (!fgets(instruction, MAX_LINE, imemin_fp))
        {
            break;
        }

        /*Decode instruction.*/
        decode_instruction(instruction, &opcode, &rd, &rs, &rt, &rm, &imm1, &imm2);

        /*Write instruction to trace.*/
        write_to_trace(trace_fp, instruction, imm1, imm2);

        /*Execute instruction.*/
        execute_instruction(hwregtrace_fp, leds_fp, display7seg_fp, &opcode, &rd, &rs, &rt, &rm, &imm1, &imm2);

        /*Handle disk.*/
        if (io_registers[17])
        {
            handle_disk();
        }

        /*Handle interrupts.*/
        handle_interrupts(interrupts);

        /*Increment clock.*/
        handle_clock_cycles();
    }

    /*Writing to all output files at the end of the program run.*/
    return end_of_run(argv, imemin_fp, trace_fp, hwregtrace_fp, leds_fp, display7seg_fp, &interrupts);
}

/**
 * @brief Function to strip new line characters from the end of a command line argument.
 *
 * @param s A string representing the command line argument.
 */
void strip_newline(char *s)
{
    size_t len;
    if (!s)
    {
        return;
    }
    len = strlen(s);
    if (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[len - 1] = '\0';
    }
}

/**
 * @brief Function for initializing everything needed in the beginning of the program.
 *
 * @param argc Number of command line arguments.
 * @param argv The command line arguments which contains all file names.
 * @param interrupts A pointer to an array that contains the clock cyclces in which an interrupt should be triggered.
 * At the end of the run contains the clock cycle in increasing order.
 * @param imemin_fp A pointer to the file pointer of imemin.txt input file.
 * @param trace_fp A pointer to the file pointer of trace.txt output file.
 * @param hwregtrace_fp A pointer to the file pointer of hwregtrace.txt output file.
 * @param leds_fp A pointer to the file pointer of leds.txt output file.
 * @param display7seg_fp A pointer to the file pointer of display7seg.txt output file.
 * @return 0 on successful initialization, 1 on failure.
 */
int first_init(int argc, char *argv[], int **interrupts, FILE **imemin_fp, FILE **trace_fp, FILE **hwregtrace_fp, FILE **leds_fp, FILE **display7seg_fp)
{
    int i;
    /*Check for valid number of command line arguments.*/
    if (argc != 15)
    {
        return 1;
    }
    /*Stripping new line characters from all command line arguments.*/
    for (i = 0; i < argc; i++)
    {
        strip_newline(argv[i]);
    }
    /*Initialize arrays used to represent parts of the computer.*/
    if (init_memory(argv[2]))
    {
        return 1;
    }
    if (init_disk(argv[3]))
    {
        return 1;
    }
    if (create_interrupts_array(argv[4], interrupts))
    {
        return 1;
    }
    /*Open input and output file used during fetch-decode-execute loop.*/
    *imemin_fp = fopen(argv[1], "r");
    if (!(*imemin_fp))
    {
        return 1;
    }
    *trace_fp = fopen(argv[7], "w");
    if (!(*trace_fp))
    {
        fclose(*imemin_fp);
        return 1;
    }
    *hwregtrace_fp = fopen(argv[8], "w");
    if (!(*hwregtrace_fp))
    {
        fclose(*imemin_fp);
        fclose(*trace_fp);
        return 1;
    }
    *leds_fp = fopen(argv[10], "w");
    if (!(*leds_fp))
    {
        fclose(*imemin_fp);
        fclose(*trace_fp);
        fclose(*hwregtrace_fp);
        return 1;
    }
    *display7seg_fp = fopen(argv[11], "w");
    if (!(*display7seg_fp))
    {
        fclose(*imemin_fp);
        fclose(*trace_fp);
        fclose(*hwregtrace_fp);
        fclose(*leds_fp);
        return 1;
    }

    return 0;
}

/**
 * @brief Function that initializes an array that represents the initial memory of the program.
 *
 * @param dmemin_file Pointer to dmemin.txt file that represents the initial memory.
 * @return 0 on succesful initialization, 1 on failure.
 */
int init_memory(const char *dmemin_file)
{
    FILE *fp = fopen(dmemin_file, "r");
    int i;
    char mem_line[9];
    if (!fp)
    {
        return 1;
    }
    i = 0;
    while (fgets(mem_line, sizeof(mem_line), fp) != NULL)
    {
        mem_line[strcspn(mem_line, "\r\n\t")] = '\0';
        if (mem_line[0] == '\0')
        {
            continue;
        }
        if (i >= MEM_DEPTH)
        {
            fclose(fp);
            return 1;
        }
        memory[i] = (int)strtol(mem_line, NULL, 16);
        i++;
    }
    depth = i;
    fclose(fp);
    return 0;
}

/**
 * @brief Function that initializes a 2D array that represents the initial state of the disk.
 *
 * @param diskin_file Pointer to diskin.txt file that represents the initial memory.
 * @return 0 on succesful initialization, 1 on failure.
 */
int init_disk(const char *diskin_file)
{
    FILE *fp = fopen(diskin_file, "r");
    int i = 0, j = 0;
    char disk_line[9];
    if (!fp)
    {
        return 1;
    }
    while (fgets(disk_line, sizeof(disk_line), fp) != NULL)
    {
        disk_line[strcspn(disk_line, "\r\n")] = '\0';
        if (disk_line[0] == '\0')
        {
            continue;
        }
        if ((i * DISK_SECTORS + j) >= (DISK_SECTORS * DISK_SECTORS))
        {
            fclose(fp);
            return 0;
        }
        disk[i][j] = (int)strtol(disk_line, NULL, 16);
        j++;
        if (j == DISK_SECTORS)
        {
            i++;
            j = 0;
        }
    }
    disk_offset = i * DISK_SECTORS + j;
    if (disk_offset >= DISK_SECTORS * DISK_SECTORS)
    {
        disk_offset = DISK_SECTORS * DISK_SECTORS;
    }
    fclose(fp);
    return 0;
}

/**
 * @brief Function for initializing the interrupts array absed on the irq2in.txt input file.
 *
 * @param irq2in_file The name of irq2in.txt input file.
 * @param interrupts A pointer to an array that contains the clock cyclces in which an interrupt should be triggered.
 * At the end of the run contains the clock cycle in increasing order.
 * @return 0 on successful initialization, 1 on failure
 */
int create_interrupts_array(const char *irq2in_file, int **interrupts)
{
    int value, i = 0;
    FILE *fp = fopen(irq2in_file, "r");
    if (!fp)
    {
        return 1;
    }
    while (fscanf(fp, "%d", &value) == 1)
    {
        max_interrupts++;
    }
    rewind(fp);
    if (max_interrupts == 0)
    {
        *interrupts = NULL;
        fclose(fp);
        return 0;
    }
    *interrupts = calloc(max_interrupts, sizeof(int));
    if (!(*interrupts))
    {
        fclose(fp);
        return 1;
    }
    while (fscanf(fp, "%d", &value) == 1)
    {
        (*interrupts)[i++] = value;
    }
    fclose(fp);
    return 0;
}

/**
 * @brief Function for checking the irq2status register.
 * The function sets the irq2status register to 1 when irq 2 is triggered.
 *
 * @param interrupts An array that stores the clock cycle in which irq 2 is triggered.
 */
void check_irq2in(int *interrupts)
{
    if (interrupt_index < max_interrupts && io_registers[8] == interrupts[interrupt_index])
    {
        io_registers[5] = TRUE;
        interrupt_index++;
    }
}

/**
 * @brief Function for updating the timer of the processor. timer is updated iff register timeranable==1.
 * In the clock cycle in which timercurrent==timermax, timercurrent=0 and irq0 is triggered.
 */
void update_timer()
{
    /*iterate timer*/
    if (io_registers[11] & 1)
    {
        io_registers[12]++;
    }

    /*Check if timer is equal to the value in the timer limit register.*/
    if (io_registers[12] == io_registers[13])
    {
        io_registers[12] = 0;
        io_registers[3] = TRUE;
    }
}

/**
 * @brief Function for handling clock cycles.
 * The function increments the clck register each time,
 * and resets it when reaching the max value of 0XFFFFFFFF.
 */
void handle_clock_cycles()
{
    io_registers[8]++;
    if ((unsigned int)io_registers[8] == 0XFFFFFFFF)
    {
        io_registers[8] = 0;
    }
}

/**
 * @brief Function for handling interrupts in the system.
 * The function stores the current program counter in irqreturn register,
 * and changes the program counter to be the address in irqhandler.
 * finally it sets in_isr=TRUE, indicating that the processor is in interrupt.
 *
 * @param interrupts An array that stores the clock cycle in which irq 2 is triggered.
 * @return 1 if an interrupt occurred, 0 otherwise.
 */
int handle_interrupts(int *interrupts)
{
    /*check for interrupts*/
    check_interrupts(interrupts);

    /*the clock cycle in which the interrupt is received*/
    if (irq && !in_isr)
    {
        io_registers[7] = pc;
        pc = io_registers[6];
        in_isr = TRUE;
        return 1;
    }
    return 0;
}

/**
 * @brief Function for checking if an interrupt is triggered.
 * The function sets irq=1 iff an interrupt is triggered, and irq=0 otherwise.
 *
 * @param interrupts An array that stores the clock cycle in which irq 2 is triggered.
 */
void check_interrupts(int *interrupts)
{
    int irq0enable, irq1enable, irq2enable, irq0status, irq1status, irq2status;

    /*Update the timer and check if timer == timermax*/
    update_timer();
    /*Check an interrupt should be triggerd in the current clock cycle.*/
    check_irq2in(interrupts);
    irq0enable = io_registers[0] & 1;
    irq1enable = io_registers[1] & 1;
    irq2enable = io_registers[2] & 1;
    irq0status = io_registers[3] & 1;
    irq1status = io_registers[4] & 1;
    irq2status = io_registers[5] & 1;

    irq = (irq0enable & irq0status) | (irq1enable & irq1status) | (irq2enable & irq2status);
}

/**
 * @brief Function for handling the disk.
 * The function is called only when the disk is busy with reading from or writing to the disk.
 * The function increases the disk cycle counter by 1 each cycle where the disk is busy,
 * and when reaching 1024 disk cycles performs the operation.
 */
void handle_disk()
{
    if (disk_cycles == DISK_CYCLES)
    {
        disk_cycles = 0;
        if (io_registers[14] == 1)
        {
            read_sector();
        }
        else if (io_registers[14] == 2)
        {
            write_sector();
        }
        io_registers[14] = 0;
        io_registers[17] = 0;
        io_registers[4] = TRUE;
    }
    else
    {
        disk_cycles++;
    }
}

/**
 * @brief Function for reading a sector from the disk to the specified location in memory.
 * The function reads sector based on disksector register
 * and writes it to the location in memory based on diskbuffer register.
 */
void read_sector()
{
    int i, sector = io_registers[15], buffer = io_registers[16];
    for (i = 0; i < DISK_SECTORS; i++)
    {
        memory[buffer + i] = disk[sector][i];
    }
    if (buffer + i + 1 > depth)
    {
        depth = buffer + i + 1;
    }
}

/**
 * @brief Function for writing a sector to disk from the specified location in memory.
 * The function reads data from memory based on diskbuffer register
 * and writes it to disk the sector specified on disksector register.
 */
void write_sector()
{
    int i, sector = io_registers[15], buffer = io_registers[16], current_offset;
    for (i = 0; i < DISK_SECTORS; i++)
    {
        disk[sector][i] = memory[buffer + i];
        current_offset = sector * DISK_SECTORS + i;
        if (current_offset > disk_offset)
        {
            disk_offset = current_offset + 1;
        }
    }
}

/**
 * @brief Function that decodes an instruction acording to the instuction format.
 *
 * @param instruction The instruction to be decoded.
 * @param opcode A pointer to the opcode. At the end of the run contains the value of the opcode.
 * @param rd A pointer to the rd register. At the end of the run contains the value of rd.
 * @param rs A pointer to the rs register. At the end of the run contains the value of rs.
 * @param rt A pointer to the rt register. At the end of the run contains the value of rt.
 * @param rm A pointer to the rm register. At the end of the run contains the value of rm.
 * @param imm1 A pointer to the imm1 register. At the end of the run contains the value of imm1.
 * @param imm2 A pointer to the imm2 register. At the end of the run contains the value of imm2.
 */
void decode_instruction(char *instruction, int *opcode, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    char opcode_str[3], rd_str[2], rs_str[2], rt_str[2], rm_str[2], imm1_str[4], imm2_str[4];
    /* Split the instruction into fields*/
    strncpy(opcode_str, instruction, 2);
    opcode_str[2] = '\0';
    strncpy(rd_str, instruction + 2, 1);
    rd_str[1] = '\0';
    strncpy(rs_str, instruction + 3, 1);
    rs_str[1] = '\0';
    strncpy(rt_str, instruction + 4, 1);
    rt_str[1] = '\0';
    strncpy(rm_str, instruction + 5, 1);
    rm_str[1] = '\0';
    strncpy(imm1_str, instruction + 6, 3);
    imm1_str[3] = '\0';
    strncpy(imm2_str, instruction + 9, 3);
    imm2_str[3] = '\0';

    /* Convert each field from hexadecimal string to integer*/
    *opcode = strtol(opcode_str, NULL, 16);
    *rd = strtol(rd_str, NULL, 16);
    *rs = strtol(rs_str, NULL, 16);
    *rt = strtol(rt_str, NULL, 16);
    *rm = strtol(rm_str, NULL, 16);
    *imm1 = strtol(imm1_str, NULL, 16);
    if (*imm1 >= 0x800)
    {
        *imm1 -= 0x1000;
    }

    *imm2 = strtol(imm2_str, NULL, 16);
    if (*imm2 >= 0x800)
    {
        *imm2 -= 0x1000;
    }
}

/**
 * @brief Function for executing a single instruction.
 *
 * @param hwregtrace_fp A pointer to output file hwretrace.txt.
 * @param leds_fp A pointer to output file leds.txt.
 * @param display7seg A pointer to output file display7seg.txt.
 * @param opcode A pointer to the opcode. At the end of the run contains the value of the opcode.
 * @param rd A pointer to the rd register. At the end of the run contains the value of rd.
 * @param rs A pointer to the rs register. At the end of the run contains the value of rs.
 * @param rt A pointer to the rt register. At the end of the run contains the value of rt.
 * @param rm A pointer to the rm register. At the end of the run contains the value of rm.
 * @param imm1 A pointer to the imm1 register. At the end of the run contains the value of imm1.
 * @param imm2 A pointer to the imm2 register. At the end of the run contains the value of imm2.
 */
void execute_instruction(FILE *hwregtrace_fp, FILE *leds_fp, FILE *display7seg_fp, int *opcode, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    if (*opcode == 0)
    {
        add(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 1)
    {
        sub(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 2)
    {
        mac(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 3)
    {
        and_func(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 4)
    {
        or_func(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 5)
    {
        xor_func(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 6)
    {
        sll(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 7)
    {
        sra(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 8)
    {
        srl(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 9)
    {
        beq(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 10)
    {
        bne(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 11)
    {
        blt(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 12)
    {
        bgt(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 13)
    {
        ble(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 14)
    {
        bge(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 15)
    {
        jal(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 16)
    {
        lw(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 17)
    {
        sw(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 18)
    {
        reti(rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 19)
    {
        in(hwregtrace_fp, rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 20)
    {
        out(hwregtrace_fp, leds_fp, display7seg_fp, rd, rs, rt, rm, imm1, imm2);
    }
    else if (*opcode == 21)
    {
        halt();
    }
}

/**
 * @brief Function that sets the value of a register in an instruction.
 *
 * @param reg Apointer to the register number from the instruction.
 * @param imm1 A pointer to the first immediate value from the instruction.
 * @param imm2 A pointer to the second immediate value from the instruction.
 * @return The correct value of the register as indicated by the instruction.
 */
int set_register(int *reg, int *imm1, int *imm2)
{
    if (*reg == 1)
    {
        return *imm1;
    }
    else if (*reg == 2)
    {
        return *imm2;
    }
    else
    {
        return cpu_registers[*reg];
    }
}

/**
 * @brief Function performing add instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void add(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = rs_val + rt_val + rm_val;
    pc++;
}

/**
 * @brief Function performing sub instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void sub(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = rs_val - rt_val - rm_val;
    pc++;
}

/**
 * @brief Function performing mac instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void mac(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = (rs_val * rt_val) + rm_val;
    pc++;
}

/**
 * @brief Function performing an instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void and_func(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = rs_val & rt_val & rm_val;
    pc++;
}

/**
 * @brief Function performing or instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void or_func(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = rs_val | rt_val | rm_val;
    pc++;
}

/**
 * @brief Function performing xor instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void xor_func(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = rs_val ^ rt_val ^ rm_val;
    pc++;
}

/**
 * @brief Function performing sll instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void sll(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    cpu_registers[*rd] = rs_val << rt_val;
    pc++;
}

/**
 * @brief Function performing sra instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void sra(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    cpu_registers[*rd] = rs_val >> rt_val;
    pc++;
}

/**
 * @brief Function performing srl instruction. The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void srl(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);

    cpu_registers[*rd] = (unsigned int)rs_val >> rt_val;
    pc++;
}

/**
 * @brief Function performing beq instruction.
 * The function changes the program counter to the correct address if the condition is met,
 * and increases the program counter by one if not.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void beq(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    if (rs_val == rt_val)
    {
        pc = rm_val & 0xfff;
    }
    else
    {
        pc++;
    }
}

/**
 * @brief Function performing bne instruction.
 * The function changes the program counter to the correct address if the condition is met,
 * and increases the program counter by one if not.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void bne(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    if (rs_val != rt_val)
    {
        pc = rm_val & 0xfff;
    }
    else
    {
        pc++;
    }
}

/**
 * @brief Function performing blt instruction.
 * The function changes the program counter to the correct address if the condition is met,
 * and increases the program counter by one if not.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void blt(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    if (rs_val < rt_val)
    {
        pc = rm_val & 0xfff;
    }
    else
    {
        pc++;
    }
}

/**
 * @brief Function performing bgt instruction.
 * The function changes the program counter to the correct address if the condition is met,
 * and increases the program counter by one if not.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void bgt(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    if (rs_val > rt_val)
    {
        pc = rm_val & 0xfff;
    }
    else
    {
        pc++;
    }
}

/**
 * @brief Function performing ble instruction.
 * The function changes the program counter to the correct address if the condition is met,
 * and increases the program counter by one if not.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void ble(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    if (rs_val <= rt_val)
    {
        pc = rm_val & 0xfff;
    }
    else
    {
        pc++;
    }
}

/**
 * @brief Function performing bge instruction.
 * The function changes the program counter to the correct address if the condition is met,
 * and increases the program counter by one if not.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void bge(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    if (rs_val >= rt_val)
    {
        pc = rm_val & 0xfff;
    }
    else
    {
        pc++;
    }
}

/**
 * @brief Function performing jal instruction.
 * The function saves the next instruction address and registers[*rd],
 * and updates the program counter to be the 12 lower bits of *rm.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void jal(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{

    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = pc + 1;
    pc = rm_val & 0xfff;
}

/**
 * @brief Function performing lw instruction.
 * The function loads a word from the memory and stores it in registers[*rd].
 * The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 * @param memory An array that represents the memory of the program.
 */
void lw(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    cpu_registers[*rd] = memory[rs_val + rt_val] + rm_val;
    pc++;
}

/**
 * @brief Function performing sw instruction.
 * The function stores a word to the memory at location rm+rd.
 * The function increases the program counter by one.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void sw(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rd_val = set_register(rd, imm1, imm2);
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    int i = rs_val + rt_val;
    memory[i] = rm_val + rd_val;
    /*Updtae maximum depth of memory.*/
    if (i + 1 > depth)
    {
        depth = i + 1;
    }
    pc++;
}

/**
 * @brief Function performing reti instruction. The function updates the program counter to be io_registers[7],
 * and sets in_isr=FALSE, indicating the processor finished handling the interrupt.
 *
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void reti(int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    pc = io_registers[7];
    in_isr = FALSE;
}

/**
 * @brief Function performing in instruction.
 * The function writes to the hwregtrace.txt output file the operation that was performed.
 * The function increases the program counter by one.
 *
 * @param hwregtrace_fp A pointer to the hwregtrace.txt output file.
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void in(FILE *hwregtrace_fp, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int reg = rs_val + rt_val;

    cpu_registers[*rd] = (reg == 22) ? 0 : io_registers[reg];
    /*Write to hwregtrace.txt output file.*/
    write_to_hwregtrace(hwregtrace_fp, io_registers[8], READ, reg, io_registers[reg], NULL, NULL);
    pc++;
}

/**
 * @brief Function performing out instruction.
 * The function writes to the hwregtrace.txt output file the operation that was performed.
 * The function also writes to the monitor/disk in the clock cycle in which the monitor/disk was updated.
 * The function increases the program counter by one.
 *
 * @param hwregtrace_fp A pointer to the hwregtrace.txt output file.
 * @param leds_fp A pointer to output file leds.txt.
 * @param display7seg A pointer to output file display7seg.txt.
 * @param rd A pointer to the rd register.
 * @param rs A pointer to the rs register.
 * @param rt A pointer to the rt register.
 * @param rm A pointer to the rm register.
 * @param imm1 A pointer to the imm1 register.
 * @param imm2 A pointer to the imm2 register.
 */
void out(FILE *hwregtrace_fp, FILE *leds_fp, FILE *display7seg_fp, int *rd, int *rs, int *rt, int *rm, int *imm1, int *imm2)
{
    int rd_val = set_register(rd, imm1, imm2);
    int rs_val = set_register(rs, imm1, imm2);
    int rt_val = set_register(rt, imm1, imm2);
    int rm_val = set_register(rm, imm1, imm2);
    int reg = rs_val + rt_val;
    int pixel_row = io_registers[20] / 256, pixel_col = io_registers[20] % 256;

    io_registers[reg] = rm_val;
    /*Write to hwregtrace.txt output file.*/
    write_to_hwregtrace(hwregtrace_fp, io_registers[8], WRITE, reg, rm_val, leds_fp, display7seg_fp);

    /*Write to monitor.*/
    if (reg == 22 && rm_val == 1)
    {
        monitor[pixel_row][pixel_col] = io_registers[21];
        io_registers[reg] = 0;
        max_monitor_offset = (max_monitor_offset < io_registers[20]) ? io_registers[20] : max_monitor_offset;
    }
    /*Disk operations.*/
    if (reg == 14 && (rm_val == 1 || rm_val == 2))
    {
        io_registers[17] = 1;
        disk_cycles = 0;
    }

    pc++;
}

/**
 * @brief Function performing halt instruction.
 * The function changes the global variable cont to be FALSE (=0), indicating that the program has been stopped.
 */
void halt()
{
    cont = FALSE;
}

/**
 * @brief Function that writes to the trace.txt output file.
 *
 * @param fp Pointer to trace.txt output file.
 * @param instruction The current instruction.
 * @param imm1 The value of the first immediate value.
 * @param imm2 The value of the second immediate value.
 */
void write_to_trace(FILE *fp, char *instruction, int imm1, int imm2)
{
    int i;
    instruction[12] = '\0';
    fprintf(fp, "%03X ", pc);
    fprintf(fp, "%s ", instruction);
    fprintf(fp, "00000000 ");
    fprintf(fp, "%08X ", imm1 & 0xFFFFFFFF);
    fprintf(fp, "%08X ", imm2 & 0xFFFFFFFF);
    for (i = 3; i < CPU_REG_NUM; i++)
    {
        fprintf(fp, "%08X ", cpu_registers[i] & 0xFFFFFFFF);
    }
    fprintf(fp, "\n");
}

/**
 * @brief Function that writes to the hwregtrace.txt output file.
 * The function also writes to leds.txt and display7seg.txt output files during the clock cycle
 * in which a change has occurred in the corresponding registers.
 *
 * @param fp Pointer to to hwregtrace.txt file.
 * @param cycle The clock cycle in which the operation is performed.
 * @param action The action that is performed (READ or WRITE).
 * @param reg_num The number of the hardware register.
 * @param data The data that was read from or written to the register.
 * @param leds_fp A pointer to output file leds.txt.
 * @param display7seg A pointer to output file display7seg.txt.
 */
void write_to_hwregtrace(FILE *fp, int cycle, char *action, int reg_num, int data, FILE *leds_fp, FILE *display7seg_fp)
{
    char *name = find_io_reg(reg_num);
    fprintf(fp, "%d %s %s %08X\n", cycle, action, name, data & 0xFFFFFFFF);
    if (!(leds_fp == NULL && display7seg_fp == NULL))
    {
        if (reg_num == 9)
        {
            /*Write to leds.txt output file.*/
            write_to_leds_and_display(leds_fp, cycle, data);
        }
        if (reg_num == 10)
        {
            /*Write to display7seg.txt output file.*/
            write_to_leds_and_display(display7seg_fp, cycle, data);
        }
    }
}

/**
 * @brief Function that finds the name of the io register based on the number of the register.
 * @param reg_num The number of the io register.
 * @return The name of the io register.
 */
char *find_io_reg(int reg_num)
{
    char *name = "";
    if (reg_num == 0)
    {
        name = "irq0enable";
    }
    else if (reg_num == 1)
    {
        name = "irq1enable";
    }
    else if (reg_num == 2)
    {
        name = "irq2enable";
    }
    else if (reg_num == 3)
    {
        name = "irq0status";
    }
    else if (reg_num == 4)
    {
        name = "irq1status";
    }
    else if (reg_num == 5)
    {
        name = "irq2status";
    }
    else if (reg_num == 6)
    {
        name = "irqhandler";
    }
    else if (reg_num == 7)
    {
        name = "irqreturn";
    }
    else if (reg_num == 8)
    {
        name = "clks";
    }
    else if (reg_num == 9)
    {
        name = "leds";
    }
    else if (reg_num == 10)
    {
        name = "display7seg";
    }
    else if (reg_num == 11)
    {
        name = "timerenable";
    }
    else if (reg_num == 12)
    {
        name = "timercurrent";
    }
    else if (reg_num == 13)
    {
        name = "timermax";
    }
    else if (reg_num == 14)
    {
        name = "diskcmd";
    }
    else if (reg_num == 15)
    {
        name = "disksector";
    }
    else if (reg_num == 16)
    {
        name = "diskbuffer";
    }
    else if (reg_num == 17)
    {
        name = "diskstatus";
    }
    else if (reg_num == 20)
    {
        name = "monitoraddr";
    }
    else if (reg_num == 21)
    {
        name = "monitordata";
    }
    else if (reg_num == 22)
    {
        name = "monitorcmd";
    }

    return name;
}

/**
 * @brief Function for writing to leds.txt or display7seg.txt output files.
 * The function writes to one of the files based on the register that was changed during the current clock cycle.
 *
 * @param fp A pointer to one of the output files: leds.txt or display7seg.txt.
 * @param cycle The clock cycle in which the change has occurred.
 * @param status The status of the register (leds register or dispay7seg register).
 */
void write_to_leds_and_display(FILE *fp, int cycle, int status)
{
    fprintf(fp, "%d %08X\n", cycle, status & 0xFFFFFFFF);
}

/**
 * @brief Function for writing to the output files at the end of the program.
 * The function writes to dmemout.txt, regout.txt, cycles.txt, diskout.txt, monitor.txt and monitor.yuv output files.
 *
 * @param argv The command line arguments which contains all file names.
 * @param cycles The number of cycles performed by the program.
 * @return 0 on successful writing to all files, 1 on failure.
 */
int write_output_files(char *argv[], int cycles)
{
    /*Write to dmemout.txt output file.*/
    if (write_to_dmemout(argv[5]))
    {
        return 1;
    }
    /*Write to regout.txt output file.*/
    if (write_to_regout(argv[6]))
    {
        return 1;
    }
    /*Write to cycles.txt output file.*/
    if (write_to_cycles(argv[9], cycles))
    {
        return 1;
    }
    /*Write to diskout.txt output file.*/
    if (write_to_diskout(argv[12]))
    {
        return 1;
    }
    /*Write to monitor.txt and monitor.yuv output files.*/
    if (write_to_monitor(argv[13], argv[14]))
    {
        return 1;
    }
    return 0;
}

/**
 * @brief Function for writing to dmemout.txt output file.
 *
 * @param dmem_out_file The name of the file.
 * @return 0 on successful writing to file, 1 on failure.
 */
int write_to_dmemout(const char *dmemout_file)
{
    FILE *fp = fopen(dmemout_file, "w");
    int i;
    if (!fp)
    {
        return 1;
    }
    /*Write up to maximum depth of memory.*/
    for (i = 0; i < depth; i++)
    {
        fprintf(fp, "%08X\n", memory[i] & 0xFFFFFFFF);
    }

    fclose(fp);
    return 0;
}

/**
 * @brief Function for writing to regout.txt output file.
 *
 * @param regout_file The name of the file.
 * @return 0 on successful writing to file, 1 on failure.
 */
int write_to_regout(const char *regout_file)
{
    FILE *fp = fopen(regout_file, "w");
    int i;
    if (!fp)
    {
        return 1;
    }
    for (i = 3; i < CPU_REG_NUM; i++)
    {
        fprintf(fp, "%08X\n", cpu_registers[i] & 0xFFFFFFFF);
    }
    fclose(fp);
    return 0;
}

/**
 * @brief Function for writing to cycles.txt output file.
 *
 * @param cycles_file The name of the file.
 * @param cycles Number of clock cycles preformed by the program.
 * @return 0 on successful writing to file, 1 on failure.
 */
int write_to_cycles(const char *cycles_file, int cycles)
{
    FILE *fp = fopen(cycles_file, "w");
    int i, disk_remain;
    /*Calculating the remaining disk cycles if program halted before disk finished.*/
    disk_remain = io_registers[17] ? DISK_CYCLES - disk_cycles : 0;
    if (!fp)
    {
        return 1;
    }
    fprintf(fp, "%d", cycles + disk_remain);
    fclose(fp);
    return 0;
}

/**
 * @brief Function for writing to diskout.txt output file.
 *
 * @param diskout_file The name of the file.
 * @return 0 on successful writing to file, 1 on failure.
 */
int write_to_diskout(const char *diskout_file)
{
    FILE *fp = fopen(diskout_file, "w");
    int i, j;
    if (!fp)
    {
        return 1;
    }
    for (i = 0; i < DISK_SECTORS; i++)
    {
        for (j = 0; j < DISK_SECTORS; j++)
        {
            /*Write up to maximum disk offset.*/
            if (i * DISK_SECTORS + j >= disk_offset)
            {
                fclose(fp);
                return 0;
            }
            fprintf(fp, "%08X\n", disk[i][j] & 0xFFFFFFFF);
        }
    }
    fclose(fp);
    return 0;
}

/**
 * @brief Function for writing to the monitor.txt ad monitor.yuv output files.
 *
 * @param monitor_txt_file The name of the monitor.txt file.
 * @param monitor_yuv_file The name of the monitor.yuv file
 * @return 0 on successful writing to file, 1 on failure.
 */
int write_to_monitor(const char *monitor_txt_file, const char *monitor_yuv_file)
{
    FILE *fp_txt, *fp_yuv;
    int i, j, count = 0;

    fp_txt = fopen(monitor_txt_file, "w");
    if (!fp_txt)
    {
        return 1;
    }
    fp_yuv = fopen(monitor_yuv_file, "w");
    if (!fp_yuv)
    {
        fclose(fp_txt);
        return 1;
    }

    for (i = 0; i < PIXELS; i++)
    {
        for (j = 0; j < PIXELS; j++)
        {
            /*Write to monitor.txt up to maximum monitor offsetin.*/
            if (count <= max_monitor_offset)
            {
                fprintf(fp_txt, "%02X\n", monitor[i][j]);
            }
            fprintf(fp_yuv, "%c", monitor[i][j]);
            count++;
        }
    }

    fclose(fp_txt);
    fclose(fp_yuv);
    return 0;
}

/**
 * @brief Function for handling everything that happens at the end of the run.
 * The function writes to the output files, closes all open files, and frees allocated memory.
 *
 * @param argv The command line arguments which contains all file names.
 * @param imemin_fp A pointer to imemin.txt file for closing the file.
 * @param trace_fp A pointer to trace.txt file for closing the file.
 * @param hwregtrace_fp A pointer to hwregtrace.txt file for closing the file.
 * @param interrupts An array that contains the clock cyclces in which an interrupt should be triggered.
 * The memory allocated by the array is freed.
 * @return 0 on success, 1 on failure.
 */
int end_of_run(char *argv[], FILE *imemin_fp, FILE *trace_fp, FILE *hwregtrace_fp, FILE *leds_fp, FILE *display7seg_fp, int **interrupts)
{
    /*Close all open files adn free allocated memory.*/
    free(*interrupts);
    fclose(imemin_fp);
    fclose(trace_fp);
    fclose(hwregtrace_fp);
    fclose(leds_fp);
    fclose(display7seg_fp);

    return write_output_files(argv, io_registers[8]);
}