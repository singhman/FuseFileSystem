/*
 * file:        homework.c
 * description: skeleton file for CS 5600 homework 3
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, updated April 2012
 * $Id: homework.c 452 2011-11-28 22:25:31Z pjd $
 */

#define FUSE_USE_VERSION 27
#define FILENAME_MAXLENGTH 43
#define MAX_DIRENT 16
#define IS_DIR 1
#define IS_FILE 0

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "cs5600fs.h"
#include "blkdev.h"

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 512-byte SECTORS, while the
 * file system uses 1024-byte BLOCKS. Remember to multiply everything
 * by 2.
 */

extern struct blkdev *disk;

static const char *root_path = "/";
static const char *delim = "/";

/* superblock - details about file system 
 * fat - file allocation table
 * directory - directory enteries of a block */
struct cs5600fs_super superblock;
struct cs5600fs_entry *fat = NULL;
struct cs5600fs_dirent directory[MAX_DIRENT];

/* function declarations */
void flush_fat(void);
void convert_cs5600fs_linux_stat(struct cs5600fs_dirent *, struct stat *);
int parse_path(char *, char **, int);
int lookup_helper(const char *, struct cs5600fs_dirent *, int *);
int lookup(const char *, struct cs5600fs_dirent *);
int get_unused_fat();
int within_same_directory(const char *, const char *);

/* init - this is called once by the FUSE framework at startup.
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void* hw3_init(struct fuse_conn_info *conn)
{
    int superblock_rc;
    
    /* allocate a buffer to read superblock from disk
     * into memory of size FS_BLOCK_SIZE = 1024 */
    char *superblock_buf = calloc(FS_BLOCK_SIZE,1);
    
    superblock_rc = disk->ops->read(disk, 0, 2, superblock_buf);
    
    superblock.magic = *((unsigned int *)superblock_buf);
    superblock.blk_size = *((unsigned int *) (superblock_buf + 4));
    superblock.fs_size = *((unsigned int *)(superblock_buf + 8));
    superblock.fat_len = *((unsigned int *) (superblock_buf + 12));
    superblock.root_dirent = *((struct cs5600fs_dirent *) (superblock_buf + 16));

    char *fat_buf = calloc(FS_BLOCK_SIZE, superblock.fat_len);
    
    int i = 0;
    for(i = 0; i < superblock.fat_len; ++i){
        disk->ops->read(disk, (i + 1)* 2, 2, fat_buf + (i * FS_BLOCK_SIZE));
    }
    
    fat = (struct cs5600fs_entry *) fat_buf;
    
    return NULL;
}

/* commit fat changes to disk */
void flush_fat(){
    int i =0;
    for (i = 0; i < superblock.fat_len; ++i){
        disk->ops->write(disk, (i + 1) * 2, 2, (void *)fat + (i * FS_BLOCK_SIZE));
    }
}

/* convert cs5600fs stat to linux stat */
void convert_cs5600fs_linux_stat(struct cs5600fs_dirent *dirent, struct stat *sb){
    /* set to 0 (cs5600fs -> linux) */
    sb->st_dev = 0;
    sb->st_ino = 0;
    sb->st_rdev = 0;
    sb->st_blocks = (dirent->length + FS_BLOCK_SIZE -1)/FS_BLOCK_SIZE;
    sb->st_blksize = 0;
    
    /* set to 1 (cs5600fs -> linux) */
    sb->st_nlink = 1;
    
    /* copy directly (cs5600fs -> linux) */
    sb->st_uid = dirent->uid;
    sb->st_gid = dirent->gid;
    sb->st_size = dirent->isDir ? 0 : dirent->length;
    sb->st_mtime = dirent->mtime;
    
    /* set to same value as st_mtime (cs5600fs -> linux) */
    sb->st_atime = dirent->mtime;
    sb->st_ctime = dirent->mtime;
    
    /* mode (cs5600fs -> linux) */
    sb->st_mode = dirent->mode | (dirent->isDir ? S_IFDIR : S_IFREG);
}

/* split the path into strings */
int parse_path(char *path, char **args, int n){
    
    char **ap;
    for( ap = args; (*ap = strtok(path,delim)) != NULL; path = NULL){
        if(++ap >= &args[n]){
            break;
        }
    }
    return ap-args;
}

