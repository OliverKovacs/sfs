#ifndef SFS_H
#define SFS_H #include <stdbool.h> #include <stdint.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <fuse.h>

#include "sys.h"

#include <math.h>
#include <errno.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// improve if ino 0 set error
#define CHECK_INO(ino) if (ino == INO_INVALID || ino < 0) return ino;

#define BLOCK_SIZE 512
#define INODE_COUNT 512 #define BLOCK_COUNT 1900

#define FS_PATH_LEN_MAX 256
#define FS_NAME_LEN_MAX 64
#define FS_DIR_MAX 1024

#define SUCCESS 0

const size_t MB = 1048576;
const size_t DISK_SIZE = 1 * MB;
const size_t INODE_SIZE = 128;
const size_t BLOCK_TOTAL_COUNT = DISK_SIZE / BLOCK_SIZE;

const uint16_t BLK_INVALID =  0;
const uint16_t INO_INVALID = 0;

const size_t INODE_BLOCK_COUNT = 128;

const uint32_t FS_ENUM_PRE = -1;
const uint32_t FS_ENUM_POST = -2;

#define BLOCK_POINTERS 6

typedef struct fs_inode {
    uint16_t ino;
    uint8_t uid;
    uint8_t gid;
    uint16_t mode;
    uint16_t refs;
    uint32_t size;
    uint32_t time;
    uint16_t block[BLOCK_POINTERS];
    uint16_t block_p;
    uint16_t block_pp;
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
    uint16_t blocks_all;
    uint16_t blocks_header;
    uint16_t blocks_inode;
    uint16_t blocks;
    uint16_t blocks_total;

    uint16_t inodes;
    uint16_t inodes_total;

    uint16_t header_size;
    uint16_t inode_size;
    uint16_t block_size;

    uint16_t blockp_len;

    uint32_t name_max;

    uint16_t max_ino;
    uint16_t root_ino;
    uint16_t free_ino;
    uint16_t free_blk;
} fs_header;

typedef struct fs_fs {
    fs_header *header;
    fs_inode *inodes;
    fs_block *blocks;
    fs_block *raw;
} fs_fs;

typedef struct fs_dentry {
    uint16_t ino;
    uint16_t len;
    char name;
} fs_dentry;

typedef struct fs_dir {
    int32_t size;
    uint8_t *buffer;
} fs_dir;

typedef struct fs_ino_rw_cb_args {
    fs_fs *fs;
    void *buffer;
    int32_t size;
    int32_t output;
} fs_ino_rw_cb_args;

fs_fs *FS;

fs_header *fs_get_header(fs_block *raw) {
    return (fs_header *)raw;
}

fs_inode *fs_get_inodes(fs_block *raw) {
    return (fs_inode *)(raw + 1);
}

fs_block *fs_get_blocks(fs_block *raw) {
    fs_header *header = fs_get_header(raw);
    return raw + 1 + header->blocks_inode;
}

static inline fs_inode *fs_get_inode(fs_fs *fs, uint16_t ino) {
    assert(ino != INO_INVALID);
    assert(ino <= fs->header->max_ino);
    fs_inode *inode = &fs->inodes[ino];
    return inode;
}

uint32_t fs_ino_size(fs_fs *fs, uint16_t ino) {
    return fs_get_inode(fs, ino)->size;
}

