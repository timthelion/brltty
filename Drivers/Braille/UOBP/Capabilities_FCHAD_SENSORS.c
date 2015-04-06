/*
 * BRLTTY - A background process providing access to the console screen (when in
 *          text mode) for a blind person using a refreshable braille display.
 *
 * Copyright (C) 2012 by Timothy Hobbs and
 * Copyright (C) 1995-2011 by The BRLTTY Developers.
 *
 * BRLTTY and the FCHAD software comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version. Please see the file LICENSE-GPL for details.
 *
 * Web Page: http://mielke.cc/brltty/ and  http://brmlab.cz/user/timthelion
 *
 * This software is maintained by Timothy Hobbs <timothyhobbs@seznam.cz>.
 */
#include "Capabilities_FCHAD_SENSORS.h"
/////////////////////////////////////////////////
///FCHAD Sensors/////////////////////////////////
/////////////////////////////////////////////////
void * initFCHADSensorsState(FrameInfo frameInfo){
  unsigned char * information = frameInfo.info;
  BrailleDisplay * brl = frameInfo.brl;
  FCHADSensorsState * thisState =
    (FCHADSensorsState *)
     malloc(sizeof(FCHADSensorsState));
  thisState->rows =
    (uint16_t)information[0]<<8;
  thisState->rows +=
    (uint16_t)information[1];
  thisState->cols =
    (uint16_t)information[2]<<8;
  thisState->cols +=
    (uint16_t)information[3];
  thisState->sensors =
    (unsigned char *)malloc(thisState->rows*thisState->cols*sizeof(unsigned char));
  int i,j;
  for(i=0;i<thisState->rows;i++)
   for(j=0;j<thisState->cols;j++)
    thisState->sensors[i*thisState->rows+j]=0;

  /* Despite the fact that the UOBP standard supports multiple nodes of a single capability,
  BRLTTY doesn't really support having multiple braille buffers.
  It is not meaningfull to add such support at this time.
  The case in which a device would have multiple nodes
  (say we could plug in a larger 2D sensor grid)
  is far away in the future.
  Here we just use the largest node as the "primary" node
  and let the nodes with smaller number of sensors be subsets of the "primary".*/
  if(thisState->rows>brl->textRows){
   brl->textRows = thisState->rows;
   brl->resizeRequired  = 1;
  }
  if(thisState->cols>brl->textColumns){
   brl->textColumns = thisState->cols;
   brl->resizeRequired  = 1;
  }
  logMessage
   (LOG_DEBUG
   ,"Size of new node %dx%d"
   ,brl->textRows
   ,brl->textColumns);
   //,thisState->rows
   //,thisState->cols);
  thisState->prevRow=0;
  thisState->prevCol=0;
  return thisState;
}

void freeFCHADSensorsState
 (FCHADSensorsState * thisSensorState){
  free(thisSensorState->sensors);
  free(thisSensorState);
}

void reactToSensorUp(FrameInfo * frameInfo){
  reactToSensorAction(frameInfo,SENSOR_UP);
}

void reactToSensorDown(FrameInfo * frameInfo){
  reactToSensorAction(frameInfo,SENSOR_DOWN);
}

void reactToSensorAction(FrameInfo * frameInfo,
                         unsigned char action){
  unsigned char * information = frameInfo->info;
  unsigned char node = information[0];
  if(!frameInfo->capabilityStates[4][node]){
   logMessage(LOG_WARNING,"Received frame event for uninitialized node %d of capability 4.",node);
   return;
  }
  CapabilityState * thisCapabilityNode =
   frameInfo->capabilityStates[4][node];

  FCHADSensorsState * myState =
    (FCHADSensorsState*)thisCapabilityNode->state;
  uint16_t row =
    (uint16_t)information[1]<<8;
  row +=
    (uint16_t)information[2];
  uint16_t col =
    (uint16_t)information[3]<<8;
  col +=
    (uint16_t)information[4];
  /*If the touch was inside the buffer
  (It will be outside only in the case of a coruption somewhere.*/
  if(row < myState->rows
           &&
     col < myState->cols){
   myState->sensors[row*myState->rows+col]=action;
   updateFCHADFromSensorValues
    (thisCapabilityNode
    ,node
    ,frameInfo);
  }else logMessage(LOG_ERR,"Sensor action %d outside buffer row %d col %d of cols %d",action,row,col,myState->cols);
}

