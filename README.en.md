# CHIP-8

This project is a CHIP-8 interpreter, using SDL for video and input. It runs Pong and classic CHIP-8 ROMs. It passes the `3-corax+` and `4-flags` tests from Timendus' test suite.

![Pong running in the emulator](docs/pong.png)

## About this project

This is the first in a series of projects I have planned to go deeper into low level programming, which is what I like most, so I can find out which specific area interests me the most. It also helps me gain experience and fluency in C.

The project itself is a CHIP-8 interpreter, and the final goal was to have a version complete enough to run Pong and some ROMs from that era, deliberately leaving out the more detailed stuff like sound or a debugging UI. Things that went beyond the original idea of the project.

## Build and usage

Needs `gcc` and SDL2:

```bash
sudo apt install libsdl2-dev
gcc -Wall -Wextra -g chip8.c -o chip8 $(sdl2-config --cflags --libs)
./chip8 roms/pong.ch8         # run
./chip8 roms/pong.ch8 -d      # the -d flag dumps memory and asks the disassembler to print the ROM's instructions to the console
```

## Controls

CHIP-8 used a 4×4 hexadecimal keypad. The standard among CHIP-8 interpreters is to map that 4×4 block geometrically onto the keyboard, like this:

```
 CHIP-8 keypad          Keyboard
   1 2 3 C               1 2 3 4
   4 5 6 D       →       Q W E R
   7 8 9 E               A S D F
   A 0 B F               Z X C V
```

Worth pointing out that the implementation reads input using scancodes so it also works on keyboards with a different key layout (AZERTY for example), keeping the geometric arrangement.

Every ROM has its own controls, but they are rarely documented: you have to try the different keys and see what they do. In Pong specifically, `1` moves the paddle up and `Q` moves it down.

## ROMs

The ROMs are not in this repository since some of them have licenses covering their distribution. There are two useful sources to download them:

- [Timendus/chip8-test-suite](https://github.com/Timendus/chip8-test-suite) - test ROMs
- [kripod/chip8-roms](https://github.com/kripod/chip8-roms) - several classic games

Note that some modern ROMs (Octojam or the [CHIP-8 Archive](https://johnearnest.github.io/chip8Archive/), for example) are made for later versions like SCHIP or XO-CHIP, and this interpreter won't be able to run them. You have to look for the plain CHIP-8 versions.

## How it works

Technically CHIP-8 is not hardware but a bytecode interpreter that Joseph Weisbecker wrote in 1977. Its purpose was to give videogame hobbyists a tool to create games without having to learn 1802 assembly.

Programs are written as 2 byte instructions, big-endian, and get loaded starting at address `0x200`. The first 512 bytes were where the interpreter's own code lived.

What's nice about CHIP-8 is that it's simple enough that you can picture the whole thing without much trouble: 4 KB of RAM, 16 8-bit registers (`V0`-`VF`), a 16-bit register `I` for indexing, a program counter, a stack that only holds return addresses, two 60 Hz timers (one of them for sound), a 64×32 black and white display and a 4×4 keypad.

The main loop is synchronized with the frames, and the program runs at roughly 60 fps. On each frame we process the SDL event queue, execute around 11 instructions, render, suspend execution until the 16 ms are complete (I explain why further down) and finally tick each timer.

## Design decisions

### The disassembler came first

The first thing I did was the program's disassembler, before trying to execute any instruction.

This was useful mainly so I could focus only on getting the instruction decoding right, independently of their implementation. Thanks to this you can check that the instructions the interpreter is receiving are the correct ones, because even if the implementation of the instructions is fine, if we are executing the wrong instruction it's useless.

It also works as a debugging tool that can still be used with the `-d` flag.

An important detail about the disassembler is that it's linear, and therefore it cannot tell code apart from sprites/data (ROMs store both in the same memory space, one after the other, with no distinction whatsoever). This means it will report instructions correctly until it enters the data region, and from there on any instruction it decodes is going to be garbage.

This is not a bug or a flaw in the implementation, it's the expected behaviour. Deciding whether a section is code or data statically is, in general, undecidable. Real disassemblers use a technique called *recursive traversal*, which works very well, but it still fails when there are jumps whose target is decided at runtime.

### The PC increments during fetch, not during execute

In my implementation the fetch happens and the PC is automatically increased by 2 (instructions are 2 bytes) before the instruction executes. This way the PC always points at the next instruction, keeping the machine coherent while we execute an instruction. For example:

- `2NNN` (CALL) pushes a return address, which is already the incremented PC. Nothing needs to be corrected.
- Conditional skips add +2 to the PC, which makes it clear that we're skipping one instruction (instead of doing +4, where it may not be so obvious where that comes from).

Also, this is standard practice in real hardware. For example, `CALL` on x86 pushes the address pointing at the next instruction, and `jal` on RISC-V saves `pc+4`.

That said, this brings a problem you have to keep in mind: if an instruction fails, the PC is no longer aligned with the instruction that failed. Real processors solve this with a dedicated register that holds the exception address: `mepc` on RISC-V, `EPC` on MIPS, `ELR_ELx` on ARM64, `SRR0` on PowerPC. x86 doesn't have a dedicated register and pushes it onto the exception stack frame, but it also distinguishes between *faults* and *traps* precisely by which of the two addresses it needs to save: a fault saves the instruction that failed, because it has to be retried, and a trap saves the next one, because the instruction already completed.

### One byte per pixel

The display (`uint8_t display[32][64]`) takes up 2048 bytes but stores 2048 bits, so almost 90% of the space is wasted. The original CHIP-8 packed 8 pixels per byte, ending up with 256 bytes, which is clearly more efficient in terms of memory.

Still, I think my decision to implement it as a 32×64 byte array makes more sense nowadays for the following reason: if we pack the pixels, every access requires several operations in a row (division, modulo, etc.) and `DXYN` (the most complicated instruction) does A LOT of pixel accesses.

Back in the 70s it made sense to optimise memory usage as much as possible because memory was expensive and scarce. Today in 2026, giving up 1.8 KB to make the code clearer (we access it as `display[y][x]`) is something we can afford, and in my opinion it's the right call.

### The machine state is kept in a struct

I didn't use global variables for the machine state. Everything is stored in a struct called `Chip8` (memory, registers, timers, etc.) and gets passed by pointer to the functions that need to change the state.

This way we have more clarity about what a function can and cannot touch: for example `fetchOpcode(size_t PC, const uint8_t* memory)` receives a position and the memory, which it only reads, and nothing else. With global variables we lose clarity about what each function is capable of, and on top of that all the code would have access to the whole machine, which isn't right.

### Synchronizing execution with the frames

This part gave me a hard time, honestly. At first I had implemented a loop that ran the fetch-decode-execute cycle and kept track of the delta time with an accumulator to figure out when to decrement the 60 Hz timers. The problem was that the loop ran as fast as it possibly could, literally around 10 million times per second, and Pong went at an absurd speed. To give an idea, the original CHIP-8 ran about 700 instructions per second.

The solution was to synchronize with the framerate. Execution was fixed at roughly 60 fps, with a loop that processes around 11 instructions and stops, draws on screen, waits until the frame's 16 ms are complete and only then decrements the timers once (since they run at 60 Hz), which removed the whole delta time counter and the nanosecond arithmetic.

The wait is calculated: we measure how long the frame took to process and sleep only for what's left. In practice the work takes less than a millisecond, so almost always the full 16 are slept, but the logic is there for the case where a frame is heavy to compute. It's 16 and not 16.67 because `SDL_Delay` works in whole milliseconds, so it actually runs at around 62 fps.

As a result we get about 62 fps × ~11 instructions per frame, giving a total of ~690 instructions per second. As a side effect, the timers also run at ~62 Hz instead of 60, 4% faster than they should.

### If the stack fails, the program ends

The CHIP-8 stack is made to hold return addresses only, you can't push operands or anything else onto it. Mine has a depth of 16 addresses (which is enforced in `stackPush` and `stackPop`), the modern convention.

If the stack gets corrupted, I abort the whole execution and print the PC, because everything that comes after can be garbage and makes the error show up a lot of instructions later, which makes it harder to find when exactly the stack went wrong.

## Quirks

CHIP-8 doesn't have a single specification. There's the one from 77 and then a whole bunch of variants that make different decisions, so several instructions have two "correct" implementations. Those behavioural differences are known as *quirks*, and you have to pick which one you implement. Timendus' tests let you see which quirk you picked in each case. This is a summary of the ones I went with:

| Quirk | This implementation | Note |
|---|---|---|
| `8XY6` / `8XYE` (shifts) | Operate on `VX`, ignore `VY` | CHIP-48/SCHIP behaviour, which is the one Cowgod documents. The original does `VX = VY >> 1`. |
| `FX55` / `FX65` (register store/load) | `I` is left unchanged | The original left `I` pointing past the last byte, as a side effect of how the 77 interpreter walked through memory. |
| Sprite clipping in `DXYN` | Clipped at the edges | The origin wraps modulo 64/32, but the sprite itself does not wrap around. |
| `BNNN` (jump with offset) | Uses `V0` | Original behaviour. SCHIP uses `VX`. |
| `FX0A` (wait for key) | Returns as soon as it detects a pressed key | The original completed the instruction when the key was **released**. With this variant, a held key can trigger the instruction on successive frames. |
| `FX1E` (`ADD I, Vx`) | Doesn't modify `VF` | There are implementations (inherited from an Amiga version) that set `VF` to 1 if `I` goes over `0x0FFF`. |
| VF reset on `8XY1` / `8XY2` / `8XY3` | **Not implemented** | The original sets `VF` to 0 as a side effect of OR/AND/XOR. |
| Display wait | **Not implemented** | The original synchronized `DXYN` with the vertical blank, which limits drawing to one sprite per frame. |

The two that say "not implemented" are the reason the sprites flicker (I go into that further down). It's not that I didn't notice: I chose not to get into it for now in this project.

## Testing

Verified against [Timendus' CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite):

- **`3-corax+`** - passes. Covers most of the instruction set.
- **`4-flags`** - passes. This is the interesting one: it checks that `VF` is always written (0 as well as 1, because otherwise a flag from a previous operation stays on), that the flag is computed *before* the result overwrites an operand, and that it's written *after* the result, so that `8FY4` and similar instructions leave the flag and not the sum in `VF`.
- **`5-quirks`** - results in the table above.

Side note: when I ran `3-corax+`, luckily it passed all the tests, but I had to break some implementation on purpose to check that the tests were actually working.

## Limitations

- **There's no sound.** The timer counts down, but it doesn't make any noise.
- **Sprite flickering.** Some games with a lot of movement on screen look like they flicker. This happens because to move an object in CHIP-8 you first erase the previous sprite and then draw it again offset. Many times a frame gets drawn when the sprite has already been erased but the new one hasn't been drawn yet, and as a result it looks like it disappears and reappears. There are a couple of known solutions, but they were outside the scope of this project and they have their trade-offs. For now I decided to leave them aside.
- **The disassembler decodes data as instructions**, though for a linear disassembler that's fine.
- **Memory accesses are not bounded.** Nothing guarantees that the PC or `I` stay within the 4096 bytes, so a ROM that goes off the rails could read or write outside the array. It doesn't happen with the ROMs I tested, but it's something that has to be closed.

## References

- [Tobias V. Langhoff, *Guide to making a CHIP-8 emulator*](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/) - explains what each part has to do without giving you the code, and documents the quirks well.
- [*Cowgod's Chip-8 Technical Reference v1.0*](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM) - the opcode reference, open in a tab for the whole project.
- [Timendus' CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite)
