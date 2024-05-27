#ifndef SFS_H
#define SFS_H #include <stdbool.h> #include <stdint.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <fuse.h>

#include "sys.h"

#include <math.h>
#include <errno.h>
#include <unistd.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define BLOCK_SIZE 512
#define INODE_COUNT 512 #define BLOCK_COUNT 1900

#define FS_PATH_LEN_MAX 256
#define FS_NAME_LEN_MAX 64

const size_t MB = 1048576;
const size_t DISK_SIZE = 1 * MB;
const size_t INODE_SIZE = 128;
const size_t BLOCK_TOTAL_COUNT = DISK_SIZE / BLOCK_SIZE;

const uint16_t BLOCK_INVALID = -1;
const uint16_t INODE_INVALID = -1;

const size_t INODE_BLOCK_COUNT = 128;

const uint16_t FS_MODE_BITS = 9;
const uint16_t FS_MODE_MASK = 0x1F;

const uint8_t FS_TYPE_FLAG_REG = 1;
const uint8_t FS_TYPE_FLAG_DIR = 2;
const uint8_t FS_TYPE_FLAG_LNK = 4;
const uint8_t FS_TYPE_FLAG_FREE = 128;

typedef enum fs_err {
    FS_SUCCESS = 0,
    FS_ERROR = -1,
} fs_err;

typedef struct fs_inode {
    uint16_t ino;
    uint8_t uid;
    uint8_t gid;
    uint16_t type_mode;
    uint16_t refs;
    uint32_t size;
    uint32_t time;
    uint16_t block_0;
    uint16_t block_1;
    uint16_t block_2;
    uint16_t block_3;
    uint16_t block_p;
    uint16_t block_pp;
    uint16_t block_ppp0;
    uint16_t block_ppp1;
} fs_inode;

typedef struct fs_block {
    union {
        uint8_t bytes[BLOCK_SIZE];
        struct {
            uint16_t next;
        } free;
    };
} fs_block;

typedef struct fs_header {
    uint16_t total_block_count;
    uint16_t inode_block_count;
    uint16_t block_block_count;
    uint16_t header_size;
    uint16_t inode_size;
    uint16_t block_size;
    uint16_t max_ino;
    uint16_t root;                  // ino of root inode
    uint16_t ino;                   // current ino
    uint16_t free;                  // first free block
} fs_header;

typedef struct fs_fs {
    fs_header *header;
    fs_inode *inodes;
    fs_block *blocks;
    fs_block *raw;
} fs_fs;

typedef struct fs_dirmem {
    uint16_t ino;
    uint16_t len;
    char *name;
} fs_dirmem;

typedef struct fs_dir {
    size_t size;
    uint8_t *buffer;
} fs_dir;

struct fs_ino_rw_cb_args {
    fs_fs *fs;
    uint8_t *buffer;
    size_t size;
};

fs_fs *FS;

fs_header *fs_get_header(fs_block *raw) {
    return (fs_header *)raw;
}

fs_inode *fs_get_inodes(fs_block *raw) {
    return (fs_inode *)(raw + 1);
}

fs_block *fs_get_blocks(fs_block *raw) {
    fs_header *header = fs_get_header(raw);
    return raw + 1 + header->inode_block_count;
}

uint32_t fs_ino_size(fs_fs *fs, uint16_t ino) {
    return fs->inodes[ino].size;
}

uint16_t fs_ino_type(fs_fs *fs, uint16_t ino) {
    return fs->inodes[ino].type_mode >> FS_MODE_BITS;
}

uint16_t fs_ino_mode(fs_fs *fs, uint16_t ino) {
    return fs->inodes[ino].type_mode & FS_MODE_MASK;
}

bool fs_ino_isdir(fs_fs *fs, uint16_t ino) {
    return fs_ino_type(fs, ino) & FS_TYPE_FLAG_DIR;
}

