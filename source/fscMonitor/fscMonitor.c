/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
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

#define FSC_DEBUG_FILE "/nvram/forceFSC"

typedef enum {
    LOG_SEV_ERROR,
    LOG_SEV_WARN,
    LOG_SEV_INFO
} eLogSeverity;

#ifdef FEATURE_SUPPORT_RDKLOG
#include "ccsp_trace.h"
char compName[25]="LOG.RDK.FSC";
#define DEBUG_INI_NAME  "/etc/debug.ini"
#define FSC_LOG(x, ...) { if((x)==(LOG_SEV_INFO)){CcspTraceInfo((__VA_ARGS__));}else if((x)==(LOG_SEV_WARN)){CcspTraceWarning((__VA_ARGS__));}else if((x)==(LOG_SEV_ERROR)){CcspTraceError((__VA_ARGS__));} }
#else
// Log macros
#define FSC_LOG(x, fmt, args...) \
    { \
        struct tm *gtime; time_t now; \
        char buf[80]; \
        time(&now); gtime = gmtime(&now); \
        strftime(buf,80,"%y%m%d-%H:%M:%S",gtime); \
        fprintf(stderr, "%s [FSC_LOG] %s(), " fmt, \
                buf,__FUNCTION__,##args); fflush(stderr); \
    }
#endif

// We need to put this after the ccsp_trace.h above, since it re-defines CHAR
#include "platform_hal.h"

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
        FSC_LOG(LOG_SEV_ERROR, "Error version.txt file not found! \n");
        return TRUE;
    }

    fp = popen(cmd, "r");
    if (fp == NULL) {
        /* Could not run command should we mark image as bad? */
    	FSC_LOG(LOG_SEV_ERROR, "Error opening command pipe! \n");
    	return TRUE;
    }

    fgets(buf, DATA_SIZE, fp);

    if ( buf[0] != 0 && strcmp(buf, "PROD") == 0 ) {
        FSC_LOG(LOG_SEV_INFO, "Production image detected, FSC check active\n");
    	isProd = TRUE;
    } else {
        FSC_LOG(LOG_SEV_INFO, "Debug/VBN image detected\n");
    }

    if (pclose(fp) != 0) {
        /* Error reported by pclose() */
    	FSC_LOG(LOG_SEV_ERROR, "Error closing command pipe! \n");
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
            FSC_LOG(LOG_SEV_ERROR, "Error opening command pipe! \n");
            return FALSE;
        }

        fgets(buf, DATA_SIZE, fp);

        if (buf[0] != 0 && strlen(buf) > 0) {
            FSC_LOG(LOG_SEV_INFO, "XConf reported a firmware name of %s \n", buf);
            isValid = TRUE;
        } else {
            FSC_LOG(LOG_SEV_WARN, "XConf response exists, but did not respond with a valid firmware image name! \n");
        }

        if (pclose(fp) != 0) {
            /* Error reported by pclose() */
            FSC_LOG(LOG_SEV_ERROR, "Error closing command pipe! \n");
        }
	} else {
        FSC_LOG(LOG_SEV_WARN, "Xconf response file does not exist yet, xconf has not responded \n");
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
    BOOLEAN bValidImage = FALSE;

    struct timespec t1, t2;
    double elapsedTime;

#ifdef FEATURE_SUPPORT_RDKLOG
    pComponentName = compName;
    rdk_logger_init(DEBUG_INI_NAME);
#endif

    FSC_LOG(LOG_SEV_INFO, "Started power manager\n");


    // Tell the hal what the image validation expiry time is.
    platform_hal_SetDeviceCodeImageTimeout(FSC_TIMEOUT_VALUE);

    // Check to see if we have our debug override file in place
    if ((bDebugOverride = doesFileExist(FSC_DEBUG_FILE))) {
        FSC_LOG(LOG_SEV_INFO, "Debug override file /nvram/forceFSC exists, forcing FSC check\n");
    }

    // Check to see if this is a production image
    bIsProduction = isProductionImage();

    if (!bDebugOverride && !bIsProduction) {
        bValidImage = TRUE;
    } else {
        // get start time
        clock_gettime(CLOCK_MONOTONIC, &t1);
        FSC_LOG(LOG_SEV_INFO, "Starting Firmware Sanity Checker Process...\n");
    }

    while(!bValidImage)
    {
        sleep(sampleInterval);

        // get our current delta time
        clock_gettime(CLOCK_MONOTONIC, &t2);
        // compute the elapsed time in seconds
        elapsedTime = TimeSpecToSeconds(&t2) - TimeSpecToSeconds(&t1);

        // FSC_LOG(LOG_SEV_INFO, "Test for valid XConf response at %f seconds \n", elapsedTime);

        // Check to see if we have a valid xconf connection.
        if (!(bValidImage = checkXconfValid()))
        {
            if (elapsedTime >= (double) (FSC_TIMEOUT_VALUE - timeOffset)) // adjust expiry time by 5 minutes
            {
                FSC_LOG(LOG_SEV_INFO, "Time expired waiting for valid xconf connection \n");
                // If we got here our time is expired without getting an xconf connection - fall out and fail
                break;
            }
        }
    }

    // call the platform hal to tell them if this image is valid or not.
    platform_hal_SetDeviceCodeImageValid(bValidImage);

    FSC_LOG(LOG_SEV_INFO, "Firmware Sanity Checker Exit with valid image: %s\n", (bValidImage?"true":"false"));

    return 0;
}
