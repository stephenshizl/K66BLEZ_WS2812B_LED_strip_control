/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file RNG.c
* RNG implementation file for the ARM CORTEX-M4 processor
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* o Redistributions of source code must retain the above copyright notice, this list
*   of conditions and the following disclaimer.
*
* o Redistributions in binary form must reproduce the above copyright notice, this
*   list of conditions and the following disclaimer in the documentation and/or
*   other materials provided with the distribution.
*
* o Neither the name of Freescale Semiconductor, Inc. nor the names of its
*   contributors may be used to endorse or promote products derived from this
*   software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "RNG_Interface.h"
#include "FunctionLib.h"
#include "SecLib.h"
#include "fsl_device_registers.h"
#include "fsl_clock_manager.h"


#if FSL_FEATURE_SOC_TRNG_COUNT
    #include "fsl_trng_driver.h"
#elif FSL_FEATURE_SOC_RNG_COUNT
    #include "fsl_rnga_driver.h"
#endif


/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */
#ifndef gRNG_UsePhyRngForInitialSeed_d
#define gRNG_UsePhyRngForInitialSeed_d 0
#endif

#define mPRNG_NoOfBits_c      (160)
#define mPRNG_NoOfBytes_c     (mPRNG_NoOfBits_c/8)
#define mPRNG_NoOfLongWords_c (mPRNG_NoOfBits_c/32)


/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
static uint32_t XKEY[mPRNG_NoOfLongWords_c];
static uint32_t mPRNG_Requests = gRngMaxRequests_d;

uint32_t mRandomNumber;

/*! *********************************************************************************
*************************************************************************************
* Public prototypes
*************************************************************************************
********************************************************************************** */
#if (FSL_FEATURE_SOC_RNG_COUNT == 0) && \
    (FSL_FEATURE_SOC_TRNG_COUNT == 0) && \
    (gRNG_UsePhyRngForInitialSeed_d)
extern void PhyGetRandomNo(uint32_t *pRandomNo);
#endif


/*! *********************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
********************************************************************************** */
static uint8_t RNG_HwGetRandomNo(uint32_t* pRandomNo);
#if FSL_FEATURE_SOC_TRNG_COUNT
static void TRNG_ISR(void);
#endif


/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
* \brief  Initialize the RNG HW module
*
* \return  Status of the RNG initialization sequence
*
********************************************************************************** */
uint8_t RNG_Init(void)
{
#if FSL_FEATURE_SOC_RNG_COUNT
    const rnga_user_config_t config = {
        .isIntMasked = 1,
        .highAssuranceEnable = 0
    };

    RNGA_DRV_Init(0, &config);
    if( kStatus_RNGA_Success != RNGA_DRV_GetRandomData(0, &mRandomNumber, sizeof(uint32_t)) )
    {
        return gRngInternalError_d;
    }        

#elif FSL_FEATURE_SOC_TRNG_COUNT
    trng_user_config_t config;

    TRNG_DRV_InitUserConfigDefault(&config);
    config.frequencyCountLimit.minimum = 0x00000100;
    config.frequencyCountLimit.maximum = 0x000F0000;
    config.ringOscDiv = kTRNGRingOscDiv0;
    config.entropyDelay = 1200;

    OSA_InstallIntHandler(g_trngIrqId[0], TRNG_ISR);

    if( kStatus_TRNG_Success != TRNG_DRV_Init(0, &config) )
    {
        return gRngInternalError_d;
    }

    if( kStatus_TRNG_Success != TRNG_DRV_GetRandomData(0, &mRandomNumber, sizeof(uint32_t)) )
    {
        return gRngInternalError_d;
    }

#else
    #if gRNG_UsePhyRngForInitialSeed_d
    /* Use 802.15.4 PHY to generate the initial seed */
    PhyGetRandomNo(&mRandomNumber);
    #else
    /* Use MCU unique Id for initial seed */
    mRandomNumber = SIM_UIDL;
    #endif
#endif

    /* Init Successfull */
    return gRngSuccess_d;
}


/*! *********************************************************************************
* \brief  Read a random number from the HW RNG module.
*         If the HW fails, the SW PRNG is used as backup.
*
* \param[out] pRandomNo - pointer to location where the value will be stored
*
* \return  status of the RNG module
*
********************************************************************************** */
static uint8_t RNG_HwGetRandomNo(uint32_t* pRandomNo)
{
#if FSL_FEATURE_SOC_RNG_COUNT
    if( kStatus_RNGA_Success == RNGA_DRV_GetRandomData(0, pRandomNo, sizeof(uint32_t)) )
    {
        return gRngSuccess_d;
    }
#elif FSL_FEATURE_SOC_TRNG_COUNT
    if( TRNG_HAL_GetEntropyValidCmd(g_trngBase[0]) &&
        (kStatus_TRNG_Success == TRNG_DRV_GetRandomData(0, pRandomNo, sizeof(uint32_t))) )
    {
        return gRngSuccess_d;
    }
#endif

    mRandomNumber = (mRandomNumber * 6075) + 1283;
    FLib_MemCpy(pRandomNo, &mRandomNumber, sizeof(uint32_t));  

    return gRngSuccess_d;
}


