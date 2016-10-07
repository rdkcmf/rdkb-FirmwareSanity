/******************************************************************************
   Copyright [2016] [Comcast]

   Comcast Proprietary and Confidential

   All Rights Reserved.

   Unauthorized copying of this file, via any medium is strictly prohibited

******************************************************************************/
/*
 * @file fscMonitor.c
 * @brief Firmware Sanity Checker
 *
 * This process will check the status of the Xconf connection and call the platform hal functions
 * with the proper value. The hal functions are listed below:
 *
 * INT platform_hal_SetDeviceCodeImageTimeout(INT seconds);
 * INT platform_hal_SetDeviceCodeImageValid(BOOL flag);
 *
 * From the RDKB standpoint, there is not much for us to do since we do not have access to mark images
 * as valid or not, or to be able to switch banks. We must assume that the OEM Vendor will provide
 * such functions hooked into the above hal calls. In addition, we are assuming that the OEM vendor
 * has a watchdog timer active in the event that this process is not able to be started or respond in
 * the appropriate timeout duration.
 *
 * We will be checking for firmware sanity only on production images and will provide a debug override.
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>

#include "platform_hal.h"

#define FSC_DEBUG_FILE "/nvram/forceFSC"

FILE* debugLogFile;

// Log macros
#define FSC_LOG_INFO(fmt, args...) \
    { \
        struct tm *gtime; time_t now; \
        char buf[80]; \
        time(&now); gtime = gmtime(&now); \
        strftime(buf,80,"%y%m%d-%H:%M:%S",gtime); \
        fprintf(debugLogFile, "%s [RDK_LOG_INFO] %s(), " fmt, \
                buf,__FUNCTION__,##args); fflush(debugLogFile); \
    }

#define FSC_LOG_WARN(fmt,args...) \
    { \
        struct tm *gtime; time_t now; \
        char buf[80]; \
        time(&now); gtime = gmtime(&now); \
        strftime(buf,80,"%y%m%d-%H:%M:%S",gtime); \
        fprintf(debugLogFile, "%s [RDK_LOG_WARN] %s(), " fmt, \
                buf,__FUNCTION__,##args); fflush(debugLogFile); \
    }

#define FSC_LOG_ERROR(fmt,args...) \
    { \
        struct tm *gtime; time_t now; \
        char buf[80]; \
        time(&now); gtime = gmtime(&now); \
        strftime(buf,80,"%y%m%d-%H:%M:%S",gtime); \
        fprintf(debugLogFile, "%s [RDK_LOG_ERROR] %s(), " fmt, \
                buf,__FUNCTION__,##args); fflush(debugLogFile); \
    }

// 60 minute timeout value (in seconds), but we will shift the time by 5 minutes to account for the startup offset.
#define FSC_TIMEOUT_VALUE 60*60

const int sampleInterval = 30;
const int timeOffset = 300;
BOOLEAN bDebugOverride = FALSE;
BOOLEAN bIsProduction = FALSE;

#define DATA_SIZE 1024


/*
 * Check to see if a file exists
 */
BOOLEAN doesFileExist(const char *filename) {
    struct stat st;
    int result = stat(filename, &st);
    return result == 0;
}

/*
 * Check to see if this is a production image
 */
BOOLEAN isProductionImage()
{
	FILE *fp;
    char buf[DATA_SIZE] = {0};
	char cmd[DATA_SIZE] = {0};
	BOOLEAN isProd = FALSE;


    // Check the location of version.txt file
    if (doesFileExist("/fss/gw/version.txt"))
    {
    	sprintf(cmd, "cat /fss/gw/version.txt | grep '^imagename' | sed 's/imagename[:=]//' | cut -f2 -d'_'");
    } else if (doesFileExist("/version.txt")) {
    	sprintf(cmd, "cat /version.txt | grep '^imagename' | sed 's/imagename[:=]//' | cut -f2 -d'_'");
    } else {
        // Version file not found, should we mark this as bad?
        FSC_LOG_ERROR("Error version.txt file not found! \n");
        return TRUE;
    }

    fp = popen(cmd, "r");
    if (fp == NULL) {
        /* Could not run command should we mark image as bad? */
    	FSC_LOG_ERROR("Error opening command pipe! \n");
    	return TRUE;
    }

    fgets(buf, DATA_SIZE, fp);

    if ( buf[0] != 0 && strcmp(buf, "PROD") == 0 ) {
        FSC_LOG_INFO("Production image detected, FSC check active\n");
    	isProd = TRUE;
    } else {
        FSC_LOG_INFO("Debug/VBN image detected\n");
    }

    if (pclose(fp) != 0) {
        /* Error reported by pclose() */
    	FSC_LOG_ERROR("Error closing command pipe! \n");
    }

    return isProd;
}

/*
 * Check XConf response
 */