/* look file or directory and parent start block in path */
int lookup_helper(const char *path, struct cs5600fs_dirent *dirent, int * parent_start_blk){
    int i,j, depth;
    
    char path_copy[strlen(path) + 1];
    strncpy(path_copy, path, sizeof(path_copy));
    
    char *names[10];
    depth = parse_path(path_copy, names, 10);
    
    memcpy(dirent, &superblock.root_dirent, sizeof(struct cs5600fs_dirent));
    
    for(i = 0; i < depth; i++){
        
        disk->ops->read(disk, dirent->start * 2, 2, (void *)directory);
        for(j = 0; j < MAX_DIRENT; j++){
            if(strcmp(directory[j].name, names[i]) == 0){
                if(directory[j].isDir != 1 && i < depth -1){
                    return -ENOTDIR;
                }
                else if(directory[j].valid == 1){
                    *parent_start_blk = dirent->start;
                    memcpy(dirent, &directory[j], sizeof(struct cs5600fs_dirent));
                    break;
                }
            }
        }
        if(j == MAX_DIRENT){
            return -ENOENT;
        }
    }
    return j;
}

/* lookup file or directory in path */
int lookup(const char *path, struct cs5600fs_dirent *dirent){
    int x = 0;
    return lookup_helper(path, dirent, &x);
}

/* return index of unused block from fat */
int get_unused_fat(){
    int i = 0;
    for (i = 0; i < superblock.fs_size; ++i) {
        if(0 == fat[i].inUse){
            fat[i].inUse = 1;
            fat[i].eof = 1;
            return i;
        }
    }
    
    return -i;
}

/* return 1 if src and destination within same directory, otherwise 0 */
int within_same_directory(const char *src_path, const char *dst_path){
    
    char src[strlen(src_path) + 1];
    strncpy(src, src_path, sizeof(src));
    
    char dst[strlen(dst_path) + 1];
    strncpy(dst, dst_path, sizeof(dst));
    
    char *src_name = strrchr(src, '/');
    char *dst_name = strrchr(dst, '/');
    
    /* Case: length of 1 level up is different */
    if((src - src_name) != (dst - dst_name)){
        return 0;
    }
    
    /* Case: root direcotry */
    if((0 == (src - src_name)) && (0 == (dst - dst_name))){
        return 1;
    }
    
    *src_name = 0;
    *dst_name = 0;
    /* Case: Root level to 1 level up */
    if(0 == strcmp(src, dst)){
        return 1;
    }
    
    return 0;
}

/* getattr - get file or directory attributes.
 */
static int hw3_getattr(const char *path, struct stat *sb)
{
    if(0 == strcmp(path, root_path)){
        convert_cs5600fs_linux_stat(&superblock.root_dirent, sb);
        return 0;
    }
    
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    
    int dirent_pos = lookup(path, dirent);
    if(0 > dirent_pos){
        return dirent_pos;
    }
    
    convert_cs5600fs_linux_stat(dirent, sb);
    return 0;
}

/* readdir - get directory contents.
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int hw3_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    char *name;
    struct stat sb;
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    
    int start_block = 0;
    
    if (0 == strcmp(root_path, path)) {
        start_block = superblock.root_dirent.start;
    } else {
        int dirent_pos = lookup(path,dirent);
        if(0 > dirent_pos){
            return dirent_pos;
        }
        if (dirent->isDir != 1) {
            return -ENOTDIR;
        }
        start_block = dirent->start;
    }
    
    disk->ops->read(disk, start_block * 2, 2, (void *)directory);

    memset(&sb, 0, sizeof(sb));
    int i=0;
    for (i = 0; i < MAX_DIRENT; ++i) {
        if(0 == directory[i].valid){
            continue;
        }
        
        name = directory[i].name;
        
        convert_cs5600fs_linux_stat(&directory[i], &sb);
        filler(buf, name, &sb, 0); /* invoke callback function */
    }
    return 0;
}

