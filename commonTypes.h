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

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H


#include <stdio.h>

// Select sample format. 
 #if 1
 #define PA_SAMPLE_TYPE  paFloat32
 typedef float SAMPLE;
 #define SAMPLE_SILENCE  (0.0f)
 #define PRINTF_S_FORMAT "%.8f"
 #elif 1
 #define PA_SAMPLE_TYPE  paInt16
 typedef short SAMPLE;
 #define SAMPLE_SILENCE  (0)
 #define PRINTF_S_FORMAT "%d"
 #elif 0
 #define PA_SAMPLE_TYPE  paInt8
 typedef char SAMPLE;
 #define SAMPLE_SILENCE  (0)
 #define PRINTF_S_FORMAT "%d"
 #else
 #define PA_SAMPLE_TYPE  paUInt8
 typedef unsigned char SAMPLE;
 #define SAMPLE_SILENCE  (128)
 #define PRINTF_S_FORMAT "%d"
 #endif

typedef int BOOL;

#ifndef TRUE
#  define       TRUE    (1==1)
#  define       FALSE   (!TRUE)                                               
#endif

//#define TRUE (1)
//#define FALSE (0)
#define ON (TRUE)
#define OFF (FALSE)
#define FWD (1)
#define REV (-1)

// this is essentially the number of stereo samples per callback
// A set of two samples that occur simultaneously
#define FRAMES_PER_BUFFER (512)

// 5 min. max before wrap
#define NUM_SECONDS     (60*5)
#define NUM_PLAYBACK_SECONDS (10)
//#define SAMPLE_RATE  (8000)
#define SAMPLE_RATE  (44100)

#define DITHER_FLAG     (0) 
//Set to 1 if you want to capture the recording to a file. 
#define WRITE_TO_FILE   (0)

#define RING_BUFFER_LEN   (NUM_SECONDS * SAMPLE_RATE * NUM_CHANNELS)
#define NUM_CHANNELS    (2)
#define LED_PIN 0       // LED contol, on = 1
#define BUTTON_PIN 2    // push-button switch 
#define DIRECTION_PIN 3 // rewind FWD/REV direction
#define ERROR_PIN 21    // ~error
#define REBOOT_PIN 25   // reboot

SAMPLE*      recordedSamples2X;
SAMPLE*      recordedSamples4X;

// states for the state machine
typedef enum  { S_INIT = 0,
	S_COLLECTING,
	S_REWIND_10,
	S_REWIND_PLAYBACK,
	S_PLAYBACK,
	S_STOP } states_t;


typedef enum  { 
    LOAD = 0,
	STORE } ringbufferType_t;

typedef struct
{
    int          frameIndex;  // Index into sample array. 
    int          maxFrameIndex;
    SAMPLE*      recordedSamples;
}
paTestData;

typedef struct
{
    SAMPLE*                   in;
    SAMPLE*                   out;
    int                       number;
    int*                      pointer;
    int                       direction;
    int                       decimate;
    ringbufferType_t          type;
} ringBufferRAM_t;

#endif