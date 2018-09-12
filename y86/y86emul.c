#include "y86emul.h"
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
    free(registry);
}

//The rrmovl instruction opcode 2 for this.
void rrmovl()
{
    registry->byte = memory[++indx];
    int rB = registry->intstore.high;
    int rA = registry->intstore.low;
    registers[rB] = registers[rA];
    indx++;
    
}

//The irmovl instruction, opcode is 3.
void irmovl()
{
    indx++;
    registry->byte = memory[indx];
    int V = registry->intstore.high;
    indx++;
    int * value = (int *)&memory[indx];
    registers[V] = *value;
    indx += 4;
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
    memcpy(&memory[registers[rB] + *length], &registers[rA], 4);
    indx += 4;
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
    memcpy(&registers[rA], &memory[registers[rB] + *length], 4);
    indx += 4;
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
    int *x = &registers[rA];
    int *y = &registers[rB];
    int val = 0;
    switch(opcode)
    {
        case 0://addl
            OF = *x > 0 && *y > INT_MAX - *x ? 1 :
            *x < 0 && *y < INT_MIN - *x ? 1 : 0;
            
            val = *x + *y;
            break;
        case 1://subl
            OF = *x > 0 && *y < INT_MIN + *x ? 1 :
            *x < 0 && *y > INT_MAX + *x ? 1 : 0;
            
            val = *y - *x;
            break;
        case 2://andl
            OF = 0;
            val = *y & *x;
            break;
        case 3://xorl
            val = *y^*x;
            break;
        case 4://mull
            val = *x * *y;
            
            OF = *x > 0 && *y > 0 && val <= 0 ? 1 ://both args are > 0 but result is < 0
            *x < 0 && *y < 0 && val <= 0 ? 1 ://both args are < 0 but result is  < 0
            val > 0 ? 1 : //one of the args is < 0 but result is > 0
            0;//no overflow
            
            break;
        case 5://cmpl
            checkFlags(*y - *x);
            indx++;
            return;
        default:
            fprintf(stderr, "Error: Invalid instruction.\n");
            exit(1);
            return;
    }
    registers[rB] = val;
    checkFlags(registers[rB]);
    indx++;
}

//The jump instruction, opcode is 7.
void jXX()
{
    int opcode = opcodes->intstore.high;
    indx++;
    unsigned int destination;
    switch(opcode)
    {
        case 0://jmp
            break;
        case 1://jle
            if (ZF == 1 || SF != OF)
                break;
            indx += 3;
            return;
        case 2://jl
            if (SF != OF)
                break;
            indx += 3;
            return;
        case 3://je
            if (ZF == 1)
                break;
            indx += 4;
            return;
        case 4://jne
            if (ZF == 0)
                break;
            indx += 3;
            return;
        case 5://jge
            if (SF == OF)
                break;
            indx += 3;
            return;
        case 6://jg
            if (ZF == 0 && SF == OF)
                break;
            indx += 3;
            return;
        default:
            fprintf(stderr, "Error: Invalid instruction.\n");
            exit(1);
            return;
    }
    memcpy(&destination, &memory[indx], 4);
    indx = destination;
}

//The call instruction, opcode is 8.
void call()
{
    indx++;
    unsigned int offset = *((unsigned int*)&memory[indx]);
    registers[4] -= 4;
    indx+= 4;
    *((unsigned int *)&(memory[registers[4]])) = indx;
    indx = offset;
}

//The ret instruction, opcode is 9.
void ret()
{
    memcpy(&indx, &memory[registers[4]], 4);
    registers[4] += 4;
}

//The pushl instruction, opcode is 10.
void pushl()
{
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    int value = registers[4] -= 4;
    *((unsigned int*)&memory[value]) = registers[rA];
    indx++;
}

//The popl instruction, opcode is 11.
void popl()
{
    indx++;
    registry->byte = memory[indx];
    int rA = registry->intstore.low;
    int value = registers[4];
    registers[rA] = *((unsigned int *)&(memory[value]));
    registers[4] += 4;
    indx++;
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
    
    switch(opcode)
    {
        case 0:
        {
            char byte;
            ZF = scanf("%c", &byte) == EOF ? 1 : 0;
            memcpy(&memory[registers[rA] + *offset], &byte, 1);
            indx += 4;
            break;
        }
        case 1:
        {
            int value;
            ZF = scanf("%d", &value) == EOF ? 1 : 0;
            memcpy(&memory[registers[rA] + *offset], &value, 4);
            indx += 4;
            break;
        }
        default:
            fprintf(stderr, "Error: Invalid instruction.\n");
            exit(1);
            return;
    }
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
    switch(opcode)
    {
        case 0:
            printf("%c", memory[registers[rA] + *offset]);
            break;
        case 1:
            printf("%d", *(int *)&memory[registers[rA] + *offset]);
            break;
        default:
            fprintf(stderr, "Error: Invalid instruction.\n");
            exit(1);
            return;
    }
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
    registers[rA] = (int)(int8_t)memory[registers[rB] + *offset];
    indx += 4;
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
    while (instruct != 0x10) {
        count++;
        instruct = memory[indx];
        opcodes = malloc(sizeof(bitfield));
        opcodes->byte = instruct;
        int opcode = opcodes->intstore.low;
        registry = malloc(sizeof(bitfield));
        switch(opcode)
        {
            case 0: noOp(); break;
            case 1: halt(); return;
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
    return 0;
}
