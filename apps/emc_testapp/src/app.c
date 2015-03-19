/**
********************************************************************************
\file   app.c

\brief  Demo MN application which implements a running light

This file contains a demo application for digital input/output data.
It implements a running light on the digital outputs. The speed of
the running light is controlled by the read digital inputs.

\ingroup module_demo_mn_console
*******************************************************************************/

/*------------------------------------------------------------------------------
Copyright (c) 2015, Kalycito Infotech Private Ltd.
Copyright (c) 2014, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
Copyright (c) 2013, SYSTEC electronik GmbH

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
#include <oplk/oplk.h>

#include "app.h"
#include "xap.h"

//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------
#define DEFAULT_MAX_CYCLE_COUNT 50      // 6 is very fast
#define APP_LED_COUNT_1         12       // number of LEDs for CN1
#define APP_LED_COUNT_HALF_1    (APP_LED_COUNT_1/2)
#define APP_LED_MASK_1          (1 << (APP_LED_COUNT_1 - 2))
#define APP_LED_MASK_2          (1 << ((APP_LED_COUNT_1/APP_LED_COUNT_HALF_1) - 1))
#define APP_LED_MASK_3          (1 << (APP_LED_COUNT_1 - 1))
#define APP_LED_MASK_4          (0x0A95)                    // 1 & 3 & 5 & 8 & 10 & 12
#define APP_LED_MASK_5          (0x056A)                    // 7 & 9 & 11 & 6 & 4 & 2 
#define MAX_NODES               255
#define DATA_CYLE_DELAY         1
//------------------------------------------------------------------------------
// module global vars
//------------------------------------------------------------------------------
UINT   cnt_g;
extern tErrorCounters errorCounter_g;
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
    UINT            outleds;
    UINT            ledsOld;
    UINT            input;
    UINT            inleds;
    UINT            inputOld;
    UINT            period;
    int             toggle;
    UINT            outLedState;
    UINT            inLedState;
    UINT            dataErrors;
    tNmtState       nmtState;
} APP_NODE_VAR_T;


typedef enum eLedState
{
    kLedUnknown = 0,
    kLedAscending,
    kLedDescending,
    kLedSplitOne,
    kLedSplitTwo,
} tLedState;

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------
static int                  usedNodeIds_l[] = {1, 0};
static APP_NODE_VAR_T       nodeVar_l[MAX_NODES];
static PI_IN*               pProcessImageIn_l;
static PI_OUT*              pProcessImageOut_l;
static UINT                 appCycle_l;
static tCommInstance*       pCommInstance_l;
//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------
static tOplkError initProcessImage(void);

//============================================================================//
//            P U B L I C   F U N C T I O N S                                 //
//============================================================================//

//------------------------------------------------------------------------------
/**
\brief  Initialize the synchronous data application

The function initializes the synchronous data application

\return The function returns a tOplkError error code.

\ingroup module_demo_mn_console
*/
//------------------------------------------------------------------------------
tOplkError initApp(tCommInstance* pCommInstance_p)
{
    tOplkError ret = kErrorOk;
    int        i;

    cnt_g = 0;

    if (pCommInstance_p == NULL)
        return kErrorApiInvalidParam;

    pCommInstance_l = pCommInstance_p;

    for (i = 0; (i < MAX_NODES) && (usedNodeIds_l[i] != 0); i++)
    {
        nodeVar_l[i].outleds = 0;
        nodeVar_l[i].ledsOld = 0;
        nodeVar_l[i].input = 0;
        nodeVar_l[i].inputOld = 0;
        nodeVar_l[i].toggle = 0;
        nodeVar_l[i].period = 0;
        nodeVar_l[i].outLedState = kLedUnknown;
        nodeVar_l[i].inLedState = kLedUnknown;
        nodeVar_l[i].dataErrors = 0;
        nodeVar_l[i].nmtState = kNmtCsBasicEthernet;
    }

    if (pCommInstance_p->appCycle > 0)
        appCycle_l = pCommInstance_p->appCycle;
    else
        appCycle_l = DATA_CYLE_DELAY;

    ret = initProcessImage();

    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Shutdown the synchronous data application

The function shuts down the synchronous data application

\return The function returns a tOplkError error code.

\ingroup module_demo_mn_console
*/
//------------------------------------------------------------------------------
void shutdownApp(void)
{
    oplk_freeProcessImage();
}

//------------------------------------------------------------------------------
/**
\brief  Synchronous data handler

The function implements the synchronous data handler.

\return The function returns a tOplkError error code.

\ingroup module_demo_mn_console
*/
//------------------------------------------------------------------------------
tOplkError processSync(void)
{
    tOplkError          ret = kErrorOk;
    int                 i;
    static int          inCnt;

    ret = oplk_waitSyncEvent(100000);
    if (ret != kErrorOk)
        return ret;

    ret = oplk_exchangeProcessImageOut();
    if (ret != kErrorOk)
        return ret;

    cnt_g++;

    nodeVar_l[0].input = pProcessImageOut_l->CN1_M00_DigitalInput_00h_AU12_DigitalInput;

    for (i = 0; (i < MAX_NODES) && (usedNodeIds_l[i] != 0); i++)
    {
        // Check the input sequence to track data errors
        if (nodeVar_l[i].nmtState != kNmtCsOperational)
            continue;

        switch (nodeVar_l[i].inLedState)
        {
            case kLedUnknown:
            {
                nodeVar_l[i].inleds = 0x1;
                // move to next stage only if first input is recieved
                if (nodeVar_l[i].input == nodeVar_l[i].inleds)
                {
                    nodeVar_l[i].inleds <<= 2;
                    nodeVar_l[i].inLedState = kLedAscending;
                    inCnt = 0;
                }
                else
                {
                    if (inCnt == appCycle_l)
                    {
                        pCommInstance_l->errorCounter.dataError++;
                        inCnt = 0;
                    }
                    else
                    {
                        inCnt++;
                    }
                }

                break;
            }
            case kLedAscending:
            {
                // Leds to should be ascending
                if (nodeVar_l[i].input == APP_LED_MASK_1)
                {
                    nodeVar_l[i].inleds <<= 1;
                    nodeVar_l[i].inLedState = kLedDescending;
                    inCnt = 0;
                    break;
                }
                else if (nodeVar_l[i].input == nodeVar_l[i].inleds)
                {
                    // Update the next value in sequence
                    nodeVar_l[i].inleds <<= 2;
                    inCnt = 0;
                }
                else
                {
                    // Is this the error?? should we move to next value or wait for last
                    if (inCnt == appCycle_l)
                    {
                        pCommInstance_l->errorCounter.dataError++;
                        inCnt = 0;
                    }
                    else
                    {
                        inCnt++;
                    }
                }

                break;
            }
            case kLedDescending:
            {
                if (nodeVar_l[i].inleds == 0x02 && nodeVar_l[i].input == 0x02)
                {
                    // end of descending sequence
                    nodeVar_l[i].inleds = APP_LED_MASK_4;
                    nodeVar_l[i].inLedState = kLedSplitOne;

                    inCnt = 0;
                    break;
                }

                if (nodeVar_l[i].input == nodeVar_l[i].inleds)
                {
                    nodeVar_l[i].inleds >>= 2;
                    inCnt = 0;
                }
                else
                {
                    // Is this the error?? should we move to next value or wait for last
                    if (inCnt == appCycle_l)
                    {
                        pCommInstance_l->errorCounter.dataError++;
                        inCnt = 0;
                    }
                    else
                    {
                        inCnt++;
                    }
                }
                break;
            }
            case kLedSplitOne:
            {
                if (nodeVar_l[i].input == nodeVar_l[i].inleds)
                {
                    nodeVar_l[i].inleds = APP_LED_MASK_5;
                    nodeVar_l[i].inLedState = kLedSplitTwo;
                    inCnt = 0;
                }
                else
                {
                    //error increase the data error count, stay here?
                    if (inCnt == appCycle_l)
                    {
                        pCommInstance_l->errorCounter.dataError++;
                        inCnt = 0;
                    }
                    else
                    {
                        inCnt++;
                    }
                }

                break;
            }
            case kLedSplitTwo:
            {
                if (nodeVar_l[i].input == nodeVar_l[i].inleds)
                {
                    nodeVar_l[i].inLedState = kLedUnknown;
                    inCnt = 0;
                }
                else
                {
                    //error increase the data error count, stay here?
                    if (inCnt == appCycle_l)
                    {
                        pCommInstance_l->errorCounter.dataError++;
                        inCnt = 0;
                    }
                    else
                    {
                        inCnt++;
                    }
                }
                break;
            }
        }

        /* Running LEDs */
        /* period for LED flashing determined by CYCLE_DELAY */
        nodeVar_l[i].period = appCycle_l;
        if (cnt_g % nodeVar_l[i].period == 0)
        {
            switch (nodeVar_l[i].outLedState)
            {
                case kLedUnknown:
                {
                    nodeVar_l[i].outleds = 0x1;
                    nodeVar_l[i].outLedState = kLedAscending;
                    break;
                }
                case kLedAscending:
                {
                    if (nodeVar_l[i].outleds == APP_LED_MASK_1)
                    {
                        nodeVar_l[i].outleds <<= 1;
                        nodeVar_l[i].outLedState = kLedDescending;
                        break;
                    }
                    nodeVar_l[i].outleds <<= 2;
                    break;
                }
                case kLedDescending:
                {
                    nodeVar_l[i].outleds >>= 2;
                    if (nodeVar_l[i].outleds == 0x02)
                    {
                        nodeVar_l[i].outLedState = kLedSplitOne;
                    }
                    break;
                }
                case kLedSplitOne:
                {
                    nodeVar_l[i].outleds = APP_LED_MASK_4;
                    nodeVar_l[i].outLedState = kLedSplitTwo;
                    break;
                }
                case kLedSplitTwo:
                {
                    nodeVar_l[i].outleds = APP_LED_MASK_5;
                    nodeVar_l[i].outLedState = kLedUnknown;
                    break;
                }
            }
        }

        if (nodeVar_l[i].input != nodeVar_l[i].inputOld)
        {
            nodeVar_l[i].inputOld = nodeVar_l[i].input;
        }

        if (nodeVar_l[i].outleds != nodeVar_l[i].ledsOld)
        {
            nodeVar_l[i].ledsOld = nodeVar_l[i].outleds;
        }
    }

    pProcessImageIn_l->CN1_M00_DigitalOutput_00h_AU12_DigitalOutput = nodeVar_l[0].outleds;

    ret = oplk_exchangeProcessImageIn();

    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Update NMT state for node

The function updates the NMT state for the specified node.

\param nodeId_p     Node Id of the node to update.
\param nmtState_p   NMT state to update.

\return The function returns a tOplkError error code.

\ingroup module_demo_mn_console
*/
//------------------------------------------------------------------------------
void updateNodeOperationalState(UINT nodeId_p, tNmtState nmtState_p)
{
    nodeVar_l[nodeId_p - 1].nmtState = nmtState_p;
}

//============================================================================//
//            P R I V A T E   F U N C T I O N S                               //
//============================================================================//
/// \name Private Functions
/// \{

//------------------------------------------------------------------------------
/**
\brief  Initialize process image

The function initializes the process image of the application.

\return The function returns a tOplkError error code.
*/
//------------------------------------------------------------------------------
static tOplkError initProcessImage(void)
{
    tOplkError      ret = kErrorOk;

    printf("Initializing process image...\n");
    printf("Size of input process image: %ld\n", sizeof(PI_IN));
    printf("Size of output process image: %ld\n", sizeof(PI_OUT));
    ret = oplk_allocProcessImage(sizeof(PI_IN), sizeof(PI_OUT));
    if (ret != kErrorOk)
    {
        return ret;
    }

    pProcessImageIn_l = (PI_IN*)oplk_getProcessImageIn();
    pProcessImageOut_l = (PI_OUT*)oplk_getProcessImageOut();

    printf("Piin %p piout %p\n", pProcessImageIn_l, pProcessImageOut_l);
    ret = oplk_setupProcessImage();

    return ret;
}

/// \}
