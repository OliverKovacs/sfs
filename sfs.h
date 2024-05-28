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

#define BLOCK_SIZE 512
#define INODE_COUNT 512 #define BLOCK_COUNT 1900

#define FS_PATH_LEN_MAX 256
#define FS_NAME_LEN_MAX 64

const size_t MB = 1048576;
const size_t DISK_SIZE = 1 * MB;
const size_t INODE_SIZE = 128;
const size_t BLOCK_TOTAL_COUNT = DISK_SIZE / BLOCK_SIZE;

const uint16_t BLK_INVALID =  0;
const uint16_t INO_INVALID = 0;

const size_t INODE_BLOCK_COUNT = 128;

const uint16_t FS_MODE_BITS = 9;
const uint16_t FS_MODE_MASK = 0x1F;

const uint8_t FS_TYPE_FLAG_REG = 1;
const uint8_t FS_TYPE_FLAG_DIR = 2;
const uint8_t FS_TYPE_FLAG_LNK = 4;
const uint8_t FS_TYPE_FLAG_FREE = 128;

const uint32_t FS_ENUM_FLAG_PRE = 0x10000;
const uint32_t FS_ENUM_FLAG_POST = 0x20000;
const uint32_t FS_ENUM_IDX_MASK = 0x0FFFF;

typedef enum fs_err {
    FS_SUCCESS = 0,
    FS_ERROR = -1,
    FS_ENOENT = -2,
    FS_ENOTDIR = -3,
    FS_EFBIG = -4,
    FS_EIO = -5,
} fs_err;

typedef struct fs_inode {
    uint16_t ino;
    uint8_t uid;
    uint8_t gid;
    uint16_t type_mode;
    uint16_t refs;
    uint32_t size;
    uint32_t time;
    // TODO
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
    uint8_t *buffer;
    int32_t size;
    int32_t output;
} fs_ino_rw_cb_args;

fs_fs *FS;

int16_t error;

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
    // assert(inode->ino == ino);
    return inode;
}

uint32_t fs_ino_size(fs_fs *fs, uint16_t ino) {
    return fs_get_inode(fs, ino)->size;
}

static inline uint16_t fs_ino_type(fs_fs *fs, uint16_t ino) {
    return fs_get_inode(fs, ino)->type_mode >> FS_MODE_BITS;
}

static inline uint16_t fs_ino_mode(fs_fs *fs, uint16_t ino) {
    return fs_get_inode(fs, ino)->type_mode & FS_MODE_MASK;
}

static inline bool fs_ino_isdir(fs_fs *fs, uint16_t ino) {
    return fs_ino_type(fs, ino) & FS_TYPE_FLAG_DIR;
}

void print_header(fs_header *header) {
    printf("-------- HEADER --------\n");
    printf("blocks_all: %d\n", header->blocks_all);
    printf("blocks_header: %d\n", header->blocks_header);
    printf("blocks_inode: %d\n", header->blocks_inode);
    puts("");

    printf("blocks: %d\n", header->blocks);
    printf("blocks_free: %d\n", header->blocks_total - header->blocks);
    printf("blocks_avail: %d\n", header->blocks_total - header->blocks);
    printf("blocks_total: %d\n", header->blocks_total);
    puts("");

    printf("inodes: %d\n", header->inodes);
    printf("inodes_free: %d\n", header->inodes_total - header->inodes);
    printf("inodes_avail: %d\n", header->inodes_total - header->inodes);
    printf("inodes_total: %d\n", header->inodes_total);
    puts("");

    printf("header_size: %d B\n", header->header_size);
    printf("inode_size: %d B\n", header->inode_size);
    printf("block_size: %d B\n", header->block_size);
    printf("blockp_len: %d\n", header->blockp_len);
    puts("");

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
        uint16_t *p = &fs->inodes[i].block_0;
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
    if (!callback(&inode->block_0, i, args)) return;
    i++;

    if (!callback(&inode->block_p, i | FS_ENUM_FLAG_PRE, args)) return;

    uint16_t *pblock = (uint16_t *)&fs->blocks[inode->block_p];
    
    for (size_t j = 0; j < 32; j++) {
        if (!callback(&pblock[j], i, args)) return;
        i++;
    }

    if (!callback(&inode->block_p, i | FS_ENUM_FLAG_POST, args)) return;

    // TODO larger files
}

