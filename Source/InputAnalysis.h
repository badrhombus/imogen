/*
  ==============================================================================

    InputAnalysis.h
    Created: 14 Dec 2020 6:32:56pm
    Author:  Ben Vining

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GlobalDefinitions.h"


class PitchTracker
{
public:
	PitchTracker();
	~PitchTracker();
	
	float findPitch(AudioBuffer<float>& inputAudio, const int inputChan, const int startSample, const int numSamples, const double samplerate);
	
private:
	mutable AudioBuffer<float> yinBuffer;
	Array<float> powerTerms;
	int yinBufferSize;
	
	void difference(AudioBuffer<float>& inputBuffer, const int inputChan, const int inputBufferLength, const int startSample);
	
	void cumulativeMeanNormalizedDifference() const;
	
	int absoluteThreshold();
	
	float parabolicInterpolation(int tauEstimate) const;
	
	JUCE_LEAK_DETECTOR(PitchTracker)
};




class EpochFinder
{
public:
	EpochFinder();
	~EpochFinder();
	
	Array<int> extractEpochSampleIndices(AudioBuffer<float>& inputAudio, const int inputChan, const int startSample, const int numSamples, const double samplerate);
	
private:
	CriticalSection lock;
	
	Array<float> y;
	Array<float> y2;
	Array<float> y3;
	
	Array<int> epochs;
	
	JUCE_LEAK_DETECTOR(EpochFinder)
};