/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file MemManager.c
* This is the source file for the Memory Manager.
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


/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */
#include "EmbeddedTypes.h"
#include "fsl_os_abstraction.h"
#include "Panic.h"
#include "MemManager.h"

/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */

#define _block_size_  {
#define _number_of_blocks_  ,
#define _eol_  },
           
poolInfo_t poolInfo[] =
{
  PoolsDetails_c
  {0, 0} /*termination tag*/
};

#undef _block_size_
#undef _number_of_blocks_
#undef _eol_

#define _block_size_ (sizeof(listHeader_t)+
#define _number_of_blocks_ ) *
#define _eol_  +

#define heapSize_c (PoolsDetails_c 0)

// Heap
uint8_t memHeap[heapSize_c];
const uint32_t heapSize = heapSize_c;

#undef _block_size_
#undef _number_of_blocks_
#undef _eol_

#define _block_size_ 0 *
#define _number_of_blocks_ + 0 *
#define _eol_  + 1 +

#define poolCount (PoolsDetails_c 0)

// Memory pool info and anchors.
pools_t memPools[poolCount];

#undef _block_size_
#undef _number_of_blocks_
#undef _eol_

#ifdef MEM_TRACKING

#define _block_size_ 0*
#define _number_of_blocks_ +
#define _eol_  +

#define mTotalNoOfMsgs_d (PoolsDetails_c 0)
static const uint16_t mTotalNoOfMsgs_c = mTotalNoOfMsgs_d;
blockTracking_t memTrack[mTotalNoOfMsgs_d];

#undef _block_size_
#undef _number_of_blocks_
#undef _eol_

#endif /*MEM_TRACKING*/