BOOLEAN validXConfResponse()
{
	FILE *fp;
    char buf[DATA_SIZE] = {0};
	char cmd[DATA_SIZE] = {0};
	BOOLEAN isValid = FALSE;

	if (doesFileExist("/tmp/response.txt")) {
        // We'll check for a couple responses in the file to make sure that we have an actual firmware
	    // filename to fetch.
        // If this is in the file, the xconf server does not recognize us
	    // sprintf(cmd1, "cat /tmp/response.txt | grep -o '404 NOT FOUND'");

	    // Otherwise there should be a firmware name in the file
        sprintf(cmd, "cat /tmp/response.txt | grep -o '\"firmwareFilename[^,]*' | sed 's/\"firmwareFilename\"://'");

        fp = popen(cmd, "r");
        if (fp == NULL) {
            /* Could not run command mark image as bad? */
            FSC_LOG_ERROR("Error opening command pipe! \n");
            return FALSE;
        }

        fgets(buf, DATA_SIZE, fp);

        if (buf[0] != 0 && strlen(buf) > 0) {
            FSC_LOG_INFO("XConf reported a firmware name of %s \n", buf);
            isValid = TRUE;
        } else {
            FSC_LOG_WARN("XConf response exists, but did not respond with a valid firmware image name! \n");
        }

        if (pclose(fp) != 0) {
            /* Error reported by pclose() */
            FSC_LOG_ERROR("Error closing command pipe! \n");
        }
	} else {
        FSC_LOG_WARN("Xconf response file does not exist yet, xconf has not responded \n");
	}

    return isValid;
}

/*
 * Check to see if we've received an XConf response.
 */
BOOLEAN checkXconfValid()
{
    // Fetch xconf response
    BOOLEAN bValidXconf = validXConfResponse();

    // Basically we have to check to see if we have a /tmp/response.txt file. If so, we were
    // able to get a response back from XConf.
    // Only do this check if we are a production image or the nvram debug flag is set.
    if ( (bDebugOverride && bValidXconf) || (bIsProduction && bValidXconf) || (!bDebugOverride && !bIsProduction))
    {
        // XConf connection is valid
    	return TRUE;
    }

    return FALSE;
}

static double TimeSpecToSeconds(struct timespec* ts)
{
    return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

/*
 * Main routine
 */
int main(int argc, char* argv[])
{
    debugLogFile = stderr;
    BOOLEAN bValidImage = FALSE;

    struct timespec t1, t2;
    double elapsedTime;

    int idx = 0;
    for (idx = 0; idx < argc; idx++)
    {
        if ( (strcmp(argv[idx], "-LOGFILE") == 0) )
        {
            // We assume argv[1] is a filename to open
            debugLogFile = fopen( argv[idx + 1], "a+" );

            /* fopen returns 0, the NULL pointer, on failure */
            if ( debugLogFile == 0 )
            {
                debugLogFile = stderr;
                FSC_LOG_WARN("Invalid Entry for -LOGFILE input \n");
            }
            else
            {
                FSC_LOG_INFO("Log File [%s] Opened for Writing in Append Mode \n", argv[idx+1]);
            }
        }
    }

    // Tell the hal what the image validation expiry time is.
    platform_hal_SetDeviceCodeImageTimeout(FSC_TIMEOUT_VALUE);

    // Check to see if we have our debug override file in place
    if ((bDebugOverride = doesFileExist(FSC_DEBUG_FILE))) {
        FSC_LOG_INFO("Debug override file /nvram/forceFSC exists, forcing FSC check\n");
    }

    // Check to see if this is a production image
    bIsProduction = isProductionImage();

    if (!bDebugOverride && !bIsProduction) {
        bValidImage = TRUE;
    } else {
        // get start time
        clock_gettime(CLOCK_MONOTONIC, &t1);
        FSC_LOG_INFO("Starting Firmware Sanity Checker Process...\n");
    }

    while(!bValidImage)
    {
        sleep(sampleInterval);

        // get our current delta time
        clock_gettime(CLOCK_MONOTONIC, &t2);
        // compute the elapsed time in seconds
        elapsedTime = TimeSpecToSeconds(&t2) - TimeSpecToSeconds(&t1);

        // FSC_LOG_INFO("Test for valid XConf response at %f seconds \n", elapsedTime);

        // Check to see if we have a valid xconf connection.
        if (!(bValidImage = checkXconfValid()))
        {
            if (elapsedTime >= (double) (FSC_TIMEOUT_VALUE - timeOffset)) // adjust expiry time by 5 minutes
            {
                FSC_LOG_INFO("Time expired waiting for valid xconf connection \n");
                // If we got here our time is expired without getting an xconf connection - fall out and fail
                break;
            }
        }
    }

    // call the platform hal to tell them if this image is valid or not.
    platform_hal_SetDeviceCodeImageValid(bValidImage);

    FSC_LOG_INFO("Firmware Sanity Checker Exit with valid image: %s\n", (bValidImage?"true":"false"));
    if(debugLogFile)
    {
        fclose(debugLogFile);
    }

    return 0;
}
