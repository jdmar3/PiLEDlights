/**************************************************************************
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>

 *
 * A network activity light for the Raspberry Pi, using an LED connected to a GPIO pin.
 * Based on hddled.c - http://members.optusnet.com.au/foonly/whirlpool/code/hddled.c -
 * This program uses the WiringPi library by Gordon Henderson - http://wiringpi.com/ - Thanks, Gordon!
 * 
 *
 * To compile:
 *   gcc -Wall -O3 -o netledPi netledPi.c -lwiringPi
 *
 * Options:
 * -d, --detach               Detach from terminal (become a daemon)
 * -p, --pin=VALUE            GPIO pin (using wiringPi numbering scheme) where LED is connected (default: 11)
 * -r, --refresh=VALUE        Refresh interval (default: 20 ms)
 *
 * GPIO pin ----|>|----[330]----+
 *              LED             |
 *                             ===
 *                            Ground
 *
 * Default LED Pin - wiringPi pin 11 is BCM_GPIO 7, physical pin 26 on the Pi's P1 header.
 * Note: This pin is also used for the SPI interface. If you have SPI add-ons connected,
 * you'll have to use the -p option to change it to another, unused pin.
 */


#define NETDEVICES "/proc/net/dev"


#define _GNU_SOURCE

#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pigpio.h>


static unsigned int o_refresh = 20; /* milliseconds */
static unsigned int o_gpiopin_tx = 2;
static unsigned int o_gpiopin_rx = 3;
static int o_detach = 0;

static volatile sig_atomic_t running = 1;
static char *line = NULL;
static size_t len = 0;

/* Update the TX LED */
void tx_led(int on) {
        static int tx_current = 1; /* Ensure the LED turns off on first call */
        if (tx_current == on)
                return;

        if (on)
		gpioWrite( o_gpiopin_tx, PI_HIGH );
        else
                gpioWrite( o_gpiopin_tx, PI_LOW );

        tx_current = on;
}

/* Update the RX LED */
void rx_led(int on) {
        static int rx_current = 1; /* Ensure the LED turns off on first call */
        if (rx_current == on)
                return;

        if (on)
		gpioWrite( o_gpiopin_rx, PI_HIGH );
        else
                gpioWrite( o_gpiopin_rx, PI_LOW );

        rx_current = on;
}

/* Reread the netdevices file */
int activity(FILE *netdevices) {
        static unsigned int prev_inpackets, prev_outpackets;
        unsigned int inpackets, outpackets;
        unsigned int device_inpackets, device_outpackets;
        int found_inpackets, found_outpackets;
        int result;
        char *ptr;
        char device[32];

        /* Go to the beginning of the netdevices file */
        result = TEMP_FAILURE_RETRY(fseek(netdevices, 0L, SEEK_SET));
        if (result) {
                perror("Could not rewind " NETDEVICES);
                return result;
        }

        /* Clear glibc's buffer */
        result = TEMP_FAILURE_RETRY(fflush(netdevices));
        if (result) {
                perror("Could not flush input stream");
                return result;
        }

        /* Extract the I/O stats */
        inpackets = outpackets = 0;
        found_inpackets = found_outpackets = 0;
        device_inpackets = device_outpackets = 0;
        while (getline(&line, &len, netdevices) != -1 && errno != EINTR) {
                ptr = line;
                while (*ptr == ' ') ptr++; // Skip leading spaces
                if (sscanf(ptr, "%s %*u %u %*u %*u %*u %*u %*u %*u %*u %u %*u %*u %*u %*u %*u %*u", device, &device_inpackets, &device_outpackets)) {
                        if(strstr(device, "lo:")) continue; // Skip loopback interface
                        found_inpackets++;
                        found_outpackets++;
                        inpackets += device_inpackets;
                        outpackets += device_outpackets;
                }

        }
        if (!found_inpackets || !found_outpackets) {
                fprintf(stderr, "Could not find required lines in " NETDEVICES);
                return -1;
        }

        /* Anything changed? */
        if( prev_inpackets  != inpackets )
                rx_led( PI_HIGH );
        else
                rx_led( PI_LOW );
        
        if( prev_outpackets != outpackets )
                tx_led( PI_HIGH );
        else
                tx_led( PI_LOW );

        prev_inpackets = inpackets;
        prev_outpackets = outpackets;

        return 0;
}



