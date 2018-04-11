#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include "defrag.h"
#include "boolean.h"

/*
 * Main method
 */
int main(int argc, char *argv[]) {
    if (argc == 2) {
        //TODO: parse command here to ensure the input is valid (maybe the file will not be able to open?)
        int returnVal = parseCmd(argc, argv);

        if (returnVal == error) {
            printDirections();
        } else if (returnVal == help) {
            printManPage();
        } else {
            //TODO: implement de-fragmenting the file....
            printf("secondArgument: %s\n", argv[1]);
            //TODO: check if defrag succeeded
            defragment(argv[1]);
        }
    } else {
        //if there are fewer than or more than two arguments to the command line, an error message with how to run the program should print
        printDirections();
    }

    return 0;
}

/*
 * Print the directions of how to use the program when just "./defrag" is input
 */
void printDirections() {
    printf("Run the program with ./defrag <fragmented disk file>.\n");
}

/*
 * Method for printing the man page
 */
void printManPage() {
    printf("NAME\n");
    printf("\tdefrag -- defragment a Unix-like file system\n");
    printf("SYNOPSIS\n");
    printf("\tdefrag <fragmented disk file\n");
    printf("DESCRIPTION\n");
    printf("\tThe defragmenter should be invoked as follows:\n\t./defrag <fragmented disk file>.\n");
    printf("\tThe defragmenter outputs a new disk image with \"-defrag\" concatenated to the end of the input file name.\n");
    printf("\tFor instance, ./defrag myfile should produce the output file \"myfile-defrag\"\n");
}

/*
 * Method to parse the input command from the user
 */
int parseCmd(int argc, char *argv[]) {
    //input flags
    char *helpFlag = "-h\0";

    if (argc != 2) {
        return error;
    } else {
        if (strcmp(argv[1], helpFlag) == 0) {
            return help;
        } else {
            //assume the input is a file name
            return defrag;
        }
    }
}

boolean defragment(char *inputFile) {
    //File naming setup
    char *readingFlag = "rb+\0";
    char *writingFlag = "wb+\0";
    char *defragExtension = "-defrag\0";
    char *inputFileName = strdup(inputFile); //TODO: free at the end!!!
    char *outputFileName = malloc(strlen(inputFile) + strlen(defragExtension) + 1);
    strcpy(outputFileName, inputFile);
    strcat(outputFileName, defragExtension);

    //File opening
    FILE *filePtr = fopen(inputFileName, readingFlag);
    FILE *outputPtr = fopen(outputFileName, writingFlag);

    if (filePtr != NULL && outputPtr != NULL) {
        //filePtr and outputPtr are valid file pointers

        //File reading into memory
        fseek(filePtr, 0L, SEEK_END);
        long inputFileSize = ftell(filePtr);
        rewind(filePtr);
        printf("Number bytes in file: %ld\n", inputFileSize);
        void *allOfInputFile = malloc(inputFileSize); //TODO: free this at the end!
        if (allOfInputFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }

        fread(allOfInputFile, inputFileSize, 1, filePtr); //TODO: recognize here if read was over maximum allowed size!

        //transfer over boot block, first
        char *bootBlockPtr = allOfInputFile;
        fwrite(bootBlockPtr, SIZEOFBOOTBLOCK, 1, outputPtr);

        //read in and store the superblock, inode region pointer, and data region pointers
        superblock *superblockPtr = (superblock *) (((void *) allOfInputFile) + SIZEOFBOOTBLOCK);

        //set some values based on superblock that will be useful
        int size = superblockPtr->size;
        //TODO: get offset of inode region and data region based on superblock values
        long firstNodeOffsetInFile = offsetBytes(size, superblockPtr->inode_offset); //location of where FIRST inode starts in input file
        long dataBlockOffsetInFile = offsetBytes(size, superblockPtr->data_offset); //start of data region in file
        long swapBlockOffsetInFile = offsetBytes(size, superblockPtr->swap_offset);
        printf("Size: %d\n", size);

        inode *inodePtr = (inode *) (((void *) allOfInputFile) + firstNodeOffsetInFile);
        void *dataBlockPtr = (((void *) allOfInputFile) + dataBlockOffsetInFile);

        //print inodes and data blocks prior to reorganization...
        printf("head of inode list %d\n", superblockPtr->free_inode);
        printInodes(inodePtr, size, superblockPtr->inode_offset, superblockPtr->data_offset);
        printf("head of free list %d\n", superblockPtr->free_block);
        printDataBlocks(dataBlockPtr, size, superblockPtr->data_offset, superblockPtr->swap_offset);

        //write original superblock and inodes to output file, so that reorganization can occur of data blocks
        fwrite(superblockPtr, SIZEOFSUPERBLOCK, 1, outputPtr);
        fwrite((((void *) superblockPtr) + SIZEOFSUPERBLOCK), superblockPtr->data_offset * size, 1, outputPtr); //TODO: check this is the region I want to be writing...??

        long currentDataBlock = 0; //start the counter that keeps track of the data blocks that are being reorganized...
        inode *currentInode = inodePtr;

        //read all the inodes and reorganize the data blocks
        for(int i=0; i<NUMINODES; i++) {
            //check if inode is free or not...if not free, then do data management and organization (otherwise, just output to output file...)
            if (currentInode->nlink == UNUSED) {
                //simply leave inode as is without any worries, since it will get re-copied at end
            } else {
                //TODO: flesh out with proper writing of data blocks to output file and adjusting the boot blocks values!
                //check how much nesting is used, here
                if (currentInode->size <= DBLOCKS) {
                    //have to put these blocks into order...
                    printf("got here\n");
//                    currentDataBlock = orderDBlocks(currentDataBlock, &inodePtr, dataBlockOffsetInFile, size, filePtr, outputPtr);
                } else if (currentInode->size > DBLOCKS && currentInode->size <= IBLOCKS) {
//                    currentDataBlock = orderDBlocks(currentDataBlock, &inodePtr, dataBlockOffsetInFile, size, filePtr, outputPtr);

                } else if (currentInode->size > IBLOCKS && currentInode->size <= I2BLOCKS) {
//                    currentDataBlock = orderDBlocks(currentDataBlock, &inodePtr, dataBlockOffsetInFile, size, filePtr, outputPtr);

                } else if (currentInode->size > I2BLOCKS && currentInode->size <= I3BLOCKS) {
//                    currentDataBlock = orderDBlocks(currentDataBlock, &inodePtr, dataBlockOffsetInFile, size, filePtr, outputPtr);

                } else { //last one is an error, since cannot use more than I3BLOCKS...

                }
            }

            //update currentInode's value
            currentInode = ((void *) currentInode) + sizeof(inode);
        }

        //TODO: rewrite inode and super block regions to file now that they're modified correctly! (the image being held in memory is correct for superblock-inode region)
//        //write the superblock to the output file
//        fwrite(superblockPtr, SIZEOFBOOTBLOCK, 1, outputPtr);
//        //TODO: free all pointers as needed!!
//        free(superblockPtr); //TODO: put where best!!
//
        return TRUE; //TODO: remove once not debugging...
    } else {
        return FALSE;
    }

    //TODO: close files! and free pointers from beginning
    return TRUE;
}

