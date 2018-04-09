//
// Created by Sarah Depew on 4/8/18.
//

#ifndef HW6_MAIN_H
#define HW6_MAIN_H
#include "boolean.h"

#define SIZEOFBOOTBLOCK 512
#define N_DBLOCKS 10
#define N_IBLOCKS 4

//enums
enum {help = 0, defrag = 1, error = 2};

//structs
typedef struct superblock {
    int size;
    int inode_offset;
    int data_offset;
    int swap_offset;
    int free_inode;
    int free_block;
} superblock;

typedef struct inode {
    int next_inode;
    int protect;
    int nlink;
    int size;
    int uid;
    int gid;
    int ctime;
    int mtime;
    int atime;
    int dblocks[N_DBLOCKS];
    int iblocks[N_IBLOCKS];
    int i2block;
    int i3block;
} inode;

void printDirections();
void printManPage();
int parseCmd(int argc, char *argv[]);
boolean defragment(char *inputFile);
long inodeOffsetBytes(int blockSize, int offset);

#endif //HW6_MAIN_H
