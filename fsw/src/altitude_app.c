/************************************************************************
 * NASA Docket No. GSC-18,719-1, and identified as “core Flight System: Bootes”
 *
 * Copyright (c) 2020 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * \file
 *   This file contains the source code for the Altitude App.
 */

/*
** Include Files:
*/
#include "altitude_app_events.h"
#include "altitude_app_version.h"
#include "altitude_app.h"

#include <string.h>
#include "mpl3115a2.h"

/*
** global data
*/
ALTITUDE_APP_Data_t ALTITUDE_APP_Data;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/*                                                                            */
/* Application entry point and main process loop                              */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void ALTITUDE_APP_Main(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(ALTITUDE_APP_PERF_ID);

    /*
    ** Perform application specific initialization
    ** If the Initialization fails, set the RunStatus to
    ** CFE_ES_RunStatus_APP_ERROR and the App will not enter the RunLoop
    */
    status = ALTITUDE_APP_Init();
    if (status == CFE_SUCCESS)
    {
      status = sensor_mpl3115a2_init();
      if (status != CFE_SUCCESS)
      {
        ALTITUDE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        CFE_EVS_SendEvent(ALTITUDE_APP_DEV_INF_EID, CFE_EVS_EventType_ERROR,
          "ALTITUDE APP: Error initializing MPL3115A2\n");
        }
    }else{
      ALTITUDE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }


    /*
    ** ALTITUDE Runloop
    */
    while (CFE_ES_RunLoop(&ALTITUDE_APP_Data.RunStatus) == true)
    {
        /*
        ** Performance Log Exit Stamp
        */
        CFE_ES_PerfLogExit(ALTITUDE_APP_PERF_ID);

        /* Pend on receipt of command packet */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, ALTITUDE_APP_Data.CommandPipe, CFE_SB_PEND_FOREVER);

        /*
        ** Performance Log Entry Stamp
        */
        CFE_ES_PerfLogEntry(ALTITUDE_APP_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            ALTITUDE_APP_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(ALTITUDE_APP_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "ALTITUDE APP: SB Pipe Read Error, App Will Exit");

            ALTITUDE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Performance Log Exit Stamp
    */
    CFE_ES_PerfLogExit(ALTITUDE_APP_PERF_ID);

    CFE_ES_ExitApp(ALTITUDE_APP_Data.RunStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/*                                                                            */
/* Initialization                                                             */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
int32 ALTITUDE_APP_Init(void)
{
    int32 status;

    ALTITUDE_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Initialize app command execution counters
    */
    ALTITUDE_APP_Data.CmdCounter = 0;
    ALTITUDE_APP_Data.ErrCounter = 0;
    ALTITUDE_APP_Data.SeaPressure = 1013.26;
    ALTITUDE_APP_Data.AltitudeOffset = 113;

    /*
    ** Initialize app configuration data
    */
    ALTITUDE_APP_Data.PipeDepth = ALTITUDE_APP_PIPE_DEPTH;

    strncpy(ALTITUDE_APP_Data.PipeName, "ALTITUDE_APP_CMD_PIPE", sizeof(ALTITUDE_APP_Data.PipeName));
    ALTITUDE_APP_Data.PipeName[sizeof(ALTITUDE_APP_Data.PipeName) - 1] = 0;

    /*
    ** Initialize event filter table...
    */
    ALTITUDE_APP_Data.EventFilters[0].EventID = ALTITUDE_APP_STARTUP_INF_EID;
    ALTITUDE_APP_Data.EventFilters[0].Mask    = 0x0000;
    ALTITUDE_APP_Data.EventFilters[1].EventID = ALTITUDE_APP_COMMAND_ERR_EID;
    ALTITUDE_APP_Data.EventFilters[1].Mask    = 0x0000;
    ALTITUDE_APP_Data.EventFilters[2].EventID = ALTITUDE_APP_COMMANDNOP_INF_EID;
    ALTITUDE_APP_Data.EventFilters[2].Mask    = 0x0000;
    ALTITUDE_APP_Data.EventFilters[3].EventID = ALTITUDE_APP_COMMANDRST_INF_EID;
    ALTITUDE_APP_Data.EventFilters[3].Mask    = 0x0000;
    ALTITUDE_APP_Data.EventFilters[4].EventID = ALTITUDE_APP_INVALID_MSGID_ERR_EID;
    ALTITUDE_APP_Data.EventFilters[4].Mask    = 0x0000;
    ALTITUDE_APP_Data.EventFilters[5].EventID = ALTITUDE_APP_LEN_ERR_EID;
    ALTITUDE_APP_Data.EventFilters[5].Mask    = 0x0000;
    ALTITUDE_APP_Data.EventFilters[6].EventID = ALTITUDE_APP_PIPE_ERR_EID;
    ALTITUDE_APP_Data.EventFilters[6].Mask    = 0x0000;
    ALTITUDE_APP_Data.EventFilters[7].EventID = ALTITUDE_APP_DEV_INF_EID;
    ALTITUDE_APP_Data.EventFilters[7].Mask    = 0x0000;

    /*
    ** Register the events
    */
    status = CFE_EVS_Register(ALTITUDE_APP_Data.EventFilters, ALTITUDE_APP_EVENT_COUNTS, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Altitude App: Error Registering Events, RC = 0x%08lX\n", (unsigned long)status);
        return status;
    }

    /*
    ** Initialize housekeeping packet (clear user data area).
    */
    CFE_MSG_Init(CFE_MSG_PTR(ALTITUDE_APP_Data.HkTlm.TelemetryHeader), CFE_SB_ValueToMsgId(ALTITUDE_APP_HK_TLM_MID),
                 sizeof(ALTITUDE_APP_Data.HkTlm));

    /*
    ** Initialize output RF packet.
    */
    CFE_MSG_Init(CFE_MSG_PTR(ALTITUDE_APP_Data.OutData.TelemetryHeader), CFE_SB_ValueToMsgId(ALTITUDE_APP_RF_DATA_MID),
                 sizeof(ALTITUDE_APP_Data.OutData));
    ALTITUDE_APP_Data.OutData.App_Pckg_Counter = 0;

    /*
    ** Initialize temperature packet.
    */
    CFE_MSG_Init(CFE_MSG_PTR(ALTITUDE_APP_Data.TempData.TelemetryHeader), CFE_SB_ValueToMsgId(ALTITUDE_APP_TEMP_MID),
                sizeof(ALTITUDE_APP_Data.TempData));

    /*
    ** Create Software Bus message pipe.
    */
    status = CFE_SB_CreatePipe(&ALTITUDE_APP_Data.CommandPipe, ALTITUDE_APP_Data.PipeDepth, ALTITUDE_APP_Data.PipeName);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Altitude App: Error creating pipe, RC = 0x%08lX\n", (unsigned long)status);
        return status;
    }

    /*
    ** Subscribe to Housekeeping request commands
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ALTITUDE_APP_SEND_HK_MID), ALTITUDE_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Altitude App: Error Subscribing to HK request, RC = 0x%08lX\n", (unsigned long)status);
        return status;
    }

    /*
    ** Subscribe to RF command packets
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ALTITUDE_APP_SEND_RF_MID), ALTITUDE_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Altitude App: Error Subscribing to Command, RC = 0x%08lX\n", (unsigned long)status);

        return status;
    }

    /*
    ** Subscribe to Read command packets
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ALTITUDE_APP_READ_MID), ALTITUDE_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Altitude App: Error Subscribing to Command, RC = 0x%08lX\n", (unsigned long)status);

        return status;
    }

    /*
    ** Subscribe to Send Temp command packets
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ALTITUDE_APP_SEND_TP_MID), ALTITUDE_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Altitude App: Error Subscribing to Command, RC = 0x%08lX\n", (unsigned long)status);

        return status;
    }

    /*
    ** Subscribe to ground command packets
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ALTITUDE_APP_CMD_MID), ALTITUDE_APP_Data.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("Altitude App: Error Subscribing to Command, RC = 0x%08lX\n", (unsigned long)status);

        return status;
    }

    CFE_EVS_SendEvent(ALTITUDE_APP_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION, "Altitude Tlm App Initialized.%s",
                     ALTITUDE_APP_VERSION_STRING);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*     This routine will process any packet that is received on the ALTITUDE    */
/*     command pipe.                                                          */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
void ALTITUDE_APP_ProcessCommandPacket(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case ALTITUDE_APP_CMD_MID:
            ALTITUDE_APP_ProcessGroundCommand(SBBufPtr);
            break;

        case ALTITUDE_APP_SEND_HK_MID:
            ALTITUDE_APP_ReportHousekeeping((CFE_MSG_CommandHeader_t *)SBBufPtr);
            break;

        case ALTITUDE_APP_SEND_RF_MID:
            ALTITUDE_APP_ReportRFTelemetry((CFE_MSG_CommandHeader_t *)SBBufPtr);
            break;

        case ALTITUDE_APP_READ_MID:
            ALTITUDE_APP_ReadSensor((CFE_MSG_CommandHeader_t *)SBBufPtr);
            break;

        case ALTITUDE_APP_SEND_TP_MID:
            ALTITUDE_APP_SendTemp((CFE_MSG_CommandHeader_t *)SBBufPtr);
            break;

        default:
            CFE_EVS_SendEvent(ALTITUDE_APP_INVALID_MSGID_ERR_EID, CFE_EVS_EventType_ERROR,
                              "ALTITUDE: invalid command packet,MID = 0x%x", (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            break;
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* ALTITUDE ground commands                                                     */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
void ALTITUDE_APP_ProcessGroundCommand(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    /*
    ** Process "known" ALTITUDE app ground commands
    */
    switch (CommandCode)
    {
        case ALTITUDE_APP_NOOP_CC:
            if (ALTITUDE_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ALTITUDE_APP_NoopCmd_t)))
            {
                ALTITUDE_APP_Noop((ALTITUDE_APP_NoopCmd_t *)SBBufPtr);
            }

            break;

        case ALTITUDE_APP_RESET_COUNTERS_CC:
            if (ALTITUDE_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ALTITUDE_APP_ResetCountersCmd_t)))
            {
                ALTITUDE_APP_ResetCounters((ALTITUDE_APP_ResetCountersCmd_t *)SBBufPtr);
            }

            break;

        case ALTITUDE_APP_SET_SEA_PRESSURE_CC:
            if (ALTITUDE_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ALTITUDE_APP_SetSeaPressureCmd_t)))
            {
                ALTITUDE_APP_SetSeaPressure((ALTITUDE_APP_SetSeaPressureCmd_t *)SBBufPtr);
            }

            break;

        case ALTITUDE_APP_SET_ALTITUDE_OFFSET_CC:
            if (ALTITUDE_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ALTITUDE_APP_SetAltitudeOffsetCmd_t)))
            {
                ALTITUDE_APP_SetAltitudeOffset((ALTITUDE_APP_SetAltitudeOffsetCmd_t *)SBBufPtr);
            }

            break;

        case ALTITUDE_APP_RESET_ALTOFF_SEAPR_CC:
            if (ALTITUDE_APP_VerifyCmdLength(&SBBufPtr->Msg, sizeof(ALTITUDE_APP_ResetAltOffSeaPressCmd_t)))
            {
                ALTITUDE_APP_ResetAltOffSeaPress((ALTITUDE_APP_ResetAltOffSeaPressCmd_t *)SBBufPtr);
            }

            break;

        /* default case already found during FC vs length test */
        default:
            CFE_EVS_SendEvent(ALTITUDE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Invalid ground command code: CC = %d", CommandCode);
            break;
    }
}