/* Signal handler -- break out of the main loop */
void shutdown(int sig) {
        running = 0;
}

/* Argp parser function */
error_t parse_options(int key, char *arg, struct argp_state *state) {
        switch (key) {
        case 'd':
                o_detach = 1;
                break;
        case 'r':
                o_refresh = strtol(arg, NULL, 10);
                if (o_refresh < 10)
                        argp_failure(state, EXIT_FAILURE, 0,
                                "refresh interval must be at least 10");
                break;
        case 3:
                o_gpiopin_tx = strtol(arg, NULL, 10);
                if ((o_gpiopin_tx < 0) || (o_gpiopin_tx > 29))
                        argp_failure(state, EXIT_FAILURE, 0,
                                "pin number must be between 0 and 29");
                break;
        case 4:
                o_gpiopin_rx = strtol(arg, NULL, 10);
                if ((o_gpiopin_rx < 0) || (o_gpiopin_rx > 29))
                        argp_failure(state, EXIT_FAILURE, 0,
                                "pin number must be between 0 and 29");
                break;
        }
        return 0;
}

int main(int argc, char **argv) {
        struct argp_option options[] = {
                { "detach",  'd',      NULL, 0, "Detach from terminal" },
                { "tx" ,     0,   "VALUE", 0, "GPIO pin where LED is connected (transmit data) (default: BCM 2 physical pin 3 on the P1 header)" },
                { "rx",      1,   "VALUE", 0, "GPIO pin where LED is connected (receive data) (default: BCM 3 physical pin 5 on the P1 header)" },
                { "refresh", 'r',   "VALUE", 0, "Refresh interval (default: 20 ms)" },
                { 0 },
        };
        struct argp parser = {
                NULL, parse_options, NULL,
                "Show network activity using an LED wired to a GPIO pin.",
                NULL, NULL, NULL
        };
        int status = EXIT_FAILURE;
        FILE *netdevices = NULL;
        struct timespec delay;

        /* Parse the command-line */
        parser.options = options;
        if (argp_parse(&parser, argc, argv, ARGP_NO_ARGS, NULL, NULL))
                goto out;

        delay.tv_sec = o_refresh / 1000;
        delay.tv_nsec = 1000000 * (o_refresh % 1000);

	/* If we can't set up pigpio, then just bail */
	if( gpioInitialise() < 0 ) {
		fprintf( stderr, "Unable to setup the piGPIO library. STOP."); 
		return -1;
	}
	gpioSetMode( o_gpiopin_rx, PI_OUTPUT );
	gpioSetMode( o_gpiopin_tx, PI_OUTPUT );


        /* Open the netdevices file */
        netdevices = fopen(NETDEVICES, "r");
        if (!netdevices) {
                perror("Could not open " NETDEVICES " for reading");
                goto out;
        }

        /* Ensure the LED is off */
        tx_led(0);
        rx_led(0);

        /* Save the current I/O stat values */
        if (activity(netdevices) < 0)
                goto out;

        /* Detach from terminal? */
        if (o_detach) {
                pid_t child = fork();
                if (child < 0) {
                        perror("Could not detach from terminal");
                        goto out;
                }
                if (child) {
                        /* I am the parent */
                        status = EXIT_SUCCESS;
                        goto out;
                }
        }

        /* We catch these signals so we can clean up */
        {
                struct sigaction action;
                memset(&action, 0, sizeof(action));
                action.sa_handler = shutdown;
                sigemptyset(&action.sa_mask);
                action.sa_flags = 0; /* We block on usleep; don't use SA_RESTART */
                sigaction(SIGHUP, &action, NULL);
                sigaction(SIGINT, &action, NULL);
                sigaction(SIGTERM, &action, NULL);
        }

        /* Loop until signal received */
        while (running) {
                if (nanosleep(&delay, NULL) < 0)
                        break;
                if( activity(netdevices) < 0 )
                        break;
        }

        /* Ensure the LED is off */
        tx_led(0);
        rx_led(0);

        status = EXIT_SUCCESS;

out:
	/* Halt any library functions */
	gpioTerminate();

        if (line) free(line);
        if (netdevices) fclose(netdevices);
        return status;
}

