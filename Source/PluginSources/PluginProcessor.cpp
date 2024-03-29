
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
 
 PluginProcessor.cpp: This file contains the guts of Imogen's AudioProcessor code.
 
======================================================================================================================================================*/


#include "PluginEditor.h"
#include "PluginProcessor.h"


ImogenAudioProcessor::ImogenAudioProcessor():
    AudioProcessor(makeBusProperties()),
    tree(*this, nullptr, "IMOGEN_PARAMETERS", createParameters())
#if IMOGEN_ONLY_BUILDING_STANDALONE
    , denormalsWereDisabledWhenTheAppStarted(juce::FloatVectorOperations::areDenormalsDisabled())
#endif
{
#if BV_USE_NE10
    ne10_init();  // if you use the Ne10 library, you must initialize it in your constructor like this!
#endif
    
    jassert (AudioProcessor::getParameters().size() == IMGN_NUM_PARAMS);
    initializeParameterPointers();
    initializeParameterListeners();
    updateParameterDefaults();
    
    if (isUsingDoublePrecision())
        initialize (doubleEngine);
    else
        initialize (floatEngine);
    
#if IMOGEN_ONLY_BUILDING_STANDALONE
    //  if running as a standalone app, denormals are disabled for the lifetime of the app (instead of scoped within the processBlock).
    juce::FloatVectorOperations::disableDenormalisedNumberSupport (true);
    juce::FloatVectorOperations::enableFlushToZeroMode (true);
#endif
}

ImogenAudioProcessor::~ImogenAudioProcessor()
{
#if IMOGEN_ONLY_BUILDING_STANDALONE
    juce::FloatVectorOperations::disableDenormalisedNumberSupport (denormalsWereDisabledWhenTheAppStarted);
    juce::FloatVectorOperations::enableFlushToZeroMode (denormalsWereDisabledWhenTheAppStarted);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename SampleType>
inline void ImogenAudioProcessor::initialize (bav::ImogenEngine<SampleType>& activeEngine)
{
    auto initSamplerate = getSampleRate();
    if (initSamplerate <= 0.0) initSamplerate = 44100.0;
    
    auto initBlockSize = getBlockSize();
    if (initBlockSize <= 0) initBlockSize = 512;
    
    activeEngine.initialize (initSamplerate, initBlockSize);
    
    updateAllParameters (activeEngine);
    
    setLatencySamples (activeEngine.reportLatency());
}


void ImogenAudioProcessor::prepareToPlay (const double sampleRate, const int samplesPerBlock)
{
    if (isUsingDoublePrecision())
        prepareToPlayWrapped (sampleRate, doubleEngine, floatEngine);
    else
        prepareToPlayWrapped (sampleRate, floatEngine,  doubleEngine);
    
    paramChanges.reserveSize (samplesPerBlock);
    
    currentMessages.ensureStorageAllocated (samplesPerBlock);
}


template <typename SampleType1, typename SampleType2>
inline void ImogenAudioProcessor::prepareToPlayWrapped (const double sampleRate,
                                                        bav::ImogenEngine<SampleType1>& activeEngine,
                                                        bav::ImogenEngine<SampleType2>& idleEngine)
{
    if (! idleEngine.hasBeenReleased())
        idleEngine.releaseResources();
    
    updateAllParameters (activeEngine);
    
    jassert (activeEngine.getLatency() > 0);
    
    activeEngine.prepare (sampleRate);
    
    setLatencySamples (activeEngine.reportLatency());
}


void ImogenAudioProcessor::releaseResources()
{
    if (! doubleEngine.hasBeenReleased())
        doubleEngine.releaseResources();
    
    if (! floatEngine.hasBeenReleased())
        floatEngine.releaseResources();
}


void ImogenAudioProcessor::reset()
{
    if (isUsingDoublePrecision())
        doubleEngine.reset();
    else
        floatEngine.reset();
}


void ImogenAudioProcessor::editorPitchbend (int wheelValue)
{
    nonParamEvents.pushMessage (pitchBendFromEditor, pitchbendNormalizedRange.convertTo0to1(float(wheelValue)));
}


/*
 These four functions represent the top-level callbacks made by the host during audio processing. Audio samples may be sent to us as float or double values; both of these functions redirect to the templated processBlockWrapped() function below.
 The buffers sent to this function by the host may be variable in size, so I have coded defensively around several edge cases & possible buggy host behavior and created several layers of checks that each callback passes through before individual chunks of audio are actually rendered.
*/

void ImogenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockWrapped (buffer, midiMessages, floatEngine, mainBypass->get());
}


void ImogenAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockWrapped (buffer, midiMessages, doubleEngine, mainBypass->get());
}


void ImogenAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (! mainBypass->get())
        mainBypass->setValueNotifyingHost (1.0f);
    
    processBlockWrapped (buffer, midiMessages, floatEngine, true);
}


void ImogenAudioProcessor::processBlockBypassed (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    if (! mainBypass->get())
        mainBypass->setValueNotifyingHost (1.0f);
    
    processBlockWrapped (buffer, midiMessages, doubleEngine, true);
}


// LAYER 2 ---------------------------------------------------------------------------------

template <typename SampleType>
inline void ImogenAudioProcessor::processBlockWrapped (juce::AudioBuffer<SampleType>& buffer,
                                                       juce::MidiBuffer& midiMessages,
                                                       bav::ImogenEngine<SampleType>& engine,
                                                       const bool isBypassedThisCallback)
{
    jassert (! engine.hasBeenReleased() && engine.hasBeenInitialized());
    
#if ! IMOGEN_ONLY_BUILDING_STANDALONE
    juce::ScopedNoDenormals nodenorms;
#endif
    
    updateAllParameters (engine);
    //processQueuedParameterChanges (engine);
    processQueuedNonParamEvents (engine);

    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;
   
    juce::AudioBuffer<SampleType> inBus  = getBusBuffer (buffer, true, getBusesLayout().getMainInputChannelSet() == juce::AudioChannelSet::disabled());
    juce::AudioBuffer<SampleType> outBus = getBusBuffer (buffer, false, 0);
    
    engine.process (inBus, outBus, midiMessages, isBypassedThisCallback);
}


/*===========================================================================================================================
 ============================================================================================================================*/


// standard and general-purpose functions -----------------------------------------------------------------------------------------------------------

double ImogenAudioProcessor::getTailLengthSeconds() const
{
    if (adsrToggle->get())
        return double(adsrRelease->get()); // ADSR release time in seconds
    
    return 0.005;  // "quick kill" time in seconds -- must be the same as the ms value defined in the macro in bv_Harmonizer.cpp !!
}


inline juce::AudioProcessor::BusesProperties ImogenAudioProcessor::makeBusProperties() const
{
    auto stereo = juce::AudioChannelSet::stereo();
    auto mono   = juce::AudioChannelSet::mono();

    return BusesProperties().withInput ("Input",  stereo, true)
                            .withInput ("Sidechain", mono, false)
                            .withOutput("Output", stereo, true);
}


bool ImogenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto disabled = juce::AudioChannelSet::disabled();
    
    if (layouts.getMainInputChannelSet() == disabled && layouts.getChannelSet(true, 1) == disabled)
        return false;
    
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}


juce::AudioProcessorEditor* ImogenAudioProcessor::createEditor()
{
    return new ImogenAudioProcessorEditor(*this);
}


// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ImogenAudioProcessor();
}
