#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*Constants*/

#define MEM_DEPTH 4096
#define MAX_LINE 500
#define MAX_LABEL 50
#define MAX_WORDS 7

/*Label List struct*/
typedef struct Label
{
    char name[MAX_LABEL];
    int address;
    struct Label *next;
} Label;

/*Function Prototypes*/

void strip_newline(char *s);
int open_files(int argc, char *argv[], FILE **asm_fp, FILE **imemin_fp, FILE **dmemin_fp);
void first_pass(FILE *fp, Label **labels);
void second_pass(FILE *asm_fp, FILE *imemin_fp, FILE *dmemin_fp, Label *labels);
void parse_line_imemin(char *tokens[], FILE *imemin_fp, Label *labels);
void set_memory(char *tokens[], int dmemin[], int *dmemin_depth);
void dmemin_write(FILE *dmemin_fp, int dmemin[], int dmemin_depth);
int line_status(char *token);
int parse_opcode(char *opcode);
int parse_reg(char *reg);
int parse_imm(char *imm, Label *labels);
int parse_label(char *label, Label *labels);

int main(int argc, char *argv[])
{
    FILE *asm_fp = NULL, *imemin_fp = NULL, *dmemin_fp = NULL;
    Label *labels = NULL;

    /*Open files.*/
    if (open_files(argc, argv, &asm_fp, &imemin_fp, &dmemin_fp))
    {
        return 1;
    }

    /*First pass.*/
    first_pass(asm_fp, &labels);

    /*Second pass.*/
    second_pass(asm_fp, imemin_fp, dmemin_fp, labels);

    /*Close files.*/
    fclose(asm_fp);
    fclose(imemin_fp);
    fclose(dmemin_fp);
    return 0;
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
 * @brief Function for opening files at the begining to the program.
 *
 * @param argc Number of command line arguments.
 * @param argv The command line arguments which contain all file names.
 * @param asm_fp A pointer to the file pointer of program.txt input file.
 * @param imemin_fp A pointer to the file pointer of imemin.txt output file.
 * @param dmemin_fp A pointer to the file pointer of dmemin.txt output file.
 * @return 0 on successful initialization, 1 on failure.
 */
int open_files(int argc, char *argv[], FILE **asm_fp, FILE **imemin_fp, FILE **dmemin_fp)
{
    int i;
    /*Check for valid number of command line arguments.*/
    if (argc != 4)
    {
        return 1;
    }
    /*Stripping new line characters from all command line arguments.*/
    for (i = 0; i < argc; i++)
    {
        strip_newline(argv[i]);
    }
    /*Open input and output files.*/
    *asm_fp = fopen(argv[1], "r");
    if (!*asm_fp)
    {
        return 1;
    }
    *imemin_fp = fopen(argv[2], "w");
    if(!*imemin_fp)
    {
        fclose(*asm_fp);
        return 1;
    }
    *dmemin_fp = fopen(argv[3], "w");
    if (!*dmemin_fp)
    {
        fclose(*asm_fp);
        fclose(*imemin_fp);
        return 1;
    }
    return 0;
}

/**
 * @brief Function for searching label addresses in .asm file.
 * This function searches for labels in the .asm file and saves them in a list along with their addresses.
 *
 * @param fp A pointer to an .asm file.
 * @param label A pointer to the head of a labels list.
 */
void first_pass(FILE *fp, Label **labels)
{
    Label *curr = NULL, *tmp_label = NULL;
    int add_line, char_idx, label_count = 0, row_count = 0;
    char line[MAX_LINE], *first_word, *second_word;
    /*Find all labels.*/
    while (fgets(line, MAX_LINE, fp) != NULL)
    {
        add_line = 1;
        char_idx = 0;
        first_word = strtok(line, " \t\n\r,");

        /*Skip empty lines, .word and comments.*/
        if (first_word == NULL || first_word[0] == '#' || first_word[0] == '.')
        {
            continue;
        }

        /*Check for label.*/
        while (first_word[char_idx] != '\0')
        {

            /*Skip comments.*/
            if (first_word[char_idx] == '#')
            {
                break;
            }

            /*Found new label.*/
            else if (first_word[char_idx] == ':')
            {
                /*Create a new label.*/
                tmp_label = (Label *)malloc(sizeof(Label));
                if (tmp_label == NULL)
                {
                    return;
                }
                strncpy(tmp_label->name, first_word, char_idx);
                tmp_label->name[char_idx] = '\0';
                tmp_label->address = row_count;
                tmp_label->next = NULL;

                /*Add label to list.*/
                if (!*labels)
                {
                    *labels = tmp_label;
                    curr = tmp_label;
                }
                else
                {
                    curr->next = tmp_label;
                    curr = tmp_label;
                }

                second_word = strtok(NULL, " \t\n\r,");
                add_line = line_status(second_word) == 1 ? 1 : 0;
                label_count++;
                break;
            }
            char_idx++;
        }
        row_count += add_line;
    }
    rewind(fp);
}

/**
 * @brief Parses an input .asm file and writes the results to two output .txt files.
 *
 * @param asm_fp A pointer to the input .asm file.
 * @param imemin_fp A pointer to  instruction memory output .txt file.
 * @param dmemin_fp A pointer to data memory output .txt file.
 * @param labels List of labels, containing all labels in the .asm file and their addresses.
 */
void second_pass(FILE *asm_fp, FILE *imemin_fp, FILE *dmemin_fp, Label *labels)
{
    int word_count, status, dmemin_depth = 0, dmemin[MEM_DEPTH] = {0};
    char line[MAX_LINE], *word, *words[MAX_WORDS];

    /*parse the assembly file.*/
    while (fgets(line, MAX_LINE, asm_fp) != NULL)
    {
        word_count = 0;
        word = strtok(line, " \t\n\r,");
        status = line_status(word);

        /*Skip empty lines and comments.*/
        if (status == 0)
        {
            continue;
        }

        /*If there is a label check the rest of the line.*/
        else if (status == 3)
        {
            word = strtok(NULL, " \t\n\r,");
            status = line_status(word);
        }

        /*Parse the line into words.*/
        while (word != NULL && word_count < MAX_WORDS)
        {
            words[word_count++] = word;
            word = strtok(NULL, " \t\n\r,");
        }

        /*Skip empty lines and comments.*/
        if (status == 0)
        {
            continue;
        }

        /*Write to imemin.*/
        else if (status == 1)
        {
            parse_line_imemin(words, imemin_fp, labels);
        }

        /*Parse initial data memory image.*/
        else if (status == 2)
        {
            set_memory(words, dmemin, &dmemin_depth);
        }
    }
    /*Write to dmemin.*/
    dmemin_write(dmemin_fp, dmemin, dmemin_depth);
}

/**
 * @brief Function for checking the type of line in the .asm file by checking the first word in the line.
 *
 * @param token The first word in the line.
 * @return 0 if it is an empty line or a comment line,
 * 1 if it is an instruction line,
 * 2 if it is a pseudo-instruction line,
 * and 3 if it is label.
 */
int line_status(char *token)
{
    int i = 0;

    /*Skip empty lines and comments.*/
    if (token == NULL || token[0] == '#')
    {
        return 0;
    }

    /*Check for initial data memory image command.*/
    else if (token[0] == '.')
    {
        return 2;
    }

    /*Search for lables.*/
    while (token[i] != '\0')
    {
        if (token[i] == ':')
        {
            return 3;
        }
        i++;
    }

    /*Line relevant to imemin.*/
    return 1;
}

/**
 * @brief Parses an instruction line and writes the result to the output file.
 *
 * @param tokens An array of strings representing the 7 parts of the instruction.
 * @param imemin_fp A pointer to the output imemin.txt file to write the result of the parsing.
 * @param labels List of labels, containing all labels in the .asm file and their addresses.
 */
void parse_line_imemin(char *tokens[], FILE *imemin_fp, Label *labels)
{
    int opcode, rd, rs, rt, rm, imm1, imm2;

    /*Parse the line into opcode, registers and immediate values.*/
    opcode = parse_opcode(tokens[0]);
    rd = parse_reg(tokens[1]);
    rs = parse_reg(tokens[2]);
    rt = parse_reg(tokens[3]);
    rm = parse_reg(tokens[4]);
    imm1 = parse_imm(tokens[5], labels);
    imm2 = parse_imm(tokens[6], labels);

    /*Write to imemin.*/
    fprintf(imemin_fp, "%02X%01X%01X%01X%01X%03X%03X\n", opcode, rd, rs, rt, rm, imm1 & 0xFFF, imm2 & 0xFFF);
}

/**
 * @brief Function that represents the initial memory image using an array.
 *
 * @param tokens An array of strings representing the 3 parts of a pseudo-instruction.
 * @param dmemin An integer array representing the the memory.
 * @param dmemin_depth A pointer to the maximum depth of memory.
 */
void set_memory(char *tokens[], int dmemin[], int *dmemin_depth)
{
    int address, data;

    /*Parse the line into address and data.*/
    address = (int)strtol(tokens[1], NULL, 0);
    data = (int)strtol(tokens[2], NULL, 0);
    /*Set the data at the corresponding address in the helper array.*/
    dmemin[address] = data;

    /*Update the depth of dmemin.*/
    *dmemin_depth = (*dmemin_depth > address + 1) ? *dmemin_depth : address + 1;
}

/**
 * @brief Function to write the initial memory image to the dmemin.txt output file.
 *
 * @param dmemin_fp A pointer to the output dmemin.txt file to write the initial memory image.
 * @param dmemin An array representing the initial memory image.
 * @param dmemin_depth The depth of the initial memory image.
 */
void dmemin_write(FILE *dmemin_fp, int dmemin[], int dmemin_depth)
{
    int i;

    /*Write to dmemin.*/
    for (i = 0; i < dmemin_depth; i++)
    {
        fprintf(dmemin_fp, "%08X\n", dmemin[i] & 0xFFFFFFFF);
    }
}

/**
 * @brief Parses the opcodes by assigning each opcode its corresponding value.
 *
 * @param opcode The opcode to be parsed.
 * @return The corresponding number of each opcode.
 */
int parse_opcode(char *opcode)
{
    /*Parse the opcode.*/
    if (strcmp(opcode, "add") == 0)
    {
        return 0;
    }
    if (strcmp(opcode, "sub") == 0)
    {
        return 1;
    }
    if (strcmp(opcode, "mac") == 0)
    {
        return 2;
    }
    if (strcmp(opcode, "and") == 0)
    {
        return 3;
    }
    if (strcmp(opcode, "or") == 0)
    {
        return 4;
    }
    if (strcmp(opcode, "xor") == 0)
    {
        return 5;
    }
    if (strcmp(opcode, "sll") == 0)
    {
        return 6;
    }
    if (strcmp(opcode, "sra") == 0)
    {
        return 7;
    }
    if (strcmp(opcode, "srl") == 0)
    {
        return 8;
    }
    if (strcmp(opcode, "beq") == 0)
    {
        return 9;
    }
    if (strcmp(opcode, "bne") == 0)
    {
        return 10;
    }
    if (strcmp(opcode, "blt") == 0)
    {
        return 11;
    }
    if (strcmp(opcode, "bgt") == 0)
    {
        return 12;
    }
    if (strcmp(opcode, "ble") == 0)
    {
        return 13;
    }
    if (strcmp(opcode, "bge") == 0)
    {
        return 14;
    }
    if (strcmp(opcode, "jal") == 0)
    {
        return 15;
    }
    if (strcmp(opcode, "lw") == 0)
    {
        return 16;
    }
    if (strcmp(opcode, "sw") == 0)
    {
        return 17;
    }
    if (strcmp(opcode, "reti") == 0)
    {
        return 18;
    }
    if (strcmp(opcode, "in") == 0)
    {
        return 19;
    }
    if (strcmp(opcode, "out") == 0)
    {
        return 20;
    }
    if (strcmp(opcode, "halt") == 0)
    {
        return 21;
    }
    return -1;
}

/**
 * @brief Parses the registers by assigning each register its corresponding value.
 *
 * @param reg The register to be parsed.
 * @return The corresponding number of each register.
 */
int parse_reg(char *reg)
{
    /*Parse the register.*/
    if (strcmp(reg, "$zero") == 0)
    {
        return 0;
    }
    if (strcmp(reg, "$imm1") == 0)
    {
        return 1;
    }
    if (strcmp(reg, "$imm2") == 0)
    {
        return 2;
    }
    if (strcmp(reg, "$v0") == 0)
    {
        return 3;
    }
    if (strcmp(reg, "$a0") == 0)
    {
        return 4;
    }
    if (strcmp(reg, "$a1") == 0)
    {
        return 5;
    }
    if (strcmp(reg, "$a2") == 0)
    {
        return 6;
    }
    if (strcmp(reg, "$t0") == 0)
    {
        return 7;
    }
    if (strcmp(reg, "$t1") == 0)
    {
        return 8;
    }
    if (strcmp(reg, "$t2") == 0)
    {
        return 9;
    }
    if (strcmp(reg, "$s0") == 0)
    {
        return 10;
    }
    if (strcmp(reg, "$s1") == 0)
    {
        return 11;
    }
    if (strcmp(reg, "$s2") == 0)
    {
        return 12;
    }
    if (strcmp(reg, "$gp") == 0)
    {
        return 13;
    }
    if (strcmp(reg, "$sp") == 0)
    {
        return 14;
    }
    if (strcmp(reg, "$ra") == 0)
    {
        return 15;
    }
    return -1;
}

/**
 * @brief Function for parsing the immediate fields.
 *
 * @param imm The immediate field to be parsed.
 * @param labels List of labels, containing all labels in the .asm file and their addresses.
 * @return The parsed immediate field.
 */
int parse_imm(char *imm, Label *labels)
{
    /*Parse the immediate value.*/

    /*Check if the immediate value is a hexadecimal or decimal value.*/
    if (isalpha(imm[0]))
    {
        return parse_label(imm, labels);
    }

    /*Check if immediate value is a hex or decimal value.*/
    else if (imm[0] == '0' && (imm[1] == 'x' || imm[1] == 'X'))
    {
        return (int)strtol(imm, NULL, 16);
    }
    else
    {
        return atoi(imm);
    }
}

/**
 * @brief Function for parsing a label by searching it in the labels list.
 *
 * @param labels The label to parse.
 * @param labels List of labels, containing all labels in the .asm file and their addresses.
 * @return The address of the label as the parsed value of the label.
 */
int parse_label(char *label, Label *labels)
{
    Label *curr = labels;

    /*Search for the label in the list.*/
    while (curr != NULL)
    {
        if (strcmp(label, curr->name) == 0)
        {
            return curr->address;
        }
        curr = curr->next;
    }
    return -1;
}