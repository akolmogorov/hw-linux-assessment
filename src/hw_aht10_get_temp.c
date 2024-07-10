#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <linux/i2c-dev.h>

int validate_inputs(int, char **);
float get_temperature_celsius(char *, int);

int main(int argc, char **argv) {
    /*char *device_name = (char*)"/dev/i2c-1";
    int dev_addr = 0x38;*/
    if( validate_inputs(argc, argv) == 1) {
        char *device_name = argv[1];
        int dev_addr = (int)strtol(argv[2], NULL, 16);
	printf("%0.2f\n", get_temperature_celsius(device_name, dev_addr));
    }
    else {
	    printf("\n Usage: %s device_name device address\n\n", argv[0]);
    }
    return 0;
}

int validate_inputs(int argc, char **argv) {
    char *cp, *program_name = argv[0];

    // skip over program name
    --argc;
    ++argv;

    if (argc < 2) {
        fprintf(stderr,"\n %s: not (all) arguments specified\n", program_name);
        return -1;
    }

    cp = *argv;
    if (*cp == 0) {
        fprintf(stderr,"\n %s: argument an empty string\n", program_name);
        return -1;
    }

    return 1;
}

float get_temperature_celsius(char *device_name, int dev_addr) {
    unsigned char data[6] = {0};
    int i2c_handler, length, comp_temp;
    float temperature_celsius;

    /* Get I2C handler */
    if ((i2c_handler = open(device_name, O_RDWR)) < 0)
    {
        /* Error getting the file descriptor from device */
        printf("Unable to open I2C device");
        return -1;
    }
    /* Calling kernel layer from user space to connect with I2C device */
    if (ioctl(i2c_handler, I2C_SLAVE, dev_addr) < 0)
    {
        /* Bus access failed */
        printf("Unable to connect to low-level device I2C.\n");
        return -1;
    }

    /* Handshake (initialization command) according AHT10 sensor datasheet */
    data[0] = 0xE1;
    data[1] = 0x08;
    data[2] = 0x00;
    length = 3;
    /* Send initialization command to I2C device */
    if (write(i2c_handler, data, length) != length)
    {
        /* Unable to handshake with i2c device */
        printf("Unable to handshake the i2c bus.\n");
    }

    /* Wait 20ms before proceeding with the next write command */
    sleep(0.02);

    /* Trigger measurement (request command) acc. AHT10 sensor datasheet */
    data[0] = 0xAC;
    data[1] = 0x33;
    data[2] = 0x00;
    length = 3;
    /* Send trigger measurement command to I2C device */
    if (write(i2c_handler, data, length) != length)
    {
        /* Unable to request data from I2C device */
        printf("Unable to request data from the I2C bus.\n");
    }

    /* Wait 20ms before proceeding with the next read command */
    sleep(0.02);

    /* Read the 6 bytes answer from I2C */
    length = 6;
    if (read(i2c_handler, data, length) != length)
    {
        /* Unable to get data from I2C device */
        printf("Unable to read output temp. from the i2c AHT10 sensor.\n");
    }
    else
    {
        comp_temp = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
        temperature_celsius = ((comp_temp*200.0)/1048576) -50.0;
    }
    return temperature_celsius;
}