uint16_t fs_alloc_block(fs_fs *fs) {
    uint16_t blk = fs->header->free_blk;
    fs->header->free_blk = fs->blocks[blk].free.next;       // blocks[BLK_INVALID] should always point at itself
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
    fs_block *block = &fs->blocks[blk];
    block->free.next = fs->header->free_blk;
    fs->header->free_blk = blk;
    fs->header->blocks--;
}

// TODO only free up to size
bool fs_ino_free_blocks_cb(uint16_t *block, int32_t i, void *args) {
    printf("cb-free %d %d\n", *block, i);
    if (*block == BLK_INVALID) return false;
    if (i & FS_ENUM_FLAG_PRE) return false;
    fs_fs *fs = (fs_fs *)args;
    fs_free_block(fs, *block);
    *block = BLK_INVALID;
    return true;
}

void fs_ino_free_blocks(fs_fs *fs, uint16_t ino) {
    fs_inode *inode = fs_get_inode(fs, ino);
    fs_ino_enumerate_blocks(fs, ino, fs_ino_free_blocks_cb, fs);
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
    if (i & (FS_ENUM_FLAG_PRE | FS_ENUM_FLAG_POST)) return true;
    i &= FS_ENUM_IDX_MASK;
    fs_ino_rw_cb_args *args = (fs_ino_rw_cb_args *)vargs;
    int32_t offset = i * BLOCK_SIZE;
    int32_t left = args->size - offset;
    if (left <= 0) return false;
    assert(*block != BLK_INVALID);
    size_t min = MIN(left, BLOCK_SIZE);
    _memcpy(args->buffer + offset, &args->fs->blocks[*block], min);
    return true;
}

int32_t fs_ino_read(fs_fs *fs, uint16_t ino, uint8_t *buffer, size_t size) {
    int32_t min = MIN(size, fs_ino_size(fs, ino));
    fs_ino_rw_cb_args args = { fs, buffer, min, 0 };
    fs_ino_enumerate_blocks(fs, ino, fs_ino_read_cb, &args);
    return min;
}

bool fs_ino_write_cb(uint16_t *block, int32_t i, void *vargs) {
    printf("cb-write %d %d\n", *block, i);
    fs_ino_rw_cb_args *args = (fs_ino_rw_cb_args *)vargs;

    if (i & FS_ENUM_FLAG_POST) return true;
    if (i & FS_ENUM_FLAG_PRE) {
        if (*block == BLK_INVALID)
            *block = fs_alloc_block(args->fs);
        if (*block == BLK_INVALID)
            return false;
        return true;
    }

    i &= FS_ENUM_IDX_MASK;

    int32_t offset = i * BLOCK_SIZE;
    int32_t left = args->size - offset;
    if (left <= 0) return false;
    if (*block == BLK_INVALID)
        *block = fs_alloc_block(args->fs);
    if (*block == BLK_INVALID)
        return false;
    size_t min = MIN(left, BLOCK_SIZE);
    _memcpy(&args->fs->blocks[*block], args->buffer + offset, min);
    args->output += min;
    return true;
}

