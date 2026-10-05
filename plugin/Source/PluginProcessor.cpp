#include "PluginProcessor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PolyPitchAudioProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterInt>   (juce::ParameterID { "semitones", 1 }, "Semitones", -12, 12, -12));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tone", 1 }, "Tone", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "response", 1 }, "Response", juce::StringArray { "Fast", "Balanced", "Clean" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "quality", 2 }, "Quality", juce::StringArray { "Full", "Lite" }, 0));     // (version hint 2: added after the first four)
    return l;
}

PolyPitchAudioProcessor::PolyPitchAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "params", layout())
{
    pSemi = apvts.getRawParameterValue ("semitones"); pMix = apvts.getRawParameterValue ("mix"); pTone = apvts.getRawParameterValue ("tone"); pResp = apvts.getRawParameterValue ("response"); pQual = apvts.getRawParameterValue ("quality");
}

bool PolyPitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && in == out;
}

void PolyPitchAudioProcessor::prepareToPlay (double sampleRate, int)
{
    processor.setSemitones ((int) std::lround (pSemi->load())); processor.setMix (pMix->load() / 100.0); processor.setTone (pTone->load() / 100.0);
    processor.setResponse ((int) std::lround (pResp->load())); processor.setLite (pQual->load() > 0.5f);
    processor.prepare (sampleRate);
    setLatencySamples (0);                        // the dry path is not delayed; the shifted sound's own delay is part of the effect
}

void PolyPitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    processor.setSemitones ((int) std::lround (pSemi->load())); processor.setMix (pMix->load() / 100.0); processor.setTone (pTone->load() / 100.0);
    processor.setResponse ((int) std::lround (pResp->load())); processor.setLite (pQual->load() > 0.5f);
    float* L = buffer.getWritePointer (0); float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    processor.process (L, R, L, R, buffer.getNumSamples());
}

void PolyPitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}
void PolyPitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes)) if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PolyPitchAudioProcessor(); }
