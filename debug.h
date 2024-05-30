#ifndef DEBUG_H
#define DEBUG_H

#include "sfs.h"

void print_statistics(size_t ino_size, size_t block_size) {
    printf("ino size:           %4ld bit / %3ld B\n", ino_size * 8, ino_size);
    printf("inode size:         %4ld bit / %3ld B\n", ino_size * 8 * 8, ino_size * 8);
    printf("block size:         %4ld bit / %3ld B\n", block_size * 8, block_size);

    size_t max_inodes = (size_t)pow(256, ino_size);
    printf("max inodes:         %16ld\n", max_inodes);
    printf("max fs size:        %13ld MB\n", max_inodes * block_size / MB);

    size_t bpb = block_size / ino_size;
    size_t max_blk = (ino_size == 2)
        ? (bpb - 3) + bpb + (size_t)pow(bpb, 2) + (size_t)pow(bpb, 3)
        : 1 + bpb + (size_t)pow(bpb, 2) + (size_t)pow(bpb, 3);

    printf("max blk/blk:        %16ld\n", bpb);
    printf("max file blks:        %14ld\n", max_blk);
    printf("max file size:        %11ld MB\n", max_blk * block_size / MB);
    puts("");
}

void print_header(fs_header *header) {
    printf("-------- HEADER --------\n");
    printf("blocks_all: %d\n", header->blocks_all);
    printf("blocks_header: %d\n", header->blocks_header);
    printf("blocks_inode: %d\n\n", header->blocks_inode);

    printf("blocks: %d\n", header->blocks);
    printf("blocks_free: %d\n", header->blocks_total - header->blocks);
    printf("blocks_avail: %d\n", header->blocks_total - header->blocks);
    printf("blocks_total: %d\n\n", header->blocks_total);

    printf("inodes: %d\n", header->inodes);
    printf("inodes_free: %d\n", header->inodes_total - header->inodes);
    printf("inodes_avail: %d\n", header->inodes_total - header->inodes);
    printf("inodes_total: %d\n\n", header->inodes_total);

    printf("header_size: %d B\n", header->header_size);
    printf("inode_size: %d B\n", header->inode_size);
    printf("block_size: %d B\n", header->block_size);
    printf("blockp_len: %d\n\n", header->blockp_len);

    printf("max_ino: %d\n", header->max_ino);
    printf("root_ino: %d\n", header->root_ino);
    printf("free_ino: %d\n", header->free_ino);
    printf("free_blk: %d\n", header->free_blk);
    printf("------------------------\n");
}

void print_debug(fs_fs *fs) {
    printf("-------- DEBUG ---------\n");
    for (uint16_t i = 1; i < fs->header->inodes_total; i++) {
        if (fs->inodes[i].ino != i) continue;
        printf("ino: %d [%d]\n", i, fs->inodes[i].size);
        uint16_t *p = &fs->inodes[i].block[0];
        for (size_t j = 0; j < 4; j++) {
            if (p[j] == BLK_INVALID) continue;
            printf("  blk: %d    idx: %ld\n", p[j], j);
        }
    }
    printf("------------------------\n");
}

#endif /* DEBUG_H */