static inline bool fs_ino_isdir(fs_fs *fs, uint16_t ino) {
    return fs_get_inode(fs, ino)->mode & (S_IFDIR >> 3);
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

void fs_ino_enumerate_blocks(
    fs_fs *fs,
    uint8_t ino,
    bool(*callback)(uint16_t *, int32_t, void *),
    void *args
) {
    fs_inode *inode = fs_get_inode(fs, ino);
    
    size_t i = 0;
    while (i < BLOCK_POINTERS) {
        if (!callback(&inode->block[i], i, args)) return;
        i++;
    }

    if (!callback(&inode->block_p, FS_ENUM_PRE, args)) return;

    uint16_t *pblock = (uint16_t *)&fs->blocks[inode->block_p];
    
    for (size_t j = 0; j < fs->header->blockp_len; j++) {
        if (!callback(&pblock[j], i, args)) return;
        i++;
    }

    if (!callback(&inode->block_p, FS_ENUM_POST, args)) return;

    // TODO larger files
}

uint16_t fs_alloc_block(fs_fs *fs) {
    uint16_t blk = fs->header->free_blk;
    printf("ALLOC BLOCK: %d\n", blk);
    fs->header->free_blk = fs->blocks[blk].free.next;       // blocks[BLK_INVALID] should always point at itself
    printf("-- %d\n", fs->header->free_blk);
    fs->header->blocks++;
    return blk;
}

uint16_t fs_alloc_inode(fs_fs *fs) {
    uint16_t ino = fs->header->free_ino;
    fs->header->free_ino = fs_get_inode(fs, ino)->ino;      // inodes[INO_INVALID] should always point to itself
    fs->header->inodes++;
    return ino;
}

void fs_free_block(fs_fs *fs, uint16_t blk) {
    printf("FREE BLOCK %d\n", blk);
    fs_block *block = &fs->blocks[blk];
    block->free.next = fs->header->free_blk;
    fs->header->free_blk = blk;
    fs->header->blocks--;
}

// TODO only free up to size
bool fs_ino_free_blocks_cb(uint16_t *block, int32_t i, void *args) {
    printf("cb-free %d %d\n", *block, i);
    if (*block == BLK_INVALID) return false;
    if (i == FS_ENUM_PRE) return false;
    fs_fs *fs = (fs_fs *)args;
    fs_free_block(fs, *block);
    *block = BLK_INVALID;
    return true;
}

void fs_ino_free_blocks(fs_fs *fs, uint16_t ino) {
    fs_ino_enumerate_blocks(fs, ino, fs_ino_free_blocks_cb, fs);
    fs_inode *inode = fs_get_inode(fs, ino);
    inode->size = 0;
}

void fs_free_inode(fs_fs *fs, uint16_t ino) {
    fs_ino_free_blocks(fs, ino);

    fs_inode *inode = fs_get_inode(fs, ino);
    // TODO invalidate fields here

    inode->ino = fs->header->free_ino;
    fs->header->free_ino = ino;
    fs->header->inodes--;
}

bool fs_ino_read_cb(uint16_t *block, int32_t i, void *vargs) {
    printf("cb-read %d %d\n", *block, i);
    if (i == FS_ENUM_PRE || i == FS_ENUM_POST) return true;
    fs_ino_rw_cb_args *args = (fs_ino_rw_cb_args *)vargs;
    int32_t offset = i * args->fs->header->block_size;
    int32_t left = args->size - offset;

    if (left <= 0) return false;
    assert(*block != BLK_INVALID);

    size_t min = MIN(left, args->fs->header->block_size);
    _memcpy((uint8_t *)args->buffer + offset, &args->fs->blocks[*block], min);
    return true;
}

int32_t fs_ino_read(fs_fs *fs, uint16_t ino, void *buffer, size_t size) {
    int32_t min = MIN(size, fs_ino_size(fs, ino));
    fs_ino_rw_cb_args args = { fs, buffer, min, 0 };
    fs_ino_enumerate_blocks(fs, ino, fs_ino_read_cb, &args);
    return min;
}

// TODO improve
bool fs_ino_write_cb(uint16_t *block, int32_t i, void *vargs) {
    printf("cb-write %d %d\n", *block, i);
    fs_ino_rw_cb_args *args = (fs_ino_rw_cb_args *)vargs;
    if (i == FS_ENUM_POST) return true;

    int32_t offset = i * args->fs->header->block_size;
    int32_t left = args->size - offset;

    if (left <= 0) return false;
    if (*block == BLK_INVALID) *block = fs_alloc_block(args->fs);
    if (*block == BLK_INVALID) return false;
    if (i == FS_ENUM_PRE) return true;

    size_t min = MIN(left, args->fs->header->block_size);
    _memcpy(&args->fs->blocks[*block], (uint8_t *)args->buffer + offset, min);
    args->output += min;

    return true;
}

// TODO improve efficiency
int32_t fs_ino_write(fs_fs *fs, uint16_t ino, const void *buffer, size_t size) {
    fs_ino_free_blocks(fs, ino);

    fs_ino_rw_cb_args args = { fs, (void *)buffer, (int32_t)size, 0 };
    fs_ino_enumerate_blocks(fs, ino, fs_ino_write_cb, &args);

    fs_inode *inode = fs_get_inode(fs, ino);
    inode->size = args.output;
    return args.output;
}

int32_t fs_ino_write_cstr(fs_fs *fs, uint16_t ino, const char *string) {
    size_t size = _strlen(string);
    return fs_ino_write(fs, ino, (uint8_t *)string, size);
}

// TODO improve directory handling
fs_dentry *fs_dir_entry(fs_dir *dir) {
    if (dir->size <= 0) return NULL;
    return (fs_dentry *)dir->buffer;
}

// TODO improve
fs_dentry *fs_dir_next(fs_dir *dir) {
    fs_dentry *dentry = fs_dir_entry(dir);
    if (dentry == NULL) return NULL;
    dir->size -= 5 + dentry->len;
    dir->buffer += 5 + dentry->len;
    return fs_dir_entry(dir);
}

fs_dentry *fs_dir_search(fs_dir *dir, const char *name) {
    fs_dentry *dentry = fs_dir_entry(dir);
    while (dentry != NULL) {
        if (!_strcmp(&dentry->name, name)) return dentry;
        dentry = fs_dir_next(dir);
    }
    return NULL;
}

int32_t fs_ino_readdir(fs_fs *fs, uint16_t ino, fs_dir *dir, size_t size) {
    dir->size = fs_ino_size(fs, ino);
    int32_t read = fs_ino_read(fs, ino, dir->buffer, size);
    if (read < 0) return read;
    if (read != dir->size) return -EFBIG;
    return SUCCESS;
}

void fs_ino_refs_inc(fs_fs *fs, uint16_t ino) {
    fs_inode *inode = fs_get_inode(fs, ino);
    inode->refs++;
}

void fs_ino_refs_dec(fs_fs *fs, uint16_t ino) {
    fs_inode *inode = fs_get_inode(fs, ino);
    inode->refs--;
    if (inode->refs == 0) fs_free_inode(fs, ino);
}

int32_t fs_ino_link(fs_fs *fs, uint16_t parent_ino, uint16_t ino, const char *name) {
    // TODO check dup

    uint8_t buffer[FS_DIR_MAX];
    fs_dir dir = { .buffer = buffer };
    int32_t err = fs_ino_readdir(fs, parent_ino, &dir, FS_DIR_MAX);
    if (err) return err;

    size_t size = dir.size;
    fs_dentry *new_dentry = (fs_dentry *)(dir.buffer + size);

    size_t len = _strlen(name);
    new_dentry->ino = ino;
    new_dentry->len = len;
    _memcpy(&new_dentry->name, name, len + 1);

    // TODO error
    fs_ino_write(fs, parent_ino, buffer, size + 5 + len);
    fs_ino_refs_inc(fs, ino);

    return SUCCESS;
}

// TODO improve logic
int32_t fs_ino_unlink(fs_fs *fs, uint16_t parent_ino, const char *name) {
    uint8_t buffer[FS_DIR_MAX];
    fs_dir dir = { .buffer = buffer };
    int32_t err = fs_ino_readdir(fs, parent_ino, &dir, FS_DIR_MAX);
    if (err) return err;
   
    size_t size = fs_get_inode(fs, parent_ino)->size;
    
    fs_dentry *dentry = fs_dir_search(&dir, name);
    if (dentry == NULL) return -ENOENT;
    if (fs_ino_isdir(FS, dentry->ino)) return -EISDIR;

    // TODO improve
    uint8_t *buf = dir.buffer;
    uint16_t len = dentry->len;

    fs_dir_next(&dir);
    // TODO UB
    _memcpy(buf, dir.buffer, dir.size);

    err = fs_ino_write(fs, parent_ino, buffer, size - (5 + len));
    if (err < 0) return err;

    fs_ino_refs_dec(fs, dentry->ino);
    return SUCCESS;
}

void fs_init_inode(fs_fs *fs, uint16_t ino, uint16_t mode) {
    fs_inode *inode = fs_get_inode(fs, ino);

    inode->ino = ino;
    inode->uid = 0;     // TODO
    inode->gid = 0;     // TODO
    inode->mode = mode;
    inode->refs = 0;
    inode->time = 0;    // TODO
    for (size_t i = 0; i < BLOCK_POINTERS; i++) {
        inode->block[i] = BLK_INVALID;
    }
    inode->block_p = BLK_INVALID;
    inode->block_pp = BLK_INVALID;
}

int32_t fs_ino_mk(fs_fs *fs, uint16_t parent_ino, const char *name, uint16_t mode) {
    if (!fs_ino_isdir(fs, parent_ino)) return -ENOTDIR;

    int32_t ino = fs_alloc_inode(fs);
    CHECK_INO(ino);

    fs_init_inode(fs, ino, mode);
    fs_ino_link(fs, parent_ino, ino, name);
    return ino;
}

int32_t fs_ino_mkdir(fs_fs *fs, uint16_t parent_ino, const char *name, uint16_t mode) {
    int16_t ino = fs_ino_mk(fs, parent_ino, name, mode | (S_IFDIR >> 3));
    CHECK_INO(ino);

    // TODO error
    fs_ino_link(fs, ino, ino, ".");
    fs_ino_link(fs, ino, parent_ino, "..");
    return ino;
}

int32_t fs_ino_mknod(fs_fs *fs, uint16_t parent_ino, const char *name, uint16_t mode) {
    return fs_ino_mk(fs, parent_ino, name, mode);
}

int32_t fs_name_to_ino(fs_fs *fs, uint16_t ino, const char *name) {
    if (*name == '\0') return ino;

    uint8_t buffer[FS_DIR_MAX];
    fs_dir dir = { .buffer = buffer };
    int32_t err = fs_ino_readdir(fs, ino, &dir, FS_DIR_MAX);
    if (err) return err;

    fs_dentry *dentry = fs_dir_search(&dir, name);
    if (dentry == NULL) return -ENOENT;
    return dentry->ino;
}

int32_t fs_path_to_parent_ino(fs_fs *fs, const char *path) {
    int32_t ino = fs->header->root_ino;

    // TODO broken
    // TODO logic
    char buffer[1000];

    // TODO len
    if (*path == '/') path++;

    const char *ptr = path;
    while (true) {
        if (*ptr == '/') {
            size_t len = ptr - path;
            _memcpy(buffer, path, len);
            *(buffer + len) = '\0';
            ino = fs_name_to_ino(fs, ino, buffer);
            CHECK_INO(ino);
            ptr++;
            path = ptr;
            continue;
        }
        if (*ptr == '\0') break;
        ptr++;
    }
    return ino;
}

const char *fs_path_get_name(const char *path) {
    size_t len = _strlen(path);
    for (const char *ptr = path + len - 1; ptr >= path; ptr--) {
        if (*ptr == '/') return ptr + 1;
    }
    return NULL;
}

int32_t fs_path_to_ino_rel(fs_fs *fs, const char *path, uint16_t ino) {
    int32_t parent_ino = fs_path_to_parent_ino(fs, path);
    CHECK_INO(parent_ino);

    const char *name = fs_path_get_name(path);
    if (name == NULL) return -EINVAL;

    return fs_name_to_ino(fs, parent_ino, name);
}

int32_t fs_path_to_ino(fs_fs *fs, const char *path) {
    if (*path != '/') return -EINVAL;

    if (_strlen(path) > FS_PATH_LEN_MAX) return -ENAMETOOLONG;
    char buffer[FS_PATH_LEN_MAX];
    _memcpy(buffer, path, FS_PATH_LEN_MAX);

    return fs_path_to_ino_rel(fs, buffer, fs->header->root_ino);
}

void fs_init_blocks(fs_fs *fs) {
    size_t size = fs->header->blocks_total;

    fs->header->free_blk = 1;
    fs->blocks[BLK_INVALID].free.next = BLK_INVALID;
    for (size_t i = 1; i < size - 1; i++) {
        fs->blocks[i].free.next = i + 1;
    }
    fs->blocks[size - 1].free.next = BLK_INVALID;
}

void fs_init_inodes(fs_fs *fs) {
    size_t size = fs->header->inodes_total;

    fs->header->free_ino = 2;
    fs->inodes[0].ino = 0;
    for (size_t i = fs->header->free_ino; i < size - 1; i++) {
        fs_get_inode(fs, i)->ino = i + 1;
    }
    fs_get_inode(fs, size - 1)->ino = INO_INVALID;
}

void fs_create(fs_fs *fs, fs_block *raw, size_t size) {
    _memset(raw, 0, size);

    // map keys of fs to memory

    fs->header = fs_get_header(raw);

    fs->header->blocks_all = size;
    fs->header->blocks_header = 1;    // depends on sizeof(fs_header)
    fs->header->blocks_inode = 64;
    fs->header->inodes = 1;
    fs->header->inodes_total = fs->header->blocks_inode * 8 * 2;
    fs->header->blocks = 0;
    fs->header->blocks_total = fs->header->blocks_all - fs->header->blocks_inode;

    fs->header->header_size = sizeof(fs_header);
    fs->header->inode_size = sizeof(fs_inode);
    fs->header->block_size = sizeof(fs_block);
    fs->header->blockp_len = fs->header->block_size / sizeof(uint16_t);
    fs->header->name_max = FS_PATH_LEN_MAX;

    fs->header->max_ino = fs->header->inodes_total - 1;
   
    uint16_t root_ino = 1;
    fs->header->root_ino = root_ino;

    fs->inodes = fs_get_inodes(raw);
    fs->blocks = fs_get_blocks(raw);

    fs_init_blocks(fs);
    fs_init_inodes(fs);
    
    // create root inode

    fs_init_inode(fs, root_ino, (S_IFDIR >> 3));
    fs_ino_link(fs, root_ino, root_ino, ".");
    fs_ino_link(fs, root_ino, root_ino, "..");
}

mode_t fs_mode_to_unix(uint16_t mode) {
    uint32_t file_type = mode & (S_IFMT >> 3);
    uint32_t file_mode = mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    return file_mode | (file_type << 3);
}

int16_t fs_mode_to_sfs(mode_t mode) {
    uint32_t file_type = mode & S_IFMT;
    uint32_t file_mode = mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    return file_mode | (file_type >> 3);
}

int32_t sfs_getattr(const char *path, struct stat *st) {
    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);

    fs_inode *inode = fs_get_inode(FS, ino);
    st->st_mode = fs_mode_to_unix(inode->mode);
    st->st_nlink = inode->refs;
    st->st_size = fs_ino_size(FS, ino);
    // st->st_uid = inode->uid;
    // st->st_gid = inode->gid;
    st->st_uid = getuid();  // TODO
    st->st_gid = getgid();  // TODO

    return SUCCESS;
}

