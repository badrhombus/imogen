/*
    Part of module: bv_GeneralUtils
    Parent file: bv_GeneralUtils.h
*/


#include "bv_GeneralUtils/bv_GeneralUtils.h"


namespace bav

{
    

template<typename SampleType>
class DelayBuffer
{
public:
    
    DelayBuffer (const int numChannels, const int size)
    {
        initialize (numChannels, size);
    }
    
    DelayBuffer()
    {
        writeIndex = 0;
        storedSamples = 0;
        base.setSize (0, 0);
    }
    
    ~DelayBuffer()
    { }
    
    
    void initialize (const int numChannels, const int size)
    {
        writeIndex = 0;
        storedSamples = 0;
        
        base.setSize (numChannels, size);
        
        constexpr SampleType zero = SampleType(0.0);
        
        for (int chan = 0; chan < numChannels; ++chan)
            juce::FloatVectorOperations::fill (base.getWritePointer(chan), zero, size);
    }
    
    
    void releaseResources()
    {
        base.setSize (0, 0);
        writeIndex = 0;
        storedSamples = 0;
    }
    
    
    void changeSize (const int newNumChannels, const int newSize)
    {
        if (base.getNumSamples() == newSize && base.getNumChannels() == newNumChannels)
            return;
        
        base.setSize (newNumChannels, newSize, true, true, true);
        
        if (writeIndex >= newSize)
            writeIndex -= newSize;
    }
    
    
    void pushSamples (const juce::AudioBuffer<SampleType>& inputBuffer, const int inputChannel, const int inputStartSample, const int numSamples, const int destChannel)
    {
        pushSamples (inputBuffer.getReadPointer(inputChannel, inputStartSample),
                     numSamples, destChannel);
    }
    
    
    void pushSamples (const SampleType* inputSamples, const int numSamples, const int destChannel)
    {
        const int length = base.getNumSamples();
        
        jassert (length > 0 && base.getNumChannels() > 0);
        jassert (numSamples <= length);
        
        SampleType* writing = base.getWritePointer(destChannel);
        
        int index = writeIndex;
        
        for (int s = 0; s < numSamples; ++s, ++index)
        {
            if (index >= length) index = 0;
            writing[index] = inputSamples[s];
        }
        
        writeIndex = index;
        storedSamples += numSamples;
    }
    
    
    void popSamples (juce::AudioBuffer<SampleType>& destBuffer, const int destChannel, const int destStartSample,
                     const int numSamples, const int readingChannel)
    {
        jassert (destStartSample + numSamples <= destBuffer.getNumSamples());
        
        popSamples (destBuffer.getWritePointer(destChannel) + destStartSample,
                    numSamples, readingChannel);
    }
    
    
    void popSamples (SampleType* output, const int numSamples, const int readingChannel)
    {
        const int length = base.getNumSamples();
        
        jassert (length > 0 && base.getNumChannels() > 0);
        
        int readIndex = writeIndex - numSamples;
        if (readIndex < 0) readIndex += length;
        
        jassert (readIndex >= 0 && readIndex < length);
        
        const SampleType* reading = base.getReadPointer(readingChannel);
        SampleType* writing = base.getWritePointer(readingChannel);
        
        constexpr SampleType zero = SampleType(0.0);
        
        for (int s = 0; s < numSamples; ++s, ++readIndex)
        {
            if (readIndex == length) readIndex = 0;
            output[s] = reading[readIndex];
            writing[s] = zero;
        }
        
        storedSamples -= numSamples;
    }
    
    
    int numStoredSamples() const noexcept
    {
        return storedSamples;
    }
    
    
    int getSize() const noexcept
    {
        return base.getNumSamples();
    }
    
    
private:
    
    juce::AudioBuffer<SampleType> base;
    
    int writeIndex;
    int storedSamples;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayBuffer)
};


} // namespace