void print_header(fs_header *header) {
    printf("-------- HEADER --------\n");
    printf("total_block_count: %d\n", header->total_block_count);
    printf("inode_block_count: %d\n", header->inode_block_count);
    printf("block_block_count: %d\n", header->block_block_count);
    printf("header_size: %d B\n", header->header_size);
    printf("inode_size: %d B\n", header->inode_size);
    printf("block_size: %d B\n", header->block_size);
    printf("max_ino: %d\n", header->max_ino);
    printf("root: %d\n", header->root);
    printf("ino: %d\n", header->ino);
    printf("free: %d\n", header->free);
    printf("------------------------\n");
}

void fs_ino_enumerate_blocks(
    fs_fs *fs,
    uint8_t ino,
    int32_t left,
    bool(*callback)(uint16_t *, size_t, void *),
    void *args
) {
    fs_inode *inode = &fs->inodes[ino];

    if (left == 0) return;
    
    size_t i = 0;
    if(!callback(&inode->block_0, i, args)) return;
    left -= BLOCK_SIZE;
    i++;

    uint16_t *pblock = (uint16_t *)&fs->blocks[inode->block_p];
    
    for (size_t j = 0; j < 32 && left > 0; j++) {
        if(!callback(&pblock[j], i, args)) return;
        left -= BLOCK_SIZE;
        i++;
    }

    uint16_t *ppblock = (uint16_t *)&fs->blocks[inode->block_pp];

    // TODO larger files
}

bool fs_ino_free_blocks_cb(uint16_t *block, size_t i, void *args) {
    printf("cb-free %d %ld\n", *block, i);
    fs_fs *fs = args;
    fs->blocks[*block].free.next = fs->header->free;
    fs->header->free = *block;
    return true;
}

void fs_ino_free_blocks(fs_fs *fs, uint16_t ino) {
    fs_inode *inode = &fs->inodes[ino];
    fs_ino_enumerate_blocks(fs, ino, inode->size, fs_ino_free_blocks_cb, fs);
    inode->size = 0;
}

bool fs_ino_alloc_blocks_cb(uint16_t *block, size_t i, void *args) {
    printf("cb-alloc %d %ld\n", *block, i);
    fs_fs *fs = args;
    uint16_t ptr = fs->header->free;
    fs_block *free_block = &fs->blocks[ptr];
    fs->header->free = free_block->free.next;
    *block = ptr;
    return true;
}

void fs_ino_alloc_blocks(fs_fs *fs, uint16_t ino, size_t size) {
    fs_ino_enumerate_blocks(fs, ino, size, fs_ino_alloc_blocks_cb, fs);
    fs_inode *inode = &fs->inodes[ino];
    inode->size = size;
}

bool fs_ino_write_cb(uint16_t *block, size_t i, void *voidargs) {
    printf("cb-write %d %ld\n", *block, i);
    struct fs_ino_rw_cb_args *args = voidargs;
    size_t left = args->size - i * BLOCK_SIZE;
    _memcpy(
        &args->fs->blocks[*block],
        args->buffer + i * BLOCK_SIZE,
        MIN(left, BLOCK_SIZE)
    );
    return true;
}

// TODO improve efficiency
int32_t fs_ino_write(fs_fs *fs, uint16_t ino, uint8_t *buffer, size_t size) {
    fs_ino_free_blocks(fs, ino);
    fs_ino_alloc_blocks(fs, ino, size);
    struct fs_ino_rw_cb_args args = {
        .fs = fs,
        .buffer = buffer,
        .size = size,
    };

    fs_ino_enumerate_blocks(fs, ino, size, fs_ino_write_cb, &args);
    return size;
}

void fs_ino_write_cstr(fs_fs *fs, uint16_t ino, const char *string) {
    size_t size = _strlen(string);
    fs_ino_write(fs, ino, (uint8_t *)string, size);
}