int32_t sfs_readlink(const char *path, char *buffer, size_t size) {
    // TODO implement
    UNUSED(path);
    UNUSED(buffer);
    UNUSED(size);
    return 1;
}

int32_t sfs_mknod(const char *path, mode_t mode, dev_t dev) {
    UNUSED(dev);

    int32_t parent_ino = fs_path_to_parent_ino(FS, path);
    CHECK_INO(parent_ino);

    const char *name = fs_path_get_name(path);
    if (name == NULL) return -EINVAL;

    int32_t file_mode = fs_mode_to_sfs(mode & (S_IRWXU | S_IRWXG | S_IRWXO));
    return fs_ino_mknod(FS, parent_ino, name, file_mode);
}

int32_t sfs_mkdir(const char *path, mode_t mode) {
    int32_t parent_ino = fs_path_to_parent_ino(FS, path);
    CHECK_INO(parent_ino);

    const char *name = fs_path_get_name(path);
    if (name == NULL) return -EINVAL;

    int32_t file_mode = fs_mode_to_sfs(mode & (S_IRWXU | S_IRWXG | S_IRWXO));
    int32_t ino = fs_ino_mkdir(FS, parent_ino, name, file_mode);
    CHECK_INO(ino);
    return SUCCESS;
}

int32_t sfs_unlink(const char *path) {
    int32_t parent_ino = fs_path_to_parent_ino(FS, path);
    CHECK_INO(parent_ino);
    
    const char *name = fs_path_get_name(path);
    if (name == NULL) return -EINVAL;

    return fs_ino_unlink(FS, parent_ino, name);
}