/* create a file or directory into file system, commit to disk */
int create_ent(const char *path, mode_t mode, int isDir){
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int start_block;
    
    char path_copy[strlen(path) + 1];
    strncpy(path_copy, path, sizeof(path_copy));
    
    if(0 == strcmp(root_path,path_copy)){
        return -EEXIST;
    }
    
    int dirent_pos = lookup(path_copy,dirent);
    if(dirent_pos >= 0){
        return dirent_pos;
    }
    
    char *truncated_name = strrchr(path_copy, '/');
    char name[FILENAME_MAXLENGTH + 1];
    if(truncated_name != path_copy){
        strcpy(name, truncated_name + 1);
        *truncated_name = 0;
        dirent_pos = lookup(path_copy, dirent);
        if(dirent->isDir != 1){
            return -ENOTDIR;
        }
        start_block = dirent->start;
    }else{
        strcpy(name, path_copy + 1);
        start_block = superblock.root_dirent.start;
    }
    
    disk->ops->read(disk, start_block *2, 2, (void*)directory);
    int i=0;
    for(i = 0; i < MAX_DIRENT; ++i){
        if(directory[i].valid == 0){
            break;
        }
    }
    
    int unused_block = get_unused_fat();
    if(i == MAX_DIRENT || 0 > unused_block){
        return -ENOSPC;
    }
    
    directory[i].valid = 1;
    directory[i].isDir = 1 == isDir ? 1 : 0;
    directory[i].mode = mode & 0777;
    directory[i].start = unused_block;
    directory[i].length = 0;
    directory[i].uid = getuid();
    directory[i].gid = getgid();
    directory[i].mtime = time(NULL);
    strncpy(directory[i].name, name, sizeof(name));
    
    disk->ops->write(disk, start_block * 2, 2, (void *)directory);
    if(isDir){
        disk->ops->read(disk, unused_block * 2, 2, (void *)directory);
        for(i = 0; i < MAX_DIRENT; ++i){
            directory[i].valid = 0;
        }
        disk->ops->write(disk, unused_block * 2, 2, (void *)directory);
    }
    
    flush_fat();
    return 0;
}

/* create - create a new file with permissions (mode & 01777)
 * Errors - path resolution, EEXIST
 * If a file or directory of this name already exists, return -EEXIST.
 */
