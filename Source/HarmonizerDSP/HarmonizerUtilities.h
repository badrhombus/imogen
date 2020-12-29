/*
  ==============================================================================

    HarmonizerUtilities.h
    Created: 28 Dec 2020 9:15:11pm
    Author:  Ben Vining

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GlobalDefinitions.h"


class PitchConverter
{
public:

	PitchConverter(const int initialConcertPitch, const int initialRootNote, const int initialNotesPerOctave):
	concertPitchHz(initialConcertPitch), rootNote(initialRootNote), notesPerOctave(initialNotesPerOctave)
	{ };

	float mtof(const float midiNote) const // converts midiPitch to frequency in Hz
	{
		jassert(midiNote >= 0.0f && midiNote <= 127.0f);
		return concertPitchHz * std::pow(2.0f, ((midiNote - rootNote) / notesPerOctave));
	};

	int mtof(const int midiNote) const // midiPitch to frequency with integers instead of floats
	{
		jassert(isPositiveAndBelow(midiNote, 128));
		return round(concertPitchHz * std::pow(2.0f, ((midiNote - rootNote) / notesPerOctave)));
	};


	float ftom(const float inputFreq) const // converts frequency in Hz to midiPitch (as a float)
	{
		jassert(inputFreq >= 0);
		return notesPerOctave * log2(inputFreq / concertPitchHz) + rootNote;
	};

	int ftom(const int inputFreq) const // frequency to midiPitch with integers
	{
		jassert(inputFreq >= 0);
		return round(notesPerOctave * log2(inputFreq / concertPitchHz) + rootNote);
	};

	void setConcertPitchHz(const int newConcertPitch) noexcept
	{
		jassert(newConcertPitch >= 0);
		concertPitchHz = newConcertPitch;
	};

	int getCurrentConcertPitchHz() const noexcept { return concertPitchHz; };

	void setNotesPerOctave(const int newNPO) noexcept
	{
		jassert(newNPO > 0);
		notesPerOctave = newNPO;
	};

	int getCurrentNotesPerOctave() const noexcept { return notesPerOctave; };

	void setRootNote(const int newRoot) noexcept
	{
		jassert(newRoot >= 0);
		rootNote = newRoot;
	};

	int getCurrentRootNote() const noexcept { return rootNote; };


private:

	int concertPitchHz; // the frequency in Hz of the root note. Usually 440 in standard Western tuning.

	int rootNote; // the midiPitch that corresponds to concertPitchHz. Usually 69 (A4) in Western standard tuning.

	int notesPerOctave; // the number of notes per octave. Usually 12 in standard Western tuning.

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchConverter)

};




class PitchBendHelper
{
public:
	PitchBendHelper(const int initialStUp, const int initialStDwn):
	rangeUp(initialStUp), rangeDown(initialStDwn), lastRecievedPitchbend(64)
	{ };

	void setRange(const int newStUp, const int newStDown) noexcept
	{
		jassert(newStUp >= 0 && newStDown >= 0);
		rangeUp = newStUp;
		rangeDown = newStDown;
	};

	int getCurrentRangeUp() const noexcept { return rangeUp; };

	int getCurrentRangeDown() const noexcept { return rangeDown; };

	int getLastRecievedPitchbend() const noexcept { return lastRecievedPitchbend; };

	float newNoteRecieved(const int newMidiPitch)
	{
		jassert(isPositiveAndBelow(newMidiPitch, 128));
		return getMidifloat(newMidiPitch, lastRecievedPitchbend);
	};

	void newPitchbendRecieved(const int newPitchbend) noexcept
	{
		jassert(isPositiveAndBelow(newPitchbend, 128));
		lastRecievedPitchbend = newPitchbend;
	};


private:

	int rangeUp, rangeDown;

	int lastRecievedPitchbend;

	float getMidifloat(const int midiPitch, const int pitchbend)
	{
		jassert(isPositiveAndBelow(midiPitch, 128) && isPositiveAndBelow(pitchbend, 128));
		if(pitchbend == 64)
			return midiPitch;
		else if (pitchbend > 64)
			return ((rangeUp * (pitchbend - 65)) / 62) + midiPitch;
		else
			return (((1 - rangeDown) * pitchbend) / 63) + midiPitch - rangeDown;
	};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchBendHelper)
};





class VelocityHelper
{
public:
	VelocityHelper(const int initialSensitivity):
	sensitivity(initialSensitivity/100.0f)
	{ };

	void setSensitivity(const int newSensitivity) noexcept
	{
		jassert(isPositiveAndBelow(newSensitivity, 101));
		sensitivity = newSensitivity / 100.0f;
	};

	void setFloatSensitivity(const float newSensitivity) noexcept
	{
		jassert(newSensitivity >= 0.0f && newSensitivity <= 1.0f);
		sensitivity = newSensitivity;
	};

	float getCurrentSensitivity() const noexcept { return sensitivity; };

	float intVelocity(const int midiVelocity)
	{
		jassert(isPositiveAndBelow(midiVelocity, 128));
		return getGainMult(midiVelocity / 127.0f, sensitivity);
	};

	float floatVelocity(const float floatVelocity)
	{
		jassert(floatVelocity >= 0.0f && floatVelocity <= 1.0f);
		return getGainMult(floatVelocity, sensitivity);
	};

	
private:

	float sensitivity;

	float getGainMult(const float floatVelocity, const float floatSensitivity)
	{
		return ((1.0f - floatVelocity) * (1.0f - floatSensitivity) + floatVelocity);
	};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelocityHelper)
};





class MidiLatchManager
{
public:

	MidiLatchManager(const int maxNumVoices)
	{
		currentlyHeldNotes.ensureStorageAllocated(maxNumVoices);
	};

	Array<int> turnOffLatch()
	{
		return currentlyHeldNotes;
	};

	void noteOnRecieved(const int pitch)
	{
		if(! currentlyHeldNotes.contains(pitch))
			currentlyHeldNotes.add(pitch);
	};

	void noteOffRecieved(const int pitch)
	{
		if(currentlyHeldNotes.contains(pitch))
			currentlyHeldNotes.remove(currentlyHeldNotes.indexOf(pitch));
	};

	int getNumHeldNotes() const noexcept
	{
		return currentlyHeldNotes.size();
	};

	void reset()
	{
		currentlyHeldNotes.clear();
	};


private:
	Array<int> currentlyHeldNotes;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLatchManager)
};