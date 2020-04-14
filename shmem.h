#ifndef SHMEM_H
#define SHMEM_H

typedef struct
{
    unsigned int secs;
    unsigned int nans;
} simclock;

typedef struct
{
    int shareable;
    int inventory;
    int available;
    int request[18];
    int release[18];
    int allocator[18];
} descriptor;

typedef struct
{
    descriptor resourceclass[20];
    simclock smtime;
} shmem;

#endif /* SHMEM_H */