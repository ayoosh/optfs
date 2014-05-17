// Compile command
// gcc master_test.c -o bigtest.o -lrt -lftdi1 -g

#define RAND_FLAG 0         // If zero random test does not happens
#define ITERATIONS 2
#define BLOCK_NUM (100 * 256)
#define ADDR_RANGE (BLOCK_NUM * 5)
#define LAST_BLOCK_SIZE  128     // In KB
#define SRAND_RANGE 1000000

// Stride is also used as sleep duration multiplier as a different test
#define STRIDE_START    1000*1000*5
#define STRIDE_INC      1000*1000
#define STRIDE_END      1000*1000*15

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
#include <sched.h>


struct record {
	unsigned int num;
	unsigned long time;
    int seed;
    bool pers;
} rec [BLOCK_NUM + 1];

struct ftdi_context *ftdi;
char command;

int64_t get_data_acked (char *fname) {

	FILE *fp;
	char str[1024];
	char *index;
    int64_t data = 0;
	
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
    int n, device, i;
    char d, rot[50], val[100];

    for (n = 0; n < 100; n++) {
        val[n] = '\0';
    }

    d = 'c';
    i = 0;
    n = 1;
    sprintf (rot, "/sys/block/sdc/queue/rotational");
    while (1) {
        while ((device = open(rot, O_RDONLY)) == -1) {
            sprintf(rot, "/sys/block/sd%c/queue/rotational", d);
	    }
        i++;
	    read(device, val, 10);
        close(device);
        n = atoi(val);
        if (n == 0) {
            return d;
        }
        if (i == 10000) {
            printf("Just avoided infinite loop while finding SSD, what is wrong?? ;(\n");
            return -1;
        }
        SSD_ON; 
        sleep(5);
    }
}

void relay_setup (void) {
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
    return;
}

void relay_cleanup () {
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

unsigned int rand_addr_generate (unsigned int index) {

    unsigned int seek, i;
    seek = rand() % ADDR_RANGE;
    for(i = 0; i < index; i++) {
        if (seek == rec[i].num) {
            seek = rand_addr_generate (index);
            return seek;
        }
    }

    return seek;
}

unsigned long get_pers_time (int64_t data_acked) {
    int i;
    unsigned long ack_time, pers_time;

    for (i = 0; data_acked > 0; data_acked -= 4, i++) {
        if (rec[i].pers) {
            pers_time = rec[i].time;
        }

        ack_time = rec[i].time;
    }
    return ((ack_time > pers_time) ? (ack_time - pers_time) : 0) ;
}

int main (int argc, char *argv[])
{
    struct sched_param param;
    param.sched_priority = 80;
    sched_setscheduler(0, SCHED_FIFO, &param);
    
    
    unsigned int    device, i, j, seek, sprintf_size, unserialised_flag, rand_flag = RAND_FLAG, status, stride = STRIDE_START;
    unsigned int    outfile, tracefile, block_size, rec_acked, rec_pers = 0, iters = ITERATIONS, match_bytes, pers_data, corrupt_data, blktrace_issue; 
    char            d, buf2[1000], device_path[20], devname[20], buf[4096], buf_test[4096];
    struct timespec	ref, end;
	pid_t			child_pid;
    int64_t data_acked;

    srand(time(NULL));
    memset(buf2, '\0', 1000);
	unlink(OUT_FILE);
    if ((outfile = open(OUT_FILE, O_RDWR | O_CREAT, O_SYNC)) == -1) {
        printf("Could not open an output file, exiting\n");
        exit(1);
    }
    sprintf_size =  sprintf (buf2,
        "Block | Type | Iter | Application Wrote | Drive Acknowledged | Persistent Data | Data Corrupted | Application Lost | Iter Time | Loss Time (us) | BW\n");
    if((write (outfile, buf2, sprintf_size)) == -1) {
        printf("Could not write to output file\n");
    }

    relay_setup();
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
		sprintf(buf2, "echo noop > /sys/block/sd%c/queue/scheduler", d);
		system(buf2);
		sprintf(buf2, "echo %d > /sys/block/sd%c/queue/max_sectors_kb", block_size / 1024, d);
		system(buf2);

		unlink ("./blk_out.txt");
        if ((child_pid = fork()) == 0) {
		    sprintf(buf2, "blktrace -d /dev/sd%c -o - -a complete | blkparse -i -t -v - > ./blk_out.txt", d);
			system(buf2);
            _exit(0);
		}
        param.sched_priority = 79;
        sched_setscheduler(child_pid + 2, SCHED_FIFO, &param);
        param.sched_priority = 78;
        sched_setscheduler(child_pid + 3, SCHED_FIFO, &param);
        sleep(30); 

        if ((device = open(device_path, O_RDWR, O_DIRECT)) == -1) {
			printf("Could not open device: %s \nexiting\n", device_path);
            exit(1);
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
            //usleep(stride * 10) ;

			clock_gettime(CLOCK_MONOTONIC, &end);
			rec[i].time = LOCAL_NSEC / 1000;

            if (rand_flag) {
                seek = rand_addr_generate(i);
            } else {
                seek += 1 ;//stride;
            }
		}
        usleep(stride);
        //fsync(device);
        //printf("Device syncing is ON !!!!!!!!!!!!!!!!!!!!!!!\n");
		SSD_OFF;
        check_rec_sanity();
        close(device);

        sleep(60);
        blktrace_issue = 0;
        if (kill(child_pid + 2, SIGINT) == -1) {
            printf("blktrace process could not be killed\n");
            blktrace_issue = 1;
        }
        if (kill(child_pid + 3, SIGINT) == -1) {
            printf("blkparse process could not be killed\n");
        }
        waitpid(child_pid + 3, &status, 0);
        waitpid(child_pid + 2, &status, 0);
        waitpid(child_pid + 1, &status, 0);
        waitpid(child_pid + 0, &status, 0);
        sleep(30);
		data_acked = get_data_acked("./blk_out.txt");
        if (data_acked == 0) {
            printf("Data Acked by drive was 0 ?!?!?!?! Is going to cause problems\n");
        }
        if (data_acked > BLOCK_NUM * 4) {
            data_acked = BLOCK_NUM * 4;
        }

        // Stage 2 check persistent data
        SSD_ON;
		sleep(5);
		d = dev_name_update();
        sprintf(device_path, "/dev/sd%c", d);
        printf("Device path = %s\n", device_path);
		if ((device = open(device_path, O_RDWR, O_DIRECT)) == -1) {
			printf("Could not open device: %s for checking, exiting\n", device_path);
            exit(1);    
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
            "%5u |  %c%2d | %4u | %17u | %18u | %15u | %14u | %16u | %9lu | %14lu | lu", block_size / 1024, rand_flag ? 'R' : 'S', stride/1000000, ITERATIONS - iters + 1,
            BLOCK_NUM * 4, data_acked, pers_data, ((data_acked - pers_data) > 0) ? (data_acked - pers_data) : 0 ,  corrupt_data, rec[BLOCK_NUM -1].time, get_pers_time(data_acked));//, (BLOCK_NUM * 4 / 1024) / (rec[BLOCK_NUM -1].time / 1000000) );
        write (outfile, buf2, sprintf_size);
        if (blktrace_issue) {
            sprintf_size =  sprintf (buf2, "*\n");
            write (outfile, buf2, sprintf_size);
        } else {
            sprintf_size =  sprintf (buf2, "\n");
            write (outfile, buf2, sprintf_size);
        }   
        
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
    close(device);
	printf("normal exit\n");
    SSD_ON;
	relay_cleanup ();
    close(outfile);
    return;
}

