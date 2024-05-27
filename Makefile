CFLAGS=-I/usr/include/fuse -lfuse -lm -D_FILE_OFFSET_BITS=64 -g -Wall -Wextra
MOUNT=mnt
ARGS=-d -f $(MOUNT)

run: main
	./$^ $(ARGS)
	xxd disk > hex

debug: main
	valgrind --leak-check=full ./$^ $(ARGS)
	
main: main.c sfs.h
	$(CC) $^ $(CFLAGS) -o $@

fix:
	fusermount -uz $(MOUNT)

install:
	sudo apt install libfuse-dev
	mkdir $(MOUNT)
	touch disk

