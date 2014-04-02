// Compile command
// gcc master_test.c -o bigtest.o -lrt -lftdi1 -g

#define RAND_FLAG 0         // If zero random test does not happens
#define ITERATIONS 3
#define BLOCK_NUM (200 * 256)
#define ADDR_RANGE (BLOCK_NUM * STRIDE_END)
#define LAST_BLOCK_SIZE  8     // In KB
#define SRAND_RANGE 1000

#define STRIDE_START    1
#define STRIDE_INC      1
#define STRIDE_END      3
#define _GNU_SOURCE
#define _SVID_SOURCE

#define NSEC_PER_SEC    1000000000
#define NSEC_PER_MSEC   1000000
#define NSEC_PER_USEC   1000
#define OUT_FILE "./result.txt"
#define LOCAL_NSEC ((unsigned long) ((end.tv_nsec - ref.tv_nsec) + ((end.tv_sec - ref.tv_sec) * NSEC_PER_SEC)))

#define SSD_ON  command = 0xFF; if (ftdi_write_data(ftdi, &command, 1) < 0) \
                {printf("SSD ON failed, error %s\n", ftdi_get_error_string(ftdi));}

#define SSD_OFF command = 0; if (ftdi_write_data(ftdi, &command, 1) < 0) \
                {printf("SSD ON failed, error %s\n", ftdi_get_error_string(ftdi));}

#define PAUSE   printf("Paused for user input. Enter any charater to continue\n"); \
                getchar(); getchar();

#include <stdio.h>          // printf
#include <stdlib.h>         // malloc
#include <sys/types.h>      // block IO
#include <sys/stat.h>       // block IO
#include <fcntl.h>          // block IO
#include <time.h>           // clock
#include <libftdi1/ftdi.h>	// FTDI
#include <dirent.h>			// Directory
#include <sys/dir.h>
#include <string.h>			// String Ops
#include <signal.h>         // Kill command
#include <unistd.h>         // _exit
#include <stdbool.h>        // bool type

struct record {
	unsigned int num;
	unsigned long time;
    int seed;
    bool pers;
} rec [BLOCK_NUM + 1];

unsigned int get_data_acked (char *fname) {

	FILE *fp;
	char str[1024];
	char *index;
    unsigned long data = 0;
	
	fp = fopen(fname, "r");
	
	if(fp == NULL) {
		printf ("Error opening trace file");
		return(-1);
	}
	
	while (fgets (str, 1024, fp) != NULL) {
        if((index = strstr(str, "CPU")) != NULL) {
			while (fgets (str, 1024, fp) != NULL) {
				if((index = strstr(str, "Writes Completed:")) != NULL) {
					if((index = strstr(index, ",")) != NULL) {
						data += atoi(index + 1);
                        break;
					}
				}
			}
		}
    }
	fclose(fp);
	return data;
}


int is_sdc(struct direct *entry) {
	if (strcmp(entry->d_name, "sdc")== 0) {
		return (1); // True
	} else {
		return (0); // False
	}
}

int is_sdd(struct direct *entry) {
	if (strcmp(entry->d_name, "sdd")== 0) {
		return (1); // True
	} else {
		return (0); // False
	}
}

char dev_name_update(void) {
	struct dirent **namelist;
    int n;
    char d;

	n = scandir("/dev/.", &namelist, &is_sdc, NULL);
    if (n != 0) {
		free(namelist[0]);
        free(namelist);
		return 'c';
	}
	
	n = scandir("/dev/.", &namelist, &is_sdd, NULL);
    if (n != 0) {
		free(namelist[0]);
        free(namelist);
		return 'd';
	}
}

