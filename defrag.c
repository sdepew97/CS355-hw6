#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "defrag.h"
#include "boolean.h"

/*
 * Main method
 */
int main(int argc, char *argv[]) {
    if (argc == 2) {
        int returnVal = parseCmd(argc, argv);

        if (returnVal == error) {
            printDirections();
        } else if (returnVal == help) {
            printManPage();
        } else {
            //TODO: check if de-fragmentation succeeded!
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
 * Method to parse the input command from the user //TODO: add error checking, here
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
    char *inputFileName = strdup(inputFile); //TODO: free memory at the end!!!
    char *outputFileName = "NewDataBlocks\0";
    char *outputFinalFileName = malloc(strlen(inputFile) + strlen(defragExtension) + 1);
    strcpy(outputFinalFileName, inputFile);
    strcat(outputFinalFileName, defragExtension);

    //File opening
    FILE *filePtr = fopen(inputFileName, readingFlag);
    FILE *outputPtr = fopen(outputFileName, writingFlag);
    FILE *finalOutputPtr = fopen(outputFinalFileName, writingFlag);

    if (filePtr != NULL && outputPtr != NULL && finalOutputPtr != NULL) {
        //Read original disk image into memory //TODO: add a max here
        fseek(filePtr, 0L, SEEK_END);
        long inputFileSize = ftell(filePtr);
        rewind(filePtr);
        printf("Number bytes in input file: %ld\n", inputFileSize);
        void *allOfInputFile = malloc(inputFileSize); //TODO: free this at the end!
        if (allOfInputFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }

        fread(allOfInputFile, inputFileSize, 1, filePtr); //TODO: recognize here if read was over maximum allowed size!

        //transfer over boot block, first to middle file
        char *bootBlockPtr = allOfInputFile;

        //read in and store the superblock, inode region pointer, and data region pointers
        superblock *superblockPtr = (((void *) allOfInputFile) + SIZEOFBOOTBLOCK);

        //set some values based on superblock that will be useful
        int size = superblockPtr->size;
        long firstNodeOffsetInFile = offsetBytes(size,
                                                 superblockPtr->inode_offset); //location of where FIRST inode starts in input file
        long dataBlockOffsetInFile = offsetBytes(size, superblockPtr->data_offset); //start of data region in file
        long swapBlockOffsetInFile = offsetBytes(size, superblockPtr->swap_offset);

        inode *inodePtr = (((void *) allOfInputFile) + firstNodeOffsetInFile);
        void *dataBlockPtr = (((void *) allOfInputFile) + dataBlockOffsetInFile);

        //print inodes and data blocks prior to reorganization...
        //TODO: remove debugging information at end!
        printf("head of inode list %d\n", superblockPtr->free_inode);
//        printInodes(inodePtr, size, superblockPtr->inode_offset, superblockPtr->data_offset);
        printf("head of free list %d\n", superblockPtr->free_block);
//        printDataBlocks(dataBlockPtr, size, superblockPtr->data_offset, superblockPtr->swap_offset);

        long currentDataBlock = 0; //start the counter that keeps track of the data blocks that are being reorganized...
        inode *currentInode = inodePtr;

        //read all the inodes and reorganize their data blocks individually!
        for (int i = 0; i < NUMINODES; i++) {
            //check if inode is free or not...if not free, then do data management and organization (otherwise, just output to output file...)
            if (currentInode->nlink == UNUSED) {
                //simply leave inode as is without any worries, since it will get re-copied at end
            } else {
                //check how much nesting is used, here
                if (currentInode->size <= DBLOCKS) {
                    float divisionResult = (float) currentInode->size / (float) size;
                    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

                    currentDataBlock = orderDBlocks(numBlocks, currentDataBlock, currentInode->dblocks, dataBlockPtr,
                                                    size, outputPtr);
                } else if (currentInode->size > DBLOCKS && currentInode->size <= IBLOCKS) {
                    float divisionResult = (float) currentInode->size / (float) size;
                    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

                    currentDataBlock = orderDBlocks(N_DBLOCKS, currentDataBlock, currentInode->dblocks, dataBlockPtr,
                                                    size, outputPtr);
                    numBlocks -= 10;

                    //calculate number of blocks total and number of indirect layers required to get those blocks...
                    divisionResult = ((float) numBlocks) / ((float) (size) / (float) (sizeof(int)));
                    long numIndirect = ceilf(divisionResult);
                    printf("numIndirect: %ld\n", numIndirect);
                    currentDataBlock = orderIBlocks(numIndirect, numBlocks, currentDataBlock, currentInode->iblocks,
                                                    dataBlockPtr, size, outputPtr);
                } //TODO: check if this works!
                else if (currentInode->size > IBLOCKS && currentInode->size <= I2BLOCKS) {
//                    currentDataBlock = orderDBlocks(currentDataBlock, &inodePtr, dataBlockOffsetInFile, size, filePtr, outputPtr);
                    //TODO: implement this one
                } else if (currentInode->size > I2BLOCKS && currentInode->size <= I3BLOCKS) {
//                    currentDataBlock = orderDBlocks(currentDataBlock, &inodePtr, dataBlockOffsetInFile, size, filePtr, outputPtr);
                    //TODO: implement this one
                } else { //last one is an error, since cannot use more than I3BLOCKS...

                }
            }

            //update currentInode's value
            currentInode = ((void *) currentInode) + sizeof(inode);
        }

        //TODO: tie together free inodes list!!!!

        fclose(outputPtr);

        outputPtr = fopen(outputFileName, readingFlag);
        //read all the new data blocks into memory
        fseek(outputPtr, 0L, SEEK_END);
        long outputFileSize = ftell(outputPtr);
        rewind(outputPtr);
        printf("Number bytes in output (middle) file: %ld\n", outputFileSize);
        void *allOfDataRegion = malloc(outputFileSize); //TODO: free this at the end!
        if (allOfDataRegion == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfDataRegion, outputFileSize, 1, outputPtr); //TODO: recognize here if read was over maximum allowed size! (have second read, but if it works, then make a note)

        int valueToTransfer;

        //assemble the free list of inodes and write to file
        superblockPtr->free_inode = currentDataBlock;
        for(int i=currentDataBlock; i<swapBlockOffsetInFile; i++) {
            valueToTransfer = i + 1;
            ((block *) (allOfInputFile + ((superblockPtr->data_offset + i) *size)))->next = valueToTransfer;
        }
        ((block *) (allOfInputFile + ((superblockPtr->data_offset + swapBlockOffsetInFile -1) *size)))->next = -1; //set value for last block

        //Write new inode region! and write the entire file!
        fwrite(bootBlockPtr, SIZEOFBOOTBLOCK, 1, finalOutputPtr);
        fwrite(superblockPtr, SIZEOFSUPERBLOCK, 1, finalOutputPtr);
        fwrite((((void *) superblockPtr) + SIZEOFSUPERBLOCK), superblockPtr->data_offset * size, 1,
               finalOutputPtr);
        fwrite(allOfDataRegion, outputFileSize, 1, finalOutputPtr);
        fwrite((((void *) superblockPtr) + SIZEOFSUPERBLOCK), superblockPtr->data_offset * size, 1,
               finalOutputPtr);
        //TODO: add swap region and add free blocks, here

        //TODO: close files once done! and remove error checking, here! REMOVE MIDDLE FILE!
        fclose(filePtr);
        fclose(outputPtr);
        fclose(finalOutputPtr);

        free(allOfInputFile);
        free(allOfDataRegion);

        //TODO: free here!
        filePtr = fopen(outputFinalFileName, readingFlag);

        //Read original disk image into memory //TODO: add a max here
        fseek(filePtr, 0L, SEEK_END);
        inputFileSize = ftell(filePtr);
        rewind(filePtr);
        printf("Number bytes in file: %ld\n", inputFileSize);
        allOfInputFile = malloc(inputFileSize); //TODO: free this at the end!
        if (allOfInputFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfInputFile, inputFileSize, 1, filePtr); //TODO: recognize here if read was over maximum allowed size!

        //read in and store the superblock, inode region pointer, and data region pointers
        superblockPtr = (((void *) allOfInputFile) + SIZEOFBOOTBLOCK);
        inodePtr = (((void *) allOfInputFile) + firstNodeOffsetInFile);
        dataBlockPtr = (((void *) allOfInputFile) + dataBlockOffsetInFile);

        //print inodes and data blocks prior to reorganization..., output swap region, and make free list, again
        //TODO: build free list, here with currentDataBlock as the head of the list!
        printf("Final Print\n");
        printf("head of inode list %d\n", superblockPtr->free_inode);
//        printInodes(inodePtr, size, superblockPtr->inode_offset, superblockPtr->data_offset);
        printf("head of free list %d\n", superblockPtr->free_block);
//        printDataBlocks(dataBlockPtr, size, superblockPtr->data_offset, superblockPtr->swap_offset);
//TODO: not going to work until blocks are output to file correctly!!!

        //TODO: remove at the end
        //open and read both files into memory for debugging purposes...
        fclose(filePtr);
        filePtr = fopen(inputFileName, readingFlag);
        outputPtr = fopen(outputFinalFileName, readingFlag);

        //File reading into memory
        fseek(filePtr, 0L, SEEK_END);
        long oldFileSize = ftell(filePtr);
        rewind(filePtr);
        printf("Number bytes in old file: %ld\n", oldFileSize);
        void *allOfOldFile = malloc(oldFileSize); //TODO: free this at the end!
        if (allOfOldFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfOldFile, oldFileSize, 1, filePtr); //TODO: recognize here if read was over maximum allowed size!

        fseek(outputPtr, 0L, SEEK_END);
        long newFileSize = ftell(outputPtr);
        rewind(outputPtr);
        printf("Number bytes in new file: %ld\n", newFileSize);
        void *allOfNewFile = malloc(newFileSize); //TODO: free this at the end!
        if (allOfNewFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfNewFile, newFileSize, 1, outputPtr); //TODO: recognize here if read was over maximum allowed size!

        inode *oldInodePtr = allOfOldFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 3 * sizeof(inode);
        inode *newInodePtr = allOfNewFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 3 * sizeof(inode);
        void *dataBlockOld = allOfOldFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->data_offset * size;
        void *dataBlockNew = allOfNewFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->data_offset * size;

        //TODO: also see if works with pointer from new data file
        outputDFile(oldInodePtr, newInodePtr, size, dataBlockOld, dataBlockNew, "old 3\0", "new 3\0");

        oldInodePtr = allOfOldFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 0 * sizeof(inode);
        newInodePtr = allOfNewFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 0 * sizeof(inode);

        outputIFile(oldInodePtr, newInodePtr, size, dataBlockOld, dataBlockNew, "old 0\0", "new 0\0");

        //TODO: finish freeing memory
        free(allOfInputFile);
        free(allOfOldFile);
        free(allOfNewFile);
        free(inputFileName);
        free(outputFinalFileName);

        fclose(filePtr);
        fclose(outputPtr);

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
long orderDBlocks(int numToWrite, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    long nodeLocationValue = nodeLocation;

    for (int i = 0; i < numToWrite; i++) {
        void *dataBlock = (dataPtr + offsets[i] * size); //TODO: check computation
        //dataBlock is now pointing to the data block to move (put in current node location and set array value accordingly)
        fwrite(dataBlock, size, 1, outputFile);
        offsets[i] = nodeLocationValue;
        nodeLocationValue++;
    }

    return nodeLocationValue;
}

//TODO: check today in office hours this is correct! (no, got better idea)
/*
 * Method returns updated current node location with location of where to put next node! (-1 if failure)
 */
long orderIBlocks(int numToWriteIBlock, int numToWriteData, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    long nodeLocationValue = nodeLocation;
    long maxArray = size / sizeof(int);
    void *currentIBlockOffsetsValue;

    int numToWrite = numToWriteData;

    for (int i = 0; i < numToWriteIBlock; i++) {
        //write out indirect block to file
        currentIBlockOffsetsValue = (dataPtr + (offsets[i] * size));
        offsets[i] = nodeLocationValue;
        fwrite(currentIBlockOffsetsValue, size, 1, outputFile);
        nodeLocationValue++; //TODO: did this fix it??

        //write out data blocks to file
        if (numToWrite > maxArray) {
            nodeLocationValue = orderDBlocks(maxArray, nodeLocationValue, (int *) currentIBlockOffsetsValue, dataPtr,
                                             size, outputFile);
            numToWrite -= maxArray;
        } else {
            nodeLocationValue = orderDBlocks(numToWrite, nodeLocationValue, (int *) currentIBlockOffsetsValue, dataPtr,
                                             size, outputFile);
        }
    }

    return nodeLocationValue;
}

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

//Error checking methods
void printDBlocks(int numToWrite, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    for (int i = 0; i < numToWrite; i++) {
        void *dataBlock = (dataPtr + offsets[i] * size); //TODO: check computation
        //dataBlock is now pointing to the data block to move (put in current node location and set array value accordingly)
        fwrite(dataBlock, size, 1, outputFile);
    }
}

void printIBlocks(int numToWriteIBlock, int numToWriteData, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    long maxArray = size / sizeof(int);
    void *currentIBlockOffsetsValue;
    int numToWrite = numToWriteData;

    for (int i = 0; i < numToWriteIBlock; i++) {
        //write out indirect block to file
        currentIBlockOffsetsValue = (dataPtr + (offsets[i] * size));

        //write out data blocks to file
        if (numToWrite > maxArray) {
            printDBlocks(maxArray, (int *) currentIBlockOffsetsValue, dataPtr,
                         size, outputFile);
            numToWrite -= maxArray;
        } else {
            printDBlocks(numToWrite, (int *) currentIBlockOffsetsValue, dataPtr,
                         size, outputFile);
        }
    }
}

/*
 * Error checking method //TODO: write to allow for indirect files!
 */
void outputDFile(inode *fileToOutputOriginal, inode *fileToOutputNew, int size, void *dataRegionOld, void *dataRegionNew, char *oldOutputName, char *newOutputName) {
    char *writingFlag = "wb+\0";
    char *outputOldFileName = strdup(oldOutputName); //TODO: free at the end!!!
    char *outputNewFileName = strdup(newOutputName); //TODO: free at the end!!!

    //File opening
    FILE *oldOutput = fopen(outputOldFileName, writingFlag);
    FILE *newOutput = fopen(outputNewFileName, writingFlag);

    //read and output old file's data blocks
    float divisionResult = (float) fileToOutputOriginal->size / (float) size;
    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)
    printDBlocks(numBlocks, fileToOutputOriginal->dblocks, dataRegionOld, size, oldOutput);

    //read and output new file's data blocks

    divisionResult = (float) fileToOutputNew->size / (float) size;
    numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

    printDBlocks(numBlocks, fileToOutputNew->dblocks, dataRegionNew, size, newOutput);

    free(outputOldFileName);
    free(outputNewFileName);
    fclose(oldOutput);
    fclose(newOutput);
}

void outputIFile(inode *fileToOutputOriginal, inode *fileToOutputNew, int size, void *dataRegionOld, void *dataRegionNew, char *oldOutputName, char *newOutputName) {
    char *writingFlag = "wb+\0";
    char *outputOldFileName = strdup(oldOutputName); //TODO: free at the end!!!
    char *outputNewFileName = strdup(newOutputName); //TODO: free at the end!!!

    //File opening
    FILE *oldOutput = fopen(outputOldFileName, writingFlag);
    FILE *newOutput = fopen(outputNewFileName, writingFlag);

    //read and output old file's data blocks
    float divisionResult = (float) fileToOutputOriginal->size / (float) size;
    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)
    printDBlocks(N_DBLOCKS, fileToOutputOriginal->dblocks, dataRegionOld, size, oldOutput);
    numBlocks -= 10;

    //calculate number of blocks total and number of indirect layers required to get those blocks...
    divisionResult = ((float) numBlocks) / ((float) (size) / (float) (sizeof(int)));
    long numIndirect = ceilf(divisionResult);
    printIBlocks(numIndirect, numBlocks, fileToOutputOriginal->iblocks, dataRegionOld, size, oldOutput);

    //read and output new file's data blocks

    divisionResult = (float) fileToOutputNew->size / (float) size;
    numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)
    printDBlocks(N_DBLOCKS, fileToOutputNew->dblocks, dataRegionNew, size, newOutput);
    numBlocks -= 10;

    //calculate number of blocks total and number of indirect layers required to get those blocks...
    divisionResult = ((float) numBlocks) / ((float) (size) / (float) (sizeof(int)));
    numIndirect = ceilf(divisionResult);
    printIBlocks(numIndirect, numBlocks, fileToOutputNew->iblocks, dataRegionNew, size, newOutput);

    free(outputOldFileName);
    free(outputNewFileName);
    fclose(oldOutput);
    fclose(newOutput);
}


/*
 *   float divisionResult = (float) currentInode->size / (float) size;
                    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

                    currentDataBlock = orderDBlocks(N_DBLOCKS, currentDataBlock, currentInode->dblocks, dataBlockPtr,
                                                    size, outputPtr);
                    numBlocks -= 10;

                    //calculate number of blocks total and number of indirect layers required to get those blocks...
                    divisionResult = ((float) numBlocks) / ((float) (size) / (float) (sizeof(int)));
                    long numIndirect = ceilf(divisionResult);
                    printf("numIndirect: %ld\n", numIndirect);
                    currentDataBlock = orderIBlocks(numIndirect, numBlocks, currentDataBlock, currentInode->iblocks,
                                                    dataBlockPtr, size, outputPtr);
 */

/*
 *   outputPtr = fopen(outputFileName, readingFlag);
        //TODO: clean this up! (A LOT!!!)
        char *outputMiddleFileName = "Middle\0"; //TODO: free at the end!!!
        void *blockToOutput;

        //File opening
        FILE *middleOutput = fopen(outputMiddleFileName, writingFlag);
        //File reading into memory
        fseek(outputPtr, 0L, SEEK_END);
        long middleFileSize = ftell(outputPtr);
        rewind(outputPtr);
        printf("Number bytes in file output: %ld\n", middleFileSize);
        void *allOfMiddleFile = malloc(middleFileSize); //TODO: free this at the end!
        if (allOfMiddleFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }

        fread(allOfMiddleFile, middleFileSize, 1, outputPtr); //TODO: recognize here if read was over maximum allowed size!

        blockToOutput = allOfMiddleFile + SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + ((superblockPtr->data_offset - superblockPtr->inode_offset) * size) + 623 * size;
        fwrite(blockToOutput, size, 1, middleOutput);

        fclose(outputPtr);


        //open and read both files into memory for debugging purposes...
        filePtr = fopen(inputFileName, readingFlag);
        outputPtr = fopen(outputFinalFileName, readingFlag);

        //File reading into memory
        fseek(filePtr, 0L, SEEK_END);
        long oldFileSize = ftell(filePtr);
        rewind(filePtr);
        printf("Number bytes in file: %ld\n", oldFileSize);
        void *allOfOldFile = malloc(oldFileSize); //TODO: free this at the end!
        if (allOfOldFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfOldFile, oldFileSize, 1, filePtr); //TODO: recognize here if read was over maximum allowed size!

        fseek(outputPtr, 0L, SEEK_END);
        long newFileSize = ftell(outputPtr);
        rewind(outputPtr);
        printf("Number bytes in file: %ld\n", newFileSize);
        void *allOfNewFile = malloc(newFileSize); //TODO: free this at the end!
        if (allOfNewFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfNewFile, newFileSize, 1, outputPtr); //TODO: recognize here if read was over maximum allowed size!

        inode *oldInodePtr = allOfOldFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset + 3 * sizeof(inode);
        inode *newInodePtr = allOfNewFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset + 3 * sizeof(inode);
        newDataRegion = allOfNewFile + offsetBytes(size, superblockPtr->data_offset);

        outputFile(oldInodePtr, newInodePtr, size, dataBlockPtr, newDataRegion, "old 3\0", "new 3\0");
 */