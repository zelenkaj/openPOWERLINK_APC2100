/**
********************************************************************************
\file   main.c

\brief  Main file of console MN demo application

This file contains the main file of the openPOWERLINK MN console demo
application.

\ingroup module_demo_mn_console
*******************************************************************************/

/*------------------------------------------------------------------------------
Copyright (c) 2014, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
Copyright (c) 2013, SYSTEC electronic GmbH
Copyright (c) 2013, Kalycito Infotech Private Ltd.All rights reserved.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holders nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------*/

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include <oplk/oplk.h>
#include <oplk/debugstr.h>

#include <system/system.h>
#include <getopt/getopt.h>
#include <console/console.h>

#include "app.h"
#include "event.h"

//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------
#define CYCLE_LEN         UINT_MAX
#define NODEID            0xF0                //=> MN
#define IP_ADDR           0xc0a86401          // 192.168.100.1
#define SUBNET_MASK       0xFFFFFF00          // 255.255.255.0
#define DEFAULT_GATEWAY   0xC0A864FE          // 192.168.100.C_ADR_RT1_DEF_NODE_ID

//------------------------------------------------------------------------------
// module global vars
//------------------------------------------------------------------------------
const BYTE aMacAddr_g[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
extern UINT   cnt_g;
char fGenerateLogs = FALSE;
static tCommInstance   commInstance_l;
//------------------------------------------------------------------------------
// global function prototypes
//------------------------------------------------------------------------------

//============================================================================//
//            P R I V A T E   D E F I N I T I O N S                           //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// local types
//------------------------------------------------------------------------------
typedef struct
{
    char        cdcFile[256];
    char*       pLogFile;
    UINT        cycleLen;
    UINT        appCycle;
} tOptions;

typedef enum 
{
    kExitUnknown = 0,
    kExitCycleError,
    kExitResetFailure,
    kExitHeartBeat,
    kExitGsOff,
    kExitNoSync,
    kExitUser = 0xFF,
} eAppExitReturn;

typedef UINT tAppExitReturn;

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------
static HANDLE      errorCounterThread_l;
static BOOL        fThreadExit = FALSE;

//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------
static int getOptions(int argc_p, char** argv_p, tOptions* pOpts_p);
static tOplkError initPowerlink(UINT32 cycleLen_p, char* pszCdcFileName_p,
                                const BYTE* macAddr_p);
static UINT loopMain(void);
static void shutdownPowerlink(void);
static DWORD WINAPI errorCounterThread(LPVOID arg_p);

//============================================================================//
//            P U B L I C   F U N C T I O N S                                 //
//============================================================================//

//------------------------------------------------------------------------------
/**
\brief  main function

This is the main function of the openPOWERLINK console MN demo application.

\param  argc                    Number of arguments
\param  argv                    Pointer to argument strings

\return Returns an exit code

\ingroup module_demo_mn_console
*/
//------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    tOplkError          ret = kErrorOk;
    tOptions            opts;
    UINT32              version;
    tAppExitReturn      appRet;
    time_t              timeStamp;
    struct tm*          p_timeVal;
    char                dateStr[20];
    char                timeStr[20];

    memset(&opts, 0, sizeof(tOptions));

    fGenerateLogs = FALSE; // default

    getOptions(argc, argv, &opts);

    if (system_init() != 0)
    {
        fprintf(stderr, "Error initializing system!");
        return 0;
    }

    if (opts.pLogFile != NULL)
        console_init(opts.pLogFile);
    else
    {
        time(&timeStamp);
        p_timeVal = localtime(&timeStamp);
        opts.pLogFile = malloc(256);
        strftime(dateStr, 20, "%Y%m%d", p_timeVal);
        strftime(timeStr, 20, "%H%M%S", p_timeVal); 
        sprintf(opts.pLogFile, "log_%s_%s.txt", dateStr, timeStr);
        console_init(opts.pLogFile);
    }

    memset(&commInstance_l, 0, sizeof(tCommInstance));

    initEvents(&commInstance_l);

    version = oplk_getVersion();
    printf("----------------------------------------------------\n");
    printf("openPOWERLINK console MN DEMO application\n");
    printf("using openPOWERLINK Stack: %x.%x.%x\n", PLK_STACK_VER(version), PLK_STACK_REF(version), PLK_STACK_REL(version));
    printf("----------------------------------------------------\n");

    if (opts.cycleLen > 0)
        commInstance_l.cycleLen = opts.cycleLen;
    else
        commInstance_l.cycleLen = CYCLE_LEN;

    commInstance_l.appCycle = opts.appCycle;

    if ((ret = initPowerlink(commInstance_l.cycleLen, opts.cdcFile, aMacAddr_g)) != kErrorOk)
        goto Exit;

    if ((ret = initApp(&commInstance_l)) != kErrorOk)
        goto Exit;

    // TODO implement a separate thread to monitor ProcessSync

    errorCounterThread_l = CreateThread(NULL,  // Default security attributes
                                0,                        // Use Default stack size
                                errorCounterThread,               // Thread routine
                                NULL,                     // Argum to the thread routine
                                0,                        // Use default creation flags
                                NULL                      // Returned thread Id
                                );

    while (1)
    {
        appRet = loopMain();
        if (appRet == kExitUser)
        {
            break;
        }
        else if (appRet == kExitGsOff || appRet == kExitHeartBeat)
        {
            shutdownPowerlink();
            shutdownApp();
            system_msleep(1000);

            if ((ret = initPowerlink(commInstance_l.cycleLen, opts.cdcFile, aMacAddr_g)) != kErrorOk)
                goto ExitFail;
            if ((ret = initApp(&commInstance_l)) != kErrorOk)
                goto ExitFail;
        }
    }


Exit:
    shutdownPowerlink();
    exitEvents();
    shutdownApp();
ExitFail:
    if (ret != kErrorOk)
        printf("Main exited with error 0x%X\n",ret);
    system_exit();

    fThreadExit = TRUE;
    system_msleep(1000);
    CloseHandle(errorCounterThread_l);
    console_exit();
    return 0;
}