void* relay_setup (void) {
	struct ftdi_context *ftdi;
	int f;

	if ((ftdi = ftdi_new()) == 0) {
        printf("ftdi_new failed\nFatal error, exiting\n");
		exit (1);
    }

    f = ftdi_usb_open(ftdi, 0x0403, 0x6001);
    if (f < 0 && f != -5) {
        printf("Unable to open ftdi device %s\nFatal error, exiting\n", ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
		exit (1);
    }

    ftdi_set_bitmode(ftdi, 0xFF, BITMODE_BITBANG);
    return (ftdi);
}

void relay_cleanup (struct ftdi_context *ftdi) {
	ftdi_disable_bitbang(ftdi);
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
}

void rand_buf_fill (char *buf, int seed, int size) {
    int i;
    int64_t *buffer;
    buffer = (int64_t *) buf;
    srand(seed);
    for (i = 0; i < (size / 8); i++) {
        *(buffer + i) = (int64_t) rand();
    }
}

int count_partial (char *buf1, char *buf2, int size) {
    int i, cnt;
    int64_t *b1, *b2;
    b1 = (int64_t *) buf1;
    b2 = (int64_t *) buf2;

    for (i = 0, cnt = 0; i < (size / 8); i++) {
        if (*(b1 + i) == *(b2 + i)) {
            cnt ++;
        }
    }
    return cnt * 8;
}

void check_rec_sanity(void) {
    int i;
    for (i = 0; i < BLOCK_NUM - 1; i++) {
        if(rec[i].time > rec[i+1].time) {
            printf("rec[%d].time = %lu and rec[%d].time = %lu\n PANIC PANIC !!!! \n",
                    i, rec[i].time, i + 1, rec[i + 1].time);
            exit(1);
        }
    }
}

int main (int argc, char *argv[])
{
    unsigned int    device, i, j, seek, sprintf_size, unserialised_flag, rand_flag = RAND_FLAG, status, data_acked, stride = STRIDE_START;
    unsigned int    outfile, tracefile, block_size, rec_acked, rec_pers = 0, iters = ITERATIONS, match_bytes, pers_data, corrupt_data; 
    char            d, buf2[1000], device_path[20], devname[20], command, buf[4096], buf_test[4096];
    struct timespec	ref, end;
	struct			ftdi_context *ftdi;
	pid_t			child_pid;
	
    srand(time(NULL));
    memset(buf2, '\0', 1000);
	unlink(OUT_FILE);
    if ((outfile = open(OUT_FILE, O_RDWR | O_CREAT, O_SYNC)) == -1) {
        printf("Could not open an output file, exiting\n");
        exit(1);
    }
    sprintf_size =  sprintf (buf2,
        "Block Size | Test Type | Iteration | Application Wrote | Drive Acknowledged | Persistent Data | Data Corrupted | Time\n");
    write (outfile, buf2, sprintf_size);

    ftdi = relay_setup();
    SSD_ON;
    sleep(5);
	d = dev_name_update();
    sprintf(buf2, "dd if=/dev/zero of=/dev/sd%c bs=4096 count=%d", d, ADDR_RANGE); // 1 GB cleanp
    system(buf2);


	/*
     * Every iteration of this loop is multiple iterations of the test with a particular block size
     */
	for (block_size = 4096; block_size <= (LAST_BLOCK_SIZE * 1024);) {

        SSD_ON;
        sleep(5);
		unserialised_flag = 0;
		d = dev_name_update();
		sprintf(device_path, "/dev/sd%c", d);
        printf("Device path = %s\n", device_path);
		sprintf(buf2, "echo noop > /sys/block/sd%c/queue/scheduler", d);
		system(buf2);
		sprintf(buf2, "echo %d > /sys/block/sd%c/queue/max_sectors_kb", block_size / 1024, d);
		system(buf2);

		unlink ("/ssdoptfs/optfs/ay_tests/data_loss/blk_out.txt");
        if ((child_pid = fork()) == 0) {
		    sprintf(buf2, "blktrace -d /dev/sd%c -o - -a complete | blkparse -i -t -v - > /ssdoptfs/optfs/ay_tests/data_loss/blk_out.txt", d);
			system(buf2);
            _exit(0);
		}
        sleep(30); 

        while ((device = open(device_path, O_RDWR, O_DIRECT)) == -1) {
			printf("Could not open device, exiting\n");
            d++;
            sprintf(device_path, "/dev/sd%c", d);
		}

        clock_gettime(CLOCK_MONOTONIC, &ref);
		
        /*
         * Write to the SSD and keep track of everything in rec structure
         */
        for(i = 0, seek = 0; i < BLOCK_NUM; i++) {

            rec[i].pers = false;
            rec[i].seed = rand() % SRAND_RANGE;
            rec[i].num  = seek;
            rand_buf_fill(buf, rec[i].seed, 4096);

            if (lseek(device, rec[i].num * 4096, SEEK_SET) == -1) {
				printf("seek failed, blknbr %d\n", rec[i].num);
			}

			if ((write(device, buf, 4096) == -1)) {
				printf("A write failed, blknbr %d\n", rec[i].num);
			}

			clock_gettime(CLOCK_MONOTONIC, &end);
			rec[i].time = LOCAL_NSEC / NSEC_PER_USEC;

            if (rand_flag) {
                seek = rand() % ADDR_RANGE;
            } else {
                seek += stride;
            }
		}

        //fsync(device);
        //printf("Device syncing is ON !!!!!!!!!!!!!!!!!!!!!!!\n");
		SSD_OFF;
        check_rec_sanity();
        close(device);

        sleep(60);
        if (kill(child_pid + 2, SIGINT) == -1) {
            printf("Child process could not be killed, exiting\n");
            exit(1);
        }
        waitpid(child_pid + 2, &status, 0);
        waitpid(child_pid + 1, &status, 0);
        waitpid(child_pid + 0, &status, 0);
        sleep(30);
		data_acked = get_data_acked("/ssdoptfs/optfs/ay_tests/data_loss/blk_out.txt");
        if (data_acked == 0) {
            printf("Data Acked by drive was 0 ?!?!?!?! Is going to cause problems\n");
        }

        // Stage 2 check persistent data
        SSD_ON;
		sleep(5);
		d = dev_name_update();
        sprintf(device_path, "/dev/sd%c", d);
        printf("Device path = %s\n", device_path);
		while ((device = open(device_path, O_RDWR, O_DIRECT)) == -1) {
			printf("Could not open device for checking, exiting\n");
            d++;
            sprintf(device_path, "/dev/sd%c", d);
		}

		/*
         * Reading phase: read from SSD and check for persistence
         */
		for (i = 0, pers_data = 0, corrupt_data = 0; i < BLOCK_NUM; i++) {

            rand_buf_fill(buf_test, rec[i].seed, 4096);
			if (lseek(device, rec[i].num * 4096, SEEK_SET) == -1) {
				printf("Seek failed, blknbr %d\n", rec[i].num);
			}
			if ((read(device, buf, 4096) == -1)) {
				printf("A read failed, blknbr %d\n", rec[i].num);
			}
            if (memcmp(buf, buf_test, 4096) == 0) {
                rec[i].pers = true;
                pers_data += 4;
            } else {
                corrupt_data += 4;
            }
		}

        sprintf_size =  sprintf (buf2,
            "%10u |   %c%4d   | %9u | %17u | %18u | %15u | %14u | %lu\n", block_size / 1024, rand_flag ? 'R' : 'S', stride, ITERATIONS - iters + 1,
            BLOCK_NUM * 4, data_acked, pers_data, ((data_acked - pers_data) > 0) ? (data_acked - pers_data) : 0 ,  (unsigned long) 0);
        write (outfile, buf2, sprintf_size);
        
        printf("30 seconds to do a clean exit start now!\n");
        sleep(30);
        printf("Too late\n");

        iters--;

        if (iters == 0) {
            iters = ITERATIONS;
            stride += STRIDE_INC;
            if ((stride > STRIDE_END) || rand_flag) {
                stride = STRIDE_START;
                block_size *= 2;
                if (rand_flag) {
                    block_size /= 2;
                    rand_flag = 0;
                }
            }
        }
        close(device);
	}

    // Cleanup Phase
	printf("normal exit\n");
	relay_cleanup (ftdi);
    close(outfile);
    return;
}

