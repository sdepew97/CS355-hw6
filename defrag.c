#include <stdio.h>
#include <string.h>
#include <errno.h>
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
    //Naming information
    char *readingFlag = "rb+\0";
    char *writingFlag = "wb+\0";
    char *defragExtension = "-defrag\0";

    char *inputFileName = malloc(sizeof(char *)); //TODO: free at the end!
    strcpy(inputFileName, inputFile);

    char *outputFileName = malloc(sizeof(char *));
//    char *outputFileName = inputFile;
    strcpy(outputFileName, inputFile);
    strcat(outputFileName, defragExtension);
    printf("created output name of %s\n", outputFileName);
    printf("input file name of %s\n", inputFileName);

    FILE *filePtr = fopen(inputFileName, readingFlag);
    FILE *outputPtr = fopen(outputFileName, writingFlag);
    printf("File pointer values %p, %p\n", filePtr, outputPtr);

    if (filePtr != NULL && outputPtr != NULL) {
    //filePtr and outputPtr are valid file pointers

    //transfer over boot block, first
        void *bootBlockPtr = malloc(SIZEOFBOOTBLOCK);
        fread(bootBlockPtr, SIZEOFBOOTBLOCK, 1, filePtr);
        fwrite(bootBlockPtr, SIZEOFBOOTBLOCK, 1, outputPtr);
        free(bootBlockPtr);
    } else {
        return FALSE;
    }

    //TODO: close files!
    return TRUE;
}
