#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX_FILE_LENGTH 3584
#define ROM_START 0x200
#define INSTRUCTION_SIZE 2
#define FONT_START 0x50
#define MAX_INSTRUCTIONS 1000


static const uint8_t FONTSET[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F

};


typedef struct{
    uint8_t memory[4096];
    uint8_t V[16];
    uint16_t I;
    uint16_t PC;
    uint16_t stack[16];
    uint8_t SP;
    uint8_t display[32][64];
    uint8_t delayTimer;
    uint8_t soundTimer;
    uint8_t keyboard[16];
} Chip8;




long loadRom(const char *path, uint8_t* memory);
void dumpMemory(long ROMsize, const uint8_t* memory);
uint16_t fetchOpcode(size_t PC, const uint8_t* memory);
void disassembler(uint16_t instrucion);
Chip8 initChip8();
int runROM(Chip8* chip8);
int execute(uint16_t opcode, Chip8* chip8);
void drawScreen(const Chip8* chip8);

int main(int argc, char *argv[]){


    if(argc<2){
        printf("Invalid argument.");
        return 1;
    }

    Chip8 chip8 = initChip8();

    long size = loadRom(argv[1], chip8.memory);
    if (size <0){
        printf("The file reading process FAILED. \n");
        return 1;
    }
    printf("The file reading process SUCCEEDED \n");

    dumpMemory(size, chip8.memory);

    //for(long i = ROM_START; i< ROM_START + size; i+=2){
    //    printf("0x%04lX: ", i);
    //    disassembler(fetchOpcode((size_t)i,chip8.memory));
    //}

    runROM(&chip8);

    return 0; 

}


long loadRom(const char *path, uint8_t* memory){

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


void dumpMemory(long ROMsize, const uint8_t* memory){
    for(long i = ROM_START; i< ROM_START + ROMsize; i++){
        if (i % 16 == 0) printf("0x%04lX: ", i);
        printf("%02X ", memory[i]);
        if (i % 16 == 15) printf("\n");
    }
    printf("\n");
}

uint16_t fetchOpcode(size_t PC, const uint8_t* memory){
    uint16_t instruction = memory[PC];
    instruction <<= 8;
    instruction |= memory[PC+1]; //ojo con leer fuera de memoria.
    return instruction;

}


void disassembler(uint16_t instruction){
    int nibbleAlto = (instruction & 0xF000) >> 12;

    switch (nibbleAlto)
    {
    
    case 0x0:
        switch (instruction & 0x00FF) {
        case 0xE0: printf("CLS\n"); break;
        case 0xEE: printf("RET\n"); break;
        default:   printf("Unknown %04X\n", instruction); break;
        }
        break;
    
    case 0x1:
        printf("JMP %03X\n", instruction & 0x0FFF);
        break;

    case 0x6:
        printf("LD V%01X, %02X\n", (instruction & 0x0F00) >> 8, instruction & 0x00FF);
        break;

    case 0xA:
        printf("LD I, %03X\n", instruction & 0x0FFF);
        break;

    case 0xD:
        printf("Draw\n");
        break;
    
    default:
        printf("Unknown instruction\n");
        break;
    }

}


Chip8 initChip8(){
    Chip8 chip8 = {0};
    chip8.PC = 0x200;
    memcpy(chip8.memory + FONT_START, FONTSET, sizeof(FONTSET));
    return chip8;
}


int execute(uint16_t opcode, Chip8* chip8){
    int nibbleAlto = (opcode & 0xF000) >> 12;

    switch (nibbleAlto)
    {
    
    case 0x0:
        switch (opcode & 0x00FF) {
        case 0xE0: memset(chip8->display, 0, sizeof(chip8->display)); break;
        case 0xEE: printf("RET\n"); break;
        default:   printf("Unknown %04X\n", opcode); break;
        }
        break;
    
    case 0x1:

        chip8->PC = opcode & 0x0FFF; //1nnn -> jmp nnn
        break;

    case 0x6:
        chip8->V[(opcode & 0x0F00)>>8] = opcode & 0x00FF; //6xnn -> ld vx nn
        break;

    case 0xA:
        chip8->I = opcode & 0x0FFF; //Annn ->  LD I nnn
        break;

    case 0xD:
        uint8_t X = (opcode & 0x0F00)>>8;
        uint8_t Y = (opcode & 0x00F0)>>4;
        uint8_t N = (opcode & 0x000F);
        chip8->V[0xF] = 0; //V[0xF] = V[15] pero en la documentacion se llama VF, es un tema de sintaxis.
        uint8_t x0 = chip8->V[X] & 63; //wrap
        uint8_t y0 = chip8->V[Y] & 31;
        for(size_t row = 0; row<N; row++){
            if(y0+row >= 32) break;
            uint8_t spriteByteRow = chip8->memory[chip8->I + row];
            for (size_t col = 0; col < 8; col++){
                if(x0+col >= 64) break;
                uint8_t bit = (spriteByteRow >> (7-col)) & 1;
                if(bit){
                    if(chip8->display[y0+row][x0+col]){
                        chip8->display[y0+row][x0+col] = 0;
                        chip8->V[0xF] = 1;
                    }
                    else{
                        chip8->display[y0+row][x0+col] = 1;
                    }
                }
                 
            }
            
        }
        
        break;
    
    default:
        printf("Unknown instruction\n");
        break;
    }

    return 0; //por ahora
}

void drawScreen(const Chip8 *chip8){
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 64; x++) {
            printf("%c", chip8->display[y][x] ? '#' : ' ');
        }
        printf("\n");
    }
}

int runROM(Chip8* chip8){
    //uint8_t running = 1;
    long cycles = 0;
    while(cycles <=MAX_INSTRUCTIONS){ //despues vemos de que condicion de corte hacer.
        uint16_t opcode = fetchOpcode(chip8->PC, chip8->memory);
        chip8->PC += 2;
        execute(opcode, chip8); // pense en poner running = execute(opcode) nose si esto sera poco declarativo, la idea es que execute devuelva 1 si se ejecuto correctamente, -1 si no (o si se colgo, adentro del execute se ve como detecto que llegue al fin, esto es temporal, despues lo cambiare.)
        cycles++;
    }

    drawScreen(chip8);
    printf("PC=%04X I=%04X V0=%02X V1=%02X\n", chip8->PC, chip8->I, chip8->V[0], chip8->V[1]);
    return 0; //despues lo cambio para que pueda devolver -1 en caso de algun error.
}
