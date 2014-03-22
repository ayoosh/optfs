#define BLOCK_SIZE  4096
#define REPEATS     100
#define RANGE       (1024 * 1024 * 10)
#define SLEEP       1
#define NSEC_PER_SEC 1000000000

#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <time.h>           // clock

int main (int argc, char *argv[])
{
    unsigned int    device, i, write_cnt, seek;
    char            *buf;
    struct timespec start, end;
    long            local_nsec, acc_nsec;

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

    srand(time(NULL));
    acc_nsec = 0;

    // Start workload
    for (i = 0, write_cnt = 0, local_nsec = 0; i < REPEATS; i++) { // jump

        sync(device);
#if 1
        if (system("hdparm -F /dev/sdc") == -1) {
            printf("hdparm -F /dev/sdc failed\n");
        }
#endif   
        if (system("echo 3 > /proc/sys/vm/drop_caches") == -1) {
            printf("drop_caches failed\n");
        }

        sleep(SLEEP);

#if 1
        if (read(device, buf, 1) == -1) {
            printf("first read failed\n");
        }
#endif

        if (lseek(device, seek = ((rand() % RANGE) * BLOCK_SIZE), SEEK_SET) == -1) {
            printf("random seek failed\n");
        }
        printf("seek = %d\n", seek);

        clock_gettime(CLOCK_MONOTONIC, &start);
        write(device, buf, BLOCK_SIZE);
        clock_gettime(CLOCK_MONOTONIC, &end);
        local_nsec = (end.tv_nsec - start.tv_nsec) + ((end.tv_sec - start.tv_sec) * NSEC_PER_SEC);
        acc_nsec += local_nsec;
        printf("%10ld\n", local_nsec);
    }
    printf ("average: %10ld\n", acc_nsec/REPEATS);

    close(device);
    return;
}

