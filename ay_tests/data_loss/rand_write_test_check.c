#define _GNU_SOURCE
#define BLOCK_NUM (100 * 1024 / 4) //100 MB worth of 4KB blocks.
#define ADDR_RANGE (BLOCK_NUM * 5)
#define BLOCK_SIZE  (4096)
#define NSEC_PER_SEC 1000000000
#define OUT_FILE_D "./durability_stamps.txt"
#define OUT_FILE_I "./inconsistent_list.txt"
#define OUT_FILE_P "./partial_inconsistent_list.txt"
#define LOCAL_NSEC ((unsigned long long) ((end.tv_nsec - start.tv_nsec) + ((end.tv_sec - start.tv_sec) * NSEC_PER_SEC)))

#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <time.h>           // clock

int main (int argc, char *argv[])
{
    unsigned int    device, i, j, seek, sprintf_size, last_blk, flag = 0;
    unsigned int    outfile_d, outfile_i, outfile_p, *addr; 
    char            *buf, *buf_test, c, d, buf2[1000], device_path[20];
    struct          timespec start, end;
    
    system("/ssdoptfs/drcontrol.py -d DAE0010T -r all -c on");
    srand(time(NULL));

    for (i = 0; i < 1000; i++) {
        buf2[i] = '\0';
    }

    if (argc != 1) {
        printf("Usage ./a.out\n");
        exit (1);
    }

    printf("Enter drive character, /dev/sd<>\n");
    d = getchar();
    getchar();
    printf("\n");

    printf("Enter test character, new one everytime\n");
    c = getchar();
    getchar();
    printf("\n");

    sprintf(device_path, "/dev/sd%c", d);
    sprintf(buf2, "echo noop > /sys/block/sd%c/queue/scheduler", d);
    printf("%s\n", buf2);
    system(buf2);
    sprintf(buf2, "echo %d > /sys/block/sd%c/queue/max_sectors_kb", BLOCK_SIZE, d);
    printf("%s\n", buf2);
    system(buf2);
    
    unlink(OUT_FILE_D);
    unlink(OUT_FILE_P);
    unlink(OUT_FILE_I);

    if (((outfile_i = open(OUT_FILE_I, O_RDWR | O_CREAT, O_SYNC)) == -1) ||
            ((outfile_d = open(OUT_FILE_D, O_RDWR | O_CREAT, O_SYNC)) == -1) ||
            ((outfile_p = open(OUT_FILE_P, O_RDWR | O_CREAT, O_SYNC)) == -1)) {
        printf("Could not open an output file, exiting\n");
        exit(1);
    }

    if (((buf = (char *)malloc(BLOCK_SIZE)) == NULL) ||
            ((buf_test = (char *)malloc(BLOCK_SIZE)) == NULL) ||
            ((addr = (unsigned int *)malloc(BLOCK_NUM * sizeof(unsigned int))) == NULL)) {
        printf("Could not allocate a buffer, exiting\n");
        exit(1);
    }

    for (i = 0; i < BLOCK_SIZE; i++) {
        *(buf + i) = c;
    }

    /*
     * Stage 1 Test Starts here
     */
    
    if ((device = open(device_path, O_RDWR, O_DIRECT)) == -1) {
        printf("Could not open device, exiting\n");
        exit(1);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (i = 0; i < BLOCK_NUM; i++) {

        if (system("echo 3 > /proc/sys/vm/drop_caches") == -1) {
            printf("drop_caches failed\n");
        }

        seek = rand() % ADDR_RANGE;
        *(addr + i) = (unsigned int) seek;

        if (lseek(device, seek * BLOCK_SIZE, SEEK_SET) == -1) {
            printf("seek failed, blknbr %d\n", seek);
        }

        if ((write(device, buf, BLOCK_SIZE) == -1)) {
            printf("A write failed, blknbr %d\n", seek);
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        sprintf_size = sprintf(buf2, "Total %6d KB written till %15llu\n", (i + 1) * BLOCK_SIZE / 1024, LOCAL_NSEC);
        write (outfile_d, buf2, sprintf_size);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    sprintf_size = sprintf(buf2, "SSD off started  at %15llu\n", LOCAL_NSEC);
    write (outfile_d, buf2, sprintf_size);

    system ("/ssdoptfs/drcontrol.py -d DAE0010T -r all -c off");
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    sprintf_size = sprintf(buf2, "SSD off finished at %15llu\n", LOCAL_NSEC);
    write (outfile_d, buf2, sprintf_size);
    
    close(outfile_d);
    close(device);
    printf("Sleeping for a bit, let poor SSD rest\n");
    sleep(10);
	
    /*
     * Stage 2 begins. Restart device, check new device name, and check for consistency
     */

    system("/ssdoptfs/drcontrol.py -d DAE0010T -r all -c on");
    printf("Enter drive character, /dev/sd<>, it might have changed during power cycle\n");
    d = getchar();
    getchar();
    sprintf(device_path, "/dev/sd%c", d);

    if ((device = open(device_path, O_RDWR)) == -1) {
        printf("Could not open device for checking, exiting\n");
        exit(1);
    }

    for (i = 0; i < BLOCK_NUM; i++) {
        
        seek = *(addr + i);

        if (lseek(device, seek * BLOCK_SIZE, SEEK_SET) == -1) {
            printf("Seek failed, blknbr %d\n", seek);
        }

        if ((read(device, buf_test, BLOCK_SIZE) == -1)) {
            printf("A read failed, blknbr %d\n", seek);
            break;
        }

        if (memcmp(buf, buf_test, BLOCK_SIZE)) {
           flag = 1;
           sprintf_size = sprintf(buf2, "Inconsistent write detected at location %d, after %d KB data\n", i, i * BLOCK_SIZE / 1024);
           write (outfile_i, buf2, sprintf_size);

           for(j = 0; j < BLOCK_SIZE; j++) {
               if (*(buf_test + i) == c) {
                   printf("partial write detected\n");
                   sprintf_size = sprintf(buf2, "Partial Inconsistent Write detected at location %d, after %d KB data\n", i, i * BLOCK_SIZE / 1024);
                   write (outfile_p, buf2, sprintf_size);
                   write (outfile_p, buf_test, BLOCK_SIZE);
                   write (outfile_p, "\n", 1);
               }
           }
       } else if (flag) {
           sprintf_size = sprintf(buf2, "Out of order Write detected at location %d, after %d KB data\n", i, i * BLOCK_SIZE / 1024);
           write (outfile_p, buf2, sprintf_size);
       }
    }
    
    printf("normal exit\n");
    close(device);
    close(outfile_i);
    close(outfile_p);
    return;
}

