#include <string.h>
#include <stdio.h>
#include <time.h>
#include "localtime.h"

#include <switch.h>


struct consLoc {
    char str[0x24];
};

struct consLoc consoleLocation;

static Service g_timeSrv;
static Service g_timeUserSystemClock;
static Service g_timeNetworkSystemClock;
static Service g_timeTimeZoneService;
static Service g_timeLocalSystemClock;



static Result _timeGetSession(Service* srv_out, u64 cmd_id) {
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = cmd_id;

    Result rc = serviceIpcDispatch(&g_timeSrv);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
        } *resp = r.Raw;

        rc = resp->result;

        if (R_SUCCEEDED(rc)) {
            serviceCreate(srv_out, r.Handles[0]);
        }
    } 

    return rc;
}

Result localTimeInit() {
    if (serviceIsActive(&g_timeSrv))
        return 0;

    Result rc;

    rc = smGetService(&g_timeSrv, "time:s");
    if (R_FAILED(rc))
        rc = smGetService(&g_timeSrv, "time:u");

    if (R_FAILED(rc))
        return rc;

    rc = _timeGetSession(&g_timeUserSystemClock, 0);

    if (R_SUCCEEDED(rc))
        rc = _timeGetSession(&g_timeNetworkSystemClock, 1);

    if (R_SUCCEEDED(rc))
        rc = _timeGetSession(&g_timeTimeZoneService, 3);

    if (R_SUCCEEDED(rc))
        rc = _timeGetSession(&g_timeLocalSystemClock, 4);

    if (R_FAILED(rc))
        timeExit();

    return rc;
}

void localTimeExit() {
    serviceClose(&g_timeLocalSystemClock);
    serviceClose(&g_timeTimeZoneService);
    serviceClose(&g_timeNetworkSystemClock);
    serviceClose(&g_timeUserSystemClock);
    serviceClose(&g_timeSrv);
}


Result getConsoleLocationName() {
    
    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 0;

    Result rc = serviceIpcDispatch(&g_timeTimeZoneService);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        ipcParse(&r);

        struct {
            u64 magic;
            u64 result;
            struct consLoc locationString;
        } *resp = r.Raw;

        rc = resp->result;

        if (R_SUCCEEDED(rc)) {
            consoleLocation = resp->locationString;
            //memcpy(consoleLocation, resp->locationString, 0x10);
        }
    }

    return rc;
}


void setupLocalTimeOffset() {
    timeExit();
    // Free up the service-handles that are needed.
    localTimeInit();
    getConsoleLocationName();
    //printf("Location is: %s\n", consoleLocation.str);
    localTimeExit();
    timeInitialize();
    // Reinitialize the time.h svc-handlers.

    for(int i = 0; i < 447; i++) {
        if(!strcmp(timezones[i], consoleLocation.str)) {
            offset = timezoneOffsets[i];
        }
    }
}


struct tm* getRealLocalTime() {

    time_t unixTime = time(NULL);

    struct tm* timeStruct = gmtime((const time_t *)&unixTime); //Gets UTC time.

    timeStruct->tm_sec += offset*3600;
    mktime(timeStruct);


    return timeStruct;
}