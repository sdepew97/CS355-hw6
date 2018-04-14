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
            if (defragment(argv[1]) != TRUE) {
                perror("An error occured while defragmenting the disk.\n");
            }
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
    char *readingFlag = "rb\0";
    char *writingFlag = "wb\0";
    char *defragExtension = "-defrag\0";

    char *inputFileName = strdup(inputFile);
    if(inputFileName == NULL) {
        perror("Strdup failed.\n");
        return FALSE;
    }

    char *outputFinalFileName = malloc(strlen(inputFile) + strlen(defragExtension) + 1);
    if(outputFinalFileName == NULL) {
        perror("Malloc failed.\n");
        return FALSE;
    }
    strcpy(outputFinalFileName, inputFile);
    strcat(outputFinalFileName, defragExtension);

    //File opening
    FILE *filePtr = fopen(inputFileName, readingFlag);
    FILE *outputPtr = fopen(outputFinalFileName, writingFlag);

    if (filePtr != NULL && outputPtr != NULL) {
        //Read original disk image into memory
        if(fseek(filePtr, 0L, SEEK_END) == ERROR) {
            perror("fseek failed.\n");
            return FALSE;
        }
        long inputFileSize = ftell(filePtr);
        if(inputFileSize == ERROR) {
            perror("File size error.\n");
            return FALSE;
        }
        if(inputFileSize > MAX) {
            //let the user know the file size is too large; I did not do the extra credit, so nothing beyond this is implemented
            printf("Please note the disk size being read into memory and operated on is larger than allowed.\n");
        }
        rewind(filePtr);

#ifdef DEBUG
        printf("Number bytes in input file: %ld\n", inputFileSize);
#endif
        void *allOfInputFile = malloc(inputFileSize);
        if (allOfInputFile == NULL) {
            perror("Malloc failed.\n");
            return FALSE;
        }

        if(fread(allOfInputFile, inputFileSize, 1, filePtr) < 1) {
            perror("Error reading in disk image.\n");
            return FALSE;
        }

        //Create pointers to important portions of disk file image in memory
        char *bootBlockPtr = allOfInputFile;
        superblock *superblockPtr = (((void *) allOfInputFile) + SIZEOFBOOTBLOCK);
        long size = superblockPtr->size;

        //Adjust the superblock value to reflect the proper value
        long dataBlockOffsetInFile = offsetBytes(size, superblockPtr->data_offset); //start of data region in file
        long bytesEndToDataRegion = inputFileSize-dataBlockOffsetInFile;
        long roomForBlocks = (long) floorf((float) ((float) bytesEndToDataRegion/ (float) size));

        //If the swap offset is greater than the end of the file
        if(superblockPtr->swap_offset > roomForBlocks) {
            superblockPtr->swap_offset = roomForBlocks + superblockPtr->data_offset;
        }

        long firstNodeOffsetInFile = offsetBytes(size,
                                                 superblockPtr->inode_offset); //location of where FIRST inode starts in input file

        long swapBlockOffsetInFile = offsetBytes(size, superblockPtr->swap_offset);

        inode *inodePtr = (((void *) allOfInputFile) + firstNodeOffsetInFile);
        void *dataBlockPtr = (((void *) allOfInputFile) + dataBlockOffsetInFile);

        //write bootblock, superblock, and inode blocks to file
//        fwrite(bootBlockPtr, SIZEOFBOOTBLOCK, 1, outputPtr);
//        fwrite(superblockPtr, SIZEOFSUPERBLOCK, 1, outputPtr);
//        fwrite(((void *) superblockPtr + SIZEOFSUPERBLOCK), (superblockPtr->inode_offset * size), 1, outputPtr); //write any gaps between inode region and superblock
//        fwrite(((void *) superblockPtr + SIZEOFSUPERBLOCK + superblockPtr->inode_offset * size), (superblockPtr->data_offset - superblockPtr->inode_offset) * size, 1, outputPtr);


#ifdef DEBUG
        //print inodes and data blocks prior to reorganization...
        printf("********Super Block Information********\n");
        printf("size of block %d\n", superblockPtr->size);
        printf("inode offset %d\n", superblockPtr->inode_offset);
        printf("data offset %d\n", superblockPtr->data_offset);
        printf("swap offset %d\n", superblockPtr->swap_offset);
        printf("head of inode list %d\n", superblockPtr->free_inode);
        printf("head of free list %d\n", superblockPtr->free_block);
        printInodes(inodePtr, dataBlockPtr, size, superblockPtr->inode_offset, superblockPtr->data_offset);
        printDataBlocks(dataBlockPtr, size, superblockPtr->data_offset, superblockPtr->swap_offset);
#endif

        long currentDataBlock = 0; //start the counter that keeps track of the data blocks that are being reorganized...
        inode *currentInode = inodePtr;

        //read all the inodes and reorganize their data blocks individually in the loop, below
        for (long i = 0; i < NUMINODES; i++) {
            //check if inode is free or not...if not free, then do data management and organization (otherwise, just output to output file...)
            if (currentInode->nlink == UNUSED) {
                //simply leave inode as is without any worries, since it will get re-copied at end and absorbed into the free list
            } else {
                //check how much nesting is used, here
                if (currentInode->size <= DBLOCKS) {
                    float divisionResult = (float) ((float) currentInode->size / (float) size);
                    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

                    currentDataBlock = orderDBlocks(numBlocks, currentDataBlock, currentInode->dblocks, dataBlockPtr,
                                                    size, outputPtr);
                } else if (currentInode->size > DBLOCKS && currentInode->size <= IBLOCKS) {
                    float divisionResult = (float) currentInode->size / (float) size;
                    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

                    currentDataBlock = orderDBlocks(N_DBLOCKS, currentDataBlock, currentInode->dblocks, dataBlockPtr,
                                                    size, outputPtr);
                    numBlocks -= N_DBLOCKS;

                    //calculate number of blocks total and number of indirect layers required to get those blocks...
                    divisionResult = (float) (((float) numBlocks) / ((float) (size) / (float) (sizeof(int))));
                    long numIndirect = ceilf(divisionResult);
#ifdef DEBUG
                    printf("numIndirect: %ld\n", numIndirect);
#endif
                    currentDataBlock = orderIBlocks(numIndirect, numBlocks, currentDataBlock, currentInode->iblocks,
                                                    dataBlockPtr, size, outputPtr);
                } else if (currentInode->size > IBLOCKS && currentInode->size <= I2BLOCKS) {
                    float divisionResult = (float) ((float) currentInode->size / (float) size);
                    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

                    currentDataBlock = orderDBlocks(N_DBLOCKS, currentDataBlock, currentInode->dblocks, dataBlockPtr,
                                                    size, outputPtr);
                    numBlocks -= N_DBLOCKS;

                    currentDataBlock = orderIBlocks(N_IBLOCKS, N_DBLOCKS, currentDataBlock, currentInode->iblocks,
                                                    dataBlockPtr,
                                                    size, outputPtr);

                    numBlocks -= N_IBLOCKS * (size / sizeof(int));

                    //calculate number of indirect blocks needed
                    divisionResult = (float) (((float) numBlocks) / ((float) (size) / (float) (sizeof(int))));
                    long numIndirect = ceilf(divisionResult);

#ifdef DEBUG
                    printf("numIndirect in I2 %ld\n", numIndirect);
#endif
                    currentDataBlock = orderI2Blocks(1, numIndirect, numBlocks, currentDataBlock,
                                                     &currentInode->i2block, dataBlockPtr, size, outputPtr);
                }
                else if (currentInode->size > I2BLOCKS && currentInode->size <= I3BLOCKS) {
                    float divisionResult = (float) ((float) currentInode->size / (float) size);
                    long numBlocks = ceilf(divisionResult); //number of blocks used, total (take ceiling)

                    currentDataBlock = orderDBlocks(N_DBLOCKS, currentDataBlock, currentInode->dblocks, dataBlockPtr,
                                                    size, outputPtr);
                    numBlocks -= N_DBLOCKS;

                    currentDataBlock = orderIBlocks(N_IBLOCKS, numBlocks, currentDataBlock, currentInode->iblocks,
                                                    dataBlockPtr,
                                                    size, outputPtr);

                    numBlocks -= N_IBLOCKS * (size / sizeof(int));

                    currentDataBlock = orderI2Blocks(1, (size / sizeof(int)), numBlocks, currentDataBlock,
                                                     &currentInode->i2block, dataBlockPtr, size, outputPtr);

                    numBlocks -= (size / sizeof(int)) * (size / sizeof(int));

                    //calculate number of indirect blocks needed
                    divisionResult = ((float) numBlocks) / (((float) (size) / (float) (sizeof(int))) *
                                                            ((float) (size) / (float) (sizeof(int))));
                    long num2Indirect = ceilf(divisionResult);

                    float otherDivisionResult = (float) ((float) numBlocks) / (((float) (size) / (float) (sizeof(int))));
                    long numIndirect = ceilf(otherDivisionResult);
#ifdef DEBUG
                    printf("num2Indirect in I3 %ld\n", num2Indirect);
                    printf("num blocks **** %ld\n", numBlocks);
#endif

                    currentDataBlock = orderI3Blocks(1, num2Indirect, numIndirect, numBlocks, currentDataBlock, &currentInode->i3block, dataBlockPtr, size, outputPtr);
                } else {
                    // last one is an error, since cannot use more than I3BLOCKS...
                    printf("File Size Too Large To Handle.\n");
                    return FALSE;
                }
            }

            //update currentInode's value
            currentInode = ((void *) currentInode) + sizeof(inode);
        }

#ifdef DEBUG
        printf("**** value of current Data Block %ld ****\n", currentDataBlock);
#endif
        //assemble the free list of inodes and write to file
        long valueToTransfer;
        superblockPtr->free_block = currentDataBlock;
        for(long i=currentDataBlock; i<(superblockPtr->swap_offset - superblockPtr->data_offset); i++) {
            valueToTransfer = i + 1;
            ((block *) (allOfInputFile + SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + ((superblockPtr->data_offset + i) *size)))->next = valueToTransfer;
        }
        ((block *) (allOfInputFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + ((superblockPtr->swap_offset - 1) *size)))->next = -1; //set value for last block

        //Write new superblock and inode region, as well as free list to file
//        fseek(outputPtr, SIZEOFBOOTBLOCK, SEEK_SET);
//        fwrite(superblockPtr, SIZEOFSUPERBLOCK, 1, outputPtr);
//        fseek(outputPtr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK, SEEK_SET);
//        fwrite(((void *) superblockPtr + SIZEOFSUPERBLOCK), (superblockPtr->inode_offset * size), 1, outputPtr);
//        fseek(outputPtr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + (superblockPtr->inode_offset * size), SEEK_SET);
//        fwrite(((void *) superblockPtr + SIZEOFSUPERBLOCK + superblockPtr->inode_offset * size), (superblockPtr->data_offset - superblockPtr->inode_offset) * size, 1, outputPtr);
//        fseek(outputPtr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + (superblockPtr->data_offset * size) + (currentDataBlock * size), SEEK_SET);
//        fwrite((dataBlockPtr + (currentDataBlock * size)), (superblockPtr->swap_offset - superblockPtr->data_offset - currentDataBlock) * size, 1, outputPtr);
//        //TODO: copy swap region! (if there is any...swap offset to end of file)

        fclose(filePtr);
        fclose(outputPtr);

#ifdef DEBUG
        //TESTING...
        free(allOfInputFile);
        filePtr = fopen(outputFinalFileName, readingFlag);

        //Read new disk image into memory
        fseek(filePtr, 0L, SEEK_END);
        inputFileSize = ftell(filePtr);
        rewind(filePtr);
        printf("Number bytes in output file: %ld\n", inputFileSize);
        allOfInputFile = malloc(inputFileSize);
        if (allOfInputFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfInputFile, inputFileSize, 1, filePtr);

        //read in and store the superblock, inode region pointer, and data region pointers
        superblockPtr = (((void *) allOfInputFile) + SIZEOFBOOTBLOCK);
        inodePtr = (((void *) allOfInputFile) + firstNodeOffsetInFile);
        dataBlockPtr = (((void *) allOfInputFile) + dataBlockOffsetInFile);

        //print inodes and data blocks prior to reorganization..., output swap region, and make free list, again
        printf("Final Print\n");
        printf("********Super Block Information********\n");
        printf("size of block %d\n", superblockPtr->size);
        printf("inode offset %d\n", superblockPtr->inode_offset);
        printf("data offset %d\n", superblockPtr->data_offset);
        printf("swap offset %d\n", superblockPtr->swap_offset);
        printf("head of inode list %d\n", superblockPtr->free_inode);
        printf("head of free list %d\n", superblockPtr->free_block);
        printf("value of currentDataBlock %ld\n", currentDataBlock);
        printInodes(inodePtr, dataBlockPtr, size, superblockPtr->inode_offset, superblockPtr->data_offset);
        printf("head of free list %d\n", superblockPtr->free_block);
        printDataBlocks(dataBlockPtr, size, superblockPtr->data_offset, superblockPtr->swap_offset);

        //open and read both files into memory for debugging purposes...
        fclose(filePtr);

        //ALSO TESTING...
        filePtr = fopen(inputFileName, readingFlag);
        outputPtr = fopen(outputFinalFileName, readingFlag);

        //File reading into memory
        fseek(filePtr, 0L, SEEK_END);
        long oldFileSize = ftell(filePtr);
        rewind(filePtr);
        printf("Number bytes in old file: %ld\n", oldFileSize);
        void *allOfOldFile = malloc(oldFileSize);
        if (allOfOldFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfOldFile, oldFileSize, 1, filePtr);

        fseek(outputPtr, 0L, SEEK_END);
        long newFileSize = ftell(outputPtr);
        rewind(outputPtr);
        printf("Number bytes in new file: %ld\n", newFileSize);
        void *allOfNewFile = malloc(newFileSize);
        if (allOfNewFile == NULL) {
            //malloc failed
            perror("Malloc failed.\n");
            return FALSE;
        }
        fread(allOfNewFile, newFileSize, 1, outputPtr);

        inode *oldInodePtr = allOfOldFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 17 * sizeof(inode);
        inode *newInodePtr = allOfNewFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 17 * sizeof(inode);
        void *dataBlockOld = allOfOldFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->data_offset * size;
        void *dataBlockNew = allOfNewFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->data_offset * size;
        outputDFile(oldInodePtr, newInodePtr, size, dataBlockOld, dataBlockNew, "old 17\0", "new 17\0");

        oldInodePtr = allOfOldFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 18 * sizeof(inode);
        newInodePtr = allOfNewFile + SIZEOFSUPERBLOCK + SIZEOFBOOTBLOCK + superblockPtr->inode_offset * size + 18 * sizeof(inode);
        outputIFile(oldInodePtr, newInodePtr, size, dataBlockOld, dataBlockNew, "old 18\0", "new 18\0");

        free(allOfOldFile);
        free(allOfNewFile);
        fclose(filePtr);
        fclose(outputPtr);
#endif
        //Free memory
        free(allOfInputFile);
        free(inputFileName);
        free(outputFinalFileName);

        return TRUE;
    } else {
        //File pointers were invalid
        return FALSE;
    }

    //default return value is false
    return FALSE;
}

