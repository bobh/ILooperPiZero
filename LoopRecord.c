//  Created by Robert Hinrichs on 1/11/16.
//  Copyright (c) 2016 wm6h. All rights reserved.

/*
 * $Id$
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/** this application is based on @file paex_record.c
 @ingroup examples_src
 @brief Record input into an array; Save array to a file;Playback recorded data.
 @author Phil Burk  http://www.softsynth.com
 */


/*
 This program uses the WiringPi Library http://wiringpi.com/
 WiringPi is released under the GNU LGPLv3 license
 * wiringPi:
 *	Arduino compatable (ish) Wiring library for the Raspberry Pi
 *	Copyright (c) 2012 Gordon Henderson
 
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *

 The Advanced Linux Sound Architecture (ALSA) comes with a kernel API and a 
 library API. 
 
 
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "portaudio.h"

//wiringPi allows access to GPIO pins
//for test and keyline
#include <wiringPi.h>

#include "commonTypes.h"

#include "ilooper_utils.h"
#include "ilooper_audio.h"

// only one channel will be recorded
SAMPLE         silenceBufferValue = SAMPLE_SILENCE;
BOOL           all_complete = FALSE;
SAMPLE*        playbackPtr = 0;
BOOL           greenLED = OFF;
BOOL           buttonPress = FALSE;
BOOL           buttonHold = FALSE;
volatile int   buttonValue = 1;
BOOL           terminate = FALSE;

int            ringbufferWPTR  = 0;
int            ringbufferRPTR  = 0;
int            playbackDirection = FWD;

int            OK_ToBlink = 1;
int            t_measure = 0;

states_t currentSTATE = S_INIT;
ringBufferRAM_t RingBufferConfig; // only one instance

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
PaStreamParameters  inputParameters, outputParameters;
PaStream*           stream;
PaError             err = paNoError;
paTestData          data;
int                 i;
int                 totalFrames;
int                 numSamples;
int                 numBytes;
SAMPLE              max, val;
double              average;
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void toggleGreenLED()
{
	if(greenLED == ON)
	{
		greenLED = OFF;
		digitalWrite (LED_PIN,  OFF) ;
	}
	else
	{
		greenLED = ON;
		digitalWrite (LED_PIN,  ON) ;
	}
}

void fatalError()
{
 fflush(stdout);
 while(1)
 {
  delay(30);
  toggleGreenLED();
 }
}


void zeroMemory(SAMPLE* sptr)
{
    int i;
	for(i=0; i<RING_BUFFER_LEN; i++ ) 
		*(sptr+i) = SAMPLE_SILENCE;
	
}

// button check when callbacks aren't running
// return 0 for no button press
//  1  for button press 
// -1  for button hold
// the no press state is 1 through a pull-up
int checkButton(void)
{
  int tempi;
  tempi = digitalRead(BUTTON_PIN);
  if(tempi == 1) // no press
   return 0;
  Pa_Sleep(50);
  if( (digitalRead(BUTTON_PIN) == 0) && (tempi == 0))
  { // if button was pressed and is still pressed, return -1
    Pa_Sleep(2000);
    if(digitalRead(BUTTON_PIN) == 0)
    {
     return -1;
    }
    else
    {
      return 1;
    }
  }
  else // it was a short press
  {
    return 1;
  }

}


// *************************************************************************
// command --  current/next state                                          
// sptr    --  optional memory for current state    
// LED blinking @ 1 sec -- recording into the ring buffer
//                         contents are overwritten
// LED on -- playback or rewind playback
// LED off -- stop state waiting for a button press or hold
//            contents preserved
// LED fast blink -- fatal error
                                
void doState(states_t command, SAMPLE* sptr)
{
    int tempi;
	
	switch(command) 
	{ 
			// ring buffer contents are initialized
			// next state determined by caller
		case S_INIT:
			currentSTATE = S_INIT;
			zeroMemory(sptr);
			break;
			
			// filling the ring buffer with samples
			// next state determined by caller
		case S_COLLECTING:
			currentSTATE = S_COLLECTING;
			//printf("\n=== Now recording!!  ===\n"); 
			//fflush(stdout);
			
			break;
			
			// simple playback of last NUM_PLAYBACK_SECONDS seconds
		case S_REWIND_10:
			currentSTATE = S_REWIND_10;
			greenLED = ON;
		    digitalWrite (LED_PIN,  ON) ;
		    
            // tell playback callback how many frames to play
			data.maxFrameIndex = totalFrames = 
			                                 NUM_PLAYBACK_SECONDS * SAMPLE_RATE;
			// reset playback count
			data.frameIndex = 0;
			
			// playback NUM_PLAYBACK_SECONDS
			// adjust pointers with mod RING_BUFFER_LEN arithmetic
			tempi = ringbufferWPTR - (data.maxFrameIndex * NUM_CHANNELS);
			if(tempi < 0)
				tempi += RING_BUFFER_LEN;
			
			ringbufferRPTR = tempi;
			
			playbackDirection = FWD;
			
			buttonPress = FALSE;
			buttonHold  = FALSE;
			
			// Playback recorded data.  ----------------------------------
			// advance state to playback
			currentSTATE = S_PLAYBACK;
			break;
			
			// rewind playback while button is held
			// playback from top of buffer but at faster sample rate
			// when button is release, advance state and playback at 
			// normal sample rate from where we released button
			// the FWD/REV i/o pin tells to go from the top of the buffer
			// in the REV speech direction or all the way from the start of the 
			// buffer (5 min back in time) in the FWD speech direction 
		case S_REWIND_PLAYBACK:
			currentSTATE = S_REWIND_PLAYBACK;
			
			//+++++++++++++++++++++++++++++++++++++++++++++++++++++
			greenLED = ON;
		    digitalWrite (LED_PIN,  ON) ;
		    
            // tell playback how many frames to play
            // say all of them but we will use the button
            // unpress to stop before this
			data.maxFrameIndex = totalFrames = 
			                                 NUM_SECONDS * SAMPLE_RATE - 4;
                                 
			// reset playback count
			data.frameIndex = 0;
			
			buttonPress = FALSE;
			buttonHold  = FALSE;
			
			if(digitalRead(DIRECTION_PIN) == 1)
            {
              // playback fast and in reverse from current write pointer			
			  playbackDirection = REV;
			  // start at buffer head
			  ringbufferRPTR = ringbufferWPTR;
			  // fast playback 4X speed
		      playback(16000); // stays here until rewind playback finished
		      // will return here when button is released
            }
            else
            {
            // playback fast and in forward direction 
            // from buffer start		
			playbackDirection = FWD;
			// all the way back 5 minutes in time
			tempi = 
			ringbufferWPTR - ( NUM_SECONDS * SAMPLE_RATE * NUM_CHANNELS - 4);
			if(tempi < 0)
				tempi += RING_BUFFER_LEN;
			
			ringbufferRPTR = tempi;
			
			// fast playback 2X speed
		    playback(32000); // stays here until rewind playback finished
		    // will return here when button is released

            }
									
			//+++++++++++++++++++++++++++++++++++++++++++++++++++++			
		    		    		    
		    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	
		    // we are finished rewinding playback	  
		    // now do regular forward playback from read pointer which has 
		    // been going in the reverse direction
		    // (or forward direction depending on io pin)
		  
		    // how far did we go back?
			// we started at buffer head
			// ringbufferRPTR = ringbufferWPTR;
            // we held the button this many samples
            tempi = ringbufferWPTR - ringbufferRPTR;
			if(tempi < 0)
				tempi += RING_BUFFER_LEN;

            
            // tell playback how many frames to play
            // pointers are in samples
            // maxFrameIndex is in frames--duh!
			data.maxFrameIndex = totalFrames = tempi / NUM_CHANNELS;            		    
		    
			// reset playback count
			data.frameIndex = 0;
			
			// reset RPTR
			// playback NUM_PLAYBACK_SECONDS
			tempi = ringbufferWPTR - (data.maxFrameIndex * NUM_CHANNELS);
			if(tempi < 0)
				tempi += RING_BUFFER_LEN;
			
			ringbufferRPTR = tempi;
						
			playbackDirection = FWD;
			
			buttonPress = FALSE;
			buttonHold  = FALSE;
			
			// Playback recorded data.  ----------------------------------
			// advance state to playback
			currentSTATE = S_PLAYBACK;
		  

			break;
			
			// playback from top of buffer at normal sample rate
			// data.maxFrameIndex should be set
		case S_PLAYBACK:
			currentSTATE = S_PLAYBACK;
		    // playback from the top of the buffer
		    // will only run interrupts until playback finished
		    // state machine will not run until returns
		    playback(SAMPLE_RATE); // stays here until playback finished

			greenLED = OFF;
		    digitalWrite (LED_PIN,  OFF) ;

		    // will return here when all samples have been played
		    currentSTATE = S_STOP;
			break;
			
			// all stop--pointer rudder is amidship
			// ring buffer contents are preserved
			// to exit this state, power down.
			// todo(maybe?): to exit this state, five button presses?
			// how would I remember that? Add more buttons? Labels? ...
		case S_STOP:
			currentSTATE = S_STOP;
			
            // leave the S_STOP state (LED OFF) when button is pressed once 
            // (10 second rewind) or held pressed (rewind playback FWD or REV 
            // 4X/2X speed)
            // Not implemented-- re-enter S_COLLECTING and overwrite samples
            // with 5 button presses (?) or something... 
            // like another hardware switch
            			  
            if(digitalRead(REBOOT_PIN) == 0)
              {
                system("/home/pi/reboot.py");
              }

            
			tempi = checkButton();
			if(tempi == OFF)
			 break;

			if(tempi == 1)
			{
			   //printf("button press\n");
			   //fflush(stdout);
			   currentSTATE = S_REWIND_10;			   
			   break;
			 }
			 else
			 { // checkButton returned minus
			   //printf("button hold\n");
			   //fflush(stdout);
			   currentSTATE = S_REWIND_PLAYBACK;

			   //terminate = TRUE;
			   //printf("terminate\n");
			   //fflush(stdout);
			   break;
			}
			break;
			
		default:
			break;
	}
	
}

// *************************************************************************
int main(void);
int main(void)
{	    
	//printf("InfiniteLooper--based on patest_record.c\n"); //fflush(stdout);
	//Record for a few seconds. 
	data.maxFrameIndex = totalFrames = NUM_SECONDS * SAMPLE_RATE; 
	data.frameIndex = 0;
	numSamples = totalFrames * NUM_CHANNELS;
	numBytes = numSamples * sizeof(SAMPLE);
	// From now on, recordedSamples is initialised.
	// 5 minutes of record buffer allocated 5% of free
	// on rPi zero. could record 90 minutes on rPi 0  
	//++++++++++++++++++++++++++++++++++++++++++++++++++++
	data.recordedSamples = (SAMPLE*) malloc( numBytes ); 
	//++++++++++++++++++++++++++++++++++++++++++++++++++++
	if( data.recordedSamples == NULL )
	{
		//printf("Could not allocate record array.\n");
		goto done;
	}
	else
	{
		//printf("allocated %d bytes\n", numBytes );
	}
		
	ringbufferWPTR = 0;
	ringbufferRPTR = 0;
    all_complete = FALSE;
    // ******************************************
    doState(S_INIT, data.recordedSamples);	
    // ******************************************
		
	// we do the recording set-up just once and cycle power
	// to do it again. This initialization should be moved to
	// ilooper_audio if multiple recordings are implemented
	
	err = Pa_Initialize();
	if( err != paNoError ) goto done;
	// default input device 
	inputParameters.device = Pa_GetDefaultInputDevice(); 
	if (inputParameters.device == paNoDevice) {
		fprintf(stderr,"Error: No default input device.\n");
		goto done;
	}
	inputParameters.channelCount = 2;                    //stereo input 
	inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	inputParameters.suggestedLatency = 
	Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL;
	
	//Record some audio. -------------------------------------------- 
	err = Pa_OpenStream
	(
	 &stream,
	 &inputParameters,
	 NULL,                  // &outputParameters, 
	 SAMPLE_RATE,           // ADC sample rate is a constant
	 FRAMES_PER_BUFFER,
	 //we won't output out of range samples so 
	 //don't bother clipping them 
	 //paClipOff, 
	 // ***wm6h for this app, let's clip
	 paNoFlag,
	 recordCallback,
	 &data 
	 );
	
	if( err != paNoError ) goto done;
	
	// the output sample rate is checked in playback()
	err = Pa_IsFormatSupported( &inputParameters, NULL, SAMPLE_RATE );
	if( err == paFormatIsSupported )
	{
		//printf("\n ADC samplerate is supported %d, FPB %d\n", \
		//         SAMPLE_RATE, FRAMES_PER_BUFFER);
	}
	else
	{
		//printf("\n ADC samplerate is NOT supported\n");
		goto done;
	}
	
	//printf(" %s\n", Pa_GetVersionText());	
	
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	
	//GPIO-0 access for Green LED and test
    //rPi J8-11 and gnd J8-6
    if(wiringPiSetup () < 0 )
    {
		fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
		return 1;
    }
    pinMode (LED_PIN, OUTPUT) ;
    digitalWrite (LED_PIN,  OFF) ;
    pinMode (BUTTON_PIN, INPUT) ;
    //pullUpDnControl(BUTTON_PIN, PUD_OFF);
    pullUpDnControl(BUTTON_PIN, PUD_UP);
    pinMode (DIRECTION_PIN, INPUT) ;
    pullUpDnControl(DIRECTION_PIN, PUD_UP); 
    pinMode (REBOOT_PIN, INPUT) ;
    pullUpDnControl(REBOOT_PIN, PUD_UP);
    
    
    pinMode (ERROR_PIN, OUTPUT) ;
    digitalWrite (ERROR_PIN, OFF) ;
   
    sync();
   
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	
	
	// underway--shift ship's colors--all ahead 8K sps
	// 
	err = Pa_StartStream( stream );
	if( err != paNoError ) goto done;

// =============================================================================	
// =============================================================================	
// collecting samples phase
    // ******************************************
    doState(S_COLLECTING, data.recordedSamples);	
    // ******************************************
	
	// we are running the callbacks
	while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
	{
		Pa_Sleep(500);
						
		if(buttonPress == TRUE)
		{
			Pa_Sleep(100);
			all_complete = TRUE; //stop the sample interrupt, exit while loop
		}
		else
		{	
		    // no button press
		    // give the state machine a chance to run	 
        // ******************************************
			doState(currentSTATE, (void*) NULL);	
	    // ******************************************		
		}
	}
// =============================================================================	
// =============================================================================	
	
	if( err < 0 ) goto done;
	// all stop
	err = Pa_CloseStream( stream );
	if( err != paNoError ) goto done;
	
	buttonValue = digitalRead(BUTTON_PIN);
	//printf("button = %d\n", buttonValue); 
	
	if(buttonValue == 0)
	{ 
		//if button still pressed, it's a hold
		buttonHold = TRUE;        
		//printf("button hold\n"); //fflush(stdout);
	}
	else
	{  //else, it's a button press
		buttonPress = FALSE; //acknowledged, reset flag
		//printf("button pressed\n"); //fflush(stdout);					 			
	}
	
	
	// Measure maximum peak amplitude. 
	// we could indicate over-driving/clipping the audio
	// or AGC it
	max = 0;
	average = 0.0;
	for( i=0; i<numSamples; i++ )
	{
		val = data.recordedSamples[i];
		if( val < 0 ) val = -val; // ABS 
		if( val > max )
		{
			max = val;
		}
		average += val;
	}
	
	average = average / (double)numSamples;
	
	//printf("sample max amplitude = "PRINTF_S_FORMAT"\n", max );
	//printf("sample average = %lf\n", average );
	
	
	// Write recorded data to a file. 
#if WRITE_TO_FILE
	{   // the file size should be 5 minutes of 8K samples * 2
	    // the entire ring buffer is recorded without regard to the 
	    // read/write pointers. Make sense of the file by editing it in
	    // Audacity
	    // todo: write the file in correct time order using rd/wr pointers
		FILE  *fid;				
		fid = fopen("recorded.raw", "wb");
		if( fid == NULL )
		{
			//printf("Could not open file.");
		}
		else
		{
			fwrite( data.recordedSamples, NUM_CHANNELS * sizeof(SAMPLE), 
				   totalFrames, fid );
			fclose( fid );
			//printf("Wrote data to 'recorded.raw'\n");
		}
	}
#endif
	

// initial state after recording
// all future state changes from within state machine
    if(buttonHold == FALSE)
    {
        // ******************************************
		doState(S_REWIND_10, (void*) NULL);
		// ******************************************	   
    }
    else
    {
        // ******************************************
		doState(S_REWIND_PLAYBACK, (void*) NULL);	
		// ******************************************    
    }
// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||	
// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||	
    while (1)
    {
        // advance state in the state machine
        // ******************************************
		doState(currentSTATE, (void*) NULL);    
		// ******************************************	
		Pa_Sleep(100);
		if(terminate != FALSE)
		 goto done;
		 
    }
// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||	
// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
	
	
done:
	Pa_Terminate();
	if( data.recordedSamples )       // Sure it is NULL or valid. 
		free( data.recordedSamples );
	if( err != paNoError )
	{
		fprintf( stderr, 
				"An error occured while using the portaudio stream\n" );
		fprintf( stderr, "Error number: %d\n", err );
		fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
		
		fatalError();
		
		err = 1;          // Always return 0 or 1, but no other return codes. 
	}
	return err;
}

