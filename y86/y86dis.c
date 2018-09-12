#include "y86dis.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>

//bitfield typedef union
typedef union bitfield{
    struct store{
        unsigned int high: 4;
        unsigned int low: 4;
    }intstore;
    unsigned char  byte;
} bitfield;

int registers[8];

char* memory;

int OF, ZF, SF;

unsigned int indx, counter, memorySize;

int foundText = 0;

bitfield *registry;
bitfield *opcodes;

//Initializes the memory, first line in y86 instructions should be .size instruction
void initializeMemory(char* line)
{
    if (strstr(line, ".size") == NULL)
    {
        fprintf(stderr,"Error: First Instruction should be .size to initialize memory.\n");
        exit(1);
    }
    char* ptr = line;
    ptr += 5;
    memorySize = 0;
    sscanf(ptr, "%x", &memorySize);
    memory = malloc(sizeof(char) * memorySize);
}

//Parses y86 directives from the input file starting at line 2.
void parseDirectives(char* line)
{
    if (line == NULL)
    {
        return;
    }
    char* linePtr = line;
    if (strstr(linePtr, ".text") != NULL)
    {
        linePtr += 5;
        sscanf(linePtr, "%x", &counter);
        indx = counter;
        linePtr++;
        while(!isspace(*linePtr))
        {
            linePtr++;
        }
        linePtr++;
        while(*linePtr != '\0')
        {
            bitfield* store = malloc(sizeof(bitfield));
            unsigned int instruct;
            sscanf(linePtr, "%2x", &instruct);
            store->byte = instruct;
            memory[counter] = store->byte;
            counter++;
            //printf("Value: %d\n", store->byte);
            linePtr += 2;
            free(store);
        }
        foundText = 1;
    }
    else if (strstr(linePtr, ".byte") != NULL)
    {
        linePtr += 5;
        unsigned int address;
        int value;
        sscanf(linePtr, "%x%x", &address, &value);
        memory[address] = value;
    }
    else if (strstr(linePtr, ".long") != NULL)
    {
        linePtr += 5;
        unsigned int address, value;
        char store[2000];
        sscanf(linePtr, "%x%s", &address, store);
        value = atoi(store);
        unsigned int* ptr = (unsigned int*) &memory[address];
        *ptr = value;
    }
    else if (strstr(linePtr, ".string") != NULL)
    {
        linePtr += 7;
        unsigned int address;
        char store[2000];
        sscanf(linePtr,"%x  \"%[^\"]\"", &address, store);
        int index;
        for (index = 0; index < strlen(store); index++)
            memory[address++] = store[index];
        memory[address] = '\0';
    }
}

//Gets the register name.
char* getRegisterName(int id)
{
    
    char* registerName =
    id == 0 ? "eax" : id == 1 ? "ecx" : id == 2 ? "edx" : id == 3 ? "ebx" :
    id == 4 ? "esp" : id == 5 ? "ebp" : id == 6 ? "esi" : "edi";
    
    return registerName;
}

//No operation - opcode is 0 for this.
void noOp()
{
    //do nothing
    indx++;
}

//Halts the program, opcode is 1 for this.
void halt()
{
    printf("Halt\n");
    indx++;
}

//The rrmovl instruction opcode 2 for this.
void rrmovl()
{
    registry->byte = memory[++indx];
    int rB = registry->intstore.high;
    int rA = registry->intstore.low;
    indx++;
    printf("rrmovl %s,%s\n", getRegisterName(rA), getRegisterName(rB));
    
}

//The irmovl instruction, opcode is 3.
void irmovl()
{
    indx++;
    registry->byte = memory[indx];
    int V = registry->intstore.high;
    indx++;
    int * value = (int *)&memory[indx];
    indx += 4;
    printf("irmovl 0x%x,%s\n", *value, getRegisterName(V));
}

//The rmmovl instruction, opcode is 4.
void rmmovl()
{
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    int rB = registry->intstore.high;
    indx++;
    int *length = (int *) &memory[indx];
    indx += 4;
    printf("rmmovl %s, %d(%s)\n", getRegisterName(rA), *length, getRegisterName(rB));
}

//THe mrmovl instruction, opcode is 5.
void mrmovl()
{
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    int rB = registry->intstore.high;
    indx++;
    int *length = (int *) &memory[indx];
    indx += 4;
    printf("mrmovl %d (%s),%s\n", *length, getRegisterName(rB), getRegisterName(rA));
}

//Checks the flags against the parameter and sets them accordingly.
void checkFlags(int check)
{
    ZF = check == 0 ? 1 : 0;
    SF = check < 0 ? 1 : 0;
    
}

