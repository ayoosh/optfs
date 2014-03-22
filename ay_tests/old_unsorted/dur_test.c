#define BLOCK_SIZE  4096
#define NSEC_PER_SEC 1000000000
#define OUT_FILE "./durability_stamps.txt"
#define USLEEP 10000
#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <time.h>           // clock
//#include <unistd.h>

int main (int argc, char *argv[])
{
    unsigned int    outfile, device, i, seek;
    char            *buf, c, buf2[1000];
    struct timespec start, end;
    unsigned long long local_nsec;

    system("echo noop > /sys/block/sdc/queue/scheduler");
    system("echo 1 > /sys/block/sdc/queue/nomerges");
    system("echo 1 > /sys/block/sdc/queue/nr_requests");
    system("echo 4 > /sys/block/sdc/queue/max_sectors_kb");
        
    if (argc < 2) {
        printf("Usage <a.out> \"device/file path\" \n");
        exit (1);
    }

    if ((device = open(argv[1], O_RDWR, O_SYNC)) == -1) {
        printf("Could not open device, exiting\n");
        exit(1);
    }
    
    unlink(OUT_FILE);

    if ((outfile = open(OUT_FILE, O_RDWR | O_CREAT, O_SYNC)) == -1) {
        printf("Could not open output file, exiting\n");
        exit(1);
    }

    // Allocate buffer
    if ((buf = (char *)malloc(BLOCK_SIZE)) == NULL) {
        printf("Could not allocate buffer, exiting\n");
        exit(1);
    }

    printf("Enter the test character. Must be different from last invocation\n");
    c = getchar();
    printf("\n");

    // Fill buffer 
    for (i = 0; i < BLOCK_SIZE; i++) {
        *(buf + i) = c;
    }
    
//    clock_gettime(CLOCK_MONOTONIC, &start);

    for(seek = 0; ; seek++) {

        if (system("echo 3 > /proc/sys/vm/drop_caches") == -1) {
            printf("drop_caches failed\n");
        }

        if (lseek(device, seek * BLOCK_SIZE, SEEK_SET) == -1) {
            printf("seek failed, blknbr %d\n", seek);
        }

        if ((write(device, buf, BLOCK_SIZE) == -1)) {
            printf("A write failed, blknbr %d\n", seek);
            break;
        }
//        sync(device);

//        clock_gettime(CLOCK_MONOTONIC, &end);
        usleep(USLEEP);

//        local_nsec = (end.tv_nsec - start.tv_nsec) + ((end.tv_sec - start.tv_sec) * NSEC_PER_SEC);
        printf("blknbr %6d written\n", seek);
 //       sprintf(buf2, "blknbr %6d written at %10llu\n", seek, local_nsec);

//      printf("blknbr %6d written at %10llu\n", seek, time(NULL));
//      sprintf(buf2, "blknbr %6d written at %10llu\n\0\0\0\0\0", seek, time(NULL));
        
        write (outfile, buf2, 37);
        
    }

    close(outfile);
    close(device);
    return;
}