void updateFCHADFromSensorValues
 (CapabilityState * thisCapabilityNode
 ,unsigned char node
 ,FrameInfo * frameInfo){
  /* We want to always display the character marked by the top left corner
  (or top right in the case of a left handed user)
  of the area that is currently being touched.
  So first we figure out what the top left corner being touched is,
  and if it is different than the previous top left corner,
  we send a new signal to the device
  to tell it to display that new character.
  If nothing is being touched,
  than we send a signal telling the device to switch it's FCHAD Cell off.*/
 if(thisCapabilityNode && frameInfo->brailleBuffer){
  FCHADSensorsState * myState =
    (FCHADSensorsState*)thisCapabilityNode->state;
  uint16_t portamento = thisCapabilityNode->settings[1].persistantValue;
  uint16_t i,j;
  j=0;/*Due to a warning...*/
  unsigned char found=0;
  /*For each FCHAD_CELL node that is paired with this array of sensors,
  display the proper character to that node.*/
  #define FCHAD_CELL_CAPABILITY_ID 3
  unsigned char pair;
  for(pair=0;pair<thisCapabilityNode->numPairs;pair++)
  if(thisCapabilityNode->pairs[pair].capability
                        ==
                FCHAD_CELL_CAPABILITY_ID){
   FCHADCellState * pairState =
    (FCHADCellState *)frameInfo->capabilityStates
      [FCHAD_CELL_CAPABILITY_ID]
      [node]->state;
   if(pairState->cellHandedness==RIGHT_HANDED){
    for(i=0;i<myState->rows&&!found;i++)
     for(j=0;j<myState->cols&&!found;j++)
       if(myState->sensors[i*myState->rows+j])found=1;
    j--;i--;
   }else{/*LEFT_HANDED*/
    for(i=0;i<myState->rows&&!found;i++)
     for(j=myState->cols;j>-1&&!found;j--)
       if(myState->sensors[i*myState->rows+j])found=1;
    j++;i--;
   }
   unsigned char charToDisplay=0;
   #ifdef LOG_EVERYTHING
   wchar_t charVisual = L' ';
   #endif
   if(found){
     if(portamento){
      //If we are moving forward:
      if (  myState->prevRow <= i
         && myState->prevCol <= j){
       uint16_t pi;
       uint16_t pj;
       for(pi = myState->prevRow;pi<=i;pi++)
        for(pj = myState->prevCol;pj<=j;pj++){
         charToDisplay=frameInfo->brailleBuffer[pi*myState->rows+pj];
         displayChar(frameInfo,pair,charToDisplay,0);
        }
      }else{
       charToDisplay=frameInfo->brailleBuffer[i*myState->rows+j];
       displayChar(frameInfo,pair,charToDisplay,0);
      }
      myState->prevRow = i;
      myState->prevCol = j;
     }else{
      charToDisplay=frameInfo->brailleBuffer[i*myState->rows+j];
      displayChar(frameInfo,pair,charToDisplay,0);
     }
     #ifdef LOG_EVERYTHING
     if(frameInfo->text)
      charVisual=frameInfo->text[i*myState->rows+j];
     else
      logMessage(LOG_WARNING,"Text buffer not initialized.");
     #endif
   } else {
      displayChar(frameInfo,pair,charToDisplay,0);
   }
  #ifdef LOG_EVERYTHING
  char * message;
  asprintf
   (&message
   ,"%d %d %d %d %d %d %d %d %d %d %d %d %d '%lc'"
   ,myState->sensors[ 0]
   ,myState->sensors[ 1]
   ,myState->sensors[ 2]
   ,myState->sensors[ 3]
   ,myState->sensors[ 4]
   ,myState->sensors[ 5]
   ,myState->sensors[ 6]
   ,myState->sensors[ 7]
   ,myState->sensors[ 8]
   ,myState->sensors[ 9]
   ,myState->sensors[10]
   ,myState->sensors[11]
   ,j
   ,charVisual);
  logMessageDateTime(message);
  free(message);
  #endif

  }
 }else if(!thisCapabilityNode){
  logMessage(LOG_ERR,"Capability node not properly initialized!");
 }else if(!frameInfo->brailleBuffer){
  logMessage(LOG_ERR,"Braille buffer not properly initialized!");
 }
}

//////////////////////////////////////////////////
///Sensor logging/////////////////////////////////
//////////////////////////////////////////////////
#ifdef LOG_EVERYTHING
void logMessageDateTime(char * message){
 struct tm *current;
 time_t now;
 time(&now);
 current = localtime(&now);
 struct timeb tp;
 ftime(&tp);

 logMessage
   (LOG_ERR
   ,"#EVENT_LOG# %d%d%d %d.%d %s"
   ,current->tm_year+1900/*Sigh*/
   ,current->tm_mon+1/*Not even worth a sigh*/
   ,current->tm_mday
   ,tp.time
   ,tp.millitm
   ,message);
}
#endif