//============================================================================//
//            P R I V A T E   F U N C T I O N S                               //
//============================================================================//
/// \name Private Functions
/// \{

//------------------------------------------------------------------------------
/**
\brief  Initialize the openPOWERLINK stack

The function initializes the openPOWERLINK stack.

\param  cycleLen_p              Length of POWERLINK cycle.
\param  macAddr_p               MAC address to use for POWERLINK interface.

\return The function returns a tOplkError error code.
*/
//------------------------------------------------------------------------------
static tOplkError initPowerlink(UINT32 cycleLen_p, char* pszCdcFileName_p,
                                const BYTE* macAddr_p)
{
    tOplkError                  ret = kErrorOk;
    static tOplkApiInitParam    initParam;
    static char                 devName[128];

    printf("Initializing openPOWERLINK stack...\n");

    memset(&initParam, 0, sizeof(initParam));
    initParam.sizeOfInitParam = sizeof(initParam);

    // pass selected device name to Edrv
    initParam.hwParam.pDevName = devName;
    initParam.nodeId = NODEID;
    initParam.ipAddress = (0xFFFFFF00 & IP_ADDR) | initParam.nodeId;

    /* write 00:00:00:00:00:00 to MAC address, so that the driver uses the real hardware address */
    memcpy(initParam.aMacAddress, macAddr_p, sizeof(initParam.aMacAddress));

    initParam.fAsyncOnly              = FALSE;
    initParam.featureFlags            = UINT_MAX;
    initParam.cycleLen                = cycleLen_p;       // required for error detection
    initParam.isochrTxMaxPayload      = 256;              // const
    initParam.isochrRxMaxPayload      = 256;              // const
    initParam.presMaxLatency          = 50000;            // const; only required for IdentRes
    initParam.preqActPayloadLimit     = 36;               // required for initialisation (+28 bytes)
    initParam.presActPayloadLimit     = 36;               // required for initialisation of Pres frame (+28 bytes)
    initParam.asndMaxLatency          = 150000;           // const; only required for IdentRes
    initParam.multiplCylceCnt         = 0;                // required for error detection
    initParam.asyncMtu                = 1500;             // required to set up max frame size
    initParam.prescaler               = 2;                // required for sync
    initParam.lossOfFrameTolerance    = 500000;
    initParam.asyncSlotTimeout        = 3000000;
    initParam.waitSocPreq             = 1000;
    initParam.deviceType              = UINT_MAX;         // NMT_DeviceType_U32
    initParam.vendorId                = UINT_MAX;         // NMT_IdentityObject_REC.VendorId_U32
    initParam.productCode             = UINT_MAX;         // NMT_IdentityObject_REC.ProductCode_U32
    initParam.revisionNumber          = UINT_MAX;         // NMT_IdentityObject_REC.RevisionNo_U32
    initParam.serialNumber            = UINT_MAX;         // NMT_IdentityObject_REC.SerialNo_U32

    initParam.subnetMask              = SUBNET_MASK;
    initParam.defaultGateway          = DEFAULT_GATEWAY;
    sprintf((char*)initParam.sHostname, "%02x-%08x", initParam.nodeId, initParam.vendorId);
    initParam.syncNodeId              = C_ADR_SYNC_ON_SOA;
    initParam.fSyncOnPrcNode          = FALSE;

    // set callback functions
    initParam.pfnCbEvent = processEvents;

#if defined(CONFIG_KERNELSTACK_DIRECTLINK)
    initParam.pfnCbSync  = processSync;
#else
    initParam.pfnCbSync  = NULL;
#endif

    // initialize POWERLINK stack
    ret = oplk_init(&initParam);
    if (ret != kErrorOk)
    {
        fprintf(stderr, "oplk_init() failed with \"%s\" (0x%04x)\n", debugstr_getRetValStr(ret), ret);
        return ret;
    }

    ret = oplk_setCdcFilename(pszCdcFileName_p);
    if (ret != kErrorOk)
    {
        fprintf(stderr, "oplk_setCdcFilename() failed with \"%s\" (0x%04x)\n", debugstr_getRetValStr(ret), ret);
        return ret;
    }

    return kErrorOk;
}