/*
 * Method returns updated current node location with location of where to put next node! (-1 if failure)
 */
long orderDBlocks(long currentNodeLocation, inode **inodePtr, long dataOffsetLocationBytes, int size, FILE *inputFile, FILE *outputFile) {
    //put the DBlocks in order
    int returnFSeek;
    long updatedCurrentNodeLocation = currentNodeLocation;
    float numBlocks = (*inodePtr)->size /
                      size; //number of blocks used, total //ceiling TODO: see if this value could end up being fractional? (yes, so how many actually used??)

    //TODO: ask dianna about if fractions of blocks could end up being used and if so, do you round up to the nearest block value?
    //all of array is filled
    if (numBlocks >= N_DBLOCKS) {
        for (int i = 0; i < N_DBLOCKS; i++) {
            void *dataBlock = getBlock(inputFile, (dataOffsetLocationBytes + size * (*inodePtr)->dblocks[i]),
                                       size); //TODO: check computations (assuming ints are simply indices into data region?)
            //dataBlock is now pointing to the data block to move (put in current node location and set array value accordingly)
            returnFSeek = fseek(outputFile, dataOffsetLocationBytes + size * updatedCurrentNodeLocation,
                                SEEK_SET); //TODO: error check here
            fwrite(dataBlock, size, 1, outputFile);
            (*inodePtr)->dblocks[i] = updatedCurrentNodeLocation;
            updatedCurrentNodeLocation++;

        }
    } else {
        //all of array is NOT filled, so only do things to blocks that are used
        //TODO: Ask Dianna about this case with rounding etc...
    }

    return updatedCurrentNodeLocation;
}

void *getBlock(FILE *inputFile, long offsetValue, long blockSize) {
    void *dataBlock = malloc(blockSize);
    //position file pointer, first...
    int returnFSeek = fseek(inputFile, offsetValue, SEEK_SET);
    fread(dataBlock, blockSize, 1, inputFile);
    return dataBlock;
}

//TODO: write "seek block" method to return the location of the block as a ptr? or the offset? not sure which

long offsetBytes(int blockSize, int offset) {
    return (SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + blockSize * offset);
}

void printInodes(inode *startInodeRegion, int blockSize, int inodeOffset, int dataOffset){
    int numInodes = ((dataOffset-inodeOffset) * blockSize)/sizeof(inode);
    printf("number inodes: %d\n", numInodes);
    inode *currentInode = startInodeRegion;

    for(int i=0; i<numInodes; i++){
        printf("inode %d, next inode %d, nlink %d, size %d\n", i, currentInode->next_inode, currentInode->nlink, currentInode->size);
        printf("dblocks:\n");
        for(int j=0; j<N_DBLOCKS; j++) {
            printf("block: %d, contents: %d\n", j, currentInode->dblocks[j]);
        }
        for(int k=0; k<N_IBLOCKS; k++) {
            printf("iblock: %d, contents %d\n", k, currentInode->iblocks[k]);
        }
        printf("i2block %d\n", currentInode->i2block);
        printf("i3block %d\n", currentInode->i3block);
        currentInode = (((void *) currentInode) + sizeof(inode));
    }
}

void printDataBlocks(void *startDataRegion, int blockSize, int dataOffset, int swapOffset) {
    int numBlocks = swapOffset - dataOffset;
    void *currentBlock = startDataRegion;

    for(int i=0; i<numBlocks; i++) {
        printf("Block Index: %d, Block Value: %d\n", i, ((block *) currentBlock)->next);
        currentBlock = (currentBlock + blockSize);
    }
}