int32_t sfs_rmdir(const char *path) {
    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);
    if (!fs_ino_isdir(FS, ino)) return -ENOTDIR;

    const char *name = fs_path_get_name(path);
    if (name == NULL) return -EINVAL; 

    fs_inode *inode = fs_get_inode(FS, ino);
    if (inode->size != 0) return -ENOTEMPTY;

    return fs_ino_unlink(FS, inode->ino, name);
}

int32_t sfs_rename(const char *src, const char *dest) {
    int32_t src_parent_ino = fs_path_to_parent_ino(FS, src);
    CHECK_INO(src_parent_ino);

    const char *src_name = fs_path_get_name(src);
    if (src_name == NULL) return -EINVAL;

    int32_t src_ino = fs_name_to_ino(FS, src_parent_ino, src_name);
    CHECK_INO(src_ino);

    int32_t dest_parent_ino = fs_path_to_parent_ino(FS, dest);
    CHECK_INO(dest_parent_ino);

    const char *dest_name = fs_path_get_name(dest);
    if (dest_name == NULL) return -EINVAL;

    int32_t err = fs_ino_link(FS, dest_parent_ino, src_ino, dest_name);
    if (err) return err;

    return fs_ino_unlink(FS, src_parent_ino, src_name);
}

int32_t sfs_chmod(const char *path, mode_t mode) {
    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);

    fs_inode *inode = fs_get_inode(FS, ino);
    inode->mode = fs_mode_to_sfs(mode);
    return SUCCESS;
}

