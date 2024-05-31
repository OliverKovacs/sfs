#ifndef SFS_H
#define SFS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <fuse.h>

#include "sys.h"

#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define UNUSED(x) (void)(x)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define SUCCESS 0

#define FS_BLOCK_SIZE 512
#define FS_DIR_MAX 1024
#define FS_PATH_LEN_MAX 256
#define FS_NAME_LEN_MAX 64
#define FS_BLOCK_POINTERS 6

#define CHECK_INO(ino)                  \
if (ino < 0) return ino;                \
if (ino == INO_INVALID) return -EINVAL;

#define ERR(term)               \
{                               \
    int32_t err = (term);       \
    if (err < 0) return err;    \
}

#define READDIR(fs, ino)                        \
uint8_t buffer[FS_DIR_MAX];                     \
fs_dir dir = { .buffer = buffer };              \
ERR(fs_ino_readdir(fs, ino, &dir, FS_DIR_MAX));

#define CALL(term) if (!(term)) return;

const size_t MB = 1048576;
const size_t DISK_SIZE = 1 * MB;

const uint16_t BLK_INVALID =  0;
const uint16_t INO_INVALID = 0;

const bool USE_CURRENT_USER = true;

typedef struct fs_inode {
    uint16_t ino;
    uint8_t uid;
    uint8_t gid;
    uint16_t mode;
    uint16_t refs;
    uint32_t size;
    uint32_t time;
    uint16_t block[FS_BLOCK_POINTERS];
    uint16_t block_p;
    uint16_t block_pp;
} fs_inode;