bool fs_ino_read_cb(uint16_t *block, size_t i, void *voidargs) {
    printf("cb-read %d %ld\n", *block, i);
    struct fs_ino_rw_cb_args *args = voidargs;
    size_t left = args->size - i * BLOCK_SIZE;
    size_t s = MIN(left, BLOCK_SIZE);
    _memcpy(args->buffer + i * BLOCK_SIZE, &args->fs->blocks[*block], s);
    return true;
}

int32_t fs_ino_read(fs_fs *fs, uint16_t ino, uint8_t *buffer, size_t size) {
    struct fs_ino_rw_cb_args args = {
        .fs = fs,
        .buffer = buffer,
        .size = size,       // TODO improve
    };

    fs_ino_enumerate_blocks(fs, ino, MIN(size, fs_ino_size(fs, ino)), fs_ino_read_cb, &args);
    return MIN(size, fs_ino_size(fs, ino));
}

bool fs_dir_member(fs_dir *dir, fs_dirmem *mem) {
    if (dir->size <= 0) {
        return false;
    }
    mem->ino = *(uint16_t *)dir->buffer;
    mem->len = *((uint16_t *)dir->buffer + 1);
    mem->name = (char *)((uint16_t *)dir->buffer + 2);
    return true;
}

void fs_dir_next(fs_dir *dir) {
    // TODO check oob
    fs_dirmem mem;
    fs_dir_member(dir, &mem);
    dir->size -= 5 + mem.len;
    dir->buffer += 5 + mem.len;
}

fs_err fs_ino_readdir(fs_fs *fs, uint16_t ino, fs_dir *dir, size_t size) {
    dir->size = fs_ino_size(fs, ino);
    // TODO check size
    fs_ino_read(fs, ino, dir->buffer, size);
    return FS_SUCCESS;
}

void fs_ino_mk_add_name(fs_fs *fs, uint16_t parent_ino, uint16_t ino, const char *name) {
    // TODO check dup

    uint8_t buffer[1000];
    fs_ino_read(fs, parent_ino, buffer, 1000);
    // TODO error handling

    size_t size = fs_ino_size(fs, parent_ino);
    uint8_t *end = buffer + size; 

    size_t len = _strlen(name);
    *(uint16_t *)end = ino;
    *(uint16_t *)(end + 2) = len;
    _memcpy(end + 4, name, len);
    *(end + 4 + len) = '\0';

    fs_ino_write(fs, parent_ino, buffer, size + 5 + len);
}

fs_err fs_ino_unlink_name(fs_fs *fs, uint16_t parent_ino, uint16_t ino) {
    uint8_t buffer[1000];
    fs_dir dir = { .buffer = buffer };
    fs_ino_readdir(fs, parent_ino, &dir, 1000);
    // TODO error handling
    //
    uint8_t *begin = dir.buffer;
    size_t size = fs->inodes[parent_ino].size;
    
    fs_dirmem mem;
    while (fs_dir_member(&dir, &mem)) {
        if (mem.ino == ino) {
            uint8_t *current_buffer = dir.buffer;
            uint16_t current_len = mem.len;

            fs_dir_next(&dir);
            _memcpy(current_buffer, dir.buffer, dir.size);

            fs_ino_write(fs, parent_ino, begin, size - (5 + current_len));
            // TODO error handling

            return FS_SUCCESS;
        }
        fs_dir_next(&dir);
    }

    return FS_ERROR;
}

int32_t fs_ino_mk(fs_fs *fs, uint16_t parent_ino, const char *name, uint8_t type) {
    uint16_t ino = fs->header->ino;
    fs->header->ino++;
    
    if (ino > fs->header->max_ino) {
        puts("ERR no inodes left");
        return -ENFILE;
    }

    if (!fs_ino_isdir(fs, parent_ino)) {
        puts("ERR can only make file in dir");
        return -ENOTDIR;
    }
    
    fs_ino_mk_add_name(fs, parent_ino, ino, name);

    fs_inode *parent_inode[parent_ino];

    fs_inode *inode = &fs->inodes[ino];

    inode->ino = parent_ino;
    inode->uid = 0;
    inode->gid = 0;
    inode->type_mode = (type << FS_MODE_BITS) | 0;
    inode->refs = 0;
    inode->time = 0;
    inode->block_0 = BLOCK_INVALID;
    inode->block_1 = BLOCK_INVALID;
    inode->block_2 = BLOCK_INVALID;
    inode->block_3 = BLOCK_INVALID;
    inode->block_p = BLOCK_INVALID;
    inode->block_pp = BLOCK_INVALID;
    inode->block_ppp0 = BLOCK_INVALID;
    inode->block_ppp1 = BLOCK_INVALID;

    return ino;
}

