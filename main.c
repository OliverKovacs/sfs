#define FUSE_USE_VERSION 29
#include <stdint.h>
#include <stdlib.h>
#include <fuse.h>

#include "sfs.h"

static struct fuse_operations myfs_ops = {
    .getattr = sfs_getattr,
    .readlink = sfs_readlink,
    .mknod = sfs_mknod,
    .mkdir = sfs_mkdir,
    .unlink = sfs_unlink,
    .rmdir = sfs_rmdir,
    .rename = sfs_rename,
    .link = sfs_link,
    .chown = sfs_chown,
    .truncate = sfs_truncate,
    .read = sfs_read,
    .write = sfs_write,
    .statfs = sfs_statfs,
    .readdir = sfs_readdir,
    .utimens = sfs_utimens,
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
    uint16_t mydir3 = fs_ino_mkdir(fs, mydir2, "mydir3");

    uint16_t abc = fs_ino_mknod(fs, 0, "abc.txt");
    fs_ino_write_cstr(fs, abc, "Hello world! :)\n");

    char b1[1000];
    fs_ino_to_name_cstr(fs, mydir3, b1);
    printf("name: '%s'\n", b1);

    char b[1000];
    fs_ino_to_path(fs, mydir3, b);
    printf("path: '%s'\n", b);

    uint16_t src_parent_ino = fs_path_to_parent_ino(FS, "/abc");
    printf("%d\n", src_parent_ino);

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
