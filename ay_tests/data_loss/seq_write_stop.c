#define _GNU_SOURCE
//#define SCALE 
//#define BLOCK_SIZE  (1024 * 128)
#define NSEC_PER_SEC 1000000000
#define OUT_FILE "./durability_stamps.txt"
//#define USLEEP 10
#define LOCAL_NSEC ((unsigned long long) ((end.tv_nsec - start.tv_nsec) + ((end.tv_sec - start.tv_sec) * NSEC_PER_SEC)))
#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <time.h>           // clock
//#include <unistd.h>

int main (int argc, char *argv[])
{
    unsigned int        outfile, device, i, seek, seekmax, block_size, sprintf_size;
    char                *buf, c, buf2[1000], device_path[20];
    struct              timespec start, end;

    system("/ssdoptfs/drcontrol.py -d DAE0010T -r all -c on");
    srand(time(NULL));

    for (i = 0; i < 1000; i++) {
        buf2[i] = '\0';
    }

    if (argc < 4) {
        printf("Usage <a.out> <device name> <block size> <test character> \n");
        exit (1);
    }

    block_size = atoi(argv[2]) * 1024;
    printf("block_size = %d\n", block_size);
    c = *argv[3];

    sprintf(device_path, "/dev/%s", argv[1]);
    sprintf(buf2, "echo noop > /sys/block/%s/queue/scheduler", argv[1]);
    printf("%s\n", buf2);
    system(buf2);
    sprintf(buf2, "echo %d > /sys/block/%s/queue/max_sectors_kb", block_size / 1024, argv[1]);
    printf("%s\n", buf2);
    system(buf2);


    if ((device = open(device_path, O_RDWR, O_DIRECT)) == -1) {
        printf("Could not open device, exiting\n");
        exit(1);
    }
    
    unlink(OUT_FILE);

    if ((outfile = open(OUT_FILE, O_RDWR | O_CREAT, O_SYNC)) == -1) {
        printf("Could not open output file, exiting\n");
        exit(1);
    }

    if ((buf = (char *)malloc(block_size)) == NULL) {
        printf("Could not allocate buffer, exiting\n");
        exit(1);
    }

    for (i = 0; i < block_size; i++) {
        *(buf + i) = c;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    // 200 + upto 10 MB write
    seekmax = ((200 + (rand() % 10)) * 1024 * 1024) / block_size;
//    seekmax = 5;

    /*
     * Test Starts here
     */
    
    for(seek = 0; seek < seekmax; seek++) {

        if (system("echo 3 > /proc/sys/vm/drop_caches") == -1) {
            printf("drop_caches failed\n");
        }

        if (lseek(device, seek * block_size, SEEK_SET) == -1) {
            printf("seek failed, blknbr %d\n", seek);
        }

        if ((write(device, buf, block_size) == -1)) {
            printf("A write failed, blknbr %d\n", seek);
            break;
        }
//        sync(device);

        clock_gettime(CLOCK_MONOTONIC, &end);
//        usleep(USLEEP);

//        printf("blknbr %6d written\n", seek);
        sprintf_size = sprintf(buf2, "blknbr %6d written at %15llu\n", seek, LOCAL_NSEC);//local_nsec);
        write (outfile, buf2, sprintf_size);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    sprintf_size = sprintf(buf2, "SSD off started  at %15llu\n", LOCAL_NSEC);
    write (outfile, buf2, sprintf_size);

    system ("/ssdoptfs/drcontrol.py -d DAE0010T -r all -c off");
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    sprintf_size = sprintf(buf2, "SSD off finished at %15llu\n", LOCAL_NSEC);
    write (outfile, buf2, sprintf_size);
    
    close(outfile);
    close(device);
    return;
}

