#define BLOCK_SIZE  4096
#define NSEC_PER_SEC 1000000000
#define OUT_FILE "./durability_stamps.txt"

#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <time.h>           // clock
#include <string.h>
int main (int argc, char *argv[])
{
    unsigned int    device, i, seek, last_blk;
    char            *buf, *buf_test, c, *buf_prev;
    struct timespec start, end;
    unsigned long long local_nsec;

    if (argc < 2) {
        printf("Usage <a.out> \"device/file path\" \n");
        exit (1);
    }

    if ((device = open(argv[1], O_RDWR, O_SYNC)) == -1) {
        printf("Could not open device, exiting\n");
        exit(1);
    }
    
    // Allocate buffer
    if ((buf = (char *)malloc(BLOCK_SIZE)) == NULL) {
        printf("Could not allocate buffer, exiting\n");
        exit(1);
    }

    if ((buf_test = (char *)malloc(BLOCK_SIZE)) == NULL) {
        printf("Could not allocate buffer, exiting\n");
        exit(1);
    }
    if ((buf_prev = (char *)malloc(BLOCK_SIZE)) == NULL) {
        printf("Could not allocate buffer, exiting\n");
        exit(1);
    }


    printf("Enter the check character. Must be same as last invocation of test\n");
    c = getchar();
    printf("\n");


    printf("Enter the number of blocks to be tested. Intended to be same as the last written by test\n");
    scanf("%d", &last_blk);
    printf("\n");

    // Fill buffer 
    for (i = 0; i < BLOCK_SIZE; i++) {
        *(buf + i) = c;
    }

    for(seek = 0; seek <= last_blk; seek++) {

        if (lseek(device, seek * BLOCK_SIZE, SEEK_SET) == -1) {
            printf("seek failed, blknbr %d\n", seek);
        }

        if ((read(device, buf_test, BLOCK_SIZE) == -1)) {
            printf("A read failed, blknbr %d\n", seek);
            break;
        }
        memcpy(buf_prev, buf, BLOCK_SIZE);
       if (memcmp(buf, buf_test, BLOCK_SIZE)) {
           printf("Inconsistent write detected at blknbr %d, sector %d \n", seek, seek * 8);
       }
    }
//    printf("Differing block : %s \n",buf_test);
//    printf("Previous block : %s \n",buf_prev);
    printf("normal exit\n");
    close(device);
    return;
}