int32_t fs_ino_mkdir(fs_fs *fs, uint16_t parent_ino, const char *name) {
    int32_t ino = fs_ino_mk(fs, parent_ino, name, FS_TYPE_FLAG_DIR);
    if (ino < 0) return ino;
    fs_ino_mk_add_name(fs, ino, ino, ".");
    fs_ino_mk_add_name(fs, ino, parent_ino, "..");
    return ino;
}

uint16_t fs_ino_mknod(fs_fs *fs, uint16_t parent_ino, const char *name) {
    return fs_ino_mk(fs, parent_ino, name, FS_TYPE_FLAG_REG);
}

uint16_t fs_name_to_ino(fs_fs *fs, uint16_t ino, const char *name) {
    uint8_t buffer[500];
    fs_dir dir = { .buffer = buffer };
    fs_ino_readdir(fs, ino, &dir, 500);

    fs_dirmem mem;
    while (fs_dir_member(&dir, &mem)) {
        if (!_strcmp(mem.name, name)) return mem.ino;
        fs_dir_next(&dir);
    }
    
    return INODE_INVALID;
}

// TODO improve logic
uint16_t fs_path_to_ino_rel(fs_fs *fs, char *path, uint16_t ino) {
    char *ptr = path;
    while (true) {
        if (*ptr == '/') {
            *ptr = '\0';
            ino = fs_name_to_ino(fs, ino, path);
            if (ino == INODE_INVALID) return INODE_INVALID;
            ptr++;
            path = ptr;
        }
        if (*ptr == '\0') {
            if (ptr == path) return ino;
            ino = fs_name_to_ino(fs, ino, path);
            if (ino == INODE_INVALID) return INODE_INVALID;
            break;
        }
        ptr++;
    }
    return ino;
}

uint16_t fs_path_to_ino_abs(fs_fs *fs, char *path) {
    return fs_path_to_ino_rel(fs, path, fs->header->root);
}

uint16_t fs_path_to_ino(fs_fs *fs, const char *path) {
    if (_strlen(path) > FS_PATH_LEN_MAX) {
        puts("ERR path too long");
        return INODE_INVALID;
    }

    char buffer[FS_PATH_LEN_MAX];
    _memcpy(buffer, path, FS_PATH_LEN_MAX);

    if (*path == '/') {
        return fs_path_to_ino_abs(fs, buffer + 1);
    }

    puts("ERR only abs paths are supported");
    printf("Recived: '%s'\n", path);
    return INODE_INVALID;
}

const char *fs_path_get_last_sep(const char *path) {
    size_t len = _strlen(path);
    path += len - 1;
    for (size_t i = 0; i < len; i++) {
        if (*path == '/') return path;
        path--;
    }
    return NULL;
}

// /x/y  → /x/
// /x/y/ → /x/y/
fs_err fs_path_to_parent_path(const char *path, char *buffer) {
    size_t len = _strlen(path);
    if (len > FS_PATH_LEN_MAX) return FS_ERROR;

    _memcpy(buffer, path, len);
    char *sep = (char *)fs_path_get_last_sep(buffer);
    if (sep == NULL) return FS_ERROR;
    *(sep + 1) = '\0';
    return FS_SUCCESS;
}

uint16_t fs_path_to_parent_ino(fs_fs *fs, const char *path) {
    char buffer[FS_PATH_LEN_MAX];
    fs_err err = fs_path_to_parent_path(path, buffer);
    if (err != FS_SUCCESS) return INODE_INVALID;
    return fs_path_to_ino(fs, buffer);
}