static int hw3_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
    return create_ent(path, mode, IS_FILE);
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */ 
static int hw3_mkdir(const char *path, mode_t mode)
{
    return create_ent(path, mode, IS_DIR);
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 */
static int hw3_unlink(const char *path)
{
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int start_block, parent_start, eof;
    
    char path_copy[strlen(path) + 1];
    strncpy(path_copy, path, sizeof(path_copy));
    
    if(0 == strcmp(root_path,path_copy)){
        return -EISDIR;
    }
    
    int dirent_pos = lookup_helper(path_copy, dirent, &parent_start);
    if(0 > dirent_pos){
        return dirent_pos;
    }
    else if(1 == dirent->isDir){
        return -EISDIR;
    }
    
    directory[dirent_pos].valid = 0;
    disk->ops->write(disk, parent_start * 2, 2, (void *)directory);
    /* Case: Find fat entries and set inUse */
    start_block = dirent->start;
    for (eof = fat[start_block].eof; eof != 1; eof = fat[start_block].eof){
        fat[start_block].inUse = 0;
        start_block = fat[start_block].next;
    }
    
    fat[start_block].inUse = 0;
    fat[start_block].eof = 0;
    flush_fat();
    
    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int hw3_rmdir(const char *path)
{
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int parent_start;
    
    struct cs5600fs_dirent current_directory[MAX_DIRENT];
    
    char path_copy[strlen(path) + 1];
    strncpy(path_copy, path, sizeof(path_copy));
    
    if(0 == strcmp(root_path,path_copy)){
        return -EINVAL;
    }
    
    int dirent_pos = lookup_helper(path_copy,dirent, &parent_start);
    if(dirent_pos < 0){
        return dirent_pos;
    }
    if(1 != dirent->isDir){
        return -ENOTDIR;
    }
    
    /* Case: directory is not empty */
    disk->ops->read(disk, dirent->start * 2, 2, (void *)current_directory);
    int i =0;
    for(i = 0; i < MAX_DIRENT; ++i){
        if(1 == current_directory[i].valid){
            return -ENOTEMPTY;
        }
    }
    
    /* Case : Remove an empty directory */
    directory[dirent_pos].valid = 0;
    disk->ops->write(disk, parent_start * 2, 2, (void *)directory);
    fat[dirent->start].inUse = 0;
    fat[dirent->start].eof = 0;
    flush_fat();
    return 0;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 */
static int hw3_rename(const char *src_path, const char *dst_path)
{
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int parent_start, dirent_pos;
    
    if(0 == strcmp(root_path, src_path)){
        return -ENOTSUP;
    } else{
        if(0 == strcmp(root_path, dst_path)){
            return -EEXIST;
        }
        if(!within_same_directory(src_path, dst_path)){
            return -EINVAL;
        }
        /* Case: Check if destination already exist */
        dirent_pos = lookup_helper(dst_path, dirent, &parent_start);
        if(0 < dirent_pos){
            return -EEXIST;
        }
        
        /* Case: Check if source dir or file already exists */
        dirent_pos = lookup_helper(src_path, dirent, & parent_start);
        if(0 > dirent_pos){
            return dirent_pos;
        }
        
        char *name = strrchr(dst_path, '/');
        
        strcpy(directory[dirent_pos].name, name+1);
        disk->ops->write(disk, parent_start * 2, 2, (void *)directory);
    }
    return 0;
}

/* chmod - change file permissions
 * Errors - path resolution, ENOENT.
 */
static int hw3_chmod(const char *path, mode_t mode)
{
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int parent_start;
    
    if(0 == strcmp(root_path, path)){
        return -ENOTSUP;
    } else{
        int dirent_pos = lookup_helper(path, dirent, &parent_start);
        if(0 > dirent_pos){
            return dirent_pos;
        }
        
        directory[dirent_pos].mode = mode;
        disk->ops->write(disk, parent_start * 2, 2, (void *) directory);
    }
    return 0;
}

/* utime - change access and modification times */
int hw3_utime(const char *path, struct utimbuf *ut)
{
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int parent_start;
    
    if(0 == strcmp(root_path, path)){
        return -ENOTSUP;
    } else{
        int dirent_pos = lookup_helper(path, dirent, &parent_start);
        if(0 > dirent_pos){
            return dirent_pos;
        }
        
        directory[dirent_pos].mtime = ut->modtime;
        disk->ops->write(disk, parent_start * 2, 2, (void *) directory);
    }
    return 0;
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int hw3_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0)
	return -EINVAL;		/* invalid argument */
    
    int start_block, parent_start, eof;
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    if(0 == strcmp(root_path, path)){
        return -EISDIR;
    }
    else {
        int dirent_pos = lookup_helper(path, dirent, &parent_start);
        if(0 > dirent_pos){
            return dirent_pos;
        }
        if(dirent->isDir){
            return -EISDIR;
        }
        start_block = dirent->start;
        directory[dirent_pos].length = 0;
        
        disk->ops->write(disk, parent_start * 2, 2, (void *)directory);
        
        fat[start_block].inUse = 1;
        fat[start_block].eof = 1;
        
        /* Set inUse = 0 for the next blocks */
        start_block = fat[start_block].next;
        for(eof = fat[start_block].eof; eof != 1; eof = fat[start_block].eof){
            fat[start_block].inUse = 0;
            start_block = fat[start_block].next;
        }
        
        fat[start_block].inUse = 0;
        fat[start_block].eof = 0;
        
        flush_fat();
    }
    
    return 0;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= len, return 0
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int hw3_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
    char read_buf[FS_BLOCK_SIZE];
    int start_block;
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    if(0 == strcmp(root_path, path)){
        return -EISDIR;
    }
    else {
        int dirent_pos = lookup(path, dirent);
        if(0 > dirent_pos){
            return dirent_pos;
        }
        if(dirent->isDir){
            return -EISDIR;
        }
        start_block = dirent->start;
    }
    
    if(offset >= dirent->length){
        return 0;
    }
    
    int blk_num = offset / FS_BLOCK_SIZE;
    int blk_offset = offset % FS_BLOCK_SIZE;
    int i;
    for (i = 0; i < blk_num; ++i) {
        start_block = fat[start_block].next;
    }
    
    int read_len = len;
    if(len + offset > dirent->length){
        read_len = dirent->length - offset;
    }
    
    while(read_len > 0){
        disk->ops->read(disk, start_block * 2, 2, read_buf);
        if(read_len > FS_BLOCK_SIZE - blk_offset){
            memcpy(buf,read_buf + blk_offset, FS_BLOCK_SIZE - blk_offset);
            buf += FS_BLOCK_SIZE - blk_offset;
            read_len -= FS_BLOCK_SIZE - blk_offset;
            blk_offset = 0;
        }
        else {
            memcpy(buf, read_buf + blk_offset, read_len);
            buf += read_len;
            read_len = 0;
            blk_offset = 0;
        }
        start_block = fat[start_block].next;
    }
    
    if (offset + len > dirent->length){
      return dirent->length - offset;
    }
    
    return len;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 */
static int hw3_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
    char read_write_buf[FS_BLOCK_SIZE];
    char dirent_buf[sizeof(struct cs5600fs_dirent)];
    struct cs5600fs_dirent *dirent = (struct cs5600fs_dirent *)dirent_buf;
    int start_block, eof, file_len;
    int parent_start, dirent_pos;
    
    if(0 == strcmp(root_path, path)){
        return -EISDIR;
    } else{
        dirent_pos = lookup_helper(path, dirent, &parent_start);
        if(0 > dirent_pos){
            return dirent_pos;
        }
        start_block = dirent->start;
        file_len = dirent->length;
        eof = fat[start_block].eof;
        if(dirent->isDir){
            return -EISDIR;
        }
    }
    
    if(offset > file_len){
        return -EINVAL;
    }
    
    if(offset + len > file_len){
        directory[dirent_pos].length = offset + len;
        disk->ops->write(disk, parent_start * 2, 2, (void *)directory);
    }
    
    int block_num = offset / FS_BLOCK_SIZE;
    int block_offset = offset % FS_BLOCK_SIZE;
    
    /* Case: write to 2nd, 3rd or so on block */
    int i;
    for(i = 0; i < block_num && eof != 1; ++i){
        start_block = fat[start_block].next;
        eof = fat[start_block].eof;
    }
    
    /* Case: reached at eof so get ununsed fat entry */
    int prev_start;
    if(i < block_num && eof == 1){
        prev_start = start_block;
        start_block = get_unused_fat();
        if(0 > start_block){
            return -ENOSPC;
        }
        fat[prev_start].next = start_block;
        fat[prev_start].eof = 0;
    }
    
    
    int written_so_far;
    disk->ops->read(disk,start_block * 2, 2, read_write_buf);
    /* Case: write starting from middle of block*/
    if(len > FS_BLOCK_SIZE - block_offset){
        memcpy(read_write_buf + block_offset, buf, FS_BLOCK_SIZE - block_offset);
        written_so_far = FS_BLOCK_SIZE - block_offset;
        buf += FS_BLOCK_SIZE - block_offset;
    }
    else {
        memcpy(read_write_buf+ block_offset, buf, len);
        written_so_far = len;
        buf += len;
    }
    disk->ops->write(disk, start_block * 2, 2, read_write_buf);
    
    /* write rest of the content */
    while(len > written_so_far){
        if(eof == 0){
            start_block = fat[start_block].next;
            eof = fat[start_block].eof;
        }
        else {
            prev_start = start_block;
            start_block = get_unused_fat();
            if(0 > start_block){
                return -ENOSPC;
            }
            fat[prev_start].next = start_block;
            fat[prev_start].eof = 0;
        }
        if(len - written_so_far > FS_BLOCK_SIZE){
            disk->ops->write(disk, start_block * 2, 2, (char *)buf);
            buf += FS_BLOCK_SIZE;
            written_so_far += FS_BLOCK_SIZE;
        }
        else {
            disk->ops->read(disk, start_block * 2, 2, read_write_buf);
            memcpy(read_write_buf, buf, len - written_so_far);
            disk->ops->write(disk, start_block * 2, 2, read_write_buf);
            buf += len - written_so_far;
            written_so_far += len - written_so_far;
        }
    }
    
    flush_fat();
    return len;
}

/* statfs - get file system statistics
 */
static int hw3_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    
    st->f_bsize = superblock.blk_size;
    int fat_size = superblock.fat_len;
    st->f_blocks = superblock.fs_size - (1 + fat_size);
    
    int i=0, num_free_blocks =0;
    
    for (i = 0; i < superblock.fs_size; i++) {
        if(fat[i].inUse == 0){
            num_free_blocks++;
        }
    }
    st->f_bfree = num_free_blocks;
    st->f_bavail = num_free_blocks;
    st->f_namemax = FILENAME_MAXLENGTH;
    
    return 0;

}

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'hw3_ops'.
 */
struct fuse_operations hw3_ops = {
    .init = hw3_init,
    .getattr = hw3_getattr,
    .readdir = hw3_readdir,
    .create = hw3_create,
    .mkdir = hw3_mkdir,
    .unlink = hw3_unlink,
    .rmdir = hw3_rmdir,
    .rename = hw3_rename,
    .chmod = hw3_chmod,
    .utime = hw3_utime,
    .truncate = hw3_truncate,
    .read = hw3_read,
    .write = hw3_write,
    .statfs = hw3_statfs,
};

