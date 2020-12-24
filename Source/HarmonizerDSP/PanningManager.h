/*
  ==============================================================================

    PanningManager.h
    Created: 14 Dec 2020 3:20:30pm
    Author:  Ben Vining

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GlobalDefinitions.h"


class PanningManager
{
public:
	
	PanningManager();
	~PanningManager();
	
	// used to change the # of polyphony voices currently active
	void setNumberOfVoices(const int newNumVoices);
	
	
	// used to update the width of the stereo field of currently available / valid pan values
	void updateStereoWidth(const int newWidth);
	
	
	// returns next available panning value. Panning values are assigned from the "middle out" - the first pan values sent will be toward the center of the stereo field, working outwards to either side as more pan values are requested
	int getNextPanVal();
	
	
	// used to tell the PanningManager when a previously assigned panningvalue is turned off - ie, is now available again for another voice 
	void panValTurnedOff(const int panVal);
	

	// used when updating stereo width -- voices should grab the new pan val that's closest to their old pan val
	int getClosestNewPanValFromOld(const int oldPan);
	
	
	// tells the PanningManager that all voices have been turned off -- ie, all the pan vals are available again
	// the boolean argument should normally be false. 'true' is used in a situation where reset() needs to be called, but a new voice will be turned on immediately, or is already on - thus, the first pan value has already been taken.
	void reset(const bool grabbedFirst64);
	
	
private:
	CriticalSection lock;
	
	Array<int> possiblePanVals; // this array stores all the possible pan values for the desired stereo width, in increasing numerical order from 0 to 127
	
	Array<int> panValsInAssigningOrder; // this array stores the pan values in the order they will be sent out, ie "middle out". Index 0 contains 64, and the highest two indices will contain 0 and 127 [if the stereo width is 100]
	
	Array<int> arrayIndexesMapped; // this array is used to facilitate the transfer of values from possiblePanVals to panValsInAssigningOrder
	
	Array<int> unsentPanVals; // this is the array we will actually be reading pan vals from! the others are for sorting.
	
	Array<int> absDistances; // used for finding which new pan value is the closest to a voice's old pan val
	
	int lastRecievedStereoWidth;
	int currentNumVoices;
	
	void mapArrayIndexes();
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanningManager)
};