void fs_init_blocks(fs_fs *fs) {
    size_t size = fs->header->total_block_count - fs->header->inode_block_count;

    fs->header->free = 0;
    for (size_t i = 0; i < size - 1; i++) {
        fs->blocks[i].free.next = i + 1;
    }
    fs->blocks[size - 1].free.next = BLOCK_INVALID;
}

void create_empty_fs(fs_fs *fs, fs_block *raw, size_t size) {
    _memset(raw, 0, size);

    // map keys of fs to memory

    fs->header = fs_get_header(raw);

    fs->header->total_block_count = size;
    fs->header->inode_block_count = 64;
    fs->header->block_block_count = fs->header->total_block_count - fs->header->inode_block_count;
    fs->header->header_size = sizeof(fs_header);
    fs->header->inode_size = sizeof(fs_inode);
    fs->header->block_size = sizeof(fs_block);
    fs->header->max_ino = fs->header->inode_block_count * 8 * 2 - 1;
    fs->header->ino = 1;

    fs->inodes = fs_get_inodes(raw);
    fs->blocks = fs_get_blocks(raw);

    fs_init_blocks(fs);
    
    // create root inode

    fs_inode *root = fs->inodes;
    root->ino = 0;
    root->uid = 100;
    root->gid = 100;
    root->type_mode = (FS_TYPE_FLAG_DIR << FS_MODE_BITS) | 0;
    root->refs = 0;
    root->time = 0;
    root->block_0 = BLOCK_INVALID;
    root->block_1 = BLOCK_INVALID;
    root->block_2 = BLOCK_INVALID;
    root->block_3 = BLOCK_INVALID;
    root->block_p = BLOCK_INVALID;
    root->block_pp = BLOCK_INVALID;
    root->block_ppp0 = BLOCK_INVALID;
    root->block_ppp1 = BLOCK_INVALID;

    fs_ino_mk_add_name(fs, 0, 0, ".");
    fs_ino_mk_add_name(fs, 0, 0, "..");
}

int fs_unlink_raw(fs_fs *fs, uint16_t ino) {
    fs_ino_free_blocks(fs, ino);
}

int fs_rmdir_raw(fs_fs *fs, uint16_t ino) {
    uint8_t buf[1000];
    fs_dir dir = { .buffer = buf };

    fs_ino_readdir(FS, ino, &dir, 1000);

    fs_inode *inode = &fs->inodes[ino];

    fs_dirmem mem;
    while (fs_dir_member(&dir, &mem)) {
        if (mem.ino != ino && mem.ino != inode->ino) {
            if (fs_ino_isdir(FS, mem.ino)) fs_rmdir_raw(FS, mem.ino);
            else fs_unlink_raw(FS, mem.ino);
        }
        fs_dir_next(&dir);
    }
}

int sfs_getattr(const char *path, struct stat *st) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    
    fs_inode *inode = &FS->inodes[ino];

    st->st_mode = (fs_ino_isdir(FS, ino) ? S_IFDIR | 0755 : S_IFREG | 0644);
    st->st_nlink = 2;
    st->st_size = fs_ino_size(FS, ino);
    st->st_uid = getuid();  // TODO
    st->st_gid = getgid();  // TODO

    return 0;
}

int sfs_readlink(const char *path, char *buffer, size_t size) {
    // TODO implement
}

int sfs_mknod(const char *path, mode_t mode, dev_t dev) {
    uint16_t parent_ino = fs_path_to_parent_ino(FS, path);
    const char *sep = fs_path_get_last_sep(path);
    const char *name = sep + 1;

    fs_ino_mknod(FS, parent_ino, sep + 1);
    return 0;
}

