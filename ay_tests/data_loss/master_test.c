#define _GNU_SOURCE
#define _SVID_SOURCE

#define NSEC_PER_SEC 1000000000
#define OUT_FILE "./result.txt"
#define LOCAL_NSEC ((unsigned long) ((end.tv_nsec - ref.tv_nsec) + ((end.tv_sec - ref.tv_sec) * NSEC_PER_SEC)))
#define ADDR_RANGE (1024 * 1024 * 1024 / 4)

#define SSD_ON  command = 0xFF; if (ftdi_write_data(ftdi, &command, 1) < 0) \
                {printf("SSD ON failed, error %s\n", ftdi_get_error_string(ftdi));}

#define SSD_OFF command = 0; if (ftdi_write_data(ftdi, &command, 1) < 0) \
                {printf("SSD ON failed, error %s\n", ftdi_get_error_string(ftdi));}


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

struct record {
	unsigned int num;
	unsigned long time;
} rec [262144];

unsigned long get_data_acked (char *fname) {

	FILE *fp;
	char str[1024];
	char *index;
	
	fp = fopen(fname, "r");
	
	if(fp == NULL) {
		printf ("Error opening trace file");
		return(-1);
	}
	
	while (fgets (str, 1024, fp) != NULL) {
		if((index = strstr(str, "Total (traceout):")) != NULL) {
			while (fgets (str, 1024, fp) != NULL) {
				if((index = strstr(str, "Writes Completed:")) != NULL) {
					if((index = strstr(index, ",")) != NULL) {
                        printf("Acked data = %d\n", atoi(index + 1));
						return atoi(index + 1);
					}
				}
			}
		}
    }
	fclose(fp);

	return(0);
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

int is_sde(struct direct *entry) {
	if (strcmp(entry->d_name, "sde")== 0) {
		return (1); // True
	} else {
		return (0); // False
	}
}

char dev_name_update(void) {
	struct dirent **namelist;
    int n;

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
	
	n = scandir("/dev/.", &namelist, &is_sde, NULL);
    if (n != 0) {
		free(namelist[0]);
        free(namelist);
		return 'e';
	}
}

// FTDI Functions and Macros

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


int main (int argc, char *argv[])
{
    unsigned int    device, i, j, seek, sprintf_size, last_blk, flag, rand_flag = 1;
    unsigned int    outfile, tracefile, block_size, rec_acked, rec_pers, block_num, iters; 
    char            *buf, *buf_test, c, d, buf2[1000], device_path[20], devname[20], command;
    struct timespec	ref, end;
	struct			ftdi_context *ftdi;
	pid_t			child_pid;
	unsigned long	data_acked, time_acked, time_pers;
	

	// Setup start
    srand(time(NULL));
	ftdi = relay_setup();
	SSD_ON;
    sleep(10);
	c = 97;
    for (i = 0; i < 1000; i++) {
        buf2[i] = '\0';
    }

    if (argc != 1) {
        printf("Usage ./a.out\n");
        exit (1);
    }

	d = dev_name_update();
    sprintf(buf2, "dd if=/dev/zero of=/dev/sd%c bs=4096 count=256", d); // 1 GB cleanp
    //sprintf(buf2, "dd if=/dev/zero of=/dev/sd%c bs=4096 count=256000", d); // 1 GB cleanp
    printf("%s\n", buf2);
    system(buf2);

	unlink(OUT_FILE);
    if ((outfile = open(OUT_FILE, O_RDWR | O_CREAT, O_SYNC)) == -1) {
        printf("Could not open an output file, exiting\n");
        exit(1);
    }
    buf = (char *)malloc(4096);
	buf_test = (char *)malloc(4 * 1024);   
    iters = 5;

	// Outer loop for sequential write tests. Multiple iterations of the test happen here.
	for (block_size = 4096; block_size <= 262144; c++) {
	
    	// Start test for this block size
        sleep(30);
		flag = 0;
		d = dev_name_update();
		system ("rm traceout.blktrace.*");
		unlink("/ssdoptfs/optfs/ay_tests/data_loss/blk_out.txt");
		
		for (i = 0; i < block_size; i++) {
			*(buf + i) = (char) c;
		}
		
		sprintf(buf2, "blktrace -d /dev/sd%c -o ./traceout -a write", d);
		if ((child_pid = fork()) == 0) {
            printf("%s\n", buf2);
			system(buf2);
		}
		
		system(buf2);
		sprintf(device_path, "/dev/sd%c", d);
		sprintf(buf2, "echo noop > /sys/block/sd%c/queue/scheduler", d);
		printf("%s\n", buf2);
		system(buf2);
		sprintf(buf2, "echo %d > /sys/block/sd%c/queue/max_sectors_kb", block_size / 1024 , d);
		printf("%s\n", buf2);
		system(buf2);
    
		if ((device = open(device_path, O_RDWR, O_DIRECT)) == -1) {
			printf("Could not open device, exiting\n");
			exit(1);
		}

		//block_num = ((200 + (rand() % 10)) * 1024 * 1024) / 4096;	
		block_num = 10;
        clock_gettime(CLOCK_MONOTONIC, &ref);
        seek = 0;
		
		for(i = 0; i < block_num; i++) {
            /*
			if (system("echo 3 > /proc/sys/vm/drop_caches") == -1) {
				printf("drop_caches failed\n");
			}
            */

			if (lseek(device, seek * 4096, SEEK_SET) == -1) {
				printf("seek failed, blknbr %d\n", seek);
			}

			if ((write(device, buf, 4096) == -1)) {
				printf("A write failed, blknbr %d\n", seek);
				break;
			}

			clock_gettime(CLOCK_MONOTONIC, &end);
			rec[i].num = seek;
			rec[i].time = LOCAL_NSEC;

            if (rand_flag) {
                seek = rand() % ADDR_RANGE;
            } else {
                seek++;
            }
		}
		
		SSD_OFF;
		sleep(10);
		kill(child_pid, SIGTERM);
        int status;
        waitpid(child_pid, &status, 0);

		system ("blkparse traceout.blktrace.* > /ssdoptfs/ay_tests/data_loss/blk_out.txt");
		SSD_ON;
		data_acked = get_data_acked("/ssdoptfs/ay_tests/data_loss/blk_out.txt");
		rec_acked = (data_acked / 4) - 1;
		
		// Stage 2 check persistent data
		sleep (10);
		d = dev_name_update();
        sprintf(device_path, "/dev/sd%c", d);
		if ((device = open(device_path, O_RDONLY)) == -1) {
			printf("Could not open device for checking, exiting\n");
			exit(1);
		}
		
		for (i = 0; i < block_num; i++) {

			seek = rec[i].num;

			if (lseek(device, seek * 4096, SEEK_SET) == -1) {
				printf("Seek failed, blknbr %d\n", seek);
			}

			if ((read(device, buf_test, 4096) == -1)) {
				printf("A read failed, blknbr %d\n", seek);
				break;
			}

			if (memcmp(buf, buf_test, 4096)) {
				flag = 1;
				//sprintf_size = sprintf(buf2, "Inconsistent write detected at location %d, after %d KB data\n", i, i * BLOCK_SIZE / 1024);
				//write (outfile_i, buf2, sprintf_size);

				for(j = 0; j < 4096; j++) {
					if (*(buf_test + i) == c) {
						printf("partial write detected\n");
						//sprintf_size = sprintf(buf2, "Partial Inconsistent Write detected at location %d, after %d KB data\n", i, i * BLOCK_SIZE / 1024);
						//write (outfile_p, buf2, sprintf_size);
						//write (outfile_p, buf_test, BLOCK_SIZE);
						//write (outfile_p, "\n", 1);
					}
				}
			} else if (flag) {
				//sprintf_size = sprintf(buf2, "Out of order Write detected at location %d, after %d KB data\n", i, i * BLOCK_SIZE / 1024);
				//write (outfile_p, buf2, sprintf_size);
			} else {
                rec_pers = i;
                time_pers = rec[i].time;
            }
		}
	
		iters--;
        if (iters == 0) {
            iters = 5;
            block_size *= 2;
            if (rand_flag) {
                block_size /= 2;
                rand_flag = 0;
            }
        }

        printf  ("%d, %lu, %d, %lu\n", rec_acked, time_acked, rec_pers, time_pers);
	}
	
    // Cleanup Phase
    free(buf);
    free(buf_test);
	printf("normal exit\n");
	relay_cleanup (ftdi);
    close(device);
    close(outfile);
    return;
}