// Free messages counter. Not used by module.
uint16_t gFreeMessagesCount;

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
* \brief   This function initializes the message module private variables. 
*          Must be called at boot time, or if device is reset.
*
* \param[in] none
*
* \return MEM_SUCCESS_c if initialization is successful. (It's always successful).
*
********************************************************************************** */
memStatus_t MEM_Init()
{
  poolInfo_t *pPoolInfo = poolInfo; // IN: Memory layout information
  pools_t *pPools = memPools;// OUT: Will be initialized with requested memory pools.
  uint8_t *pHeap = memHeap;// IN: Memory heap.
    
  uint8_t poolN;
#ifdef MEM_TRACKING
  uint16_t memTrackIndex = 0;  
#endif /*MEM_TRACKING*/

  gFreeMessagesCount = 0;
  
  for(;;) 
  {  
    poolN = pPoolInfo->poolSize;
    ListInit((listHandle_t)&pPools->anchor, poolN);
#ifdef MEM_STATISTICS
    pPools->poolStatistics.numBlocks = 0;
    pPools->poolStatistics.allocatedBlocks = 0;
    pPools->poolStatistics.allocatedBlocksPeak = 0;
    pPools->poolStatistics.allocationFailures = 0;
    pPools->poolStatistics.freeFailures = 0;
#ifdef MEM_TRACKING
    pPools->poolStatistics.poolFragmentWaste = 0;
    pPools->poolStatistics.poolFragmentWastePeak = 0;
#endif /*MEM_TRACKING*/
#endif /*MEM_STATISTICS*/

    while(poolN) 
    {
      // Add block to list of free memory.
      ListAddTail((listHandle_t)&pPools->anchor, (listElementHandle_t)&((listHeader_t *)pHeap)->link);
      ((listHeader_t *)pHeap)->pParentPool = pPools;
#ifdef MEM_STATISTICS
      pPools->poolStatistics.numBlocks++;
#endif /*MEM_STATISTICS*/

      gFreeMessagesCount++;
#ifdef MEM_TRACKING
      memTrack[memTrackIndex].blockAddr = (void *)(pHeap + sizeof(listHeader_t));
      memTrack[memTrackIndex].blockSize = pPoolInfo->blockSize;
      memTrack[memTrackIndex].fragmentWaste = 0;
      memTrack[memTrackIndex].allocAddr = NULL;
      memTrack[memTrackIndex].allocCounter = 0;
      memTrack[memTrackIndex].allocStatus = MEM_TRACKING_FREE_c;
      memTrack[memTrackIndex].freeAddr = NULL;
      memTrack[memTrackIndex].freeCounter = 0;
      memTrackIndex++;
#endif /*MEM_TRACKING*/
  
        // Add block size (without list header)
      pHeap += pPoolInfo->blockSize + sizeof(listHeader_t);
      poolN--;
    }

    pPools->blockSize = pPoolInfo->blockSize;
    pPools->nextBlockSize = (pPoolInfo+1)->blockSize;
    if(pPools->nextBlockSize == 0)
    {
      break;
    }
    
    pPools++;
    pPoolInfo++;
  }

  return MEM_SUCCESS_c;
}

/*! *********************************************************************************
* \brief    This function returns the number of available blocks greater or 
*           equal to the given size.
*
* \param[in] size - Size of blocks to check for availability.
*
* \return Number of available blocks greater or equal to the given size.
*
* \pre Memory manager must be previously initialized.
*
********************************************************************************** */
uint32_t MEM_GetAvailableBlocks
  (
  uint32_t size
  )
{
  pools_t *pPools = memPools;
  uint32_t pTotalCount = 0;
  
  for(;;)
  {
    if(size <= pPools->blockSize)
    {
      pTotalCount += ListGetSize((listHandle_t)&pPools->anchor);
    }
    
    if(pPools->nextBlockSize == 0)
    {
      break;
    }
    
    pPools++;
  }
  
  return  pTotalCount;
}

/*! *********************************************************************************
* \brief     Allocate a block from the memory pools. The function uses the 
*            numBytes argument to look up a pool with adequate block sizes.
* \param[in] numBytes - Size of buffer to allocate.
*
* \return Pointer to the allocated buffer, NULL if failed.
*
* \pre Memory manager must be previously initialized.
*
********************************************************************************** */
void* MEM_BufferAlloc
  (
  uint32_t numBytes // IN: Minimum number of bytes to allocate
  )
{
#ifdef MEM_TRACKING

  /* Save the Link Register */
  volatile uint32_t savedLR;
//  __asm("str  r14, [SP]");
  __asm("push {r2}  ");
  __asm("push {LR} ");
  __asm("pop  {r2} ");
  __asm("str  r2, [SP, #4]");
  __asm("pop {r2}");

#endif /*MEM_TRACKING*/
  
  pools_t *pPools = memPools;
  listHeader_t *pBlock;
  
#ifdef MEM_TRACKING
  uint16_t requestedSize = numBytes;
#endif /*MEM_TRACKING*/

  OSA_EnterCritical(kCriticalDisableInt);

  while(numBytes)
  {
    if(numBytes <= pPools->blockSize)
    {
      pBlock = (listHeader_t *)ListRemoveHead((listHandle_t)&pPools->anchor);
      
      if(NULL != pBlock)
      {
        pBlock++;
        gFreeMessagesCount--;
        
#ifdef MEM_STATISTICS
        pPools->poolStatistics.allocatedBlocks++;
        if ( pPools->poolStatistics.allocatedBlocks > pPools->poolStatistics.allocatedBlocksPeak )
        {
          pPools->poolStatistics.allocatedBlocksPeak = pPools->poolStatistics.allocatedBlocks;
        }
        MEM_ASSERT(pPools->poolStatistics.allocatedBlocks <= pPools->poolStatistics.numBlocks);
#endif /*MEM_STATISTICS*/
        
#ifdef MEM_TRACKING
        MEM_Track(pBlock, MEM_TRACKING_ALLOC_c, savedLR, requestedSize);
#endif /*MEM_TRACKING*/
        OSA_ExitCritical(kCriticalDisableInt);
        return pBlock;
      }
      else
      {
        if(numBytes > pPools->nextBlockSize) break;
        // No more blocks of that size, try next size.
        numBytes = pPools->nextBlockSize;   
      }
    }
      // Try next pool
    if(pPools->nextBlockSize)
      pPools++;
    else
      break;
  }
#ifdef MEM_STATISTICS
  pPools->poolStatistics.allocationFailures++;
#endif /*MEM_STATISTICS*/ 

#ifdef MEM_DEBUG
  panic( 0, (uint32_t)MEM_BufferAlloc, 0, 0);
#endif

  OSA_ExitCritical(kCriticalDisableInt);
  return NULL;
}

/*! *********************************************************************************
* \brief     Deallocate a memory block by putting it in the corresponding pool 
*            of free blocks. 
*
* \param[in] buffer - Pointer to buffer to deallocate.
*
* \return MEM_SUCCESS_c if deallocation was successful, MEM_FREE_ERROR_c if not.
*
* \pre Memory manager must be previously initialized.
*
* \remarks Never deallocate the same buffer twice.
*
********************************************************************************** */
memStatus_t MEM_BufferFree
  (
  void* buffer // IN: Block of memory to free
  )
{
#ifdef MEM_TRACKING

  /* Save the Link Register */
  volatile uint32_t savedLR;
//  __asm("str  r14, [SP]");
  __asm("push {r1}  ");
  __asm("push {LR} ");
  __asm("pop  {r1} ");
  __asm("str  r1, [SP, #4]");
  __asm("pop {r1}");
#endif /*MEM_TRACKING*/

  if(buffer == NULL) 
  {
    return MEM_FREE_ERROR_c;
  }

  OSA_EnterCritical(kCriticalDisableInt);
  
  listHeader_t *pHeader = (listHeader_t *)buffer-1;
  pools_t *pParentPool = (pools_t *)pHeader->pParentPool;

  pools_t *pool = memPools;
  for(;;)
  {
    if (pParentPool == pool)
      break;
    if(pool->nextBlockSize == 0)
    {
      /* The parent pool was not found! This means that the memory buffer is corrupt or 
        that the MEM_BufferFree() function was called with an invalid parameter */
#ifdef MEM_STATISTICS
      pParentPool->poolStatistics.freeFailures++;
#endif /*MEM_STATISTICS*/
      OSA_ExitCritical(kCriticalDisableInt);
      return MEM_FREE_ERROR_c;
    }
    pool++;
  }

  if( pHeader->link.list != NULL )
  {
      /* The memory buffer appears to be enqueued in a linked list. 
         This list may be the free memory buffers pool, or another list. */
#ifdef MEM_STATISTICS
      pParentPool->poolStatistics.freeFailures++;
#endif /*MEM_STATISTICS*/
      OSA_ExitCritical(kCriticalDisableInt);
      return MEM_FREE_ERROR_c;
  }

  gFreeMessagesCount++;

  ListAddTail((listHandle_t)&pParentPool->anchor, (listElementHandle_t)&pHeader->link);

#ifdef MEM_STATISTICS
  MEM_ASSERT(pParentPool->poolStatistics.allocatedBlocks > 0);
  pParentPool->poolStatistics.allocatedBlocks--;
#endif /*MEM_STATISTICS*/

#ifdef MEM_TRACKING
  MEM_Track(buffer, MEM_TRACKING_FREE_c, savedLR, 0);
#endif /*MEM_TRACKING*/   
  OSA_ExitCritical(kCriticalDisableInt);
  return MEM_SUCCESS_c; 
}

/*! *********************************************************************************
* \brief     Determines the size of a memory block
*
* \param[in] buffer - Pointer to buffer.
*
* \return size of memory block
*
* \pre Memory manager must be previously initialized.
*
********************************************************************************** */
uint16_t MEM_BufferGetSize
(
void* buffer // IN: Block of memory to free
)
{
    if( buffer )
    {
        return ((pools_t *)((listHeader_t *)buffer-1)->pParentPool)->blockSize;
    }
    
    return 0;
}

/*! *********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
********************************************************************************** */
/*! *********************************************************************************
* \brief     This function updates the tracking array element corresponding to the given 
*            block.
*
* \param[in] block - Pointer to the block.
* \param[in] alloc - Indicates whether an allocation or free operation was performed
* \param[in] address - Address where MEM_BufferAlloc or MEM_BufferFree was called 
* \param[in] requestedSize - Indicates the requested buffer size  passed to MEM_BufferAlloc.
*                            Has no use if a free operation was performed.
*
* \return Returns TRUE if correct allocation or dealocation was performed, FALSE if a
*         buffer was allocated or freed twice.
*
********************************************************************************** */
#ifdef MEM_TRACKING
uint8_t MEM_Track(listHeader_t *block, memTrackingStatus_t alloc, uint32_t address, uint16_t requestedSize)        
{
  uint16_t i;
  blockTracking_t *pTrack = NULL;
#ifdef MEM_STATISTICS
  poolStat_t * poolStatistics = (poolStat_t *)&((pools_t *)( (listElementHandle_t)(block-1)->pParentPool ))->poolStatistics;
#endif

  for( i=0; i<mTotalNoOfMsgs_c; i++ )
  {
      if( block == memTrack[i].blockAddr )
      {
          pTrack = &memTrack[i];
          break;
      }
  }

  if( !pTrack || pTrack->allocStatus == alloc)
  {
#ifdef MEM_DEBUG
      panic( 0, (uint32_t)MEM_Track, 0, 0);
#endif
      return FALSE;
  }

  pTrack->allocStatus = alloc; 

  if(alloc == MEM_TRACKING_ALLOC_c)                                          
  {
    pTrack->fragmentWaste = pTrack->blockSize - requestedSize;
    pTrack->allocCounter++;                     
    pTrack->allocAddr = (void *)address;
#ifdef MEM_STATISTICS
    
    poolStatistics->poolFragmentWaste += pTrack->fragmentWaste;
    if(poolStatistics->poolFragmentWaste > poolStatistics->poolFragmentWastePeak)
      poolStatistics->poolFragmentWastePeak = poolStatistics->poolFragmentWaste;
#endif /*MEM_STATISTICS*/
  }
  else
  {
#ifdef MEM_STATISTICS
    poolStatistics->poolFragmentWaste -= pTrack->fragmentWaste;
#endif /*MEM_STATISTICS*/
    pTrack->fragmentWaste = 0;
    pTrack->freeCounter++;
    pTrack->freeAddr = (void *)address;
  }

  return TRUE;
}
#endif /*MEM_TRACKING*/

/*! *********************************************************************************
* \brief     This function checks for buffer overflow when copying multiple bytes
*
* \param[in] p    - pointer to destination.
* \param[in] size - number of bytes to copy
*
* \return 1 if overflow detected, else 0
*
********************************************************************************** */
uint8_t MEM_BufferCheck(uint8_t *p, uint32_t size)
{
    poolInfo_t *pPollInfo = poolInfo;
    uint8_t* memAddr = memHeap;
    uint32_t poolBytes, blockBytes, i;

    if( (p < (uint8_t*)memHeap) || (p > ((uint8_t*)memHeap + sizeof(memHeap))) )
        return 0;

    while( pPollInfo->blockSize )
    {
        blockBytes = pPollInfo->blockSize + sizeof(listHeader_t);
        poolBytes  = blockBytes * pPollInfo->poolSize;

        /* Find the correct message pool */
        if( (p >= memAddr) && (p < memAddr + poolBytes) )
        {
            /* Check if the size to copy is greather then the size of the current block */
            if( size > pPollInfo->blockSize )
            {
#ifdef MEM_DEBUG
                panic(0,0,0,0);
#endif
                return 1;
            }

            /* Find the correct memory block */
            for( i=0; i<pPollInfo->poolSize; i++ )
            {
                if( (p >= memAddr) && (p < memAddr + blockBytes) )
                {
                    if( p + size > memAddr + blockBytes )
                    {
#ifdef MEM_DEBUG
                        panic(0,0,0,0);
#endif
                        return 1;
                    }
                    else
                    {
                        return 0;
                    }
                }

                memAddr += blockBytes;
            }
        }

        /* check next pool */
        memAddr += poolBytes;
        pPollInfo++;
    }

    return 0;
}

/*! *********************************************************************************
* \brief     Performs a write-read-verify test for every byte in all memory pools.
*
* \return Returns MEM_SUCCESS_c if test was successful, MEM_ALLOC_ERROR_c if a
*         buffer was not allocated successufuly, MEM_FREE_ERROR_c  if a
*         buffer was not freed successufuly or MEM_UNKNOWN_ERROR_c if a verify error,
*         heap overflow or data corruption occurred.
*
********************************************************************************** */
uint32_t MEM_WriteReadTest(void)
{
  uint8_t *data, count = 1;
  uintn32_t idx1,idx2,idx3;
  uint32_t freeMsgs;
  
  /*memory write test*/
  freeMsgs = MEM_GetAvailableBlocks(0);
  
  for(idx1=0; poolInfo[idx1].blockSize != 0; idx1++)
  {
    for(idx2=0; idx2 < poolInfo[idx1].poolSize; idx2++)
    {
      data = (uint8_t *)MEM_BufferAlloc(poolInfo[idx1].blockSize);
      
      if(data == NULL)
      {
        return MEM_ALLOC_ERROR_c;
      }
      
      for(idx3=0; idx3 < poolInfo[idx1].blockSize; idx3++)
      {
        if(data > memHeap + heapSize)
        {
          return MEM_UNKNOWN_ERROR_c;
        }
        *data = count & 0xff;
        data++;
      }
      count++;
    }
  }
  
  count = 1;
  data = memHeap;
  /*memory read test*/
  for(idx1=0; poolInfo[idx1].blockSize != 0; idx1++)
  {
    for(idx2=0; idx2 < poolInfo[idx1].poolSize; idx2++)
    {
      /*New block; jump over list header*/
      data = data + sizeof(listHeader_t); 
      for(idx3=0; idx3<poolInfo[idx1].blockSize; idx3++)
      {
        if(*data == count)
        {
          data++;
        }
        else
        {
          return MEM_UNKNOWN_ERROR_c;
        }
      }
      if(MEM_BufferFree( data - poolInfo[idx1].blockSize) != MEM_SUCCESS_c)
      {
        return MEM_FREE_ERROR_c;
      }
      count++;
    }
  }
  if(MEM_GetAvailableBlocks(0) != freeMsgs)
  {
    return MEM_UNKNOWN_ERROR_c;
  }
#ifdef MEM_STATISTICS
  for(idx1 = 0; poolInfo[idx1].blockSize != 0; idx1++)
  {
    memPools[idx1].poolStatistics.allocatedBlocksPeak = 0;
  }
#endif /*MEM_STATISTICS*/
  
  return MEM_SUCCESS_c;
}