int sfs_mkdir(const char *path, mode_t mode) {
    uint16_t parent_ino = fs_path_to_parent_ino(FS, path);
    if (parent_ino == INODE_INVALID) return -ENOENT;
    const char *sep = fs_path_get_last_sep(path);
    if (sep == NULL) return -EINVAL;
    const char *name = sep + 1;

    fs_ino_mkdir(FS, parent_ino, name);
    return 0;
}

int sfs_unlink(const char *path) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    // TODO improve
    if (fs_ino_isdir(FS, ino)) return -EISDIR;
    
    fs_unlink_raw(FS, ino);

    fs_inode *inode = &FS->inodes[ino];
    fs_ino_unlink_name(FS, inode->ino, ino);
    return 0;
}

int sfs_rmdir(const char *path) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    if (!fs_ino_isdir(FS, ino)) return -ENOTDIR;
    
    fs_rmdir_raw(FS, ino);

    fs_inode *inode = &FS->inodes[ino];
    fs_ino_unlink_name(FS, inode->ino, ino);
    return 0;
}

int sfs_rename(const char *src, const char *dest) {
    uint16_t src_ino = fs_path_to_ino(FS, src);
    if (src_ino == INODE_INVALID) return -ENOENT;

    // case 1 target name explicit
    uint16_t dest_parent_ino = fs_path_to_parent_ino(FS, dest);
    if (dest_parent_ino == INODE_INVALID) return -ENOENT;

    const char *sep = fs_path_get_last_sep(dest);
    if (sep == NULL) return -EINVAL;
    const char *name = sep + 1;

    fs_inode *inode = &FS->inodes[src_ino];
    fs_ino_unlink_name(FS, inode->ino, src_ino);
    inode->ino = dest_parent_ino;

    fs_ino_mk_add_name(FS, inode->ino, src_ino, name);
    return 0;
}

int sfs_chown(const char *path, uid_t uid, gid_t gid) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    fs_inode *inode = &FS->inodes[ino];
    inode->uid = uid;
    inode->gid = gid;
    return 0;
}

int sfs_truncate(const char *path, off_t offset) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    fs_inode *inode = &FS->inodes[ino];

    // TODO implement

    return 0;
}

int sfs_link(const char *dest, const char *src) {
    uint16_t dest_ino = fs_path_to_ino(FS, dest);
    if (dest_ino == INODE_INVALID) return -ENOENT;

    uint16_t src_parent_ino = fs_path_to_parent_ino(FS, src);
    if (src_parent_ino == INODE_INVALID) return -ENOENT;

    const char *sep = fs_path_get_last_sep(src);
    if (sep == NULL) return -EINVAL;
    const char *name = sep + 1;

    fs_ino_mk_add_name(FS, src_parent_ino, dest_ino, name);

    return 0;
}

int32_t sfs_read(
    const char *path, char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    return fs_ino_read(FS, ino, (uint8_t *)buffer, size);
}

int sfs_write(
    const char *path,
    const char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    return fs_ino_write(FS, ino, (uint8_t *)buffer, size);
}

int sfs_statfs(const char *path, struct statvfs *stfs) {
    // TODO implement

    stfs->f_blocks = 100000;
    stfs->f_bfree = 50000;
    stfs->f_bavail = 50000;
    stfs->f_files = 100000;
    stfs->f_ffree = 50000;
    stfs->f_favail = 50000;
    return 0;
}

int sfs_readdir(
    const char *path,
    void *buffer,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi
) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
                                        
    uint8_t buf[1000];
    fs_dir dir = { .buffer = buf };
    fs_ino_readdir(FS, ino, &dir, 1000);

    fs_dirmem mem;
    while (fs_dir_member(&dir, &mem)) {
        filler(buffer, mem.name, NULL, 0);
        fs_dir_next(&dir);
    }
    return 0;
}

int sfs_utimens(const char *path, const struct timespec tv[2]) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INODE_INVALID) return -ENOENT;
    fs_inode *inode = &FS->inodes[ino];
    inode->time = tv->tv_sec;
    return 0;
}

void sfs_print_statistics(size_t ino_size, size_t block_size) {
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

#endif /* SFS_H */