/*
 * Method returns updated current node location with location of where to put next node! (-1 if failure)
 */
long orderDBlocks(int numToWrite, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    long nodeLocationValue = nodeLocation;

    for (int i = 0; i < numToWrite; i++) {
        void *dataBlock = (dataPtr + (offsets[i] * size));
        //dataBlock is now pointing to the data block to move (put in current node location and set array value accordingly)
        fwrite(dataBlock, size, 1, outputFile);
        offsets[i] = nodeLocationValue;
        nodeLocationValue++;
    }

    return nodeLocationValue;
}

/*
 * Method returns updated current node location with location of where to put next node!
 */
long orderIBlocks(int numToWriteIBlock, int numToWriteData, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    long nodeLocationValue = nodeLocation;
    long maxArray = size / sizeof(int);
    void *currentIBlock;

    int numToWrite = numToWriteData;

    for (int i = 0; i < numToWriteIBlock; i++) {
        //write out indirect block to file
        currentIBlock = (dataPtr + (offsets[i] * size));

        //write out data blocks to file
        if (numToWrite > maxArray) {
            nodeLocationValue = orderDBlocks(maxArray, nodeLocationValue, (int *) currentIBlock, dataPtr,
                                             size, outputFile);
            numToWrite -= maxArray;
        } else {
            nodeLocationValue = orderDBlocks(numToWrite, nodeLocationValue, (int *) currentIBlock, dataPtr,
                                             size, outputFile);
            numToWrite -= maxArray;
        }

        offsets[i] = nodeLocationValue;
//        fwrite(currentIBlock, size, 1, outputFile);
        nodeLocationValue++;
    }

    return nodeLocationValue;
}