int32 ALTITUDE_APP_ReadSensor(const CFE_MSG_CommandHeader_t *Msg){
  sensor_mpl3115a2_setSeaPressure(ALTITUDE_APP_Data.SeaPressure);
  sensor_mpl3115a2_setAltitudeOffset(ALTITUDE_APP_Data.AltitudeOffset);
  ALTITUDE_APP_Data.AltitudeRead    = sensor_mpl3115a2_getAltitude();
  ALTITUDE_APP_Data.TempRead = sensor_mpl3115a2_getTemperature();

  return CFE_SUCCESS;
}

int32 ALTITUDE_APP_SendTemp(const CFE_MSG_CommandHeader_t *Msg){
  /* Copy the sensor's temperature */
  ALTITUDE_APP_Data.TempData.temperature = ALTITUDE_APP_Data.TempRead;

  /*
  ** Send temperature packet...
  */
  CFE_SB_TimeStampMsg(CFE_MSG_PTR(ALTITUDE_APP_Data.TempData.TelemetryHeader));
  CFE_SB_TransmitMsg(CFE_MSG_PTR(ALTITUDE_APP_Data.TempData.TelemetryHeader), true);

  return CFE_SUCCESS;
}

int32 ALTITUDE_APP_ReportRFTelemetry(const CFE_MSG_CommandHeader_t *Msg){

    /*
    ** Get command execution counters...
    */
    ALTITUDE_APP_Data.OutData.CommandErrorCounter = ALTITUDE_APP_Data.ErrCounter;
    ALTITUDE_APP_Data.OutData.CommandCounter      = ALTITUDE_APP_Data.CmdCounter;

    /* Get the app ID */
    ALTITUDE_APP_Data.OutData.AppID_H = (uint8_t) ((ALTITUDE_APP_HK_TLM_MID >> 8) & 0xff);
    ALTITUDE_APP_Data.OutData.AppID_L = (uint8_t) (ALTITUDE_APP_HK_TLM_MID & 0xff);

    ++ALTITUDE_APP_Data.OutData.App_Pckg_Counter;

    /* Copy the sensors data */
    uint8_t *aux_array1;
    aux_array1 = NULL;
    aux_array1 = malloc(4 * sizeof(uint8_t));
    aux_array1 = (uint8_t*)(&ALTITUDE_APP_Data.AltitudeRead);

    uint8_t *aux_array2;
    aux_array2 = NULL;
    aux_array2 = malloc(4 * sizeof(uint8_t));
    aux_array2 = (uint8_t*)(&ALTITUDE_APP_Data.SeaPressure);

    uint8_t aux_byte = (uint8_t) ALTITUDE_APP_Data.AltitudeOffset;
    uint8_t aux_array3[] = {aux_byte,0,0,0};

    for(int i=0;i<4;i++){
      ALTITUDE_APP_Data.OutData.byte_group_1[i] = aux_array1[i];
      ALTITUDE_APP_Data.OutData.byte_group_2[i] = aux_array2[i];
      ALTITUDE_APP_Data.OutData.byte_group_3[i] = aux_array3[i];
      ALTITUDE_APP_Data.OutData.byte_group_4[i] = 0;
      ALTITUDE_APP_Data.OutData.byte_group_5[i] = 0;
      ALTITUDE_APP_Data.OutData.byte_group_6[i] = 0;
    }

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(CFE_MSG_PTR(ALTITUDE_APP_Data.OutData.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(ALTITUDE_APP_Data.OutData.TelemetryHeader), true);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
int32 ALTITUDE_APP_ReportHousekeeping(const CFE_MSG_CommandHeader_t *Msg)
{
    /*
    ** Get command execution counters...
    */
    ALTITUDE_APP_Data.HkTlm.Payload.CommandErrorCounter = ALTITUDE_APP_Data.ErrCounter;
    ALTITUDE_APP_Data.HkTlm.Payload.CommandCounter      = ALTITUDE_APP_Data.CmdCounter;

    // Get the sensor data
    ALTITUDE_APP_Data.HkTlm.Payload.AltitudeRead        = ALTITUDE_APP_Data.AltitudeRead;
    ALTITUDE_APP_Data.HkTlm.Payload.SeaPressure         = ALTITUDE_APP_Data.SeaPressure;
    ALTITUDE_APP_Data.HkTlm.Payload.AltitudeOffset      = ALTITUDE_APP_Data.AltitudeOffset;

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(CFE_MSG_PTR(ALTITUDE_APP_Data.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(ALTITUDE_APP_Data.HkTlm.TelemetryHeader), true);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* ALTITUDE NOOP commands                                                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
int32 ALTITUDE_APP_Noop(const ALTITUDE_APP_NoopCmd_t *Msg)
{
    ALTITUDE_APP_Data.CmdCounter++;

    CFE_EVS_SendEvent(ALTITUDE_APP_COMMANDNOP_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: NOOP command %s",
                      ALTITUDE_APP_VERSION);

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
int32 ALTITUDE_APP_ResetCounters(const ALTITUDE_APP_ResetCountersCmd_t *Msg)
{
    ALTITUDE_APP_Data.CmdCounter = 0;
    ALTITUDE_APP_Data.ErrCounter = 0;

    CFE_EVS_SendEvent(ALTITUDE_APP_COMMANDRST_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: RESET command");

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* ALTITUDE Set Sea Pressure value for the altitude measurement by the sensor */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
int32 ALTITUDE_APP_SetSeaPressure(const ALTITUDE_APP_SetSeaPressureCmd_t *Msg)
{
    ALTITUDE_APP_Data.CmdCounter++;
    float sea_pressure = (float) Msg->Pressure;
    uint16_t bar = sea_pressure * 50;
    if((sea_pressure > 0) &&(bar < 0xFFFF)){
      ALTITUDE_APP_Data.SeaPressure = sea_pressure;
      CFE_EVS_SendEvent(ALTITUDE_APP_DEV_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Sea Pressure set to %f",ALTITUDE_APP_Data.SeaPressure);
    }else{
      ALTITUDE_APP_Data.ErrCounter++;
      CFE_EVS_SendEvent(ALTITUDE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Incorrect value for Sea Pressure");
    }


    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* ALTITUDE Set Altitude offset command                                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
int32 ALTITUDE_APP_SetAltitudeOffset(const ALTITUDE_APP_SetAltitudeOffsetCmd_t *Msg)
{
    ALTITUDE_APP_Data.CmdCounter++;
    int8_t offset = Msg->AltOffset;

    if((offset > -128) && (offset < 127)){
      ALTITUDE_APP_Data.AltitudeOffset = offset;
      CFE_EVS_SendEvent(ALTITUDE_APP_DEV_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Altitude offset set to %d",ALTITUDE_APP_Data.AltitudeOffset);
    }else{
      ALTITUDE_APP_Data.ErrCounter++;
      CFE_EVS_SendEvent(ALTITUDE_APP_COMMAND_ERR_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Altitude offset must be between -128m and 127m");
    }



    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* ALTITUDE Reset Sea Pressure and Altitude offset to default command         */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
int32 ALTITUDE_APP_ResetAltOffSeaPress(const ALTITUDE_APP_ResetAltOffSeaPressCmd_t *Msg)
{
    ALTITUDE_APP_Data.CmdCounter++;

    ALTITUDE_APP_Data.SeaPressure = 1013.26;
    ALTITUDE_APP_Data.AltitudeOffset = 0;


    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* Verify command packet length                                               */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
bool ALTITUDE_APP_VerifyCmdLength(CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    bool              result       = true;
    size_t            ActualLength = 0;
    CFE_SB_MsgId_t    MsgId        = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t FcnCode      = 0;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);

    /*
    ** Verify the command packet length.
    */
    if (ExpectedLength != ActualLength)
    {
        CFE_MSG_GetMsgId(MsgPtr, &MsgId);
        CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);

        CFE_EVS_SendEvent(ALTITUDE_APP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid Msg length: ID = 0x%X,  CC = %u, Len = %u, Expected = %u",
                          (unsigned int)CFE_SB_MsgIdToValue(MsgId), (unsigned int)FcnCode, (unsigned int)ActualLength,
                          (unsigned int)ExpectedLength);

        result = false;

        ALTITUDE_APP_Data.ErrCounter++;
    }

    return result;
}

int32 sensor_mpl3115a2_init(void){

  int rv;
  int fd;

  static const char bus_path[] = "/dev/i2c-2";
  static const char mpl3115a2_path[] = "/dev/i2c-2.mpl3115a2-0";

  // Device registration
  rv = i2c_dev_register_sensor_mpl3115a2(
    &bus_path[0],
    &mpl3115a2_path[0]
  );
  if(rv == 0)
    CFE_EVS_SendEvent(ALTITUDE_APP_DEV_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Device registered correctly at %s",
                        mpl3115a2_path);

  fd = open(&mpl3115a2_path[0], O_RDWR);
  if(fd >= 0)
  CFE_EVS_SendEvent(ALTITUDE_APP_DEV_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Device opened correctly at %s",
                    mpl3115a2_path);

  // Device configuration
  rv = sensor_mpl3115a2_begin(fd);
  if(rv != 0){
    CFE_EVS_SendEvent(ALTITUDE_APP_DEV_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Could not find sensor. Check wiring.");
  }else{
    CFE_EVS_SendEvent(ALTITUDE_APP_DEV_INF_EID, CFE_EVS_EventType_INFORMATION, "ALTITUDE: Device configured correctly");
  }

  close(fd);

  return CFE_SUCCESS;
}
