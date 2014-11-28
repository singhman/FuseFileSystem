/*
 * file:        read-img.c
 * description: image reader utility for CS 5600 homework 3,
 *              the CS5600fs file system.
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, April 2010
 * $Id: read-img.c 456 2011-11-29 18:26:27Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cs5600fs.h"

int main(int argc, char **argv)
{
    FILE *fp = fopen(argv[1], "r");
    char buf[FS_BLOCK_SIZE];

    if (fp == NULL)
	perror("can't open image"), exit(1);
    
    struct cs5600fs_super super;
    fread(buf, sizeof(buf), 1, fp);
    memcpy(&super, buf, sizeof(super));

    printf("Superblock:\n");
    printf(" magic:                     %x\n", super.magic);
    printf(" block size:                %d\n", super.blk_size);
    printf(" file system size (blocks): %d\n", super.fs_size);
    printf(" FAT length (blocks):       %d\n", super.fat_len);
    printf(" Root directory start:      %d\n", super.root_dirent.start);

#define IS_FILE 1
#define IS_DIR 2
    struct cs5600fs_entry *fat = calloc(FS_BLOCK_SIZE, super.fat_len);
    char *map = calloc(super.fs_size, 1);
    map[super.root_dirent.start] = IS_DIR;
    
    fread(fat, FS_BLOCK_SIZE, super.fat_len, fp);

    int i, blk = super.root_dirent.start;
    printf("FAT:\n");
    for (i = 0; i < super.fs_size; i++) {
	if (fat[i].inUse)
	    printf("[%d]: %d %s\n", i, fat[i].next,
		   fat[i].eof ? "EOF" : "-");
    }
    printf("\n");
    
    while (!feof(fp) && blk < super.fs_size) {
	fread(buf, 1, FS_BLOCK_SIZE, fp);
	if (map[blk] != 0) {
	    if (map[blk] == IS_DIR) {
		struct cs5600fs_dirent *de = (void*)buf;
		printf("Block %d: directory\n", blk);
		for (i = 0; i < 16; i++)
		    if (de[i].valid) {
			printf("   [%d] %s %d %s %d\n", i, de[i].name,
			       de[i].start, de[i].isDir ? "dir " : "file",
			       de[i].length);
			map[de[i].start] = de[i].isDir ? IS_DIR : IS_FILE;
		    }
	    }
	    else {
		printf("Block %d: data\n", blk);
		for (i = 0; i < 16; i++)
		    if (!isprint(buf[i]))
			buf[i] = ' ';
		printf("   '%.16s'...\n", buf);
	    }
	    if (!fat[blk].eof)
		map[fat[blk].next] = map[blk];
	}
	blk++;
    }
    fclose(fp);
    return 0;
}
