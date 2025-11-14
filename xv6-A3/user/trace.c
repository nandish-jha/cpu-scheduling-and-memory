// AAKASH JANA AAJ284 11297343
// NANDISH JHA NAJ474 11282001

/* CMPT 332 Nandish Jha NAJ474 11282001 2025 change */

// Minimal trace.c for xv6 assignment
#include "user.h"

int main(int argc, char *argv[])
{
    if(argc < 3){
        fprintf(2, "Usage: trace mask command [args...]\n");
        exit(1);
    }
    int mask = atoi(argv[1]);
    trace(mask);
    exec(argv[2], &argv[2]);
    // If exec fails
    fprintf(2, "exec failed\n");
    exit(1);
}
