#include "level2.h"

void checkForFrameAndReact(
 void handleFrame(
   uint16_t length,
   unsigned char type,
   unsigned char subType,
   unsigned char * information,
   void * passedCallerParameter
   )
 ,FrameStatus frameStatus

 ,GioEndpoint *gioEndpoint
 ,void * callerParameter
 )
{
/*
 We check if there are bytes waiting for us in our serial buffer.  If there are, we read that byte.  If we are not confused, this byte should be a START_FLAG(at which point we read in the given frame).  If we ARE confused, we ignore the byte and continue eating untill a real START_FLAG does come along.
 */
 unsigned char byte;
 #ifdef BRLTTY
 //logMessage(LOG_DEBUG,"Checking serial.");
 #endif
 if(!serialAvailable(gioEndpoint))return;
 #ifdef BRLTTY
 logMessage(LOG_DEBUG,"Received serial.");
 #endif
 #ifdef ARDUINO
  delay(1);
  /*
  This is why we cannot have nice thigs!
  Delay needed if you don't want the arduino to freeze indefinitely for no reason.
  */
 #endif
 if(frameStatus==START_OF_FRAME){
  if(!serialRead(&byte,gioEndpoint))return;
  #ifdef BRLTTY
  logMessage(LOG_DEBUG,"Byte read: %d",byte);
  #endif
 }else{
  byte=START_FLAG;
 }

 if(byte!=START_FLAG){
  #ifdef BRLTTY
  logMessage(LOG_WARNING,"Unexpected byte that was not a frame start flag. The byte was:%d",byte);
  #endif
  return;
 }
 #ifdef BRLTTY
 logMessage(LOG_DEBUG,"Reading packet...");
 #endif
 unsigned char xorChecksum=0;
 uint16_t length;
 unsigned char type;
 unsigned char subType;
 unsigned char byteInMouth;
 unsigned char indexInInformationBuffer=0;
 ReadEscapedByteExitStatus status;
 #define readAnEscapedByte(pByte) status=readEscapedByte(gioEndpoint,pByte);if(!continueReading(status,handleFrame,gioEndpoint,callerParameter))return
 /*Now we read the frames header.*/
 readAnEscapedByte(&byteInMouth);
 xorChecksum^=byteInMouth;
 length=(uint16_t)byteInMouth<<8;
 readAnEscapedByte(&byteInMouth);
 xorChecksum^=byteInMouth;
 length+=byteInMouth;
 #ifdef BRLTTY
 //logMessage(LOG_DEBUG,"length: %d",length);
 #endif
 unsigned char information[length+CHECKSUM_SIZE];

 readAnEscapedByte(&type);
 xorChecksum^=type;
 #ifdef BRLTTY
 //logMessage(LOG_DEBUG,"type: %d",type);
 #endif
 readAnEscapedByte(&subType);
 xorChecksum^=subType;
 #ifdef BRLTTY
 //logMessage(LOG_DEBUG,"subType: %d",subType);
 #endif
 #ifdef BRLTTY
 //logMessage(LOG_DEBUG,"Reading INFO section...");
 #endif
 /*Now we read our information buffer*/
 while(indexInInformationBuffer<length){
  readAnEscapedByte(&byteInMouth);
  information[indexInInformationBuffer++]=byteInMouth;
  xorChecksum^=byteInMouth;
 }
 /*Read the checksum.*/
 readAnEscapedByte(&byteInMouth);
 xorChecksum^=byteInMouth;
 /*If our checksum failed, we DROP the frame.*/
 if(xorChecksum!=0){
  #ifdef BRLTTY
  logMessage(LOG_WARNING,"Checksum failed. Packet dropped.");
  #endif
  return;
 }
 /*If our frame was not closed off by an END_FLAG we DROP the frame*/
 if(!nextChar(&byteInMouth,gioEndpoint))return;
 if(byteInMouth!=END_FLAG)return;
 /*If everything was successfull, then we call the handling code to deal with this new bit of information.  We pass this handling code the length, the type, and the subType. AND WE DO NOT FORGET, THAT THE INFORMATION BUFFER IS CONSTRAINED BY THE LENGTH AND NOT ANY KIND OF NULL TERMINATOR*/
 #ifdef BRLTTY
 //logMessage(LOG_DEBUG,"Packet read...");
 #endif
 (handleFrame)(length,type,subType,information,callerParameter);
}

unsigned char continueReading(
 ReadEscapedByteExitStatus status
 ,void handleFrame(
   uint16_t length,
   unsigned char type,
   unsigned char subType,
   unsigned char * information,
   void * passedCallerParameter
   )
 ,GioEndpoint *gioEndpoint
 ,void * callerParameter){
 switch(status){
  case READ_FAILED_UNEXPECTED_START_FLAG:
   checkForFrameAndReact(
    handleFrame,
    CONTINUATION_OF_FRAME,
    gioEndpoint,
    callerParameter);
   return 0;
  case READ_FAILED_TIMEOUT:
   #ifdef BRLTTY
   logMessage(LOG_WARNING,"Timeout. Packet dropped.");
   #endif
   return 0;
  case READ_FAILED_UNEXPECTED_END_FLAG:
   return 0;
  case READ_SUCCESS:
   return 1;
 }
}

