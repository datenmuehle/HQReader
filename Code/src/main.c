#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include "wiringPi.h"

#include "433MHzTR_API.h"

#define GPIO_PIN            2

#define BIT_DURATION_US     1500
#define BIT_0HIGH_US        500
#define BIT_1HIGH_US        1000
#define BIT_RAMP_TIME_US    200

#define INVALID_TEMP_RESET_TIME_S    300

#define MSG_BIT_CNT ((THERMOMETER_MSG_BYTE_CNT * 16)-1)

byte bThermometerMessage[THERMOMETER_MSG_BYTE_CNT];
byte *pThermometerMsg = bThermometerMessage;
byte bByte = 0U, bBitCnt = 0U, bBytePos = 0U, bRun = 1U, bMsgBytePos = 0U, bPrint = 0U;
byte *pSharedBuffer = NULL;
SensorTemperature *pSensorTemp = NULL;

uint uiLastTime = 0UL;

uint uiLastSuccessfulTempRecTime = 0UL;

pthread_mutex_t lock;

static uint uiMessageTDiffs[MSG_BIT_CNT];
static uint uiDiffPos = 0UL;

void sampleISR_TimeDiff(void)
{
    static uint uiLastISRCallTime = 0UL;
    uint now, duration;

    pthread_mutex_lock(&lock);

    now = micros();
    duration = now - uiLastISRCallTime;
    uiLastISRCallTime = now;

    if ((duration > 1900) ||
         (duration < 690))
    {
        //printf("%d\n", uiDiffPos);
        uiDiffPos = 0UL;
        pthread_mutex_unlock(&lock);
        return;
    }

    uiMessageTDiffs[uiDiffPos] = duration;

    if (uiDiffPos < MSG_BIT_CNT)
    {
        ++uiDiffPos;
    }

    pthread_mutex_unlock(&lock);
}

void msgInterpreter(void)
{
    /*       _
     * 0 = _| |__
     * H=834µS, L=1668µS
     *       __
     * 1 = _|  |_
     * H=1668µS, L=834µS
     */

    if (uiDiffPos >= MSG_BIT_CNT)
    {
        int i, msgPos=0, bitPos=0;

        pthread_mutex_lock(&lock);

        memset(bThermometerMessage, 0, sizeof(bThermometerMessage));

        for(i = 0; i < MSG_BIT_CNT; i += 2)
        {
            if (uiMessageTDiffs[i] > 1100)
            {
                bThermometerMessage[msgPos] |= 1 << bitPos;
            }

            ++bitPos;

            if (bitPos >= 8)
            {
                //printf("%002x:", bThermometerMessage[msgPos]);
                bitPos = 0; ++msgPos;
            }
        }

        //printf("\n\n"); fflush(stdout);

        memset(uiMessageTDiffs, 0, sizeof(uiMessageTDiffs));
        uiDiffPos = 0UL;

        pthread_mutex_unlock(&lock);

        if (memcmp(&bThermometerMessage[1], &bThermometerMessage[11], 8) == 0)
        {
            uiLastSuccessfulTempRecTime = (uint)time(NULL);

            memcpy(pSensorTemp->bSensorId, &bThermometerMessage[1], 8);
            pSensorTemp->usTempA = bThermometerMessage[9];
            pSensorTemp->usTempB = bThermometerMessage[19];
        }
    }

}

void sigintHandler(int sig)
{
    signal(sig, SIG_IGN);

    syslog(LOG_INFO, "got SIGINT, shutdown initialized...\n");

    pthread_mutex_lock(&lock);
    bRun = 0;
    pthread_mutex_unlock(&lock);
}

void sigusr1Handler(int sig)
{
    syslog(LOG_INFO, "TempA=%04x, TempB=%04x", pSensorTemp->usTempA, pSensorTemp->usTempB);
}

int main(void)
{
    int result = 0, shmId;
    key_t shmKey = SHM_KEY;

    do
    {
        openlog("433MHzTR", LOG_PID, LOG_LOCAL0);

        syslog(LOG_INFO, "433MHz Reader startet...\n");

        (void)signal(SIGINT, sigintHandler);
        (void)signal(SIGUSR1, sigusr1Handler);

        if (pthread_mutex_init(&lock, NULL) != 0)
        {
            syslog(LOG_ERR, "mutex init failed\n");
            break;
        }

        if (-1 == (shmId = shmget(shmKey, SHM_SIZE, IPC_CREAT | 0666 )))
        {
            result = -1;
            syslog(LOG_ERR, "shmget failed\n");
            break;
        }

        if ((pSharedBuffer = (byte*)shmat(shmId, NULL, 0)) == (void *) -1) {
            result = -1;
            syslog(LOG_ERR, "shmat failed\n");
            break;
        }

        pSensorTemp = (SensorTemperature*)pSharedBuffer;

        (void)memset(pSensorTemp, 0, SHM_SIZE);

        pthread_mutex_lock(&lock);

        (void)memset(pThermometerMsg, 0, THERMOMETER_MSG_BYTE_CNT);

        /* setup lib */
        if (0 > wiringPiSetup())
        {
            result = -2;
            syslog(LOG_ERR, "wiringpi setup failed\n");
            break;
        }

        pinMode(GPIO_PIN, INPUT);
        pullUpDnControl(GPIO_PIN, PUD_DOWN);

        /* install ISR */
        if (0 > wiringPiISR(GPIO_PIN, INT_EDGE_BOTH, sampleISR_TimeDiff))
        {
            result = -3;
            syslog(LOG_ERR, "wiringpi setup ISR failed\n");
            break;
        }

        pthread_mutex_unlock(&lock);

        syslog(LOG_INFO, "Thermometerreader running..\n");

        while(bRun)
        {
            delay(500);

            if (((uint)time(NULL)) > (uiLastSuccessfulTempRecTime + INVALID_TEMP_RESET_TIME_S))
            {
                uiLastSuccessfulTempRecTime = (uint)time(NULL);
                /* reset thermometer values */
                syslog(LOG_INFO, "No valid temp read since %lu\n", uiLastSuccessfulTempRecTime);
            }

            msgInterpreter();
        }

        syslog(LOG_INFO, "Thermometerreader stopped..\n");

    } while(0);

    return result;
}