int32_t sfs_chown(const char *path, uid_t uid, gid_t gid) {
    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);

    fs_inode *inode = fs_get_inode(FS, ino);
    inode->uid = uid;
    inode->gid = gid;
    return SUCCESS;
}

int32_t sfs_truncate(const char *path, off_t offset) {
    UNUSED(offset);

    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);

    fs_inode *inode = fs_get_inode(FS, ino);
    UNUSED(inode);

    // TODO implement
    return SUCCESS;
}

int32_t sfs_link(const char *dest, const char *src) {
    int32_t dest_ino = fs_path_to_ino(FS, dest);
    CHECK_INO(dest_ino);

    int32_t src_parent_ino = fs_path_to_parent_ino(FS, src);
    CHECK_INO(src_parent_ino);

    const char *name = fs_path_get_name(src);
    if (name == NULL) return -EINVAL;

    return fs_ino_link(FS, src_parent_ino, dest_ino, name);
}

int32_t sfs_read(
    const char *path, char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    UNUSED(offset);
    UNUSED(fi);

    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);
    return fs_ino_read(FS, ino, buffer, size);
}

int32_t sfs_write(
    const char *path,
    const char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    UNUSED(offset);
    UNUSED(fi);

    uint32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);
    // return fs_ino_write(FS, ino, buffer, size);
    return fs_ino_write(FS, ino, buffer, size);
}