//The operations instruction, opcode is 6.
void op1()
{
    int opcode = opcodes->intstore.high;
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    int rB = registry->intstore.high;
    char* opname;
    switch(opcode)
    {
        case 0://addl
            opname = "addl";
            break;
        case 1://subl
            opname = "subl";
            break;
        case 2://andl
            opname = "andl";
            break;
        case 3://xorl
            opname = "xorl";
            break;
        case 4://mull
            opname = "mull";
            break;
        case 5://cmpl
            opname = "cmpl";
            indx++;
            break;
        default:
            fprintf(stderr, "Error: Invalid instruction.\n");
            exit(1);
            return;
    }
    printf("%s %s,%s\n", opname, getRegisterName(rA), getRegisterName(rB));
    indx++;
}

//The jump instruction, opcode is 7.
void jXX()
{
    int opcode = opcodes->intstore.high;
    indx++;
    unsigned int destination;
    char* opname;
    switch(opcode)
    {
        case 0://jmp
            opname = "jmp";
            break;
        case 1://jle
            opname = "jle";
            break;
        case 2://jl
            opname = "jl";
            break;
        case 3://je
            opname = "je";
            break;
        case 4://jne
            opname = "jne";
            break;
        case 5://jge
            opname = "jge";
            break;
        case 6://jg
            opname = "jg";
            break;
        default:
            fprintf(stderr, "Error: Invalid instruction.\n");
            exit(1);
            return;
    }
    memcpy(&destination, &memory[indx], 4);
    indx += 4;
    printf("%s %x\n", opname, destination);
}

//The call instruction, opcode is 8.
void call()
{
    indx++;
    unsigned int offset = *((unsigned int*)&memory[indx]);
    indx+= 4;
    printf("call %d\n", offset);
}

//The ret instruction, opcode is 9.
void ret()
{
    indx++;
    printf("ret\n");
}

//The pushl instruction, opcode is 10.
void pushl()
{
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    
    indx++;
    printf("pushl %s\n", getRegisterName(rA));
}

//The popl instruction, opcode is 11.
void popl()
{
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    
    indx++;
    printf("popl %s\n", getRegisterName(rA));
}

//The readX instruction, opcode is 12.
void readX()
{
    int opcode = opcodes->intstore.high;
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    indx++;
    unsigned int *offset = (unsigned int *)&memory[indx];
    char* opname = opcode == 0 ? "readb" : "readl";
    printf("%s %d(%s)\n", opname, *offset, getRegisterName(rA));
    indx += 4;
}

//The write x instruction, opcode is 13.
void writeX()
{
    int opcode = opcodes->intstore.high;
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    indx++;
    unsigned int *offset = (unsigned int *)&memory[indx];
    char* opname = opcode == 0 ? "writeb" : "writel";
    printf("%s %d(%s)\n", opname, *offset, getRegisterName(rA));
    indx += 4;
}

//The movsbl instruction, opcode is 14.
void movsbl()
{
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    int rB = registry->intstore.high;
    indx++;
    int *offset = (int *) (&memory[indx]);
    indx += 4;
    printf("movsbl %s, %d(%s)\n", getRegisterName(rA), *offset, getRegisterName(rB));
}

//Executes the instructions stored in memory.
void executeInstructions()
{
    if (indx > memorySize)
    {
        fprintf(stderr,"Error: Invalid program counter start address.\n");
        exit(1);
    }
    unsigned int instruct = 0;
    int count = 0;
    while (1) {
        if (indx >= memorySize) {
            exit(0);
        }
        count++;
        instruct = memory[indx];
        opcodes = malloc(sizeof(bitfield));
        opcodes->byte = instruct;
        int opcode = opcodes->intstore.low;
        registry = malloc(sizeof(bitfield));
        switch(opcode)
        {
            case 0: noOp(); break;
            case 1: halt(); break;
            case 2: rrmovl(); break;
            case 3: irmovl(); break;
            case 4: rmmovl(); break;
            case 5: mrmovl(); break;
            case 6: op1(); break;
            case 7: jXX(); break;
            case 8: call(); break;
            case 9: ret(); break;
            case 10: pushl(); break;
            case 11: popl(); break;
            case 12: readX(); break;
            case 13: writeX(); break;
            case 14: movsbl(); break;
        }
        free(registry);
    }
}

int main(int argc, char** argv)
{
    char buffer[2500];
    if (argc != 2)
    {
        fprintf(stderr,"Error: Invalid amount of arguments, 1 required.\n");
        return -1;
    }
    if (strcmp(argv[1], "-h") == 0)
    {
        printf("Usage: Please make sure there is a y86 file in this directory.\n");
        printf("Usage: Run this program with <input file> as the only argument.\n");
        return 1;
    }
    FILE* data = fopen(argv[1], "r");
    if (data == NULL)
    {
        fprintf(stderr, "Error: File could not be found.\n");
        return 1;
    }
    if (fgets(buffer, 2500, data) == NULL)
    {
        fprintf(stderr, "Error: Empty file.\n");
        return 1;
    }
    initializeMemory(buffer);
    while(fgets(buffer, 2500, data) != NULL)
    {
        parseDirectives(buffer);
    }
    if (foundText == 0)
    {
        fprintf(stderr, "Error: File does not contain .text directive.\n");
        return 1;
    }
    executeInstructions();
    free(memory);
}
