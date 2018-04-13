//
// Created by Sarah Depew on 4/8/18.
//

#ifndef HW6_MAIN_H
#define HW6_MAIN_H
#include "boolean.h"

#define ERROR -1
#define SIZEOFBOOTBLOCK 512
#define SIZEOFSUPERBLOCK 512
#define MAX 1000000000 //Max file size is 1GB, extra credit not implemented, so only an error message prints out letting the user know the disk is large
#define N_DBLOCKS 10
#define N_IBLOCKS 4
#define PTRSIZE sizeof(int)
#define UNUSED 0
#define NUMINODES ((((superblockPtr->data_offset-superblockPtr->inode_offset) * size))/sizeof(inode))
#define DBLOCKS (N_DBLOCKS*superblockPtr->size)
#define IBLOCKS (DBLOCKS + N_IBLOCKS*(superblockPtr->size/PTRSIZE)*superblockPtr->size)
#define I2BLOCKS (IBLOCKS + (superblockPtr->size/PTRSIZE)*(superblockPtr->size/PTRSIZE)*superblockPtr->size)
#define I3BLOCKS (I2BLOCKS + (superblockPtr->size/PTRSIZE)*(superblockPtr->size/PTRSIZE)*(superblockPtr->size/PTRSIZE)*superblockPtr->size)

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

typedef struct block {
    int next;
} block;

/*
 * Methods Used
 */
void printDirections();
void printManPage();
int parseCmd(int argc, char *argv[]);
boolean defragment(char *inputFile);
long orderDBlocks(int numToWrite, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile);
long orderIBlocks(int numToWriteIBlock, int numToWriteData, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile);
long orderI2Blocks(int numToWriteI2Block, int numToWriteIBlock, int numToWriteData, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile);
long orderI3Blocks(int numToWriteI3Block, int numToWriteI2Block, int numToWriteIBlock, int numToWriteData, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile);
long offsetBytes(int blockSize, int offset);
void outputDFile(inode *fileToOutputOriginal, inode *fileToOutputNew, int size, void *dataRegionOld, void *dataRegionNew, char *oldOutputName, char *newOutputName);
void outputIFile(inode *fileToOutputOriginal, inode *fileToOutputNew, int size, void *dataRegionOld, void *dataRegionNew, char *oldOutputName, char *newOutputName);
void *getBlock(FILE *inputFile, long offsetValue, long blockSize);

/*
 * Debugging Methods
 */
void printInodes(inode *startInodeRegion, void *startOfDataRegion, int blockSize, int inodeOffset, int dataOffset);
void printDBlocks(int numToWrite, int *offsets, void *dataPtr, int size, FILE *outputFile);
void printIBlocks(int numToWriteIBlock, int numToWriteData, int *offsets, void *dataPtr, int size, FILE *outputFile);
void printDataBlocks(void *startDataRegion, int blockSize, int dataOffset, int swapOffset);

#endif //HW6_MAIN_H
