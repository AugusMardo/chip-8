#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#define SCALE 15
#define SCREEN_W 64
#define SCREEN_H 32
#define MAX_FILE_LENGTH 3584
#define ROM_START 0x200
#define INSTRUCTION_SIZE 2
#define FONT_START 0x50
#define INSTRUCTIONS_PER_FRAME 11


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
int runROM(Chip8* chip8, SDL_Renderer* renderer);
int execute(uint16_t opcode, Chip8* chip8);
void drawScreen(const Chip8* chip8);
void stackPush(Chip8 *chip8, uint16_t addr);
uint16_t stackPop(Chip8 *chip8);


int main(int argc, char *argv[]){


    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;

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

    for(long i = ROM_START; i< ROM_START + size; i+=INSTRUCTION_SIZE){
        printf("0x%04lX: ", i);
        disassembler(fetchOpcode((size_t)i,chip8.memory));
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("SDL_Init falló: %s\n", SDL_GetError());
    return 1;
    }

    window = SDL_CreateWindow("CHIP-8",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * SCALE, SCREEN_H * SCALE, 0);

    if (!window) {
        printf("CreateWindow falló: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer) {
        printf("CreateRenderer falló: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    runROM(&chip8, renderer);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

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
    uint8_t  nibbleAlto = (instruction & 0xF000) >> 12;
    uint8_t  X   = (instruction & 0x0F00) >> 8;
    uint8_t  Y   = (instruction & 0x00F0) >> 4;
    uint8_t  N   =  instruction & 0x000F;
    uint8_t  NN  =  instruction & 0x00FF;
    uint16_t NNN =  instruction & 0x0FFF;

    switch (nibbleAlto)
    {

    case 0x0:
        switch (NN) {
            case 0xE0: printf("CLS\n"); break;
            case 0xEE: printf("RET\n"); break;
            default:   printf("Unknown %04X\n", instruction); break;
        }
        break;

    case 0x1:
        printf("JMP %03X\n", NNN);
        break;

    case 0x2:
        printf("CALL %03X\n", NNN);
        break;

    case 0x3:
        printf("SE V%01X, %02X\n", X, NN);
        break;

    case 0x4:
        printf("SNE V%01X, %02X\n", X, NN);
        break;

    case 0x5:
        printf("SE V%01X, V%01X\n", X, Y);
        break;

    case 0x6:
        printf("LD V%01X, %02X\n", X, NN);
        break;

    case 0x7:
        printf("ADD V%01X, %02X\n", X, NN);
        break;

    case 0x8:
        switch (N) {
            case 0x0: printf("LD V%01X, V%01X\n",       X, Y); break;
            case 0x1: printf("OR V%01X, V%01X\n",       X, Y); break;
            case 0x2: printf("AND V%01X, V%01X\n",      X, Y); break;
            case 0x3: printf("XOR V%01X, V%01X\n",      X, Y); break;
            case 0x4: printf("ADD V%01X, V%01X\n",      X, Y); break;
            case 0x5: printf("SUB V%01X, V%01X\n",      X, Y); break;
            case 0x6: printf("SHR V%01X {, V%01X}\n",   X, Y); break;
            case 0x7: printf("SUBN V%01X, V%01X\n",     X, Y); break;
            case 0xE: printf("SHL V%01X {, V%01X}\n",   X, Y); break;
            default:  printf("Unknown %04X\n", instruction); break;
        }
        break;

    case 0x9:
        printf("SNE V%01X, V%01X\n", X, Y);
        break;

    case 0xA:
        printf("LD I, %03X\n", NNN);
        break;

    case 0xB:
        printf("JP V0, %03X\n", NNN);
        break;

    case 0xC:
        printf("RND V%01X, %02X\n", X, NN);
        break;

    case 0xD:
        printf("DRW V%01X, V%01X, %01X\n", X, Y, N);
        break;

    case 0xE:
        switch (NN) {
            case 0x9E: printf("SKP V%01X\n", X); break;
            case 0xA1: printf("SKNP V%01X\n", X); break;
            default:   printf("Unknown %04X\n", instruction); break;
        }
        break;

    case 0xF:
        switch (NN) {
            case 0x07: printf("LD V%01X, DT\n", X); break;
            case 0x0A: printf("LD V%01X, K\n", X); break;
            case 0x15: printf("LD DT, V%01X\n", X); break;
            case 0x18: printf("LD ST, V%01X\n", X); break;
            case 0x1E: printf("ADD I, V%01X\n", X); break;
            case 0x29: printf("LD F, V%01X\n", X); break;
            case 0x33: printf("LD B, V%01X\n", X); break;
            case 0x55: printf("LD [I], V0-V%01X\n", X); break;
            case 0x65: printf("LD V0-V%01X, [I]\n", X); break;
            default:   printf("Unknown %04X\n", instruction); break;
        }
        break;

    default:
        printf("Unknown %04X\n", instruction);
        break;
    }
}


Chip8 initChip8(){
    Chip8 chip8 = {0};
    chip8.PC = 0x200;
    memcpy(chip8.memory + FONT_START, FONTSET, sizeof(FONTSET));
    srand(time(NULL));
    return chip8;
}


int execute(uint16_t opcode, Chip8* chip8){
    uint8_t  nibbleAlto = (opcode & 0xF000) >> 12;
    uint8_t  X   = (opcode & 0x0F00) >> 8;
    uint8_t  Y   = (opcode & 0x00F0) >> 4;
    uint8_t  N   =  opcode & 0x000F;
    uint8_t  NN  =  opcode & 0x00FF;
    uint16_t NNN =  opcode & 0x0FFF;

    switch (nibbleAlto)
    {

    case 0x0:
        switch (NN) {
            case 0xE0: memset(chip8->display, 0, sizeof(chip8->display)); break;
            case 0xEE: chip8->PC = stackPop(chip8); break;
            default:   printf("Unknown %04X\n", opcode); break;
        }
        break;

    case 0x1:
        chip8->PC = NNN;                      // 1NNN -> JMP NNN
        break;

    case 0x2:
        stackPush(chip8, chip8->PC);
        chip8->PC = NNN;
        break;

    case 0x3: 
        if(chip8->V[X] == NN){
            chip8->PC+=2;
        }
        break;

    case 0x4:
        if(chip8->V[X] != NN){
            chip8->PC+=2;
        }
        break;

    case 0x5:
        if(chip8->V[X] == chip8->V[Y]){
        chip8->PC+=2;
        }
        break;

    case 0x6:
        chip8->V[X] = NN;                     // 6XNN -> LD Vx, NN
        break;

    case 0x7:
        chip8->V[X]+=NN;
        break;
    
    
    case 0x8:
        switch (N) {
            case 0x0: chip8->V[X] = chip8->V[Y]; break;
            case 0x1: chip8->V[X] |= chip8->V[Y]; break;
            case 0x2: chip8->V[X] &= chip8->V[Y]; break;
            case 0x3: chip8->V[X] ^= chip8->V[Y]; break;
            case 0x4:{  
                uint16_t sum = chip8->V[X] + chip8->V[Y];
                chip8->V[X] = sum;
                chip8->V[0xF] = (sum > 255) ? 1 : 0; 
                break;
            }
            case 0x5:{  
                uint8_t borrow = 0;
                if(chip8->V[X] < chip8->V[Y]) borrow = 1;
                chip8->V[X] -= chip8->V[Y];
                chip8->V[0xF] = !borrow;
                break;
            }
            case 0x6:{ //implementacion cowgod
                uint8_t bitOn = chip8->V[X] & 0x01;
                chip8->V[X] >>= 1;
                chip8->V[0xF] = bitOn;
                break;
            }
            case 0x7:{ 
                uint8_t borrow = 0;
                if(chip8->V[X] > chip8->V[Y]) borrow = 1;
                chip8->V[X] = chip8->V[Y] - chip8->V[X];
                chip8->V[0xF] = !borrow;
                break;
            }
            case 0xE:{//implementacion cowgod
                uint8_t bitOn = (chip8->V[X] & 0x80)>>7;
                chip8->V[X] <<= 1;
                chip8->V[0xF] = bitOn;
                break;
            }
            default:  printf("Unknown %04X\n", opcode); break;
        }
        break;

    case 0x9:
        if(chip8->V[X] != chip8->V[Y]) chip8->PC+=2;
        break;

    case 0xA:
        chip8->I = NNN;                       // ANNN -> LD I, NNN
        break;

    case 0xB:
        chip8->PC = chip8->V[0] + NNN;
        break;

    case 0xC:
        chip8->V[X] = (rand() % 256) & NN;
        break;

    case 0xD: {                               // DXYN -> DRW Vx, Vy, N
        chip8->V[0xF] = 0;
        uint8_t x0 = chip8->V[X] & 63;        // wrap del origen
        uint8_t y0 = chip8->V[Y] & 31;

        for (size_t row = 0; row < N; row++) {
            if (y0 + row >= 32) break;        // clipping vertical
            uint8_t spriteByteRow = chip8->memory[chip8->I + row];

            for (size_t col = 0; col < 8; col++) {
                if (x0 + col >= 64) break;    // clipping horizontal
                uint8_t bit = (spriteByteRow >> (7 - col)) & 1;
                if (bit) {
                    if (chip8->display[y0 + row][x0 + col]) {
                        chip8->display[y0 + row][x0 + col] = 0;
                        chip8->V[0xF] = 1;
                    } else {
                        chip8->display[y0 + row][x0 + col] = 1;
                    }
                }
            }
        }
        break;
        
    }

    case 0xE:
    switch (NN) {
        case 0x9E: if(chip8->keyboard[chip8->V[X] & 0x0F]) chip8->PC += 2; break;
        case 0xA1: if(!chip8->keyboard[chip8->V[X] & 0x0F]) chip8->PC += 2; break;
        default:   printf("Unknown %04X\n", opcode); break;
    }
    break;

    case 0xF:
        switch (NN) {
            case 0x07: chip8->V[X] = chip8->delayTimer; break;
            case 0x0A:{
                uint8_t keyPressed = 0;
                for(int i = 0; i<=15 && !keyPressed; i++){
                    if(chip8->keyboard[i]){
                        chip8->V[X] = i;
                        keyPressed = 1;
                    }
                }
                if(!keyPressed) chip8->PC -=2;

                break;
            }
            case 0x15: chip8->delayTimer = chip8->V[X]; break;
            case 0x18: chip8->soundTimer = chip8->V[X]; break;
            case 0x1E: chip8->I += chip8->V[X]; break;
            case 0x29: chip8->I = FONT_START+(5*chip8->V[X]); break;
            case 0x33:{ 
                uint8_t centenas = chip8->V[X] / 100;
                uint8_t decenas = (chip8->V[X]%100) /10;
                uint8_t unidades = chip8->V[X]%10;
                chip8->memory[chip8->I] = centenas;
                chip8->memory[chip8->I+1] = decenas;
                chip8->memory[chip8->I+2] = unidades;           
                break;
            }
            case 0x55: 
                for(int i = 0; i<=X; i++){ //implementacion moderna, no modifica I.
                    chip8->memory[chip8->I+i] = chip8->V[i];
                }    
                break;
            case 0x65: 
                for(int i = 0; i<=X; i++){ //implementacion moderna, no modifica I.
                    chip8->V[i] = chip8->memory[chip8->I+i];
                } 
                break;
            default:   printf("Unknown %04X\n", opcode); break;
        }
        break;
   

    default:
        printf("Unknown %04X\n", opcode);
        break;
    }

    return 0;
}

void drawScreen(const Chip8 *chip8){
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 64; x++) {
            printf("%s", chip8->display[y][x] ? "██" : "  ");
        }
        printf("\n");
    }
}

int runROM(Chip8* chip8, SDL_Renderer* renderer){
    int running = 1;
    while(running){

        Uint32 frameStart = SDL_GetTicks();
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        for(int i = 0; i<=INSTRUCTIONS_PER_FRAME; i++){
            uint16_t opcode = fetchOpcode(chip8->PC, chip8->memory);
            chip8->PC += INSTRUCTION_SIZE;
            execute(opcode, chip8);
            //drawScreen(chip8);
            //printf("PC=%04X I=%04X V0=%02X V1=%02X\n", chip8->PC, chip8->I, chip8->V[0], chip8->V[1]);
        
        }
       
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);   // negro
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  // blanco
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 64; x++) {
                if (chip8->display[y][x]) {
                    SDL_Rect pixel = { x * SCALE, y * SCALE, SCALE, SCALE };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }

        SDL_RenderPresent(renderer);

        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 16) {
            SDL_Delay(16 - frameTime);
        }

        if(chip8->delayTimer>0) chip8->delayTimer--;
        if(chip8->soundTimer>0) chip8->soundTimer--;
    }

    
    
    return 0; //despues lo cambio para que pueda devolver -1 en caso de algun error.
}

void stackPush(Chip8 *chip8, uint16_t addr){
    if (chip8->SP >= 16) {
        printf("Stack overflow en PC=%04X\n", chip8->PC);
        exit(1);
    }
    chip8->stack[chip8->SP++] = addr;
}

uint16_t stackPop(Chip8 *chip8){
    if (chip8->SP == 0) {
        printf("Stack underflow en PC=%04X\n", chip8->PC);
        exit(1);
    }
    return chip8->stack[--chip8->SP];
}
