/*
  ==============================================================================

    GlobalDefinitions.h
    Created: 23 Nov 2020 9:56:38pm
    Author:  Ben Vining
 
 	This file just defines some global variables. I put these all in 1 file for convenience of includes.

  ==============================================================================
*/

#pragma once


#ifndef NUMBER_OF_VOICES
#define NUMBER_OF_VOICES 12 // the number of instances of the harmony algorithm running concurrently
#endif

#ifndef MAX_BUFFERSIZE
#define MAX_BUFFERSIZE 1024 // an arbitrary maximum size, in samples, for the input audio buffer. If an audio vector is recieved from the host that is larger than this size, it will be sliced into a series of smaller vectors that are MAX_BUFFERSIZE or smaller.
#endif
