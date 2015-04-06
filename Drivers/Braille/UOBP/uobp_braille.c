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

#include "uobp_braille.h"

//////////////////////////////////////////////////
///Frame Handlers/////////////////////////////////
//////////////////////////////////////////////////
FrameHandler frameHandlers
              [NUM_FRAME_TYPES]
              [MAX_NUM_FRAME_SUBTYPES]
              [MAX_NUM_HANDLERS];

//////////////////////////////////////////////////
///Capabilities///////////////////////////////////
//////////////////////////////////////////////////
Capability * capabilities[NUM_CAPABILITIES];

//////////////////////////////////////////////////
///Capability Nodes///////////////////////////////
//////////////////////////////////////////////////
CapabilityState * capabilityStates
 [NUM_CAPABILITIES]
 [MAX_NUM_NODES];

//////////////////////////////////////////////////
//Serial//////////////////////////////////////////
//////////////////////////////////////////////////
static GioEndpoint *gioEndpoint;

//////////////////////////////////////////////////
//Text cells//////////////////////////////////////
//////////////////////////////////////////////////
wchar_t * textCells=NULL;

//////////////////////////////////////////////////
//Braille cells///////////////////////////////////
//////////////////////////////////////////////////
unsigned char * thisBrailleBuffer=NULL;
unsigned int currentBrailleRows=0;
unsigned int currentBrailleColumns=0;

//////////////////////////////////////////////////
//BRLTTY FUNCTIONS////////////////////////////////
//////////////////////////////////////////////////
static int
brl_construct (BrailleDisplay *brl,
               char **parameters,
               const char *device){
  logMessage(LOG_DEBUG,"Initializing serial with device %s.", device);
  if(!Serial_init(device,&gioEndpoint))return 0;
  logMessage(LOG_DEBUG,"Serial initialized.");

  initializeCapabilityInitializersArray(capabilities);
  /*Set capabilityStates to NULL.
  Note that this is not the same as freeCapabilityNodeStates.
  Free capability node states inspects values that are not null
  and free's malloced arrays within the CapabilityState structs.
  We do not want to be freeing random pointers!*/
  uint16_t i,j;
  for(i=0;i<NUM_CAPABILITIES;i++)
   for(j=0;j<MAX_NUM_NODES;j++)
    capabilityStates[i][j]=NULL;

  initializeFrameHandlers(frameHandlers);

  /*
  Send an initialization frame to the device.
  */
  unsigned char information[4];
  /*Host driver id (brltty=0)*/
  information[0]=0;
  information[1]=0;
  /*Host driver version.*/
  information[2]=0;
  information[3]=0;
  sendFrame(4,0,0,information,gioEndpoint);
  logMessage(LOG_DEBUG,"Initialization packet sent.");
  /*
  Wait for an initialization frame from the device.
  */
  unsigned char initializationStatus=0;
  while(!initializationStatus){
   checkForFrameAndReactBrltty(brl,&initializationStatus);
  }
  return 1;
}

static void
brl_destruct (BrailleDisplay *brl) {
 logMessage(LOG_DEBUG,"Destructed.");
}

static int
brl_writeWindow (BrailleDisplay *brl,
                 const wchar_t *text) {
 free(textCells);
 textCells=wcsdup(text);

 if(  currentBrailleRows    != brl->textRows
   || currentBrailleColumns != brl->textColumns
   || !thisBrailleBuffer){
  logMessage
   (LOG_DEBUG
   ,"Resizing window from %dx%d to %dx%d"
   ,currentBrailleRows
   ,currentBrailleColumns
   ,brl->textRows
   ,brl->textColumns);
  free(thisBrailleBuffer);
  thisBrailleBuffer=
   (unsigned char *)malloc(
    sizeof(unsigned char)
    *brl->textRows
    *brl->textColumns);
  currentBrailleRows = brl->textRows;
  currentBrailleColumns = brl->textColumns;
 }
 //int * from,to;
 cellsHaveChanged
  (thisBrailleBuffer
  ,brl->buffer
  ,brl->textRows*brl->textColumns
  ,NULL
  ,NULL
  ,NULL);

 //callHandler(onCellsChanged,NULL);
 return 1;
}

#ifdef BRL_HAVE_KEY_CODES
static int
brl_readKey (BrailleDisplay *brl) {
  logMessage(LOG_DEBUG,"Read key.");
  return EOF;
}

static int
brl_keyToCommand (BrailleDisplay *brl,
                  KeyTableCommandContext context,
                  int key) {
  logMessage(LOG_DEBUG,"Key converted to command.");
  return EOF;
}
#endif /* BRL_HAVE_KEY_CODES */

checkForFrameAndReactBrltty
 (BrailleDisplay * brl
 ,unsigned char  * initializationStatus){
  FrameInfo frameInfo = {
   .brl=brl,
   .gioEndpoint = gioEndpoint,
   .info = NULL,
   .length = 0,
   .text = textCells,
   .brailleBuffer = thisBrailleBuffer,
   .capabilities = capabilities,
   .capabilityStates = capabilityStates,
   .initializationStatus = initializationStatus,
   .frameHandlers = frameHandlers};
  checkForFrameAndReact(
   &handleFrame
   ,START_OF_FRAME
   ,gioEndpoint
   ,(void*)&frameInfo);
}

static int
brl_readCommand
 (BrailleDisplay *brl
 ,KeyTableCommandContext context) {
 checkForFrameAndReactBrltty(brl,NULL);
 return EOF;
}

