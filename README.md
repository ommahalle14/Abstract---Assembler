g++ -std=c++17 -O2 -Wall -Wextra -o assembler main.cpp worklet2.cpp

# Generate relocatable ELF object (default):
./assembler program.asm -o program.o

# Generate flat binary:
./assembler program.asm -o program.bin

# Generate ELF object, flat binary, and listing simultaneously:
./assembler program.asm -o program.o --bin program.bin --listing

readelf -h -S out.o
readelf -s out.o
objdump -d out.o


# Run Worklet 1 only 

.\assembler.exe prog.asm --w1

# For symbol table

readelf -s out.o

# Run Worklet 2 only 

.\assembler.exe prog.asm --w2 -o prog.bin

# Run Worklet 3 

.\assembler.exe prog.asm --dump-elf

# Run Full Connected Pipeline 

.\assembler.exe prog.asm -o prog.o --bin prog.bin