// TODO improve efficiency
int32_t fs_ino_write(fs_fs *fs, uint16_t ino, uint8_t *buffer, size_t size) {
    fs_ino_free_blocks(fs, ino);
    // fs_err err = fs_ino_alloc_blocks(fs, ino, size);
    // puts("ERR failed to allocate enought blocks");
    // if (err != FS_SUCCESS) return -1;

    fs_ino_rw_cb_args args = { fs, buffer, (int32_t)size, 0 };
    fs_ino_enumerate_blocks(fs, ino, /*size,*/ fs_ino_write_cb, &args);

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

fs_dentry *fs_dir_next(fs_dir *dir) {
    fs_dentry *dentry = fs_dir_entry(dir);
    if (dentry == NULL) return NULL;
    dir->size -= 5 + dentry->len;
    dir->buffer += 5 + dentry->len;
    return fs_dir_entry(dir);
}

int32_t fs_ino_readdir(fs_fs *fs, uint16_t ino, fs_dir *dir, size_t size) {
    dir->size = fs_ino_size(fs, ino);
    int32_t read = fs_ino_read(fs, ino, dir->buffer, size);
    if (read < 0) return read;
    // TODO think about this
    // if (read != size) return -EFBIG;
    return 0;
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

    uint8_t buffer[1000];
    fs_dir dir = { 1000, buffer };
    int32_t err = fs_ino_readdir(fs, parent_ino, &dir, 1000);
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

    return 0;
}

// TODO improve logic
fs_err fs_ino_unlink(fs_fs *fs, uint16_t parent_ino, const char *name) {
    uint8_t buffer[1000];
    fs_dir dir = { .buffer = buffer };
    fs_ino_readdir(fs, parent_ino, &dir, 1000);
    // TODO error handling
   
    uint8_t *begin = dir.buffer;
    size_t size = fs_get_inode(fs, parent_ino)->size;
    
    fs_dentry *dentry = fs_dir_entry(&dir);
    while (dentry != NULL) {
        if (!_strcmp(&dentry->name, name)) {
            uint8_t *current_buffer = dir.buffer;
            uint16_t current_len = dentry->len;

            fs_dir_next(&dir);

            // UB
            _memcpy(current_buffer, dir.buffer, dir.size);

            fs_ino_write(fs, parent_ino, begin, size - (5 + current_len));
            // TODO error handling

            fs_ino_refs_dec(fs, dentry->ino);

            return FS_SUCCESS;
        }
        dentry = fs_dir_next(&dir);
    }

    return FS_ENOENT;
}

int16_t fs_ino_mk(fs_fs *fs, uint16_t parent_ino, const char *name, uint8_t type) {
    if (!fs_ino_isdir(fs, parent_ino)) {
        puts("ERR can only make file in dir");
        error = -ENOTDIR;
        return INO_INVALID;
    }

    uint16_t ino = fs_alloc_inode(fs);
    if (ino == INO_INVALID) {
        puts("ERR no inodes left");
        error = -ENFILE;
        return INO_INVALID;
    }

    fs_inode *inode = fs_get_inode(fs, ino);

    inode->ino = ino;
    inode->uid = 0;
    inode->gid = 0;
    inode->type_mode = (type << FS_MODE_BITS) | 0;
    inode->refs = 0;
    inode->time = 0;
    inode->block_0 = BLK_INVALID;
    inode->block_1 = BLK_INVALID;
    inode->block_2 = BLK_INVALID;
    inode->block_3 = BLK_INVALID;
    inode->block_p = BLK_INVALID;
    inode->block_pp = BLK_INVALID;
    inode->block_ppp0 = BLK_INVALID;
    inode->block_ppp1 = BLK_INVALID;

    fs_ino_link(fs, parent_ino, ino, name);

    return ino;
}

uint16_t fs_ino_mkdir(fs_fs *fs, uint16_t parent_ino, const char *name) {
    int16_t ino = fs_ino_mk(fs, parent_ino, name, FS_TYPE_FLAG_DIR);
    if (ino == INO_INVALID) return ino;
    fs_ino_link(fs, ino, ino, ".");
    fs_ino_link(fs, ino, parent_ino, "..");
    return ino;
}

uint16_t fs_ino_mknod(fs_fs *fs, uint16_t parent_ino, const char *name) {
    return fs_ino_mk(fs, parent_ino, name, FS_TYPE_FLAG_REG);
}

uint16_t fs_name_to_ino(fs_fs *fs, uint16_t ino, const char *name) {
    uint8_t buffer[500];
    fs_dir dir = { .buffer = buffer };
    fs_ino_readdir(fs, ino, &dir, 500);

    fs_dentry *dentry = fs_dir_entry(&dir);
    while (dentry != NULL) {
        if (!_strcmp(&dentry->name, name)) return dentry->ino;
        dentry = fs_dir_next(&dir);
    }
    
    return INO_INVALID;
}

// TODO improve logic
uint16_t fs_path_to_ino_rel(fs_fs *fs, char *path, uint16_t ino) {
    char *ptr = path;
    while (true) {
        if (*ptr == '/') {
            *ptr = '\0';
            ino = fs_name_to_ino(fs, ino, path);
            if (ino == INO_INVALID) return INO_INVALID;
            ptr++;
            path = ptr;
        }
        if (*ptr == '\0') {
            if (ptr == path) return ino;
            ino = fs_name_to_ino(fs, ino, path);
            if (ino == INO_INVALID) return INO_INVALID;
            break;
        }
        ptr++;
    }
    return ino;
}

uint16_t fs_path_to_ino_abs(fs_fs *fs, char *path) {
    return fs_path_to_ino_rel(fs, path, fs->header->root_ino);
}

uint16_t fs_path_to_ino(fs_fs *fs, const char *path) {
    if (_strlen(path) > FS_PATH_LEN_MAX) {
        puts("ERR path too long");
        return INO_INVALID;
    }

    char buffer[FS_PATH_LEN_MAX];
    _memcpy(buffer, path, FS_PATH_LEN_MAX);

    if (*path == '/') {
        return fs_path_to_ino_abs(fs, buffer + 1);
    }

    puts("ERR only abs paths are supported");
    return INO_INVALID;
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
    if (err != FS_SUCCESS) return INO_INVALID;
    return fs_path_to_ino(fs, buffer);
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

    fs->header->max_ino = fs->header->inodes_total - 1;
   

    uint16_t root_ino = 1;
    fs->header->root_ino = root_ino;


    fs->inodes = fs_get_inodes(raw);
    fs->blocks = fs_get_blocks(raw);

    fs_init_blocks(fs);
    fs_init_inodes(fs);
    
    // create root inode

    fs_inode *root = fs_get_inode(fs, root_ino);
    root->ino = root_ino;
    root->uid = 100;
    root->gid = 100;
    root->type_mode = (FS_TYPE_FLAG_DIR << FS_MODE_BITS) | 0;
    root->refs = 0;
    root->time = 0;
    root->block_0 = BLK_INVALID;
    root->block_1 = BLK_INVALID;
    root->block_2 = BLK_INVALID;
    root->block_3 = BLK_INVALID;
    root->block_p = BLK_INVALID;
    root->block_pp = BLK_INVALID;
    root->block_ppp0 = BLK_INVALID;
    root->block_ppp1 = BLK_INVALID;

    fs_ino_link(fs, root_ino, root_ino, ".");
    fs_ino_link(fs, root_ino, root_ino, "..");
}

int sfs_getattr(const char *path, struct stat *st) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    
    fs_inode *inode = fs_get_inode(FS, ino);
    UNUSED(inode);

    st->st_mode = (fs_ino_isdir(FS, ino) ? S_IFDIR | 0755 : S_IFREG | 0644);
    st->st_nlink = 2;       // TODO
    st->st_size = fs_ino_size(FS, ino);
    st->st_uid = getuid();  // TODO
    st->st_gid = getgid();  // TODO

    return 0;
}

