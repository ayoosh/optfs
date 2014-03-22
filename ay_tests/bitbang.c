/* This program is distributed under the GPL, version 2 */

#define LOCAL_NSEC ((unsigned long long) ((end.tv_nsec - start.tv_nsec) + ((end.tv_sec - start.tv_sec) * NSEC_PER_SEC)))
#define USEC_PER_SEC 1000000
#define NSEC_PER_SEC 1000000000
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libftdi1/ftdi.h>
#include <time.h>

int main(int argc, char **argv)
{
    struct ftdi_context *ftdi;
    int f,i;
    unsigned char buf[1];
    int retval = 0;
	struct timespec start, end;

    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }

    f = ftdi_usb_open(ftdi, 0x0403, 0x6001);

    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        retval = 1;
        goto done;
    }

    printf("ftdi open succeeded: %d\n",f);

    printf("enabling bitbang mode\n");
    ftdi_set_bitmode(ftdi, 0xFF, BITMODE_BITBANG);

    usleep(3 * 1000000);

    buf[0] = 0xFF;
    printf("turning everything on\n");
    f = ftdi_write_data(ftdi, buf, 1);
    if (f < 0) {
        printf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, ftdi_get_error_string(ftdi));
    }

//    usleep(3 * 1000000); 
    getchar();

    buf[0] = 0x0;
    printf("turning everything off\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    f = ftdi_write_data(ftdi, buf, 1);
    if (f < 0)
    {
        printf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, ftdi_get_error_string(ftdi));
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("\n%15llu\n", LOCAL_NSEC);
/*
    usleep(3 * 1000000);

    for (i = 0; i < 32; i++)
    {
        buf[0] =  0 | (0xFF ^ 1 << (i % 8));
        if ( i > 0 && (i % 8) == 0)
        {
            printf("\n");
        }
        printf("%02hhx ",buf[0]);
        fflush(stdout);
        f = ftdi_write_data(ftdi, buf, 1);
        if (f < 0)
        {
            fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, ftdi_get_error_string(ftdi));
        }
        usleep(1 * 1000000);
    }
*/
    printf("\n");

    printf("disabling bitbang mode\n");
    ftdi_disable_bitbang(ftdi);

    ftdi_usb_close(ftdi);
done:
    ftdi_free(ftdi);

    return retval;
}
