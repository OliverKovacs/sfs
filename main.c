#define FUSE_USE_VERSION 29
#include <stdint.h>
#include <stdlib.h>
#include <fuse.h>

#include "sfs.h"
#include "debug.h"

static struct fuse_operations sfs_ops = {
    .getattr = sfs_getattr,
    .readlink = sfs_readlink,
    .mknod = sfs_mknod,
    .mkdir = sfs_mkdir,
    .unlink = sfs_unlink,
    .rmdir = sfs_rmdir,
    .rename = sfs_rename,
    .link = sfs_link,
	.chmod = sfs_chmod,
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
    UNUSED(argc);
    UNUSED(argv);
    UNUSED(sfs_ops);

    char *buffer = (char *)malloc(DISK_SIZE);
    load_disk("./disk", buffer);
      
    fs_fs *fs = (fs_fs *)malloc(sizeof(fs_fs));
    fs_create(fs, (fs_block *)buffer, DISK_SIZE / FS_BLOCK_SIZE);
    FS = fs;

    print_header(fs->header);

    fs_ino_mkdir(fs, fs->header->root_ino, "mydir", 0);
    uint16_t mydir2 = fs_ino_mkdir(fs, fs->header->root_ino, "mydir2", 0);
    uint16_t mydir3 = fs_ino_mkdir(fs, mydir2, "mydir3", 0);
    UNUSED(mydir3);
    uint16_t abc = fs_ino_mknod(fs, fs->header->root_ino, "abc.txt", S_IFREG >> 3);
    fs_ino_write_cstr(fs, abc, "Hello world! :)\n");

    uint16_t xyz = fs_ino_mknod(fs, fs->header->root_ino, "xyz", S_IFREG >> 3);
    fs_ino_write_cstr(fs, xyz, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    " :)\n");

    print_header(fs->header);
    print_debug(fs);

    save_disk("./disk", buffer);
    puts("saved!");
 
    // int32_t i;
    // get the device or image filename from arguments
    // for (int32_t i = 1; i < argc && argv[i][0] == '-'; i++);
    // if (i < argc) {
    //   devfile = realpath(argv[i], NULL);
    //   memcpy(&argv[i], &argv[i+1], (argc-i) * sizeof(argv[0]));
    //   argc--;
    // }

    return fuse_main(argc, argv, &sfs_ops, NULL);
}
