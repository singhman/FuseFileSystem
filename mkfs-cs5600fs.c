/*
 * file:        mkfs-cs5600fs.c
 * description: mkfs utility for CS 5600 homweork 4, the
 *              CS5600fs file system. 
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2011
 * $Id: mkfs-7600.c 160 2010-04-20 19:48:56Z pjd $
 */

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "cs5600fs.h"

int write_block(int fd, int lba, void *buf)
{
    return pwrite(fd, buf, FS_BLOCK_SIZE, lba*FS_BLOCK_SIZE);
}

int read_block(int fd, int lba, void *buf)
{
    return pread(fd, buf, FS_BLOCK_SIZE, lba*FS_BLOCK_SIZE);
}

#include <getopt.h>

unsigned long parseint(char *s)
{
    unsigned long val = strtol(s, &s, 0);
    if (toupper(*s) == 'G')
        val *= (1024*1024*1024);
    if (toupper(*s) == 'M')
        val *= (1024*1024);
    if (toupper(*s) == 'K')
        val *= 1024;
    return val;
}

void usage(void)
{
    printf("usage: mkfs-cs5600fs [--create <size>] <image-file>\n");
    exit(1);
}

int main(int argc, char **argv)
{
    char *path;
    int i, c, fd, nblocks;
    unsigned long img_size = 0;
    struct stat sb;
    void *buf = calloc(FS_BLOCK_SIZE, 1);

    /* Parse arguments: --create <size> is indicated to setting
       'img_size' to the specified value. (in bytes, k, m, or g)
     */
    static struct option options[] = {
	{"create", required_argument, NULL, 'c'}, {0, 0, 0, 0}};
    while ((c = getopt_long_only(argc, argv, "", options, NULL)) != -1) 
	switch (c) {
	case 'c': img_size = parseint(optarg); break;
	default:
	    printf("bad option\n"), usage();
	}

    if (optind >= argc)
	usage();
    path = argv[optind];

    /* Find how big the image file is (if it already exists) or create
     * it if necessary.
     */
    
    if (img_size == 0) {
	if ((fd = open(path, O_WRONLY, 0777)) < 0 ||
	    fstat(fd, &sb) < 0)
	    perror("error opening file"), exit(1);
	nblocks = sb.st_size / FS_BLOCK_SIZE;
    }
    else {
	nblocks = img_size / FS_BLOCK_SIZE;
	
	if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777)) < 0)
	    perror("error creating file"), exit(1);
	for (i = 0; i < nblocks; i++)
	    if (write(fd, buf, FS_BLOCK_SIZE) < 0)
		perror("error writing file"), exit(1);
	lseek(fd, 0, SEEK_SET);
    }

    /* write the superblock
     */
    struct cs5600fs_super *super = buf;
    super->magic = CS5600FS_MAGIC;
    super->blk_size = FS_BLOCK_SIZE;
    super->fs_size = nblocks;

    const int fats_per_block = FS_BLOCK_SIZE / sizeof(struct cs5600fs_entry);
    const int fat_len = ((nblocks + (fats_per_block-1)) / fats_per_block);
    super->fat_len = fat_len;

    super->root_dirent = (struct cs5600fs_dirent)
	{.valid = 1, .isDir = 1, .mode = 0777, .start = 1+fat_len};
		
    write_block(fd, 0, buf);

    /* Initialize the FAT table. Set the inUse flag for any entries
     * corresponding to the superblock and the file access table
     * itself.  
     */
    int j, rsvd_blocks = fat_len+2;
    struct cs5600fs_entry *e = buf;
    memset(buf, 0, FS_BLOCK_SIZE);

    for (i = 0; i < fat_len; i++) {
	for (j = 0; j < fats_per_block; j++)
	    e[j].inUse = e[j].eof = (j < rsvd_blocks) ? 1 : 0;
	rsvd_blocks -= fats_per_block;
	write_block(fd, 1+i, buf);
    }

    /* Now create the root directory
     */
    const int dirents_per_block = FS_BLOCK_SIZE / sizeof(struct cs5600fs_dirent);
    struct cs5600fs_dirent *de = buf;
    memset(buf, 0, FS_BLOCK_SIZE);
    for (i = 0; i < dirents_per_block; i++)
	de[i].valid = 0;
    write_block(fd, 1+fat_len, buf);

    close(fd);
    return 0;
}
