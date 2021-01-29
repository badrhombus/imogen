/*
  ==============================================================================

    ZeroCrossingFinding.cpp
    Created: 24 Jan 2021 1:27:14pm
    Author:  Ben Vining

  ==============================================================================
*/

#include "../../Source/DSP/HarmonizerDSP/GrainExtractor/GrainExtractor.h"


template<typename SampleType>
void GrainExtractor<SampleType>::findZeroCrossings (Array<int>& targetArray,
                                                    const SampleType* reading,
                                                    const int totalNumSamples,
                                                    const int period)
{
    targetArray.clearQuick();
    
    for (int p = 0;
         p < totalNumSamples;
         p += period)
    {
        getZeroCrossingForPeriod (targetArray, reading, p, std::min (totalNumSamples, p + period));
    }
};


template<typename SampleType>
void GrainExtractor<SampleType>::getZeroCrossingForPeriod (Array<int>& targetArray,
                                                           const SampleType* reading,
                                                           const int startSample,
                                                           const int endSample)
{
    if (reading[startSample] == 0.0)
    {
        targetArray.add (startSample);
        return;
    }
    
    const bool startedPositive = reading[startSample] > 0.0;
    
    for (int s = startSample + 1; s < endSample; ++s)
    {
        const SampleType currentSample = reading[s];
        
        if (currentSample == 0.0)
        {
            targetArray.add (s);
            return;
        }
        
        const bool isNowPositive = currentSample > 0.0;
        
        if (startedPositive != isNowPositive)
        {
            targetArray.add (s);
            return;
        }
    }
};


template class GrainExtractor<float>;
template class GrainExtractor<double>;