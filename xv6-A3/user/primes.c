/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/
#include "kernel/types.h"
#include "user/user.h"

/* Recursive sieve function: reads from read_fd, prints first prime,
sets up next sieve */
void sieve(int read_fd)
{
    int prime;
    /* Read the first number (prime) from the pipe */
    if(read(read_fd, &prime, sizeof(prime)) <= 0)
    {
        close(read_fd);
        return; /* Explicit return to terminate recursion */
    }
    printf("prime %d\n", prime);

    int p[2];
    pipe(p);
    int pid = fork();
    if(pid == 0)
    {
        /* Child: next sieve stage */
        close(p[1]);
        sieve(p[0]);
        exit(0);
    }
    else
    {
        /* Parent: filter and pass non-multiples to next pipe */
        close(p[0]);
        int num;
        while(read(read_fd, &num, sizeof(num)) > 0)
        {
            if(num % prime != 0)
            {
                write(p[1], &num, sizeof(num));
            }
        }
        close(read_fd);
        close(p[1]);
        wait(0);
        exit(0);
    }
}

int main(void)
{
    int p[2];
    pipe(p);
    int pid = fork();
    if(pid == 0)
    {
        /* Child: start sieve */
        close(p[1]);
        sieve(p[0]);
        exit(0);
    }
    else
    {
        /* Parent: write numbers 2..35 to pipe */
        close(p[0]);
        for(int i = 2; i <= 35; i++)
        {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);
        wait(0);
    }
    exit(0);
}
