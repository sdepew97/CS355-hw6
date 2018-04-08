//
// Created by Sarah Depew on 4/8/18.
//

#ifndef HW6_MAIN_H
#define HW6_MAIN_H
#include "boolean.h"

enum {help = 0, defrag = 1, error = 2};

void printDirections();
void printManPage();
int parseCmd(int argc, char *argv[]);

#endif //HW6_MAIN_H
