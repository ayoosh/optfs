#define BLOCK_SIZE  4096
#define NSEC_PER_SEC 1000000000
#define OUT_FILE "./inconsistent_list.txt"
#define OUT_FILE_P "./partial_inconsistent_list.txt"

#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <time.h>           // clock
#include <string.h>
int main (int argc, char *argv[])
{
    unsigned int    outfile_p, outfile, device, i, seek, last_blk, sprint_size, flag = 0;
    char            *buf, *buf_test, c, *buf_prev, buf2[1000];
    struct timespec start, end;
    unsigned long long local_nsec;

    if (argc < 4) {
        printf("Usage <a.out> \"device/file path\" <blocks to test> <character>\n");
        exit (1);
    }

    system("/ssdoptfs/drcontrol.py -d DAE0010T -r all -c on");
    sleep(10);

    for(i = 0; i < 1000; i++) {
        buf2[i] = '\0';
    }

    if ((device = open(argv[1], O_RDWR, O_SYNC)) == -1) {
        printf("Could not open device, exiting\n");
        exit(1);
    }
    
    unlink(OUT_FILE);
    unlink(OUT_FILE_P);

    if ((outfile = open(OUT_FILE, O_RDWR | O_CREAT, O_SYNC)) == -1) {
        printf("Could not open output file, exiting\n");
        exit(1);
    }

    if ((outfile_p = open(OUT_FILE_P, O_RDWR | O_CREAT, O_SYNC)) == -1) {
        printf("Could not open output file, exiting\n");
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

    /*if ((buf_prev = (char *)malloc(BLOCK_SIZE)) == NULL) {
        printf("Could not allocate buffer, exiting\n");
        exit(1);
    }*/

    c = *argv[3];
    printf("\n");

    last_blk = atoi(argv[2]);

    // Fill buffer 
    for (i = 0; i < BLOCK_SIZE; i++) {
        *(buf + i) = c;
    }

    for(seek = 0; seek <= last_blk; seek++) {

        if (lseek(device, seek * BLOCK_SIZE, SEEK_SET) == -1) {
            printf("Seek failed, blknbr %d\n", seek);
        }

        if ((read(device, buf_test, BLOCK_SIZE) == -1)) {
            printf("A read failed, blknbr %d\n", seek);
            break;
        }

//        memcpy(buf_prev, buf, BLOCK_SIZE);
       if (memcmp(buf, buf_test, BLOCK_SIZE)) {
           flag = 1;
           sprint_size = sprintf(buf2, "Inconsistent write detected at blknbr %d, sector %d\n", seek, seek * 8);
           write (outfile, buf2, sprint_size);

           for(i = 0; i < BLOCK_SIZE; i++) {
               if (*(buf_test + i) == c) {
                   printf("partial write detected\n");
                   sprint_size = sprintf(buf2, "Partial Inconsistent Write detected blknbr %d, sector %d\n", seek, seek * 8);
                   write (outfile_p, buf2, sprint_size);
                   write (outfile_p, buf_test, BLOCK_SIZE);
                   write (outfile_p, "\n", 1);
               }
           }
       } else if (flag) {
           printf("out of order write!!\n");
       }
    }
//    printf("Differing block : %s \n",buf_test);
//    printf("Previous block : %s \n",buf_prev);
    printf("normal exit\n");
    close(device);
    close(outfile);
    close(outfile_p);
    return;
}

