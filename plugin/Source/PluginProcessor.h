// PolyPitch plug-in: a thin JUCE wrapper around polypitch::Processor (engine/PolyPitchProcessor.h), which does all
// the work. Parameters: Semitones, Mix, Tone, Response. Copyright (c) 2026 Ben Juodvalkis. MIT License (see LICENSE).
#pragma once
#include <JuceHeader.h>
#include "PolyPitchProcessor.h"

class PolyPitchAudioProcessor : public juce::AudioProcessor
{
public:
    PolyPitchAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* pSemi = nullptr; std::atomic<float>* pMix = nullptr; std::atomic<float>* pTone = nullptr; std::atomic<float>* pResp = nullptr;
    polypitch::Processor processor;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyPitchAudioProcessor)
};
