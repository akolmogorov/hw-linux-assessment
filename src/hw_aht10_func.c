#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/i2c-dev.h>
#include "hw_aht10_func.h"

/* Shared data and sync for threads*/
pthread_mutex_t mutex;
pthread_cond_t cond;
int data_ready = e_gathering;
float temperature;

int validate_inputs(int argc, char **argv) {
    char *cp, *program_name = argv[0];

    // skip over program name
    --argc;
    ++argv;

    if (argc < 3) {
        fprintf(stderr,"\n WARNING %s: not (all) arguments specified", program_name);
        fprintf(stderr,"\n -------> starting server with default value!\n\n", program_name);
        return 0;
    }

    cp = *argv;
    if (*cp == 0) {
        fprintf(stderr,"\n WARNING %s: starting server with default values!\n", program_name);
        return 0;
    }

    return 1;
}

char * get_data_from_sensor(char *device_name, int dev_addr) {
    static unsigned char data[6] = {0};
    int i2c_handler, length, comp_temp;
    float temperature_celsius;

    /* Get I2C handler */
    if ((i2c_handler = open(device_name, O_RDWR)) < 0)
    {
        /* Error getting the file descriptor from device */
        printf("Unable to open I2C device");
        return data;
    }
    /* Calling kernel layer from user space to connect with I2C device */
    if (ioctl(i2c_handler, I2C_SLAVE, dev_addr) < 0)
    {
        /* Bus access failed */
        printf("Unable to connect to low-level device I2C.\n");
        return data;
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
    else {
        return data;
    }
}

float compute_temperature_celsius(char *data) {
    int comp_temp;
    float temperature_celsius;
    comp_temp = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
    temperature_celsius = ((comp_temp*200.0)/1048576) -50.0;
    return temperature_celsius;
}

int start_listener(unsigned short port) {
    int socket_desc;
    struct sockaddr_in server;

    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Unable to create socket.");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );

    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("Unable to bind socket.");
        return 1;
    }

    listen(socket_desc , MAX_SIZE);

    return socket_desc;
}

void *read_sensor(void *arg) {
    SensorInfo *sensor_info = (SensorInfo *)arg;

    char *data = get_data_from_sensor(sensor_info->dev_name, sensor_info->dev_address);

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < 6; ++i) {
        sensor_info->data[i] = data[i];
    }
    data_ready = e_computing;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void *compute_temperature(void *arg) {
    SensorInfo *sensor_info = (SensorInfo *)arg;

    pthread_mutex_lock(&mutex);
    while( data_ready != e_computing ) {
        pthread_cond_wait(&cond, &mutex);
    }

    temperature = compute_temperature_celsius(sensor_info->data);

    data_ready = e_ready;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void send_message(int client_socket) {
    char message[MAX_SIZE];

    sprintf(message, "Honeywell ATH10 Temperature Server\n Current temperature: %.2f°C\n", temperature);
    write(client_socket , message , strlen(message));
}

void *connection_handler(void *socket_desc) {
    int client_socket = *(int*)socket_desc;

    pthread_mutex_lock(&mutex);
    while (data_ready != e_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    send_message(client_socket);
    data_ready = e_gathering;

    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    return NULL;
}