/*
 * Method to order I2 blocks
 */
long orderI2Blocks(int numToWriteI2Block, int numToWriteIBlock, int numToWriteData, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    long nodeLocationValue = nodeLocation;
    long maxArray = size/sizeof(int);
    void *currentI2Block;

    int numToWrite = numToWriteData;
    int numToWriteIBlockValue = numToWriteIBlock;

    for (int i = 0; i < numToWriteI2Block; i++) {
        //write out indirect block to file
        currentI2Block = (dataPtr + (offsets[i] * size));

        //write out data blocks to file
        if(numToWriteIBlockValue > maxArray) {
            nodeLocationValue = orderIBlocks(maxArray, numToWrite, nodeLocationValue, (int *) currentI2Block, dataPtr, size, outputFile);

            numToWriteData -= maxArray*maxArray;
            numToWriteIBlockValue -= maxArray;
        } else {
            nodeLocationValue = orderIBlocks(numToWriteIBlockValue, numToWrite, nodeLocationValue, (int *) currentI2Block, dataPtr, size, outputFile);

            numToWriteData -= maxArray*maxArray;
            numToWriteIBlockValue -= maxArray;
        }

        offsets[i] = nodeLocationValue;
//        fwrite(currentI2Block, size, 1, outputFile);
        nodeLocationValue++;
    }

    return nodeLocationValue;
}

