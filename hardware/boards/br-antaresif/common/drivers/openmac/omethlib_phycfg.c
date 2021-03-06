/**
********************************************************************************
\file   omethlib_phycfg_ink.c

\brief  openMAC phy configuration for B&R Antares Interface

This file contains phy configuration callback for the B&R Antares Interface
used in APC2100.
*******************************************************************************/

/*------------------------------------------------------------------------------
Copyright (c) 2014, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
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
#include <omethlib.h>
#include <omethlibint.h>
#include <omethlib_phycfg.h>

//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// module global vars
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// global function prototypes
//------------------------------------------------------------------------------

//============================================================================//
//            P R I V A T E   D E F I N I T I O N S                           //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------
#define PHYCFG_DP8384_PHYCR_REGADDR     0x19
#define PHYCFG_DP8384_PHYCR_LED_CNFG0   (1 << 5)

//------------------------------------------------------------------------------
// local types
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------

//============================================================================//
//            P U B L I C   F U N C T I O N S                                 //
//============================================================================//

//------------------------------------------------------------------------------
/**
\brief  Phy configuration callback

This function configures the phy on the Antares Interface card.

\param  pEth_p  Ethernet driver handle

\return The function returns 0 if success.
*/
//------------------------------------------------------------------------------
int omethPhyCfgUser(OMETH_H pEth_p)
{
    int             ret;
    int             i;
    unsigned short  regData;
    unsigned short  regNumber;

    for (i=0; i<pEth_p->phyCount; i++)
    {
        // Read ohy control register
        regNumber = PHYCFG_DP8384_PHYCR_REGADDR;
        ret = omethPhyRead(pEth_p, i, regNumber, &regData);
        if(ret != 0)
            return ret;

        // Set LED_CNFG[0] to Mode 1 '1'
        regData |= PHYCFG_DP8384_PHYCR_LED_CNFG0;
        ret = omethPhyWrite(pEth_p, i, regNumber, regData);
        if(ret != 0)
            return ret;
    }

    return 0;
}