//------------------------------------------------------------------------------
/**
\brief  Main loop of demo application

This function implements the main loop of the demo application.
- It creates the sync thread which is responsible for the synchronous data
  application.
- It sends a NMT command to start the stack
- It loops and reacts on commands from the command line.
*/
//------------------------------------------------------------------------------
static tAppExitReturn loopMain(void)
{
    tOplkError              ret = kErrorOk;
    char                    cKey = 0;
    BOOL                    fExit = FALSE;
    tAppExitReturn          appReturn = kExitUnknown;
    static BOOL             startSync = TRUE;

#if !defined(CONFIG_KERNELSTACK_DIRECTLINK)

#if defined(CONFIG_USE_SYNCTHREAD)
    if (startSync)
    {
        system_startSyncThread(processSync);
        startSync = FALSE;
    }

#endif

#endif

    // start stack processing by sending a NMT reset command
    ret = oplk_execNmtCommand(kNmtEventSwReset);
    if (ret != kErrorOk)
    {
        return kExitResetFailure;
    }

    printf("\n-------------------------------\n");
    printf("Press Esc to leave the program\n");
    printf("Press r to reset the node\n");
    printf("Press p to clear error counters\n");
    printf("-------------------------------\n\n");

    while (!fExit)
    {
        if (console_kbhit())
        {
            cKey = (char)console_getch();
            switch (cKey)
            {
                case 'r':
                    ret = oplk_execNmtCommand(kNmtEventSwReset);
                    if (ret != kErrorOk)
                    {
                        appReturn = kExitResetFailure;
                        fExit = TRUE;
                    }
                    break;

                case 'c':
                    ret = oplk_execNmtCommand(kNmtEventNmtCycleError);
                    if (ret != kErrorOk)
                    {
                        appReturn = kExitResetFailure;
                        fExit = TRUE;
                    }
                    break;
                case 'p':
                    // clear all counters
                    memset(&commInstance_l.errorCounter, 0, sizeof(commInstance_l.errorCounter));
                    break;
                case 0x1B:
                    appReturn = kExitUser;
                    fExit = TRUE;
                    break;

                default:
                    break;
            }
        }

        if (appReturn == kExitUser)
        {
            goto Exit;
        }

        if (system_getTermSignalState() == TRUE)
        {
            fExit = TRUE;
            appReturn = kExitUser;
            printf("Received termination signal, exiting...\n");
            goto Exit;
        }

        if (oplk_checkKernelStack() == FALSE)
        {
            commInstance_l.errorCounter.heartBeatError++;
            fExit = TRUE;
            appReturn = kExitHeartBeat;
            fprintf(stderr, "Kernel stack has gone! Exiting...\n");
        }

        if (commInstance_l.errorFlags.fCycleError)
        {
            fExit = TRUE;
            commInstance_l.errorCounter.cycleError++;
            commInstance_l.errorFlags.fCycleError = FALSE;
            appReturn = kExitCycleError;
            fprintf(stderr, "Cycle error ocurred Exiting...\n");
        }

        if (commInstance_l.errorFlags.fGsOff)
        {
            fExit = TRUE;
            commInstance_l.errorFlags.fGsOff = FALSE;
            appReturn = kExitGsOff;
            fprintf(stderr, "GSOFF Exiting...\n");
        }
#if defined(CONFIG_USE_SYNCTHREAD) || defined(CONFIG_KERNELSTACK_DIRECTLINK)
        system_msleep(100);
#else
        processSync();
#endif

    }

Exit:
#if (TARGET_SYSTEM == _WIN32_)
    printf("Press Enter to quit!\n");
    //console_getch();
    return appReturn;
#endif

}

