/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/occ_gpe1/gpe1_24x7.c $                                    */
/*                                                                        */
/* OpenPOWER OnChipController Project                                     */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2018                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */

#include "pk.h"
#include "pk_app_cfg.h"
#include "ppe42_scom.h"
#include "ipc_api.h"
#include "ipc_async_cmd.h"
#include "gpe_util.h"
#include "gpe_24x7_structs.h"
#include "gpe_pba_cntl.h"
#include "gpe1_24x7.h"
#include "string.h"

/*
 * Function Specifications:
 *
 * Name: gpe_24x7
 * Description:  24x7 code on the GPE.  Owned by the performance team
 * Inputs:       cmd is a pointer to IPC msg's cmd and cmd_data struct
 * Outputs:      error: sets rc, address, and ffdc in the cmd_data's
 *                      GpeErrorStruct
 *
 * End Function Specification
 */
void gpe_24x7(ipc_msg_t* cmd, void* arg)
{
    // Note: arg was set to 0 in ipc func table (ipc_func_tables.c), so don't use it.
    // the ipc arguments passed through the ipc_msg_t structure, has a pointer
    // to the gpe_24x7_args_t struct.
   
    int      rc = 0;
    int      iter = 0;
    ipc_async_cmd_t *async_cmd = (ipc_async_cmd_t*)cmd;
    gpe_24x7_args_t *args = (gpe_24x7_args_t*)async_cmd->cmd_data;

    uint8_t ticks = args->numTicksPassed; // number of 500us ticks since last call
    static uint8_t  L_current_state = 1;  // 24x7 collection "state" to execute when called

    static uint8_t  L_DELAY_1 = 0;  
    static uint8_t  L_DELAY_2 = 0;  
    static uint8_t  L_DELAY_3 = 0;  
    static uint8_t  L_DELAY_4 = 0;  
    static uint8_t  L_DELAY_5 = 0;  
    static uint8_t  L_DELAY_6 = 0;  
    static uint8_t  L_DELAY_7 = 0;  
    static uint8_t  L_CUR_DELAY = 0;  
    static uint64_t  L_cur_speed = 0;  
    static uint64_t  L_prev_status = 0;  

    static bool L_configure = false;
    static bool L_DONT_RUN = false;
    static bool L_INIT = false;
    static bool L_PART_INIT = false;

    //
    //control block memory area.
    //
    static volatile uint64_t* L_status = (uint64_t*) (CNTL_STATUS_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_cmd = (uint64_t*) (CNTL_CMD_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_speed = (uint64_t*) (CNTL_SPEED_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_uav = (uint64_t*) (CNTL_UAV_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_mode = (uint64_t*) (CNTL_MODE_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_mode_state = (uint64_t*) (CNTL_MODE_STATE_OFFSET | PBA_ENABLE);

    static volatile uint64_t* L_version = (uint64_t*) (DBG_VER_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_tics_exceded = (uint64_t*) (DBG_TICS_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_marker = (uint64_t*) (DBG_MARK | PBA_ENABLE);
    static volatile uint64_t* L_DBG_ITER = (uint64_t*) (DBG_ITER | PBA_ENABLE);
    volatile uint8_t* L_DBG_STATE = (uint8_t*) (DBG_STATE | PBA_ENABLE);

    //uint64_t temp;
    args->error.error = 0; // default success
    args->error.ffdc = 0;

    //Populate version details
    //------------------------
    /**
     * 24x7 Version 
     * [00:03] - Major revision
     * [04:11] - Minor revision
     * [12:15] - Bug fix release number
     * [16:23] - Day
     * [24:31] - Month
     * [32:47] - Year
     * [48:63] - Reserved
     **/
    uint64_t VERSION = 0;
    static uint64_t ver_major  = 0x1;
    static uint64_t ver_minor  = 0x0;
    static uint64_t ver_bugfix = 0x0;
    static uint64_t ver_day    = 0x27;   // Day: 27
    static uint64_t ver_month  = 0x04;   // Month: 04
    static uint64_t ver_year   = 0x2018; // Year:2018

    VERSION |= (ver_major  << 60);
    VERSION |= (ver_minor  << 52);
    VERSION |= (ver_bugfix << 48);
    VERSION |= (ver_day    << 40);
    VERSION |= (ver_month  << 32);
    VERSION |= (ver_year   << 16);


    //PBA Slave setup. Do this each time you enter this loop to be safe.
    gpe_pba_reset();
    if(ticks == 0)  // First time 24x7 called since OCC started?
    {
//1. read and update the control block 
//------------------------------------
        PK_TRACE("gpe_24x7: First call since OCC started. ticks = 0");
        //set configure to true
        L_configure = true;
        L_INIT = true;
        //initialize posting area
        initialize_postings();

        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
        //set code version
        *L_version = VERSION;
        //set status as initializing
        *L_status = CNTL_STATUS_INIT;
        //initialize cmd to NOP
        *L_cmd = CNTL_CMD_NOP;
        //UAV - currently filled with system dependent static value. Need to be replaced with value from reading HDAT.
        if (*L_uav == 0)
            *L_uav = CNTL_UAV_TEMP;
        //get speed of collection and set delays accordingly.
        L_cur_speed = *L_speed;
        set_speed(&L_cur_speed,&L_CUR_DELAY,L_status);
        //check if mode is set to monitor
        //support for debug modes (flexible IMA) not present currently.
        if((*L_mode != CNTL_MODE_MONITOR)&&(*L_mode != CNTL_MODE_DEBUG1))
            *L_status = CNTL_STATUS_ERR2;
        //set Dont run if the speed and mode info is not set to legal values.
        if( (*L_status == CNTL_STATUS_ERR1)||(*L_status == CNTL_STATUS_ERR2) )
            L_DONT_RUN = true;
    }
    else if(ticks > 1)  // longer than 500us since last call?
    {
       // It has been ticks*500us since last call
        PK_TRACE("gpe_24x7: It has been 0x%02X ticks since last call", ticks);
        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
        *L_tics_exceded ++;
    }

//2. get any new command
//----------------------
    if (*L_cmd != CNTL_CMD_NOP)
    {
       putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
       switch(*L_cmd)
       {
           case CNTL_CMD_PAUSE:
               L_DONT_RUN = true;
               *L_status = CNTL_STATUS_PAUSE;
               *L_cmd = CNTL_CMD_NOP;
               PK_TRACE("gpe_24x7: in CNTL_CMD_PAUSE");
               for(iter = 0;iter < 24;iter++)
               {
                   G_PHB_pmulets[iter] = 0;
                   G_MBA_pmulets[iter] = 0;
               }
               break;
           case CNTL_CMD_RESUME:
               L_DONT_RUN = false;
               *L_status = CNTL_STATUS_RUN;
               *L_cmd = CNTL_CMD_NOP;
               PK_TRACE("gpe_24x7: in CNTL_CMD_RESUME");
               break;
           case CNTL_CMD_CLEAR:
               L_DONT_RUN = false;
               L_INIT = true;
               PK_TRACE("gpe_24x7: in CNTL_CMD_CLEAR, L_INIT set to true");
               *L_cmd = CNTL_CMD_NOP;
               break;
       }
    }

//3.get any new speed setting
//---------------------------
    if (*L_speed != L_cur_speed)
    {
        L_INIT = true;
        PK_TRACE("gpe_24x7: speed change, L_INIT set to true");
        set_speed(&L_cur_speed,&L_CUR_DELAY,L_status);
    }

//4.check for any system config changes via uav
//---------------------------------------------
    if (*L_uav != G_CUR_UAV)
    {
        L_INIT = true;
        L_PART_INIT = true;
    }

//5.check for mode change
//-----------------------
    if (*L_mode != G_CUR_MODE)
    {
        L_INIT = true;
        L_prev_status = *L_status;
        L_PART_INIT = true;
        L_DONT_RUN = false;
        PK_TRACE("gpe_24x7: mode change, L_INIT set to true");
    }
    //if(*L_mode != CNTL_MODE_MONITOR)//&&(*L_mode != CNTL_MODE_DEBUG1))
    if((*L_mode != CNTL_MODE_MONITOR)&&(*L_mode != CNTL_MODE_DEBUG1))
        *L_status = CNTL_STATUS_ERR2;
    //set Dont run if the speed and mode info is not set to legal values.
    if( (*L_status == CNTL_STATUS_ERR1)||(*L_status == CNTL_STATUS_ERR2) )
        L_DONT_RUN = true;
    //initialize postings if required from new cmd or change of speed or UAV change.
    if (L_INIT)
    {
        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
        *L_status = CNTL_STATUS_INIT;
        //need not initialize postings for UAV change.
        if (L_PART_INIT)
            L_PART_INIT = false;
        else
            initialize_postings();

        L_configure = true;
        L_cur_speed = *L_speed;
        G_CUR_UAV = *L_uav;
        G_CUR_MODE = *L_mode;
//SW399904 patch until HWP output for UAV is debugged.
        //G_CUR_UAV = CNTL_UAV_TEMP;
        *L_marker = MARKER1;

        set_speed(&L_cur_speed,&L_CUR_DELAY,L_status);
        //set the state to 1 if reconfig is required. config scoms are split across multiple states starting from 1.
        L_current_state = 1;
        PK_TRACE("gpe_24x7: in L_INIT L_current_state set to 1");
        L_INIT = false;
    }

//5. Based on the current entry state number, appropriate group posting is done.
//G1,G2(states 1,3,5,7), 
//G3(states 2,4,6,8) G4(state 2), G5(state 4), G6(state 6), G7(state 8).
//during first time entry or a re-init is trigered, the current run is used for pmu configuration.
//configuration will continue across multiple re-entry slots till all configuration scoms are done.
//scoms are generally kept at 16 per slot, to prevent from exceeding 50us runtime buget.
    if (L_DONT_RUN == false)
    {
        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
        //PK_TRACE("gpe_24x7: begin state = %d",(int)L_current_state);
        *L_DBG_STATE = L_current_state;
        switch(L_current_state)
        {
            case 1:
                if(L_configure)
                {
                    configure_pmu(L_current_state, L_cur_speed);
                }
                else
                {
                    if(L_DELAY_1 == 0)
                    {
                        post_pmu_events(G1);
                        L_DELAY_1 = L_CUR_DELAY;
                    }
                    else
                        L_DELAY_1--;

                    if(L_DELAY_2 == 0)
                    {
                        post_pmu_events(G2);
                        L_DELAY_2 = L_CUR_DELAY;
                    }
                    else
                        L_DELAY_2--;
                }
                break;
            case 2:
                if(L_configure)
                {
                    configure_pmu(L_current_state, L_cur_speed);
                }
                else
                {
                    if(L_DELAY_3 == 0)
                    {
                        post_pmu_events(G3);
                        L_DELAY_3 = L_CUR_DELAY;
                    }
                    else
                        L_DELAY_3--;
                
      		    if(L_DELAY_4 == 0)
      		    {
      		        post_pmu_events(G4);
//for groups 4,5,6,7 8ms is the fastest possible collection speed.
      		        L_DELAY_4 = L_CUR_DELAY/8;
      		    }
      		    else
      		        L_DELAY_4--;
                }
                break;
            case 3:
                if(L_configure)
                {
                    configure_pmu(L_current_state, L_cur_speed);
                }
                else
                {
    		    if(L_DELAY_1 == 0)
    		    {
    		        post_pmu_events(G1);
    		        L_DELAY_1 = L_CUR_DELAY;
    		    }
    		    else
    		        L_DELAY_1--;
                
                    if(L_DELAY_2 == 0)
                    {
                        post_pmu_events(G2);
                        L_DELAY_2 = L_CUR_DELAY;
                    }
                    else
                        L_DELAY_2--;
                }
                break;
            case 4:
                if(L_configure)
                {
                    configure_pmu(L_current_state, L_cur_speed);
                    L_configure = false;
                    *L_status = CNTL_STATUS_RUN;
                    //*L_status = L_prev_status;
                    if(L_prev_status == CNTL_STATUS_PAUSE)
                    {    
                        L_DONT_RUN = true;
                        *L_status = L_prev_status;
                    }
                    *L_mode_state = G_CUR_MODE;
                    //*L_mode = 0;
                    L_prev_status = 0;
                }
                else
                {
                    if(L_DELAY_3 == 0)
                    {
                        post_pmu_events(G3);
                        L_DELAY_3 = L_CUR_DELAY;
                    }
                    else
                        L_DELAY_3--;
                
                    if(L_DELAY_5 == 0)
                    {
                        post_pmu_events(G5);
//for groups 4,5,6,7 8ms is the fastest possible collection speed.
                        L_DELAY_5 = L_CUR_DELAY/8;
                    }
                    else
                        L_DELAY_5--;
                }
                break;
            case 5:
                if(L_DELAY_1 == 0)
                {
                    post_pmu_events(G1);
                    L_DELAY_1 = L_CUR_DELAY;
                }
                else
                    L_DELAY_1--;
            
                if(L_DELAY_2 == 0)
                {
                    post_pmu_events(G2);
                    L_DELAY_2 = L_CUR_DELAY;
                }
                else
                    L_DELAY_2--;
                break;
            case 6:
                if(L_DELAY_3 == 0)
                {
                    post_pmu_events(G3);
                    L_DELAY_3 = L_CUR_DELAY;
                }
                else
                    L_DELAY_3--;
            
                if(L_DELAY_6 == 0)
                {
                    post_pmu_events(G6);
//for groups 4,5,6,7 8ms is the fastest possible collection speed.
                    L_DELAY_6 = L_CUR_DELAY/8;
                }
                else
                    L_DELAY_6--;
                break;
            case 7:
                if(L_DELAY_1 == 0)
                {
                    post_pmu_events(G1);
                    L_DELAY_1 = L_CUR_DELAY;
                }
                else
                    L_DELAY_1--;
            
                if(L_DELAY_2 == 0)
                {
                    post_pmu_events(G2);
                    L_DELAY_2 = L_CUR_DELAY;
                }
                else
                    L_DELAY_2--;
                break;
            case 8:
                if(L_DELAY_3 == 0)
                {
                    post_pmu_events(G3);
                    L_DELAY_3 = L_CUR_DELAY;
                }
                else
                    L_DELAY_3--;
            
                if(L_DELAY_7 == 0)
                {
                    post_pmu_events(G7);
//for groups 4,5,6,7 8ms is the fastest possible collection speed.
                    L_DELAY_7 = L_CUR_DELAY/8;
                }
                else
                    L_DELAY_7--;
                break;
    
            default:
                PK_TRACE("gpe_24x7: Invalid collection state: 0x%02X", L_current_state);
                break;
        }
    }
    // Setup state to run on next call
    if(L_current_state == MAX_24x7_STATES)
        L_current_state = 1;
    else
        L_current_state++;
    //PK_TRACE("gpe_24x7: end state = %d",(int)L_current_state);

    // send back a response, IPC success even if ffdc/rc are non zeros
    rc = ipc_send_rsp(cmd, IPC_RC_SUCCESS);
    //PK_TRACE("gpe_24x7: SRN exiting thread with rc =%d", rc);
    if(rc)
    {
        *L_DBG_ITER = MARKER2;
        PK_TRACE("gpe_24x7: Failed to send response back. Halting GPE1");
        gpe_set_ffdc(&(args->error), 0x00, GPE_RC_IPC_SEND_FAILED, rc);
        pk_halt();
    }
}

/**
 * Function: configure_pmu
 **/
void configure_pmu(uint8_t state, uint64_t speed)
{//write the configuration SCOMs for all pmus.
    int i,start = (state - 1) * 16,end = state * 16;
    static volatile uint64_t* L_conf_last = (uint64_t*) (DBG_CONF_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_DBG_0 = (uint64_t*) (DBG_0 | PBA_ENABLE);
    static volatile uint64_t* L_DBG_1 = (uint64_t*) (DBG_1 | PBA_ENABLE);
    static volatile uint64_t* L_DBG_2 = (uint64_t*) (DBG_2 | PBA_ENABLE);
    static volatile uint64_t* L_DBG_3 = (uint64_t*) (DBG_3 | PBA_ENABLE);
    static volatile uint64_t* L_DBG_UAV = (uint64_t*) (DBG_UAV | PBA_ENABLE);
    putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
    
    *L_DBG_UAV = G_CUR_UAV;
    *L_DBG_3 = MARKER3;
    *L_DBG_0 = (!(G_CUR_UAV & MASK_XLNK0));
    *L_DBG_1 = (!(G_CUR_UAV & MASK_XLNK1));
    *L_DBG_2 = (!(G_CUR_UAV & MASK_XLNK2));
    if(end > TOTAL_CONFIGS)
        end = TOTAL_CONFIGS;
    for(i = start; i < end; i++)
    {
//check the availability of unit before writing the configurations to the config scoms.
//use the unit wise masks to acertain availability of a unit.
//in Nimbus, MCA is present instead of MBAs and they are updated at the location for MBAs.
//MBAs need to be checked to see which MC poerts are configured.
        if( (i==4) && !(G_CUR_UAV & MASK_MBA4) )
            continue;
        else if( (i==5) && !(G_CUR_UAV & MASK_MBA5) )
            continue;
        else if( (i==6) && !(G_CUR_UAV & MASK_MBA6) )
            continue;
        else if( (i==7) && !(G_CUR_UAV & MASK_MBA7) )
            continue;
        else if( ((i==8)||(i==9)||(i==10)||(i==11)) && (!(G_CUR_UAV & MASK_MBA4)) && (!(G_CUR_UAV & MASK_MBA5)) )
            continue;
        else if( ((i==12)||(i==13)||(i==14)||(i==15)) && (!(G_CUR_UAV & MASK_MBA6)) && (!(G_CUR_UAV & MASK_MBA7)) )
            continue;
        else if( (i==16) && !(G_CUR_UAV & MASK_MBA0) )
            continue;
        else if( (i==17) && !(G_CUR_UAV & MASK_MBA1) )
            continue;
        else if( (i==18) && !(G_CUR_UAV & MASK_MBA2) )
            continue;
        else if( (i==19) && !(G_CUR_UAV & MASK_MBA3) )
            continue;
        else if( ((i==20)||(i==21)||(i==22)||(i==23)) && (!(G_CUR_UAV & MASK_MBA0)) && (!(G_CUR_UAV & MASK_MBA1)) )
            continue;
        else if( ((i==24)||(i==25)||(i==26)||(i==27)) && (!(G_CUR_UAV & MASK_MBA2)) && (!(G_CUR_UAV & MASK_MBA3)) ) 
            continue;
        else if( (i==28) && ((!(G_CUR_UAV & MASK_XLNK0)) && (!(G_CUR_UAV & MASK_XLNK1)) && (!(G_CUR_UAV & MASK_XLNK2))) )
            continue;
        else if( ((i==29)||(i==30)||(i==31)||(i==32)) && !(G_CUR_UAV & MASK_NX) )
            continue;
        else if( (i==33) && !(G_CUR_UAV & MASK_NVLNK0) )
            continue;
        else if( (i==34) && !(G_CUR_UAV & MASK_NVLNK1) )
            continue;
        else if( (i==35) && !(G_CUR_UAV & MASK_NVLNK2) )
            continue;
        else if( (i==36) && !(G_CUR_UAV & MASK_NVLNK3) )
            continue;
        else if( (i==37) && !(G_CUR_UAV & MASK_NVLNK4) )
            continue;
        else if( (i==38) && !(G_CUR_UAV & MASK_NVLNK5) )
            continue;
        else if( ((i==39)||(i==40)) && (!(G_CUR_UAV & MASK_NVLNK5)) && (!(G_CUR_UAV & MASK_NVLNK4)) && (!(G_CUR_UAV & MASK_NVLNK3)) &&
                             (!(G_CUR_UAV & MASK_NVLNK2)) && (!(G_CUR_UAV & MASK_NVLNK1)) && (!(G_CUR_UAV & MASK_NVLNK0)) )
            continue;
        else if( (i==41) && !(G_CUR_UAV & MASK_PHB0) )
            continue;
        else if( (i==42) && !(G_CUR_UAV & MASK_PHB1) )
            continue;
        else if( (i==43) && !(G_CUR_UAV & MASK_PHB2) )
            continue;
        else if( (i==44) && !(G_CUR_UAV & MASK_PHB3) )
            continue;
        else if( (i==45) && !(G_CUR_UAV & MASK_PHB4) )
            continue;
        else if( (i==46) && !(G_CUR_UAV & MASK_PHB5) )
            continue;
        else if( (i==47) && (!(G_CUR_UAV & MASK_NVLNK1)) && (!(G_CUR_UAV & MASK_NVLNK0)) )
            continue;
        else if( (i==48) && (!(G_CUR_UAV & MASK_NVLNK3)) && (!(G_CUR_UAV & MASK_NVLNK2)) )
            continue;
        else if( (i==49) && (!(G_CUR_UAV & MASK_NVLNK5)) && (!(G_CUR_UAV & MASK_NVLNK4)) )
            continue;
        else if( ((i==50)||(i==51)||(i==52)||(i==53)) && !(G_CUR_UAV & MASK_CAPP0) )
            continue;
        else if( ((i==54)||(i==55)||(i==56)||(i==57)) && !(G_CUR_UAV & MASK_CAPP1) )
            continue;
        else
        {
            //Two sets of configurations. 
            //1.for speeds till 8ms- 8 bit prescale
            if((speed == CNTL_SPEED_1MS)||(speed == CNTL_SPEED_2MS)||(speed == CNTL_SPEED_4MS)||(speed == CNTL_SPEED_8MS))
            {
                putscom_abs(G_PMU_CONFIGS_8[i][0], G_PMU_CONFIGS_8[i][1]);
                *L_conf_last = (uint64_t)i;            
            }
            //2.for all speeds above 8 ms till 2s - 16bit prescale
            else
            {
                putscom_abs(G_PMU_CONFIGS_16[i][0], G_PMU_CONFIGS_16[i][1]);
                *L_conf_last = (uint64_t)i;            
            }
        }
    }
}

/**
 * function: post_pmu_events
 **/
void post_pmu_events(int grp)
{//read the scom pmulets. split/extract the counters.accumulate to main memory.
    volatile uint64_t* post_addr;
    static int L_phb_events =0;    
    static volatile uint64_t* L_DBG_GRP = (uint64_t*) (DBG_GRP_OFFSET | PBA_ENABLE);
    static volatile uint64_t* L_DBG_UNIT = (uint64_t*) (DBG_UNIT_OFFSET | PBA_ENABLE);
    //static volatile uint64_t* L_DBG_4 = (uint64_t*) (DBG_4 | PBA_ENABLE);
    //static volatile uint64_t* L_DBG_5 = (uint64_t*) (DBG_5 | PBA_ENABLE);
    //static volatile uint64_t* L_DBG_6 = (uint64_t*) (DBG_6 | PBA_ENABLE);
    //static volatile uint64_t* L_DBG_7 = (uint64_t*) (DBG_7 | PBA_ENABLE);
    //static uint64_t L_PHB_pmulets[24];
    //static uint64_t L_MBA_pmulets[24];
    uint64_t temp;
    volatile uint64_t temp1;
//union to split a pmulet containg 4 counters into its constituents.
    union u1
    { 
        struct event
        {
            uint16_t e[4];
        } ev;
        uint64_t pmulet;
    } u3;
   
    union u_mba
    { 
        struct event_mba
        {
            uint32_t e[2];
        } ev;
        uint64_t pmulet;
    } u3_mba;
     
    int i=0,j=0,phb_epoch=0;
    uint64_t TOD;
    int iter=0, ev_count=0;
    post_addr = (uint64_t*) (POSTING_START | PBA_ENABLE);
    putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
    //FIXME:restricting groups for test on witherspoon
    //remove once done.
    //int test_grp = G1;
    //
    switch(grp)
    //switch(test_grp)
    {
        case G1://cnpm group - always written
            if(G_CUR_MODE == CNTL_MODE_MONITOR) 
                post_addr = (uint64_t*) (POST_OFFSET_G1H | PBA_ENABLE);
            else if(G_CUR_MODE == CNTL_MODE_DEBUG1) 
                post_addr = (uint64_t*) (POST_OFFSET_DBG1H | PBA_ENABLE);
            *L_DBG_GRP = 1;
            *L_DBG_UNIT = 1;
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            post_addr++;
            for(i=0; i<8; i++)
            {   
                getscom_abs(G_PMULETS_1[i], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if(G_CUR_MODE == CNTL_MODE_MONITOR) 
                post_addr = (uint64_t*) (POST_OFFSET_G1T | PBA_ENABLE);
            else if(G_CUR_MODE == CNTL_MODE_DEBUG1) 
                post_addr = (uint64_t*) (POST_OFFSET_DBG1T | PBA_ENABLE);
            *post_addr = INC_UPD_COUNT;
            break;
        case G2://XLINKS and NX. Read scoms based on availability.
            *L_DBG_GRP = 2;
            post_addr = (uint64_t*) (POST_OFFSET_G2H | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            post_addr++;
            if ((G_CUR_UAV & MASK_XLNK0) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 1;
                post_addr = (uint64_t*) (POST_OFFSET_G2_1 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_2[0], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
                getscom_abs(G_PMULETS_2[1], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_XLNK1) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 2;
                post_addr = (uint64_t*) (POST_OFFSET_G2_2 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_2[2], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
                getscom_abs(G_PMULETS_2[3], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_XLNK2) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 3;
                post_addr = (uint64_t*) (POST_OFFSET_G2_3 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_2[4], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
                getscom_abs(G_PMULETS_2[5], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_NX) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 4;
                post_addr = (uint64_t*) (POST_OFFSET_G2_4 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_2[6], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
                getscom_abs(G_PMULETS_2[7], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            post_addr = (uint64_t*) (POST_OFFSET_G2T | PBA_ENABLE);
            *post_addr = INC_UPD_COUNT;
            break;
        case G3://NVLINKS -NTL,ATS,XTS
            *L_DBG_GRP = 3;
            post_addr = (uint64_t*) (POST_OFFSET_G3H | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            post_addr++;
            if ((G_CUR_UAV & MASK_NVLNK0) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 1;
                getscom_abs(G_PMULETS_3[0], &u3.pmulet);
                post_addr = (uint64_t*) (POST_OFFSET_G3_1 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_NVLNK1) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 2;
                getscom_abs(G_PMULETS_3[1], &u3.pmulet);
                post_addr = (uint64_t*) (POST_OFFSET_G3_2 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_NVLNK2) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 3;
                getscom_abs(G_PMULETS_3[2], &u3.pmulet);
                post_addr = (uint64_t*) (POST_OFFSET_G3_3 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_NVLNK3) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 4;
                getscom_abs(G_PMULETS_3[3], &u3.pmulet);
                post_addr = (uint64_t*) (POST_OFFSET_G3_4 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_NVLNK4) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 5;
                getscom_abs(G_PMULETS_3[4], &u3.pmulet);
                post_addr = (uint64_t*) (POST_OFFSET_G3_5 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_NVLNK5) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 6;
                getscom_abs(G_PMULETS_3[5], &u3.pmulet);
                post_addr = (uint64_t*) (POST_OFFSET_G3_6 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ( ((G_CUR_UAV & MASK_NVLNK0) > 0) || ((G_CUR_UAV & MASK_NVLNK1) > 0) || ((G_CUR_UAV & MASK_NVLNK2) > 0)||
                 ((G_CUR_UAV & MASK_NVLNK3) > 0) || ((G_CUR_UAV & MASK_NVLNK4) > 0) || ((G_CUR_UAV & MASK_NVLNK5) > 0)  )
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 7;
                getscom_abs(G_PMULETS_3[6], &u3.pmulet);
                post_addr = (uint64_t*) (POST_OFFSET_G3_7 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
                *L_DBG_UNIT = 8;
                getscom_abs(G_PMULETS_3[7], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            post_addr = (uint64_t*) (POST_OFFSET_G3T | PBA_ENABLE);
            *post_addr = INC_UPD_COUNT;
            break;
        case G4:
//PHB events are present on separate 48-bit SCOM registers. They dont clear on read.
//But wrap around. After each read, the value is subtracted from the prvious value to 
//get current increment which is then atomically added to the main memory.
//Totally 24 scoms of the PHB are split across 3 epochs for the same group.
// Only 8 scoms are read and updated to memory in an epoch to abide the 25us run time restriction.
            *L_DBG_GRP = 4;
            post_addr = (uint64_t*) (POST_OFFSET_G4H | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            post_addr++;
            if(L_phb_events > 23)
                L_phb_events = 0;  
            phb_epoch = L_phb_events/8;
            switch(phb_epoch)
            {
                case 0:
                    if ((G_CUR_UAV & MASK_PHB0) > 0)
                    {
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                        *L_DBG_UNIT = 1;
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                        post_addr = (uint64_t*) (POST_OFFSET_G4a | PBA_ENABLE);
                        for(i=0; i<4; i++)
                        {
                            getscom_abs(G_PMULETS_4a[i], &temp);
                            temp >>=16;
                            temp1 = temp;
                            temp = get_phb_event(temp, G_PHB_pmulets[L_phb_events] );
                            if(G_PHB_pmulets[L_phb_events] != 0)
                                *post_addr = temp;
                            G_PHB_pmulets[L_phb_events] = temp1;
                            post_addr++;
                            L_phb_events++;
                        }
                    }
                    else
                        L_phb_events += 4;
                    
              
                    if ((G_CUR_UAV & MASK_PHB1) > 0)
                    {
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                        *L_DBG_UNIT = 2;
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                        post_addr = (uint64_t*) (POST_OFFSET_G4b | PBA_ENABLE);
                        for(i=0; i<4; i++)
                        {
                            getscom_abs(G_PMULETS_4b[i], &temp);
                            temp >>=16;
                            temp1 = temp;
                            temp = get_phb_event(temp, G_PHB_pmulets[L_phb_events] );
                            if(G_PHB_pmulets[L_phb_events] != 0)
                                *post_addr = temp;
                            G_PHB_pmulets[L_phb_events] = temp1;
                            post_addr++;
                            L_phb_events++;
                        }
                    }
                    else
                        L_phb_events += 4;
                    
                    break;
                 case 1:
                    if ((G_CUR_UAV & MASK_PHB2) > 0)
                    {
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                        *L_DBG_UNIT = 3;
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                        post_addr = (uint64_t*) (POST_OFFSET_G4c | PBA_ENABLE);
                        for(i=0; i<4; i++)
                        {
                            getscom_abs(G_PMULETS_4c[i], &temp);
                            temp >>=16;
                            temp1 = temp;
                            temp = get_phb_event(temp, G_PHB_pmulets[L_phb_events] );
                            if(G_PHB_pmulets[L_phb_events] != 0)
                                *post_addr = temp;
                            G_PHB_pmulets[L_phb_events] = temp1;
                            post_addr++;
                            L_phb_events++;
                        }
                    }
                    else
                        L_phb_events += 4;
                    
                    if ((G_CUR_UAV & MASK_PHB3) > 0)
                    {
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                        *L_DBG_UNIT = 4;
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                        post_addr = (uint64_t*) (POST_OFFSET_G4d | PBA_ENABLE);
                        for(i=0; i<4; i++)
                        {
                            getscom_abs(G_PMULETS_4d[i], &temp);
                            temp >>=16;
                            temp1 = temp;
                            temp = get_phb_event(temp, G_PHB_pmulets[L_phb_events] );
                            if(G_PHB_pmulets[L_phb_events] != 0)
                                *post_addr = temp;
                            G_PHB_pmulets[L_phb_events] = temp1;
                            post_addr++;
                            L_phb_events++;
                        }
                    }
                    else
                        L_phb_events += 4;

                    break; 
                 case 2:
                    if ((G_CUR_UAV & MASK_PHB4) > 0)
                    {
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                        *L_DBG_UNIT = 5;
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                        post_addr = (uint64_t*) (POST_OFFSET_G4e | PBA_ENABLE);
                        for(i=0; i<4; i++)
                        {
                            getscom_abs(G_PMULETS_4e[i], &temp);
                            temp >>=16;
                            temp1 = temp;
                            temp = get_phb_event(temp, G_PHB_pmulets[L_phb_events] );
                            if(G_PHB_pmulets[L_phb_events] != 0)
                                *post_addr = temp;
                            G_PHB_pmulets[L_phb_events] = temp1;
                            post_addr++;
                            L_phb_events++;
                        }
                    }
                    else
                        L_phb_events += 4;
                    if ((G_CUR_UAV & MASK_PHB5) > 0)
                    {
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                        *L_DBG_UNIT = 6;
                        putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                        post_addr = (uint64_t*) (POST_OFFSET_G4f | PBA_ENABLE);
                        for(i=0; i<4; i++)
                        {
                            getscom_abs(G_PMULETS_4f[i], &temp);
                            temp >>=16;
                            temp1 = temp;
                            temp = get_phb_event(temp, G_PHB_pmulets[L_phb_events] );
                            if(G_PHB_pmulets[L_phb_events] != 0)
                                *post_addr = temp;
                            G_PHB_pmulets[L_phb_events] = temp1;
                            post_addr++;
                            L_phb_events++;
                        }
                    }
                    else
                        L_phb_events += 4;
                    break;  
            }
            post_addr = (uint64_t*) (POST_OFFSET_G4T | PBA_ENABLE);
            *post_addr = INC_UPD_COUNT;
            break;
        case G5://MBAs - 8 fixed counters  for MBA0-3 that dont clear on read but wraparound on overflow.
            *L_DBG_GRP = 5;
            post_addr = (uint64_t*) (POST_OFFSET_G5H | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            post_addr++;
            if ((G_CUR_UAV & MASK_MBA0) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 1;
                post_addr = (uint64_t*) (POST_OFFSET_G5_1 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_5[0], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=0;iter<2;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_5[1], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[2]);
                if(G_MBA_pmulets[2] != 0)
                    *post_addr = temp<<1;//needs scaling by 2;
                G_MBA_pmulets[2] = temp1;
            }
            if ((G_CUR_UAV & MASK_MBA1) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 2;
                post_addr = (uint64_t*) (POST_OFFSET_G5_2 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_5[2], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=3;iter<5;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_5[3], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[5]);
                if(G_MBA_pmulets[5] != 0)
                    *post_addr = temp<<1;//needs scaling by 2;
                G_MBA_pmulets[5] = temp1;
            }
            if ((G_CUR_UAV & MASK_MBA2) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 3;
                post_addr = (uint64_t*) (POST_OFFSET_G5_3 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_5[4], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=6;iter<8;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_5[5], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[8]);
                if(G_MBA_pmulets[8] != 0)
                    *post_addr = temp<<1;//needs scaling by 2;
                G_MBA_pmulets[8] = temp1;
            }
            if ((G_CUR_UAV & MASK_MBA3) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 4;
                post_addr = (uint64_t*) (POST_OFFSET_G5_4 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_5[6], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=9;iter<11;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_5[7], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[11]);
                if(G_MBA_pmulets[11] != 0)
                    *post_addr = temp<<1;//needs scaling by 2;
                G_MBA_pmulets[11] = temp1;
            }
            post_addr = (uint64_t*) (POST_OFFSET_G5T | PBA_ENABLE);
            *post_addr = INC_UPD_COUNT;
            break;
        case G6://8 more fixed scoms for MBA4-7. no clear on read.wraparound.
            *L_DBG_GRP = 6;
            post_addr = (uint64_t*) (POST_OFFSET_G6H | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            post_addr++;
            if ((G_CUR_UAV & MASK_MBA4) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 1;
                post_addr = (uint64_t*) (POST_OFFSET_G6_1 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_6[0], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=12;iter<14;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_6[1], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[14]);
                if(G_MBA_pmulets[14] != 0)
                    *post_addr = temp<<1;//needs scaling by 2;
                G_MBA_pmulets[14] = temp1;
            }
            if ((G_CUR_UAV & MASK_MBA5) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 2;
                post_addr = (uint64_t*) (POST_OFFSET_G6_2 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_6[2], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=15;iter<17;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_6[3], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[17]);
                if(G_MBA_pmulets[17] != 0)
                    *post_addr = temp<<1;//needs scaling by 2
                G_MBA_pmulets[17] = temp1;
            }
            if ((G_CUR_UAV & MASK_MBA6) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 3;
                post_addr = (uint64_t*) (POST_OFFSET_G6_3 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_6[4], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=18;iter<20;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_6[5], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[20]);
                if(G_MBA_pmulets[20] != 0)
                    *post_addr = temp<<1;//needs scaling by 2
                G_MBA_pmulets[20] = temp1;
            }
            if ((G_CUR_UAV & MASK_MBA7) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 4;
                post_addr = (uint64_t*) (POST_OFFSET_G6_4 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_6[6], &u3_mba.pmulet);
                ev_count = 0;
                for(iter=21;iter<23;iter++)
                {
                    temp = u3_mba.ev.e[ev_count];
                    temp1 = temp;
                    temp = get_mba_event(temp,  G_MBA_pmulets[iter]);
                    if(G_MBA_pmulets[iter] != 0)
                        *post_addr = temp;
                    G_MBA_pmulets[iter] = temp1;
                    post_addr++;
                    ev_count++;
                }
                //only first 32-bit counter is used to get cycles.
                getscom_abs(G_PMULETS_6[7], &u3_mba.pmulet);
                temp = u3_mba.ev.e[0];
                temp1 = temp;
                temp =  get_mba_event(temp,  G_MBA_pmulets[23]);
                if(G_MBA_pmulets[23] != 0)
                    *post_addr = temp<<1;//needs scaling by 2
                G_MBA_pmulets[23] = temp1;
            }
            post_addr = (uint64_t*) (POST_OFFSET_G6T | PBA_ENABLE);
            *post_addr = INC_UPD_COUNT;
            break;
        case G7://3 NVLINK-NPCQ's and 2 CAPP pmulets each with 4 counters.TOD present here.
            *L_DBG_GRP = 7;
            post_addr = (uint64_t*) (POST_OFFSET_G6H | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            post_addr++;
            if ( ((G_CUR_UAV & MASK_NVLNK0) > 0) || ((G_CUR_UAV & MASK_NVLNK1) > 0) )
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 1;
                post_addr = (uint64_t*) (POST_OFFSET_G7_1 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_7[0], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ( ((G_CUR_UAV & MASK_NVLNK2) > 0) || ((G_CUR_UAV & MASK_NVLNK3) > 0) )
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 2;
                post_addr = (uint64_t*) (POST_OFFSET_G7_2 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_7[1], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ( ((G_CUR_UAV & MASK_NVLNK4) > 0) || ((G_CUR_UAV & MASK_NVLNK5) > 0) )
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 3;
                post_addr = (uint64_t*) (POST_OFFSET_G7_3 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_7[2], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_CAPP0) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 4;
                post_addr = (uint64_t*) (POST_OFFSET_G7_4 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_7[3], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
                getscom_abs(G_PMULETS_7[4], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            if ((G_CUR_UAV & MASK_CAPP1) > 0)
            {
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
                *L_DBG_UNIT = 5;
                post_addr = (uint64_t*) (POST_OFFSET_G7_5 | PBA_ENABLE);
                putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
                getscom_abs(G_PMULETS_7[5], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
                getscom_abs(G_PMULETS_7[6], &u3.pmulet);
                for(j=0; j<4; j++)
                {
                    *post_addr = (uint64_t)u3.ev.e[j];
                    post_addr++;
                }
            }
            getscom_abs(TOD_VALUE_REG,&TOD);
            post_addr = (uint64_t*) (POST_OFFSET_G7_6 | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
            *post_addr = TOD;
            post_addr = (uint64_t*) (POST_OFFSET_G7T | PBA_ENABLE);
            putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_ATOMIC);
            *post_addr = INC_UPD_COUNT;
            break;
        default:
            PK_TRACE("gpe_24x7: Invalid Group:%d",grp);
            break;
    }
}

/**
 * function: initialize_postings
 **/
void initialize_postings()
{//initialize posting area.
    volatile uint64_t* post_addr;
    int i;
    putscom_abs(PBASLVCTL3_C0040030, PBASLV_SET_DMA);
    post_addr = (uint64_t*) (POSTING_START | PBA_ENABLE);
    for(i=0; i<TOTAL_POSTINGS; i++)
    {
        *post_addr = (uint64_t)0x0;
        post_addr++;
    }
}

/**
 * function: set_speed
 **/
void set_speed(uint64_t* speed, uint8_t* delay, volatile uint64_t* status)
{//set counter-scom read delay according to speed setting.
    switch(*speed)
    {
        case CNTL_SPEED_1MS: 
           *delay = 0;
           break;
        case CNTL_SPEED_2MS: 
           *delay = 1;
           break;
        case CNTL_SPEED_4MS: 
           *delay = 2;
           break;
        case CNTL_SPEED_8MS: 
           *delay = 3;
           break;
        case CNTL_SPEED_16MS: 
           *delay = 4;
           break;
        case CNTL_SPEED_32MS: 
           *delay = 5;
           break;
        case CNTL_SPEED_64MS: 
           *delay = 6;
           break;
        case CNTL_SPEED_128MS: 
           *delay = 7;
           break;
        case CNTL_SPEED_256MS: 
           *delay = 8;
           break;
        case CNTL_SPEED_512MS: 
           *delay = 9;
           break;
        case CNTL_SPEED_1024MS: 
           *delay = 10;
           break;
        case CNTL_SPEED_2048MS: 
           *delay = 11;
           break;
        default:
          *status = CNTL_STATUS_ERR1;
        break;
     }
}

/**
 * function: get_mba_event
 **/
uint64_t get_mba_event(uint64_t cur, uint64_t prev)
{
    if(cur >= prev)
        prev = cur - prev;
    else
        prev = (MAX_32 - prev) + cur;
    return prev;
}

/**
 * function: get_phb_event
 **/
uint64_t get_phb_event(uint64_t cur, uint64_t prev)
{
    if(cur >= prev)
        prev = cur - prev;
    else
        prev = (MAX_48 - prev) + cur;
    return prev;
}

