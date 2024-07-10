#ifndef HONEYWELL_ASSESSMENT_HW_AHT10_FUNC_H
#define HONEYWELL_ASSESSMENT_HW_AHT10_FUNC_H

#define MAX_SIZE 256

typedef struct {
    char dev_name[MAX_SIZE];
    int dev_address;
    char data[6];
} SensorInfo;

extern SensorInfo sensor_info;
extern pthread_mutex_t mutex;
extern pthread_cond_t cond;

typedef enum  {e_gathering, e_computing, e_ready} threads_states;

int validate_inputs(int, char **);
int start_listener(unsigned short);
char * get_data_from_sensor(char *, int);
float compute_temperature_celsius(char *);
void send_message(int);
void *read_sensor(void *);
void *compute_temperature(void *);
void *connection_handler(void *);

#endif //HONEYWELL_ASSESSMENT_HW_AHT10_FUNC_H
