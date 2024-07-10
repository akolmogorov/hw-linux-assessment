/// @file hw_aht10_temperature_server.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "hw_aht10_func.h"

pthread_mutex_t mutex; /*!< Mutex for thread synchronization (prevents deadlocks) */
pthread_cond_t cond; /*!< Condition variable for thread synchronization (prevents race conditions) */
SensorInfo sensor_info; /*!< Sensor information structure */

/**
 * \brief
 *        In this section the TCP server binds connections with
 *        clients using threads for:\n
 *          + 1) Getting data from AHT10 sensor\n
 *          + 2) Computing the current temperature, and \n
 *          + 3) Attending new temperature requests via TCP.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status
 */
int main(int argc, char **argv) {
    int socket_desc, client_sock;
    /** \brief TCP port is 5000 by default */
    unsigned short server_port = 5000;
    struct sockaddr_in client_addr;
    socklen_t client_len;

    /**
     * \brief Default values for I2C hardware:
     * + The device name by default is the file descriptor we found at /dev/i2c-1 and,
     * + The device address for the AHT10 sensor is 0x38.<br><br>
     *
     * NOTE: These values can be modified according to the connected sensor and the board
     *       when executing the program via the parameter <em>argv</em>.
     */
    sprintf(sensor_info.dev_name,"/dev/i2c-1");
    sensor_info.dev_address = 0x38;

    if (validate_inputs(argc, argv) == 1) {
        sprintf(sensor_info.dev_name,"%s", argv[1]);
        sensor_info.dev_address = (int)strtol(argv[2], NULL, 16);
        server_port = (int)strtol(argv[3], NULL, 10);
    }

    if ((socket_desc = start_listener(server_port)) < 0) {
        printf("Unable to open socket at %d", server_port);
        exit(-1);
    }
    /**
     * \brief Main loop:
     * + Once a client connection is accepted, three threads are created
     *   as mentioned above including the mutex and cond sections for
     *   <b>avoiding race conditions and/or deadlocks</b>.
     */
    while ((client_sock = accept(socket_desc,
                                 (struct sockaddr *)&client_addr,
                                 (socklen_t*)&client_len))) {
        pthread_t thread1, thread2, thread3;

        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);

        pthread_create(&thread1, NULL, read_sensor, &sensor_info);
        pthread_create(&thread2, NULL, compute_temperature, &sensor_info);
        pthread_create(&thread3, NULL, connection_handler, &client_sock);

        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);
        pthread_join(thread3, NULL);

        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }
    return 0;
}
