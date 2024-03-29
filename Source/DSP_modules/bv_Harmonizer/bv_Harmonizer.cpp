
/*======================================================================================================================================================
           _             _   _                _                _                 _               _
          /\ \          /\_\/\_\ _           /\ \             /\ \              /\ \            /\ \     _
          \ \ \        / / / / //\_\        /  \ \           /  \ \            /  \ \          /  \ \   /\_\
          /\ \_\      /\ \/ \ \/ / /       / /\ \ \         / /\ \_\          / /\ \ \        / /\ \ \_/ / /
         / /\/_/     /  \____\__/ /       / / /\ \ \       / / /\/_/         / / /\ \_\      / / /\ \___/ /
        / / /       / /\/________/       / / /  \ \_\     / / / ______      / /_/_ \/_/     / / /  \/____/
       / / /       / / /\/_// / /       / / /   / / /    / / / /\_____\    / /____/\       / / /    / / /
      / / /       / / /    / / /       / / /   / / /    / / /  \/____ /   / /\____\/      / / /    / / /
  ___/ / /__     / / /    / / /       / / /___/ / /    / / /_____/ / /   / / /______     / / /    / / /
 /\__\/_/___\    \/_/    / / /       / / /____\/ /    / / /______\/ /   / / /_______\   / / /    / / /
 \/_________/            \/_/        \/_________/     \/___________/    \/__________/   \/_/     \/_/
 
 
 This file is part of the Imogen codebase.
 
 @2021 by Ben Vining. All rights reserved.
 
 bv_Harmonizer.cpp: This file defines implementation details for the Harmonizer class. The Harmonizer is essentially a synthesizer that pitch shifts an input instead of using oscillators to make sound.
 
======================================================================================================================================================*/


#include "bv_HarmonizerVoice.cpp"
#include "GrainExtractor/GrainExtractor.cpp"


#define bvh_ADSR_QUICK_ATTACK_MS 5
#define bvh_ADSR_QUICK_RELEASE_MS 5

#define bvh_PLAYING_BUT_RELEASED_GAIN_MULTIPLIER 0.4
#define bvh_SOFT_PEDAL_GAIN_MULTIPLIER 0.65

#define bvh_PITCH_DETECTION_CONFIDENCE_THRESH 0.15

#define bvh_NUM_ANALYSIS_GRAINS 32


namespace bav
{


template<typename SampleType>
Harmonizer<SampleType>::Harmonizer()
{
    Base::setConcertPitchHz(440);
    
    pitchDetector.setConfidenceThresh (SampleType(bvh_PITCH_DETECTION_CONFIDENCE_THRESH));
    
    Base::updateQuickAttackMs (bvh_ADSR_QUICK_ATTACK_MS);
    Base::updateQuickReleaseMs (bvh_ADSR_QUICK_RELEASE_MS);
    
    Base::playingButReleasedMultiplier = float(bvh_PLAYING_BUT_RELEASED_GAIN_MULTIPLIER);
    Base::softPedalMultiplier = float(bvh_SOFT_PEDAL_GAIN_MULTIPLIER);
}
    
#undef bvh_ADSR_QUICK_ATTACK_MS
#undef bvh_ADSR_QUICK_RELEASE_MS
#undef bvh_PLAYING_BUT_RELEASED_GAIN_MULTIPLIER
#undef bvh_SOFT_PEDAL_GAIN_MULTIPLIER

    
template<typename SampleType>
void Harmonizer<SampleType>::initialized (const double initSamplerate, const int initBlocksize)
{
    pitchDetector.initialize();
    juce::ignoreUnused (initSamplerate, initBlocksize);
}


template<typename SampleType>
void Harmonizer<SampleType>::prepared (int blocksize)
{
    inputStorage.setSize (1, blocksize);
    
    indicesOfGrainOnsets.ensureStorageAllocated (blocksize);

    grains.prepare (blocksize);
    
    while (analysisGrains.size() < bvh_NUM_ANALYSIS_GRAINS)
        analysisGrains.add (new Analysis_Grain());
    
    for (auto* grain : analysisGrains)
        grain->reserveSize (blocksize);
}


template<typename SampleType>
void Harmonizer<SampleType>::samplerateChanged (double newSamplerate)
{
    pitchDetector.setSamplerate (newSamplerate);
}
    

template<typename SampleType>
void Harmonizer<SampleType>::updatePitchDetectionHzRange (const int minHz, const int maxHz)
{
    pitchDetector.setHzRange (minHz, maxHz);

    if (Base::sampleRate > 0)
        pitchDetector.setSamplerate (Base::sampleRate);
}

    
template<typename SampleType>
void Harmonizer<SampleType>::release()
{
    inputStorage.clear();
    indicesOfGrainOnsets.clear();
    grains.releaseResources();
    pitchDetector.releaseResources();
    analysisGrains.clear();
}

    
template<typename SampleType>
void Harmonizer<SampleType>::render (const AudioBuffer& input, AudioBuffer& output, juce::MidiBuffer& midiMessages)
{
    jassert (input.getNumSamples() == output.getNumSamples());
    jassert (output.getNumChannels() == 2);
    analyzeInput (input);
    Base::renderVoices (midiMessages, output);
}

    
template<typename SampleType>
void Harmonizer<SampleType>::analyzeInput (const AudioBuffer& inputAudio)
{
    jassert (Base::sampleRate > 0);
    
//    const auto inputFrequency = pitchDetector.detectPitch (inputAudio);  // outputs 0.0 if frame is unpitched
//    const bool frameIsPitched = inputFrequency > 0;
    
    const float inputFrequency = 440.0f;
    const bool frameIsPitched = true;
    
    const auto numSamples = inputAudio.getNumSamples();
    
    vecops::copy (inputAudio.getReadPointer(0), inputStorage.getWritePointer(0), numSamples);
    
    nextFramesPeriod = frameIsPitched ? juce::roundToInt (Base::sampleRate / inputFrequency)
                                      : juce::Random::getSystemRandom().nextInt (unpitchedArbitraryPeriodRange);
    
    jassert (nextFramesPeriod > 0);
    
    if (! frameIsPitched && bav::math::probability (50)) // for unpitched frames, reverse the polarity approx 50% of the time
    {
        vecops::multiplyC (inputStorage.getWritePointer(0), SampleType(-1), numSamples); // negate the samples -- reverse polarity
    }
    
    //grains.getGrainOnsetIndices (indicesOfGrainOnsets, inputStorage, nextFramesPeriod);
    
    indicesOfGrainOnsets.clear();

    indicesOfGrainOnsets.add (0);

    int grainStart = nextFramesPeriod;

    while (grainStart + nextFramesPeriod <= numSamples)
    {
        indicesOfGrainOnsets.add (grainStart);
        grainStart += nextFramesPeriod;
    }
    
    const auto grainSize = nextFramesPeriod * 2;
    
    //  write to analysis grains...
    for (int index : indicesOfGrainOnsets)
        if (auto* grain = getEmptyGrain())
            grain->storeNewGrain (inputStorage.getReadPointer(0), index, index + grainSize);
}


// adds a specified # of voices
template<typename SampleType>
void Harmonizer<SampleType>::addNumVoices (const int voicesToAdd)
{
    if (voicesToAdd == 0)
        return;
    
    for (int i = 0; i < voicesToAdd; ++i)
        Base::voices.add (new Voice(this));
    
    jassert (Base::voices.size() >= voicesToAdd);
    
    Base::numVoicesChanged();
}


template class Harmonizer<float>;
template class Harmonizer<double>;

    
#undef bvh_NUM_ANALYSIS_GRAINS

} // namespace