//------------------------------------------------------------------------------
/**
\brief  Shutdown the demo application

The function shuts down the demo application.
*/
//------------------------------------------------------------------------------
static void shutdownPowerlink(void)
{
    UINT                i;

    // NMT_GS_OFF state has not yet been reached
    commInstance_l.errorFlags.fGsOff = FALSE;

#if !defined(CONFIG_KERNELSTACK_DIRECTLINK) && defined(CONFIG_USE_SYNCTHREAD)
    system_stopSyncThread();
    system_msleep(100);
#endif

    // halt the NMT state machine so the processing of POWERLINK frames stops
    oplk_execNmtCommand(kNmtEventSwitchOff);

    // small loop to implement timeout waiting for thread to terminate
    for (i = 0; i < 1000; i++)
    {
        if (commInstance_l.errorFlags.fGsOff)
            break;
    }

    printf("Stack is in state off ... Shutdown\n");
    oplk_shutdown();
}

//------------------------------------------------------------------------------
/**
\brief  Get command line parameters

The function parses the supplied command line parameters and stores the
options at pOpts_p.

\param  argc_p                  Argument count.
\param  argc_p                  Pointer to arguments.
\param  pOpts_p                 Pointer to store options

\return The function returns the parsing status.
\retval 0           Successfully parsed
\retval -1          Parsing error
*/
//------------------------------------------------------------------------------
static int getOptions(int argc_p, char** argv_p, tOptions* pOpts_p)
{
    int                         opt;
    /* setup default parameters */
    strncpy(pOpts_p->cdcFile, "mnobd.cdc", 256);
    pOpts_p->pLogFile = NULL;

    /* get command line parameters */
    while ((opt = getopt(argc_p, argv_p, "c:l:dt:a:")) != -1)
    {
        switch (opt)
        {
            case 'c':
                strncpy(pOpts_p->cdcFile, optarg, 256);
                break;

            case 'l':
                pOpts_p->pLogFile = optarg;
                break;

            case 'd':
                // generate logs
                    fGenerateLogs = TRUE;

                break;
            case 't':
                pOpts_p->cycleLen = strtoul(optarg, NULL, 10);
                break;

            case 'a':
                pOpts_p->appCycle = strtoul(optarg, NULL, 10);
                break;

            default: /* '?' */
                printf("Usage: %s [-c CDC-FILE] [-l LOGFILE] [-t CYCLE_LEN] [-a APP_CYCLE] [-d {TRUE/FALSE}]\n", argv_p[0]);
                return -1;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
/**
\brief  Error counter display thread

This thread is used to displat the error counters.

\param  arg_p     Thread parameter. Not used!

\return The function returns the thread exit code.
*/
//------------------------------------------------------------------------------
static DWORD WINAPI errorCounterThread(LPVOID arg_p)
{
    while (!fThreadExit)
    {
        if (commInstance_l.mnState == kNmtMsOperational)
        {
            system("cls");
            printf("\n-------------------------------\n");
            printf("Press r to reset Node\nPress p to clear counters\nPress ESC to close application");
            printf("\n-------------------------------\n\n");
            printf("Cycles: %d\n", cnt_g);
            printf("Data Errors : %d\n",commInstance_l.errorCounter.dataError);
            printf("Hearbeat Errors: %d\n",commInstance_l.errorCounter.heartBeatError);
            printf("Cycle Errors: %d\n", commInstance_l.errorCounter.cycleError);
            printf("Configuration Errors: %d\n", commInstance_l.errorCounter.confError);
            printf("Stack Errors: %d\n", commInstance_l.errorCounter.stackError);
            printf("NMT Errors: %d\n", commInstance_l.errorCounter.nmtError);
            printf("Node Errors: %d\n", commInstance_l.errorCounter.nodeError);
            
        }
        system_msleep(100);
    }

    printf("Exit Error Counter Thread\n");
    return 0;
}
/// \}