/*
 * Method to order I3 blocks
 */
long orderI3Blocks(int numToWriteI3Block, int numToWriteI2Block, int numToWriteIBlock, int numToWriteData, long nodeLocation, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    long nodeLocationValue = nodeLocation;
    long maxArray = size / sizeof(int);
    void *currentI3Block;

    int numToWrite = numToWriteData;
    int numToWriteIBlockValue = numToWriteIBlock;
    int numToWriteI2BlockValue = numToWriteI2Block;

    for (int i = 0; i < numToWriteI3Block; i++) {
        //write out indirect block to file
        currentI3Block = (dataPtr + (offsets[i] * size));

        //write out data blocks to file
        if (numToWriteI2BlockValue > maxArray) {
            nodeLocationValue = orderI2Blocks(maxArray, numToWriteIBlockValue, numToWrite, nodeLocationValue,
                                              (int *) currentI3Block,
                                              dataPtr,
                                              size, outputFile);

            numToWriteI2BlockValue -= maxArray;
            numToWriteIBlockValue -= maxArray * maxArray;
            numToWrite -= maxArray * maxArray * maxArray;
        } else {
            nodeLocationValue = orderI2Blocks(numToWriteI2Block, numToWriteIBlockValue, numToWrite, nodeLocationValue,
                                              (int *) currentI3Block,
                                              dataPtr,
                                              size, outputFile);

            numToWriteI2BlockValue -= maxArray;
            numToWriteIBlockValue -= maxArray * maxArray;
            numToWrite -= maxArray * maxArray * maxArray;
        }

        offsets[i] = nodeLocationValue;
//        fwrite(currentI3Block, size, 1, outputFile);
        nodeLocationValue++;

#ifdef DEBUG
        printf("node location value %ld\n", nodeLocationValue);
#endif
    }

    return nodeLocationValue;
}