/*! *********************************************************************************
* \brief  Generate a random number
*
* \param[out] pRandomNo - pointer to location where the value will be stored
*
********************************************************************************** */
void RNG_GetRandomNo(uint32_t* pRandomNo)
{
    /* Check for NULL pointers */
    if (NULL != pRandomNo)
    {
        (void)RNG_HwGetRandomNo(pRandomNo);
    }
}


/*! *********************************************************************************
* \brief  Initialize seed for the PRNG algorithm.
*
* \param[in]  pSeed - pointer to a buffer containing 20 bytes (160 bits).
*             Can be set using the RNG_GetRandomNo() function.
*
********************************************************************************** */
void RNG_SetPseudoRandomNoSeed(uint8_t* pSeed)
{
    mPRNG_Requests = 1;
    FLib_MemCpy( XKEY, pSeed, mPRNG_NoOfBytes_c );
}


/*! *********************************************************************************
* \brief  Pseudo Random Number Generator (PRNG) implementation
*         according to NIST FIPS Publication 186-2, APPENDIX 3
*
*         Let x be the signer's private key.  
*         The following may be used to generate m values of x:
*           Step 1. Choose a new, secret value for the seed-key, XKEY.
*           Step 2. In hexadecimal notation let
*             t = 67452301 EFCDAB89 98BADCFE 10325476 C3D2E1F0.
*             This is the initial value for H0 || H1 || H2 || H3 || H4 in the SHS.
*           Step 3. For j = 0 to m - 1 do
*             a. XSEEDj = optional user input.
*             b. XVAL = (XKEY + XSEEDj) mod 2^b
*             c. xj = G(t,XVAL) mod q
*             d. XKEY = (1 + XKEY + xj) mod 2^b
*
* \param[out]    pOut - pointer to the output buffer
* \param[in]     outBytes - the number of bytes to be copyed (1-20)
* \param[in]     pXSEED - optional user SEED. Should be NULL if not used.
*
* \return  The number of bytes copied or -1 if reseed is needed
*
********************************************************************************** */
int16_t RNG_GetPseudoRandomNo(uint8_t* pOut, uint8_t outBytes, uint8_t* pXSEED)
{
    uint32_t i;
    sha1Context_t ctx;

    if(pXSEED)
    {
        mPRNG_Requests = 1;
    }

    if (mPRNG_Requests == gRngMaxRequests_d)
    {
        return -1;
    }

    mPRNG_Requests++;

    /* a. XSEEDj = optional user input. */
    if (pXSEED)
    {
        /* b. XVAL = (XKEY + XSEEDj) mod 2^b */
        for (i=0; i<mPRNG_NoOfBytes_c; i++)
        {
            ctx.buffer[i] = ((uint8_t*)XKEY)[i] + pXSEED[i];
        }
    }
    else
    {
        for (i=0; i<mPRNG_NoOfBytes_c; i++)
        {
            ctx.buffer[i] = ((uint8_t*)XKEY)[i];
        }
    }

    /* c. xj = G(t,XVAL) mod q
    ***************************/
    SHA1_Hash(&ctx, ctx.buffer, mPRNG_NoOfBytes_c);

    /* d. XKEY = (1 + XKEY + xj) mod 2^b */
    XKEY[0] += 1;
    for (i=0; i<mPRNG_NoOfLongWords_c; i++)
    {
        XKEY[i] += ctx.hash[i];
    }

    /* Check if the length provided exceeds the output data size */
    if (outBytes > mPRNG_NoOfBytes_c)
    {
        outBytes = mPRNG_NoOfBytes_c;
    }

    /* Copy the generated number */
    for (i=0; i < outBytes; i++)
    {
        pOut[i] = ((uint8_t*)ctx.hash)[i];
    }

    return outBytes;
}


/*! *********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
********************************************************************************** */
#if FSL_FEATURE_SOC_TRNG_COUNT
static void TRNG_ISR(void)
{
    TRNG_DRV_IRQHandler(0);
}
#endif

/********************************** EOF ***************************************/
