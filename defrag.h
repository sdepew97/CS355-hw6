//
// Created by Sarah Depew on 4/8/18.
//

#ifndef HW6_MAIN_H
#define HW6_MAIN_H
#include "boolean.h"

#define SIZEOFBOOTBLOCK 512
#define N_DBLOCKS 10
#define N_IBLOCKS 4
#define PTRSIZE 4
#define UNUSED 0
#define NUMINODES ((((superblockPtr->data_offset-superblockPtr->inode_offset) * size))/sizeof(inode)) //TODO: check with Dianna tomorrow in lab
#define DBLOCKS (N_DBLOCKS*superblockPtr->size)
#define IBLOCKS (DBLOCKS + N_IBLOCKS*(superblockPtr->size/PTRSIZE)*superblockPtr->size)
#define I2BLOCKS (IBLOCKS + N_IBLOCKS*(superblockPtr->size/PTRSIZE)*N_IBLOCKS*(superblockPtr->size/PTRSIZE)*superblockPtr->size)
#define I3BLOCKS (I2BLOCKS + N_IBLOCKS*(superblockPtr->size/PTRSIZE)*N_IBLOCKS*(superblockPtr->size/PTRSIZE)*N_IBLOCKS*(superblockPtr->size/PTRSIZE)*superblockPtr->size)

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
long orderDBlocks(long currentNodeLocation, inode *inodePtr, long dataOffsetLocationBytes, int size, FILE *outputFile);
long offsetBytes(int blockSize, int offset);

#endif //HW6_MAIN_H