int sfs_readlink(const char *path, char *buffer, size_t size) {
    // TODO implement
    UNUSED(path);
    UNUSED(buffer);
    UNUSED(size);
    return 1;
}

int sfs_mknod(const char *path, mode_t mode, dev_t dev) {
    // TODO use mode
    UNUSED(mode);
    UNUSED(dev);


    uint16_t parent_ino = fs_path_to_parent_ino(FS, path);
    const char *sep = fs_path_get_last_sep(path);
    if (sep == NULL) return -EINVAL;

    const char *name = sep + 1;

    fs_ino_mknod(FS, parent_ino, name);
    return 0;
}

int sfs_mkdir(const char *path, mode_t mode) {
    // TODO use mode
    UNUSED(mode);

    uint16_t parent_ino = fs_path_to_parent_ino(FS, path);
    if (parent_ino == INO_INVALID) return -ENOENT;
    const char *sep = fs_path_get_last_sep(path);
    if (sep == NULL) return -EINVAL;
    const char *name = sep + 1;

    uint16_t ino = fs_ino_mkdir(FS, parent_ino, name);
    if (ino == INO_INVALID) return error;
    return 0;
}

int sfs_unlink(const char *path) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    if (fs_ino_isdir(FS, ino)) return -EISDIR;
    
    const char *sep = fs_path_get_last_sep(path);
    if (sep == NULL) return -EINVAL;
    const char *name = sep + 1;

    fs_inode *inode = fs_get_inode(FS, ino);
    fs_ino_unlink(FS, inode->ino, name);
    return 0;
}

