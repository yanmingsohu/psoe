set path=gcc;%path%
set src=test-cpu

mipsgcc -membedded-data -mips1 -nostdlib -nodefaultlibs -c %src%.c

mipsld -ffreestanding  -Ttext 0x100 -e main -N --oformat binary -Map %src%-map.txt -o %src%.bin  %src%.o