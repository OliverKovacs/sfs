#define FUSE_USE_VERSION 29
#include <stdint.h>
#include <stdlib.h>
#include <fuse.h>

#include "sfs.h"

static struct fuse_operations myfs_ops = {
    .getattr = sfs_getattr,
    .readdir = sfs_readdir,
    .write = sfs_write,
    .read = sfs_read,
    .mkdir = sfs_mkdir,
    .unlink = sfs_unlink,
    .rmdir = sfs_rmdir,
    .rename = sfs_rename,
};

char *devfile = NULL;

void load_disk(char *path, char *buffer) {
    FILE *file = fopen(path, "r");
    fgets(buffer, DISK_SIZE, file);
}

void save_disk(char *path, char *buffer) {
    FILE *file = fopen(path, "w");
    fwrite(buffer, DISK_SIZE, 1, file);
}
 
int main(int argc, char **argv) {
    // for (size_t i = 1; i <= 8; i *= 2) {
    //     sfs_print_statistics(2, i * 64);
    //     sfs_print_statistics(4, i * 64);
    // }

    char *buffer = (char *)malloc(DISK_SIZE);
    load_disk("./disk", buffer);
      
    fs_fs *fs = (fs_fs *)malloc(sizeof(fs_fs));
    create_empty_fs(fs, (fs_block *)buffer, DISK_SIZE / BLOCK_SIZE);
    FS = fs;

    print_header(fs->header);

    fs_ino_mkdir(fs, 0, "mydir");
    uint16_t mydir2 = fs_ino_mkdir(fs, 0, "mydir2");
    fs_ino_mkdir(fs, mydir2, "mydir3");

    uint16_t abc = fs_ino_mkreg(fs, 0, "abc.txt");
    fs_ino_write_cstring(fs, abc, "Hello world! :)\n");

    save_disk("./disk", buffer);
 
    // int32_t i;
    // get the device or image filename from arguments
    // for (int32_t i = 1; i < argc && argv[i][0] == '-'; i++);
    // if (i < argc) {
    //   devfile = realpath(argv[i], NULL);
    //   memcpy(&argv[i], &argv[i+1], (argc-i) * sizeof(argv[0]));
    //   argc--;
    // }

    return fuse_main(argc, argv, &myfs_ops, NULL);
}