int sfs_rmdir(const char *path) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    if (!fs_ino_isdir(FS, ino)) return -ENOTDIR;

    const char *sep = fs_path_get_last_sep(path);
    if (sep == NULL) return -EINVAL;
    const char *name = sep + 1;

    fs_inode *inode = fs_get_inode(FS, ino);
    if (inode->size != 0) return -ENOTEMPTY;

    fs_ino_unlink(FS, inode->ino, name);
    return 0;
}

int sfs_rename(const char *src, const char *dest) {
    uint16_t src_ino = fs_path_to_ino(FS, src);
    if (src_ino == INO_INVALID) return -ENOENT;

    uint16_t src_parent_ino = fs_path_to_parent_ino(FS, src);
    if (src_parent_ino == INO_INVALID) return -ENOENT;

    const char *src_sep = fs_path_get_last_sep(src);
    if (src_sep == NULL) return -EINVAL;
    const char *src_name = src_sep + 1;

    uint16_t dest_parent_ino = fs_path_to_parent_ino(FS, dest);
    if (dest_parent_ino == INO_INVALID) return -ENOENT;

    const char *dest_sep = fs_path_get_last_sep(dest);
    if (dest_sep == NULL) return -EINVAL;
    const char *dest_name = dest_sep + 1;

    fs_ino_link(FS, dest_parent_ino, src_ino, dest_name);

    fs_ino_unlink(FS, src_parent_ino, src_name);

    return 0;
}

int sfs_chown(const char *path, uid_t uid, gid_t gid) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    fs_inode *inode = fs_get_inode(FS, ino);
    inode->uid = uid;
    inode->gid = gid;
    return 0;
}

int sfs_truncate(const char *path, off_t offset) {
    UNUSED(offset);

    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    fs_inode *inode = fs_get_inode(FS, ino);


    UNUSED(inode);
    // TODO implement

    return 0;
}

int sfs_link(const char *dest, const char *src) {
    uint16_t dest_ino = fs_path_to_ino(FS, dest);
    if (dest_ino == INO_INVALID) return -ENOENT;

    uint16_t src_parent_ino = fs_path_to_parent_ino(FS, src);
    if (src_parent_ino == INO_INVALID) return -ENOENT;

    const char *sep = fs_path_get_last_sep(src);
    if (sep == NULL) return -EINVAL;
    const char *name = sep + 1;


    fs_ino_link(FS, src_parent_ino, dest_ino, name);

    return 0;
}

int32_t sfs_read(
    const char *path, char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    UNUSED(offset);
    UNUSED(fi);

    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    return fs_ino_read(FS, ino, (uint8_t *)buffer, size);
}

int sfs_write(
    const char *path,
    const char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
) {
    UNUSED(offset);
    UNUSED(fi);

    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    return fs_ino_write(FS, ino, (uint8_t *)buffer, size);
}

int sfs_statfs(const char *path, struct statvfs *stfs) {
    UNUSED(path);

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
    UNUSED(offset);
    UNUSED(fi);

    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
                                        
    uint8_t buf[1000];
    fs_dir dir = { .buffer = buf };
    fs_ino_readdir(FS, ino, &dir, 1000);

    fs_dentry *dentry = fs_dir_entry(&dir);
    while (dentry != NULL) {
        filler(buffer, &dentry->name, NULL, 0);
        dentry = fs_dir_next(&dir);
    }
    return 0;
}

int sfs_utimens(const char *path, const struct timespec tv[2]) {
    uint16_t ino = fs_path_to_ino(FS, path);
    if (ino == INO_INVALID) return -ENOENT;
    fs_inode *inode = fs_get_inode(FS, ino);
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