typedef struct fs_block {
    union {
        uint8_t bytes[FS_BLOCK_SIZE];
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

typedef struct fs_index {
    uint32_t pre : 1;
    uint32_t post : 1;
    uint32_t index : 30;
} fs_index;

typedef struct fs_ino_rw_cb_args {
    fs_fs *fs;
    void *buffer;
    int32_t size;
    int32_t output;
} fs_ino_rw_cb_args;

typedef struct fs_ino_trunc_cb_args {
    fs_fs *fs;
    int32_t old_size;
    int32_t new_size;
    int32_t output;
} fs_ino_trunc_cb_args;

fs_fs *FS;

static inline fs_inode *fs_get_inode(fs_fs *fs, uint16_t ino) {
    assert(ino != INO_INVALID);
    assert(ino <= fs->header->max_ino);
    return &fs->inodes[ino];
}

static inline bool fs_ino_isdir(fs_fs *fs, uint16_t ino) {
    return fs_get_inode(fs, ino)->mode & (S_IFDIR >> 3);
}

void fs_ino_enumerate_blocks(
    fs_fs *fs,
    uint16_t ino,
    bool(*callback)(uint16_t *, fs_index, void *),
    void *args
) {
    // TODO error handling
    fs_inode *inode = fs_get_inode(fs, ino);
    
    uint32_t i = 0;
    while (i < FS_BLOCK_POINTERS) {
        CALL(callback(&inode->block[i], (fs_index){ 0, 0, i }, args));
        i++;
    }

    CALL(callback(&inode->block_p, (fs_index){ 1, 0, i }, args));
    uint16_t *pblock = (uint16_t *)&fs->blocks[inode->block_p];
    
    for (size_t j = 0; j < fs->header->blockp_len; j++) {
        CALL(callback(&pblock[j], (fs_index){ 0, 0, i }, args));
        i++;
    }

    CALL(callback(&inode->block_p, (fs_index){ 0, 1, i }, args));

    CALL(callback(&inode->block_pp, (fs_index){ 1, 0, i }, args));
    uint16_t *ppblock = (uint16_t *)&fs->blocks[inode->block_pp];

    for (size_t j = 0; j < fs->header->blockp_len; j++) {
        CALL(callback(&ppblock[j], (fs_index){ 1, 0, i }, args));
        uint16_t *pblock = (uint16_t *)&fs->blocks[ppblock[j]];

        for (size_t k = 0; k < fs->header->blockp_len; k++) {
            CALL(callback(&pblock[j], (fs_index){ 0, 0, i }, args));
            i++;
        }

        CALL(callback(&ppblock[j], (fs_index){ 0, 1, i }, args));
    }

    CALL(callback(&inode->block_pp, (fs_index){ 0, 1, i }, args));
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
    assert(blk != BLK_INVALID);
    assert(blk < fs->header->blocks_total);
    fs_block *block = &fs->blocks[blk];
    block->free.next = fs->header->free_blk;
    fs->header->free_blk = blk;
    fs->header->blocks--;
}

// TODO improve
bool fs_ino_truncate_cb(uint16_t *block, fs_index i, void *vargs) {
    printf("cb-trunc %d %d\n", *block, i.index);
    fs_ino_trunc_cb_args *args = (fs_ino_trunc_cb_args *)vargs;

    int32_t offset = i.index * args->fs->header->block_size;
    int32_t old_left = args->old_size - offset;
    int32_t new_left = args->new_size - offset;

    if (old_left > 0) {
        if (new_left > 0) {
            if (i.pre | i.post) return true;
            // TODO investigate
            // int32_t size = args->fs->header->block_size - old_left;
            // if (size > 0)
            //     _memset(&args->fs->blocks[*block].bytes + old_left + 1, 0, size - 1);   // why off by one?
        }
        else {
            // free
            if (i.pre) return true;
            assert(*block != BLK_INVALID);
            fs_free_block(args->fs, *block);
            *block = BLK_INVALID;
        }
        return true;
    }
    else {
        if (new_left > 0) {
            // alloc
            if (i.post) return true;
            assert(*block == BLK_INVALID);
            *block = fs_alloc_block(args->fs);
            if (*block == INO_INVALID) {
                args->output = -ENOSPC;
                return false;
            };

            _memset(&args->fs->blocks[*block], 0, args->fs->header->block_size);
            return true;
        }
        return false;
    }
}

int32_t fs_ino_truncate(fs_fs *fs, uint16_t ino, size_t size) {
    fs_inode *inode = fs_get_inode(fs, ino);
    fs_ino_trunc_cb_args args = {
        fs,
        (int32_t)inode->size,
        (int32_t)size,
        SUCCESS
    };
    fs_ino_enumerate_blocks(fs, ino, fs_ino_truncate_cb, &args);

    // TODO handle errors better
    inode->size = size;
    return args.output;
}

void fs_free_inode(fs_fs *fs, uint16_t ino) {
    fs_ino_truncate(fs, ino, 0);

    fs_inode *inode = fs_get_inode(fs, ino);
    inode->ino = fs->header->free_ino;
    fs->header->free_ino = ino;
    fs->header->inodes--;
}

// TODO error handling
bool fs_ino_read_cb(uint16_t *block, fs_index i, void *vargs) {
    printf("cb-read %d %d\n", *block, i.index);
    if (i.pre || i.post) return true;

    fs_ino_rw_cb_args *args = (fs_ino_rw_cb_args *)vargs;
    int32_t offset = i.index * args->fs->header->block_size;
    int32_t left = args->size - offset;
    if (left <= 0) return false;

    assert(*block != BLK_INVALID);
    size_t min = MIN(left, args->fs->header->block_size);
    _memcpy((uint8_t *)args->buffer + offset, &args->fs->blocks[*block], min);
    return true;
}

int32_t fs_ino_read(fs_fs *fs, uint16_t ino, void *buffer, size_t size) {
    int32_t min = MIN(size, fs_get_inode(fs, ino)->size);
    fs_ino_rw_cb_args args = { fs, buffer, min, 0 };
    fs_ino_enumerate_blocks(fs, ino, fs_ino_read_cb, &args);
    // TODO return
    return min;
}

// TODO error handling
bool fs_ino_write_cb(uint16_t *block, fs_index i, void *vargs) {
    printf("cb-write %d %d\n", *block, i.index);
    if (i.pre || i.post) return true;

    fs_ino_rw_cb_args *args = (fs_ino_rw_cb_args *)vargs;
    int32_t offset = i.index * args->fs->header->block_size;
    int32_t left = args->size - offset;
    if (left <= 0) return false;

    assert(*block != BLK_INVALID);
    size_t min = MIN(left, args->fs->header->block_size);
    _memcpy(&args->fs->blocks[*block], (uint8_t *)args->buffer + offset, min);
    args->output += min;
    return true;
}

int32_t fs_ino_write(fs_fs *fs, uint16_t ino, const void *buffer, size_t size) {
    fs_ino_truncate(fs, ino, size);

    fs_ino_rw_cb_args args = { fs, (void *)buffer, (int32_t)size, 0 };
    fs_ino_enumerate_blocks(fs, ino, fs_ino_write_cb, &args);
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
    if (!fs_ino_isdir(fs, ino)) return -ENOTDIR;
    dir->size = fs_get_inode(fs, ino)->size;
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
    READDIR(fs, parent_ino);

    fs_dir tmp = { dir.size, dir.buffer };
    fs_dentry *res = fs_dir_search(&tmp, name);
    if (res != NULL) return -EEXIST;

    int32_t size = dir.size;
    fs_dentry *new_dentry = (fs_dentry *)(dir.buffer + size);

    int32_t len = _strlen(name);
    new_dentry->ino = ino;
    new_dentry->len = len;
    _memcpy(&new_dentry->name, name, len + 1);

    int32_t written = fs_ino_write(fs, parent_ino, buffer, size + 5 + len);
    if (written != size + 5 + len) return -EIO;
    fs_ino_refs_inc(fs, ino);
    return SUCCESS;
}

int32_t fs_ino_unlink(fs_fs *fs, uint16_t parent_ino, const char *name) {
    READDIR(fs, parent_ino);
   
    size_t size = fs_get_inode(fs, parent_ino)->size;
    
    fs_dentry *dentry = fs_dir_search(&dir, name);
    if (dentry == NULL) return -ENOENT;

    uint8_t *buf = dir.buffer;
    uint16_t len = dentry->len;

    fs_dir_next(&dir);
    // TODO UB
    _memcpy(buf, dir.buffer, dir.size);

    ERR(fs_ino_write(fs, parent_ino, buffer, size - (5 + len)));

    fs_ino_refs_dec(fs, dentry->ino);
    return SUCCESS;
}

void fs_init_inode(fs_fs *fs, uint16_t ino, uint16_t mode) {
    fs_inode *inode = fs_get_inode(fs, ino);

    inode->ino = ino;
    inode->uid = 0;
    inode->gid = 0;
    inode->mode = mode;
    inode->refs = 0;
    inode->time = time(NULL);
    for (size_t i = 0; i < FS_BLOCK_POINTERS; i++) {
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
    ERR(fs_ino_link(fs, parent_ino, ino, name));
    return ino;
}

int32_t fs_ino_mkdir(fs_fs *fs, uint16_t parent_ino, const char *name, uint16_t mode) {
    int16_t ino = fs_ino_mk(fs, parent_ino, name, mode | (S_IFDIR >> 3));
    CHECK_INO(ino);

    ERR(fs_ino_link(fs, ino, ino, "."));
    ERR(fs_ino_link(fs, ino, parent_ino, ".."));
    return ino;
}

int32_t fs_ino_mknod(fs_fs *fs, uint16_t parent_ino, const char *name, uint16_t mode) {
    return fs_ino_mk(fs, parent_ino, name, mode);
}

int32_t fs_name_to_ino(fs_fs *fs, uint16_t ino, const char *name) {
    if (*name == '\0') return ino;
    
    READDIR(fs, ino);
    fs_dentry *dentry = fs_dir_search(&dir, name);
    if (dentry == NULL) return -ENOENT;
    return dentry->ino;
}

int32_t fs_path_to_parent_ino_rel(fs_fs *fs, const char *path, int32_t ino) {
    char name[FS_NAME_LEN_MAX];
    if (*path == '/') path++;

    for (const char *ptr = path; *ptr != '\0'; ptr++) {
        if (*ptr != '/') continue;
        size_t len = ptr - path;
        _memcpy(name, path, len);
        *(name + len) = '\0';
        ino = fs_name_to_ino(fs, ino, name);
        CHECK_INO(ino);
        ptr++;
        path = ptr;
    }
    return ino;
}

int32_t fs_path_to_parent_ino(fs_fs *fs, const char *path) {
    return fs_path_to_parent_ino_rel(fs, path, fs->header->root_ino);
}

const char *fs_path_get_name(const char *path) {
    size_t len = _strlen(path);
    for (const char *ptr = path + len - 1; ptr >= path; ptr--) {
        if (*ptr == '/') return ptr + 1;
    }
    return NULL;
}

int32_t fs_path_to_ino_rel(fs_fs *fs, const char *path, uint16_t ino) {
    int32_t parent_ino = fs_path_to_parent_ino_rel(fs, path, ino);
    CHECK_INO(parent_ino);

    const char *name = fs_path_get_name(path);
    if (name == NULL) return -EINVAL;

    return fs_name_to_ino(fs, parent_ino, name);
}

int32_t fs_path_to_ino(fs_fs *fs, const char *path) {
    if (*path != '/') return -EINVAL;
    return fs_path_to_ino_rel(fs, path, fs->header->root_ino);
}

void fs_init_blocks(fs_fs *fs) {
    size_t size = fs->header->blocks_total;

    fs->header->free_blk = 1;
    for (size_t i = 1; i < size - 1; i++) {
        fs->blocks[i].free.next = i + 1;
    }
    fs->blocks[BLK_INVALID].free.next = BLK_INVALID;
    fs->blocks[size - 1].free.next = BLK_INVALID;
}

void fs_init_inodes(fs_fs *fs) {
    size_t size = fs->header->inodes_total;

    fs->header->free_ino = 2;
    for (size_t i = fs->header->free_ino; i < size - 1; i++) {
        fs_get_inode(fs, i)->ino = i + 1;
    }
    fs->inodes[INO_INVALID].ino = INO_INVALID;
    fs_get_inode(fs, size - 1)->ino = INO_INVALID;
}

void fs_create(fs_fs *fs, fs_block *raw, size_t size) {
    _memset(raw, 0, size);

    fs->header = (fs_header *)raw;

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

    fs->inodes = (fs_inode *)(raw + fs->header->blocks_header);
    fs->blocks = (fs_block *)fs->inodes  + fs->header->blocks_inode;

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
    st->st_size = inode->size;
    st->st_atime = inode->time;
    st->st_ctime = inode->time;
    st->st_mtime = inode->time;
    st->st_uid = USE_CURRENT_USER ? getuid() : inode->uid;
    st->st_gid = USE_CURRENT_USER ? getgid() : inode->gid;

    return SUCCESS;
}

int32_t sfs_readlink(const char *path, char *buffer, size_t size) {
    size_t len = _strlen(path);
    if (len > size) return -ENAMETOOLONG;
    _memcpy(buffer, path, len);
    return SUCCESS;
}

int32_t sfs_mknod(const char *path, mode_t mode, dev_t dev) {
    UNUSED(dev);

    int32_t parent_ino = fs_path_to_parent_ino(FS, path);
    CHECK_INO(parent_ino);

    const char *name = fs_path_get_name(path);
    if (name == NULL) return -EINVAL;

    int32_t file_mode = fs_mode_to_sfs((mode & (S_IRWXU | S_IRWXG | S_IRWXO)) | S_IFREG);
    ERR(fs_ino_mknod(FS, parent_ino, name, file_mode));
    return SUCCESS;
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

    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);
    if (fs_ino_isdir(FS, ino)) return -EISDIR;
    
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
    
    READDIR(FS, ino);
    fs_dentry *dentry = fs_dir_entry(&dir);
    while (dentry != NULL) {
        if (_strcmp(&dentry->name, ".") && _strcmp(&dentry->name, "..")) return -ENOTEMPTY;
        dentry = fs_dir_next(&dir);
    }

    int32_t parent_ino = fs_path_to_parent_ino(FS, path);
    CHECK_INO(parent_ino);
    return fs_ino_unlink(FS, parent_ino, name);
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

    ERR(fs_ino_link(FS, dest_parent_ino, src_ino, dest_name));
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
    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);
    return fs_ino_truncate(FS, ino, offset);
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

    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);
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
    void *b,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi
) {
    UNUSED(offset);
    UNUSED(fi);

    int32_t ino = fs_path_to_ino(FS, path);
    CHECK_INO(ino);

    READDIR(FS, ino);
    fs_dentry *dentry = fs_dir_entry(&dir);
    while (dentry != NULL) {
        filler(b, &dentry->name, NULL, 0);
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
