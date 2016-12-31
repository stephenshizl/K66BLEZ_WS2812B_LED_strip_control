/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 2004-2008 Embedded Access Inc.
* Copyright 1989-2008 ARC International
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   This file contains the default protocol table for RTCS.
*
*
*END************************************************************************/

#include <rtcs.h>


uint32_t (_CODE_PTR_ const RTCS_protocol_table[])(void) = {
#if RTCSCFG_ENABLE_IGMP && RTCSCFG_ENABLE_IP4
   RTCSPROT_IGMP,
#endif 
   
#if RTCSCFG_ENABLE_UDP
   RTCSPROT_UDP,
#endif 
   
#if RTCSCFG_ENABLE_TCP
   RTCSPROT_TCP,
#endif 

   NULL
};


/* EOF */
