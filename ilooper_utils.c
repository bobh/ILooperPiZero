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

#include "commonTypes.h"
// ****************************************************************************
static testCount = 0;

void RingBuffer(ringBufferRAM_t theBuffer) 
{
	int add;
	int i;
	i = *(theBuffer.pointer);
	add = 1 - RING_BUFFER_LEN;
	
	if(theBuffer.type == LOAD)
	{	    // RECORD	
			while((theBuffer.number)-- > 0)
			{
						
				*((theBuffer.out)+i) = *(theBuffer.in++);
				i += add;
				if(i < 0) 
				{
					i += RING_BUFFER_LEN;
				}
			}
	}		
	else 
	{	// PLAYBACK	
		if(theBuffer.direction >= 0) 
		{
			
			while((theBuffer.number)-- > 0)
			{
				*(theBuffer.out++) = *((theBuffer.in)+i);
				i += theBuffer.decimate;				
				i += add;
				
				if(i < 0) 
				{
					i += RING_BUFFER_LEN;
					
				}
			}
		}
		else         
		{
			while((theBuffer.number)-- > 0)
			{
				*(theBuffer.out++) = *((theBuffer.in)+i);
				i -= theBuffer.decimate;				
				i -= 1;
				
				if(i < 0) 
				{
					i = RING_BUFFER_LEN - 1;
				}
			}
		}				
	}
	
  *(theBuffer.pointer) = i;		

}

// **************************************************************************
