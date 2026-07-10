#pragma once
#include <JuceHeader.h>
//#include <complex>
#include <array>
#include <atomic>

class AudioVisualizerAudioProcessor : public juce::AudioProcessor
{
public:
    AudioVisualizerAudioProcessor();
    ~AudioVisualizerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return {}; }
    void changeProgramName(int index, const juce::String& newName) override {}

    void getStateInformation(juce::MemoryBlock& destData) override {}
    void setStateInformation(const void* data, int sizeInBytes) override {}

    // Константи для FFT
    static constexpr int fftOrder = 11; // 2048 семплів

    static constexpr int fftSize = 1 << fftOrder;
    

    // // Мають повертати посилання на правильні std::array:
    std::array<float, fftSize>& getLeftFFTData() { return leftFFTData; }
    std::array<float, fftSize>& getRightFFTData() { return rightFFTData; }
    std::array<float, fftSize / 2>& getPhaseData() { return phaseData; }
    // Геттери для GUI компонентів
    const std::array<float, fftSize>& getLeftFFTData() const { return leftFFTData; }
    const std::array<float, fftSize>& getRightFFTData() const { return rightFFTData; }
    const std::array<float, fftSize / 2>& getPhaseData() const { return phaseData; }
    const juce::AudioSampleBuffer& getWaveformBuffer() const { return waveformRingBuffer; }
    float getPhaseCorrelation() const { return phaseCorrelation.load(); }

private:
    // FFT інструменти
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;


    // ... твої інші інструменти
    /*std::array<std::complex<float>, AudioVisualizerAudioProcessor::fftSize> complexLeft{};
    std::array<std::complex<float>, AudioVisualizerAudioProcessor::fftSize> complexRight{};*/

    // Класичні масиви для надійного FFT
    std::array<float, AudioVisualizerAudioProcessor::fftSize * 2> fftDataL{};
    std::array<float, AudioVisualizerAudioProcessor::fftSize * 2> fftDataR{};

    // Буфери для накопичення даних спектру
    std::array<float, fftSize> fftBufferL{};
    std::array<float, fftSize> fftBufferR{};

    // Результати, які забирає GUI
    std::array<float, fftSize> leftFFTData{};
    std::array<float, fftSize> rightFFTData{};
    std::array<float, fftSize / 2> phaseData{};
    
    int fifoIndex = 0;

    // Кільцевий буфер для Осцилографа та Гоніометра (на 4096 семплів)
    juce::AudioSampleBuffer waveformRingBuffer;
    int ringBufferWriteIndex = 0;

    // Атомік для моно-сумісності
    std::atomic<float> phaseCorrelation{ 1.0f };

    void pushToFFT(float sampleL, float sampleR);
    void storeInRingBuffer(float sampleL, float sampleR);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioVisualizerAudioProcessor)
};