long offsetBytes(int blockSize, int offset) {
    return (SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + blockSize * offset);
}

/*
 * Error checking methods, below
 */
void printInodes(inode *startInodeRegion, void *startOfDataRegion, int blockSize, int inodeOffset, int dataOffset){
    int numInodes = ((dataOffset-inodeOffset) * blockSize)/sizeof(inode);

    printf("Number of inodes: %d\n", numInodes);
    inode *currentInode = startInodeRegion;
    int *currentDataBlock;

    for(int i=0; i<numInodes; i++){
        printf("inode %d, next inode %d, nlink %d, size %d\n", i, currentInode->next_inode, currentInode->nlink, currentInode->size);
        printf("dblocks:\n");
        for(int j=0; j<N_DBLOCKS; j++) {
            printf("block: %d, contents: %d\n", j, currentInode->dblocks[j]);
        }
        for(int k=0; k<N_IBLOCKS; k++) {
            printf("iblock: %d, contents %d\n", k, currentInode->iblocks[k]);
            for(int l=0; l<(blockSize/sizeof(int)); l++) {
                currentDataBlock = (startOfDataRegion + currentInode->iblocks[k] * blockSize);
//                printf("\t index: %d\n", currentDataBlock[l]);
            }
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

void printDBlocks(int numToWrite, int *offsets, void *dataPtr, int size, FILE *outputFile) {
    for (int i = 0; i < numToWrite; i++) {
        void *dataBlock = (dataPtr + offsets[i] * size);
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
 * Error checking method for the datafile frag to reassemble files
 */
void outputDFile(inode *fileToOutputOriginal, inode *fileToOutputNew, int size, void *dataRegionOld, void *dataRegionNew, char *oldOutputName, char *newOutputName) {
    char *writingFlag = "wb";
    char *outputOldFileName = strdup(oldOutputName);
    char *outputNewFileName = strdup(newOutputName);

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
    char *writingFlag = "wb";
    char *outputOldFileName = strdup(oldOutputName);
    char *outputNewFileName = strdup(newOutputName);

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
