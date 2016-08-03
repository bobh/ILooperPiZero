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

#ifndef ILOOPER_AUDIO_H
#define ILOOPER_AUDIO_H

#include "commonTypes.h"

extern BOOL           all_complete;
extern void           toggleGreenLED();
extern SAMPLE         silenceBufferValue;


extern int            ringbufferWPTR;
extern BOOL           buttonPress;
extern BOOL           buttonHold;

extern SAMPLE         silenceBufferValue;
//extern BOOL           all_complete;

extern int            ringbufferRPTR;
extern int            playbackDirection;
extern PaStreamParameters  inputParameters, outputParameters;
extern PaError        err;
extern PaStream*      stream;
extern paTestData     data;
extern BOOL           buttonPress;
extern BOOL           buttonHold;
extern states_t       currentSTATE;

extern int            OK_ToBlink;
extern int            t_measure;
// This routine will be called by the PortAudio engine when audio is needed.
// ** It may be called at interrupt level on some machines so don't do anything
// ** that could mess up the system like calling malloc() or free().

static int playCallback( const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags,
						void *userData );


// This routine will be called by the PortAudio engine when audio is needed.
//** It may be called at interrupt level on some machines so don't do anything
//** that could mess up the system like calling malloc() or free().

int recordCallback( const void *inputBuffer, 
                          void *outputBuffer,
						  unsigned long framesPerBuffer,
						  const PaStreamCallbackTimeInfo* timeInfo,
						  PaStreamCallbackFlags statusFlags,
						  void *userData );


// samplerate = 8000 normal
//            = 16000 2X
//            = 32000 4X
int playback(int samplerate); 


#endif
