# -S -o test-cpu.elf
mipsgcc -membedded-data -mips1 -nostdlib -nodefaultlibs -c test-cpu.c 

# -Map memmap.txt
mipsld -ffreestanding  -Ttext 0x100 -e main -N --oformat binary -Map test-cpu-map.txt -o test-cpu.bin  test-cpu.o