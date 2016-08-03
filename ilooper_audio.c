//  Created by Robert Hinrichs on 1/11/16.
//  Copyright (c) 2016 wm6h. All rights reserved.

/*
 * $Id$
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/time.h>                // for gettimeofday()
//#include <unistd.h>
//#include <sys/reboot.h>

#include "portaudio.h"

//wiringPi allows access to GPIO pins
#include <wiringPi.h>

#include "commonTypes.h"
#include "ilooper_utils.h"
#include "ilooper_audio.h"

extern ringBufferRAM_t RingBufferConfig;
extern int ringbufferWPTR;
extern int ringbufferRPTR;

static  int intcounter = 0;
//static int t_measure = 0;

struct timeval t1, t2;
double elapsedTime;

double load;


// This routine will be called by the PortAudio engine when audio is needed.
//** It may be called at interrupt level on some machines so don't do anything
//** that could mess up the system like calling malloc() or free().

int recordCallback( const void *inputBuffer, 
				   void *outputBuffer,
				   unsigned long framesPerBuffer,
				   const PaStreamCallbackTimeInfo* timeInfo,
				   PaStreamCallbackFlags statusFlags,
				   void *userData )
{
    paTestData* data = (paTestData*)userData;
    SAMPLE* rptr = (SAMPLE*)inputBuffer;
    SAMPLE* wptr = &data->recordedSamples[0];
	
    long framesToCalc;
    long i;
    int finished;	
	
    (void) outputBuffer; // Prevent unused variable warnings. 
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;
	
    RingBufferConfig.out = wptr;
    RingBufferConfig.type = LOAD;
    RingBufferConfig.pointer = &ringbufferWPTR;
    RingBufferConfig.direction = FWD;	
    
    if(all_complete == FALSE)
    {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }
    else // early out
    {
		finished = paComplete;  		
		framesToCalc = framesPerBuffer;
	}
    
 	
    if( inputBuffer == NULL )
    {
		RingBufferConfig.in = &silenceBufferValue;
		RingBufferConfig.number = 1;
		
        for( i=0; i<framesToCalc; i++ )
        {
			RingBuffer(RingBufferConfig); 
			
			if( NUM_CHANNELS == 2 ) 
			{ 
				RingBuffer(RingBufferConfig);                     
			} 
			
        }
    }
    else
    {	 
		RingBufferConfig.in = rptr;
		RingBufferConfig.number = framesToCalc*NUM_CHANNELS;
		
		//|||||||||||||||||||||||||||
		RingBuffer(RingBufferConfig); 
		//|||||||||||||||||||||||||||
		
		if(intcounter++ % 100 == 0)
		{
		    if(OK_ToBlink == 1)
		    {
			  toggleGreenLED();
			  digitalWrite (ERROR_PIN, OFF) ;
			}
			
			if(t_measure == 1)
			{			   
				// stop timer
				gettimeofday(&t2, NULL);
				// compute and print the elapsed time in millisec
				elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;    // sec to ms
				elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
												
				//////////load = Pa_GetStreamCpuLoad( stream );
                //////////if(load > 0.008)
				if(elapsedTime >= 1200.0)
				{
					OK_ToBlink = 0;
					digitalWrite (ERROR_PIN, ON) ;
					
				///printf("\n time: %f \n", elapsedTime);
				///fflush(stdout);					
				}
				else
				{
					OK_ToBlink = 1;
					digitalWrite (ERROR_PIN, OFF) ;
					
				}
				
				t_measure = 0;
				
			}
			else
			{
				t_measure = 1;
				// start timer
				gettimeofday(&t1, NULL);
				
			}
			
		}
    }
    
    data->frameIndex += framesToCalc;
    
    if(buttonHold == FALSE) 
    { 
		if(digitalRead(BUTTON_PIN) == 0)
		{
			//low if button pressed
			//latched
			buttonPress = TRUE;
		}
    }
    
    return finished;
}

// This routine will be called by the PortAudio engine when audio is needed.
// ** It may be called at interrupt level on some machines so don't do anything
// ** that could mess up the system like calling malloc() or free().

static int playCallback( const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags,
						void *userData )
{
    paTestData* data = (paTestData*)userData;
    SAMPLE* wptr = (SAMPLE*)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = data->maxFrameIndex - data->frameIndex;
	
    (void) inputBuffer; // Prevent unused variable warnings. 
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;
    
    RingBufferConfig.in = &data->recordedSamples[0];
    RingBufferConfig.out = wptr;
    RingBufferConfig.type = STORE;
    RingBufferConfig.pointer = &ringbufferRPTR;
    RingBufferConfig.direction = playbackDirection;
	
    if( framesLeft < framesPerBuffer )
    {
        // final buffer...
        RingBufferConfig.number = framesLeft*NUM_CHANNELS;
        RingBuffer(RingBufferConfig);
		
		for( ; i<framesPerBuffer; i++ )
		{
			*wptr++ = 0;  // left 
			if( NUM_CHANNELS == 2 ) 
			{
				*wptr++ = 0;  // right 
			} 
		}
		data->frameIndex += framesLeft;
		finished = paComplete;
	}
	else
	{
		RingBufferConfig.number = framesPerBuffer*NUM_CHANNELS;
        RingBuffer(RingBufferConfig);
		
		data->frameIndex += framesPerBuffer;
		finished = paContinue;
	}	
	
	// normal playback operation (FWD) regular speed:
	// play until data->maxFrameIndex = data->frameIndex 
	// hold button operation is ignored
	
	// rewind playback operation (REV or FWD):
	// play at high sample rate until hold button is released
	// State Machine will then advance to normal FWD playback
	
	
    // if we are rewinding, look for the button to be released
    if(currentSTATE == S_REWIND_PLAYBACK)
    {
		if(digitalRead(BUTTON_PIN) == 1)
		{
			//high if button released
			//latched
			// this will cause playback to stop
			buttonPress = TRUE;
			//printf("\n rewind button released \n");
		    //fflush(stdout);
			
		}	   
	}
	
	// all_complete is set by the caller when all samples have been played or
	// button has been released (rewind play)	    
	if(all_complete == TRUE)
		
    {
		// if rewinding and button released, stop the callback
		// all_complete is set in the playback() loop
		finished = paComplete;
	}
	
	
	return finished;
}

// samplerate = 8000 sps normal
//            = 16000 2X
//            = 32000 4X
// these rates must be supported in the sound card
int playback(int samplerate) 
{
	
	// change to support more USB sound cards
	// they all support 44100 sps
	
    if(samplerate == 16000)
    {
		RingBufferConfig.decimate = 2;
    }
    else if(samplerate == 32000)
    {
		RingBufferConfig.decimate = 4;
    }
    else
    {  
		RingBufferConfig.decimate = 0;
    }
	
    samplerate = 44100; 
    
	// default output device
	outputParameters.device = Pa_GetDefaultOutputDevice();  
	if (outputParameters.device == paNoDevice) {
		fprintf(stderr,"Error: No default output device.\n");
		goto done;
	}
	outputParameters.channelCount = 2; // stereo output 
	outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
	
	//
	outputParameters.suggestedLatency = 	
	Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	
	//printf("\n=== Now playing back. ===\n"); 
	
	// this checks support for the DAC sample rate
	PaStreamParameters *inParameters = &inputParameters;
	PaStreamParameters *outParameters = &outputParameters; 
	err = Pa_IsFormatSupported( NULL, outParameters, samplerate );
	if( err == paFormatIsSupported )
	{
		//printf("\n DAC samplerate is supported\n");
	}
	else
	{
		//printf("\n DAC samplerate is NOT supported\n");
		//fflush(stdout);
		fatalError();
	}
	
	err = Pa_OpenStream
	(
	 &stream,
	 NULL, // no input 
	 &outputParameters,
	 samplerate,
	 FRAMES_PER_BUFFER,
	 // we won't output out of range samples so don't 
	 // bother clipping them 
	 paClipOff,      
	 playCallback,
	 &data 
	 );
	
	if( err != paNoError ) goto done;
	
	
	if( stream )
	{
	    all_complete = FALSE;
		err = Pa_StartStream( stream );
		if( err != paNoError ) goto done;
		
		//printf("Waiting for playback to finish.\n"); 
		//fflush(stdout);
		
        
		// playback loop
		// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		while( ( err = Pa_IsStreamActive( stream ) ) == 1 ) 
		{
			Pa_Sleep(500);
			
			if(buttonPress == 1)
			{
				all_complete = TRUE; //stop the sample interrupt, exit while loop
			}
			
		}
		if( err < 0 ) goto done;
		// |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
		
		err = Pa_CloseStream( stream );
		if( err != paNoError ) goto done;
		
		//printf("Done.\n"); 
		//fflush(stdout);
	}
	
done:
	if( err != paNoError )
	{
		fprintf( stderr, 
				"An error occured while using the portaudio stream\n" );
		fprintf( stderr, "Error number: %d\n", err );
		fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	    
	    fatalError();
	    
		err = 1;          // Always return 0 or 1, but no other return codes. 
	}
	
	// returns to state machine
	return err;	
}


