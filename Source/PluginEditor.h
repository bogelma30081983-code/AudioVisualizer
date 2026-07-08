#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ==========================================
// ЕКРАН 1: Спектроаналізатор + Автономний компресор + Фази
// ==========================================
class SpectrumScreen : public juce::Component, private juce::Timer
{
public:
    SpectrumScreen(AudioVisualizerAudioProcessor& p);
    void setFreeze(bool shouldFreeze);
    void paint(juce::Graphics& g) override;

private:
    AudioVisualizerAudioProcessor& processor;
    bool isFrozen = false;

    std::array<float, AudioVisualizerAudioProcessor::fftSize> frozenLeftFFT{};
    std::array<float, AudioVisualizerAudioProcessor::fftSize> frozenRightFFT{};
    std::array<float, AudioVisualizerAudioProcessor::fftSize / 2> frozenPhase;

    const float minFreq = 20.0f, maxFreq = 20000.0f;
    const float minDB = -70.0f, maxDB = 6.0f;

    float mapFreqToX(float freq) const;
    float mapDBToY(float db) const;

    juce::Path createSpectrumPath(const std::array<float, AudioVisualizerAudioProcessor::fftSize>& fftData);
    void drawFrequencyGrid(juce::Graphics& g);
    void drawPhaseCurve(juce::Graphics& g);
    void drawResonances(juce::Graphics& g, const std::array<float, AudioVisualizerAudioProcessor::fftSize>& fftData);
    void drawCompressionLines(juce::Graphics& g);
    void drawTargetPinkNoiseCurve(juce::Graphics& g);
    void drawCorrelationMeter(juce::Graphics& g);

    void timerCallback() override { repaint(); }
};

// ==========================================
// ЕКРАН 2: Двоканальний стабілізований осцилограф
// ==========================================
class WaveformScreen : public juce::Component, private juce::Timer
{
public:
    WaveformScreen(AudioVisualizerAudioProcessor& p);
    void setFreeze(bool shouldFreeze);
    void paint(juce::Graphics& g) override;

private:
    AudioVisualizerAudioProcessor& processor;
    bool isFrozen = false;
    std::vector<float> localBufferL, localBufferR;
    void timerCallback() override { repaint(); }
};

// ==========================================
// ЕКРАН 3: Гоніометр / Стерео-панорама (Mid/Side)
// ==========================================
class StereoScreen : public juce::Component, private juce::Timer
{
public:
    StereoScreen(AudioVisualizerAudioProcessor& p);
    void setFreeze(bool shouldFreeze);
    void paint(juce::Graphics& g) override;

private:
    AudioVisualizerAudioProcessor& processor;
    bool isFrozen = false;
    juce::Path frozenGoniometerPath;
    void timerCallback() override { repaint(); }
};

// ==========================================
// ГОЛОВНЕ ВІКНО ПЛАГІНА (EDITOR)
// ==========================================
class AudioVisualizerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    AudioVisualizerAudioProcessorEditor(AudioVisualizerAudioProcessor&);
    ~AudioVisualizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AudioVisualizerAudioProcessor& audioProcessor;

    SpectrumScreen spectrumView;
    WaveformScreen waveformView;
    StereoScreen stereoView;

    juce::TextButton freezeSpectrumButton{ "FRZ SPEC" };
    juce::TextButton freezeWaveformButton{ "FRZ WAVE" };
    juce::TextButton freezeStereoButton{ "FRZ STR" };
    juce::TextButton freezeAllButton{ "FREEZE ALL" };

    void checkIfAllAreFrozen();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioVisualizerAudioProcessorEditor)
};