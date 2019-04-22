/* definitions */
#define SHM_KEY     2307
#define SHM_SIZE    sizeof(SensorTemperature)

#define THERMOMETER_MSG_BYTE_CNT    21
#define SENSOR_ID_BYTE_CNT          8

/* typedefs */
typedef unsigned int    uint;
typedef unsigned short  ushort;
typedef unsigned char   byte;

typedef struct
{
    byte bSensorId[8];
    ushort usTempA;
    ushort usTempB;
} SensorTemperature;