int32_t sfs_statfs(const char *path, struct statvfs *stfs) {
    UNUSED(path);
    stfs->f_bsize = FS->header->block_size;
    stfs->f_frsize = FS->header->block_size;
    stfs->f_frsize = FS->header->block_size;
    stfs->f_blocks = FS->header->blocks_total;
    stfs->f_bfree = FS->header->blocks_total - FS->header->blocks;
    stfs->f_bavail = FS->header->blocks_total - FS->header->blocks;
    stfs->f_files = FS->header->inodes_total;
    stfs->f_ffree = FS->header->inodes_total - FS->header->inodes;
    stfs->f_favail = FS->header->inodes_total - FS->header->inodes;
    stfs->f_namemax = FS->header->name_max;
    return SUCCESS;
}

int32_t sfs_readdir(
    const char *path,
    void *buffer,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi
) {
    UNUSED(offset);
    UNUSED(fi);

    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);
                                        
    uint8_t buf[1000];
    fs_dir dir = { .buffer = buf };
    fs_ino_readdir(FS, ino, &dir, 1000);

    fs_dentry *dentry = fs_dir_entry(&dir);
    while (dentry != NULL) {
        filler(buffer, &dentry->name, NULL, 0);
        dentry = fs_dir_next(&dir);
    }
    return SUCCESS;
}

int32_t sfs_utimens(const char *path, const struct timespec tv[2]) {
    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);

    fs_inode *inode = fs_get_inode(FS, ino);
    inode->time = tv->tv_sec;
    return SUCCESS;
}

#endif /* SFS_H */
