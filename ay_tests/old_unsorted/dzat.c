#define BLOCK_SIZE  4096
#define REPEATS     100
#define ADDR        (7 * 4096)
#define SLEEP       30
#define RANGE       (256 * 1024 * 10)

#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <string.h>         // memcmp

char zbuf[4096] = {0};

int main (int argc, char *argv[])
{
    unsigned int    device, i, seek;
    char            *buf;
    
    srand(time(NULL));

    if (argc < 2) {
        printf("Usage <a.out> \"device/file path\"\n");
        exit (1);
    }

    // Open device
    if ((device = open(argv[1], O_RDWR)) == -1) {
        printf("Could not open device, exiting\n");
        exit(1);
    }

    // Allocate buffer
    if ((buf = (char *)malloc(BLOCK_SIZE)) == NULL) {
        printf("Could not allocate buffer, exiting\n");
        exit(1);
    }

    // Fill buffer 
    for (i = 0; i < BLOCK_SIZE; i++) {
        *(buf + i) = (char) (i & 0xFF);
    }

    for (i = 0; i < REPEATS; i++) {

        seek = rand() % RANGE;
        seek *= BLOCK_SIZE;
        if(lseek(device, seek, SEEK_SET)==-1){
            printf("Lseek Failed. At seek = %d",seek);
            exit(1);
        }// Seek to next BLOCK_SIZEd block.

        if(read(device, buf, BLOCK_SIZE)==-1){
            printf("Read failed at seek = %d ",seek);
            exit(1);
        }

        if (memcmp(zbuf, buf, BLOCK_SIZE)) {
            printf("Block not zero at %d\n", seek);
        } else {
            printf("Block zero at %d\n", seek);
        }
    } 
    
    close(device);
    return;
}