void sendFrame(
 uint16_t        length  ,
 unsigned char   type    ,
 unsigned char   subType ,
 unsigned char * information,
 GioEndpoint *gioEndpoint){
 unsigned char byteInArse;
 byteInArse=START_FLAG;
 #ifdef BRLTTY
  logMessage(LOG_DEBUG,"Sending start flag.");
 #endif
 serialWrite(gioEndpoint, byteInArse);
 #ifdef BRLTTY
  logMessage(LOG_DEBUG,"Start flag sent.");
 #endif

 unsigned char xorChecksum=0;
 byteInArse=(length&0xFF00ul)>>8;
 xorChecksum^=byteInArse;
 writeEscapedByte(byteInArse,gioEndpoint);

 byteInArse=length&0x00FFul;
 xorChecksum^=byteInArse;
 writeEscapedByte(byteInArse,gioEndpoint);

 xorChecksum^=type;
 writeEscapedByte(type,gioEndpoint);

 xorChecksum^=subType;
 writeEscapedByte(subType,gioEndpoint);

 unsigned char indexInInformationBuffer=0;
 while(indexInInformationBuffer<length){
  xorChecksum^=information[indexInInformationBuffer];
  writeEscapedByte(information[indexInInformationBuffer++],gioEndpoint);
 }
 writeEscapedByte(xorChecksum,gioEndpoint);
 byteInArse=END_FLAG;
 serialWrite(gioEndpoint, byteInArse);
 #ifdef ARDUINO
 Serial.flush();
 #endif
 #ifdef BRLTTY
  logMessage(LOG_DEBUG,"Packet sent.");
 #endif
}

void writeEscapedByte(
 unsigned char byte,
 GioEndpoint *gioEndpoint){
 unsigned char escapeChar = ESCAPE_CHAR;
 switch(byte){
  case START_FLAG:
  case END_FLAG:
  case ESCAPE_CHAR:
    serialWrite(gioEndpoint, escapeChar);
  default:
   break;
 }
 serialWrite(gioEndpoint, byte);
}

ReadEscapedByteExitStatus readEscapedByte(
 GioEndpoint *gioEndpoint,
 unsigned char * byte){
 if(!nextChar(byte,gioEndpoint))return READ_FAILED_TIMEOUT;
 switch(*byte){
  case ESCAPE_CHAR:
   if(!nextChar(byte,gioEndpoint))return READ_FAILED_TIMEOUT;
   return READ_SUCCESS;
  case START_FLAG:
   #ifdef BRLTTY
   logMessage(LOG_WARNING,"Unexpected start flag in frame. Packet dropped.");
   #endif
   return READ_FAILED_UNEXPECTED_START_FLAG;
  case END_FLAG:
   #ifdef BRLTTY
   logMessage(LOG_WARNING,"Unexpected end flag in frame. Packet dropped.");
   #endif
   return READ_FAILED_UNEXPECTED_END_FLAG;
 }return READ_SUCCESS;
}

//Read one byte from serial
unsigned char nextChar
 (unsigned char * byte
 ,GioEndpoint *gioEndpoint){
 unsigned char get = 1;
 #ifdef ARDUINO
 unsigned char timeoutCounter = 0;
 while(1){
  if(serialAvailable(gioEndpoint))break;
  if(timeoutCounter++>TIMEOUT_AFTER){
   get = 0;
   break;
  }
  delay(1);
 }
 #endif
 if(get){
  serialRead(byte,gioEndpoint);
  return get;
 }else{
  return 0;
 }
}

unsigned char serialAvailable(
 GioEndpoint *gioEndpoint){
 #ifdef ARDUINO
 return Serial.available();
 #endif
 #ifdef BRLTTY
 return gioAwaitInput(gioEndpoint,1);
 #endif
}

unsigned char serialRead
 (unsigned char * byte
 ,GioEndpoint * gioEndpoint){
 #ifdef BRLTTY
 gioReadByte(gioEndpoint, byte,1);
 return 1;
 #endif
 #ifdef ARDUINO
 unsigned char v;
 //displayAChar(255);
 if(Serial.available()){
  *byte=Serial.read();
  v=1;
 }else{
  v=0;
 }
 //displayAChar(0);
 return v;
 #endif
}

void serialWrite(
 GioEndpoint *gioEndpoint,
 unsigned char byte){
 #ifdef BRLTTY
 gioWriteData(gioEndpoint, &byte, 1);
 #endif
 #ifdef ARDUINO
 Serial.write(byte);
 #endif
}
