#include <stdio.h>
#include <stdint.h>

#define MAX_FILE_LENGTH 3584
#define ROM_START 0x200
#define INSTRUCTION_SIZE 2

uint8_t memory[4096];





long loadRom(const char *path);
int dumpMemory(long ROMsize);
uint16_t fetchOpcode(size_t IP);

int main(int argc, char *argv[]){
    if(argc<2){
        printf("Invalid argument.");
        return 1;
    }
    long size = loadRom(argv[1]);
    if (size <0){
        printf("The file reading process FAILED. \n");
        return 1;
    }
    printf("The file reading process SUCCEEDED \n");
    
    dumpMemory(size);

    return 0;

}


long loadRom(const char *path){

    FILE* file = fopen(path, "rb");

    printf("Reading file... \n");

    if(file == NULL){
        printf("NULL file \n");
        return -1;
    }
 
    fseek(file, 0, SEEK_END);
    long fileLength = ftell(file);
    
    if (fileLength < 0 || fileLength > MAX_FILE_LENGTH){
        printf("Invalid ROM size: %ld (max is: %d) \n", fileLength, MAX_FILE_LENGTH );
        fclose(file);
        return -1;
    }

    rewind(file);
    size_t bytesRead =fread(memory + ROM_START, 1, fileLength, file);
    if((size_t)fileLength != bytesRead){
        fclose(file);
        printf("Reading ROM error, bytes expected: %ld but read %zu instead. \n", fileLength, bytesRead);
        return -1;
    }

    fclose(file);
    printf("File size is: %ld \n", fileLength);
    return fileLength;

}


int dumpMemory(long ROMsize){
    for(long i = ROM_START; i< ROM_START + ROMsize; i++){
        if (i % 16 == 0) printf("0x%04lX: ", i);
        printf("%02X ", memory[i]);
        if (i % 16 == 15) printf("\n");
    }
    printf("\n");
    return 0;
}

uint16_t fetchOpcode(size_t IP){
    uint16_t instruction = memory[IP];
    instruction <<= 8;
    instruction |= memory[IP+1]; //ojo con leer fuera de memoria.
    return instruction;

}