#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioVisualizerAudioProcessor::AudioVisualizerAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    forwardFFT(fftOrder),
    window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    waveformRingBuffer.setSize(2, 4096);
    waveformRingBuffer.clear();
}

AudioVisualizerAudioProcessor::~AudioVisualizerAudioProcessor() {}

void AudioVisualizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    fifoIndex = 0;
    ringBufferWriteIndex = 0;
    waveformRingBuffer.clear();
}

void AudioVisualizerAudioProcessor::releaseResources() {}

void AudioVisualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto* channelL = buffer.getReadPointer(0);
    auto* channelR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : channelL;

    // 1. Наповнення FIFO буферів аналізу
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        pushToFFT(channelL[i], channelR[i]);
        storeInRingBuffer(channelL[i], channelR[i]);
    }

    // 2. Розрахунок кореляції фаз (Моно-сумісність)
    float dotProduct = 0.0f, leftEnergy = 0.0f, rightEnergy = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        dotProduct += channelL[i] * channelR[i];
        leftEnergy += channelL[i] * channelL[i];
        rightEnergy += channelR[i] * channelR[i];
    }

    if (leftEnergy > 0.0f && rightEnergy > 0.0f)
    {
        float correlation = dotProduct / std::sqrt(leftEnergy * rightEnergy);
        phaseCorrelation.store(phaseCorrelation.load() * 0.92f + correlation * 0.08f); // Згладжування
    }
}

void AudioVisualizerAudioProcessor::pushToFFT(float sampleL, float sampleR)
{
    fftBufferL[(size_t)fifoIndex] = sampleL;
    fftBufferR[(size_t)fifoIndex] = sampleR;
    fifoIndex++;

    if (fifoIndex == fftSize)
    {
        // Застосування вікна Ханна
        window.multiplyWithWindowingTable(fftBufferL.data(), fftSize);
        window.multiplyWithWindowingTable(fftBufferR.data(), fftSize);

        // Комплексне FFT для отримання фази та амплітуди
        std::vector<std::complex<float>> complexLeft(fftSize * 2, 0.0f);
        std::vector<std::complex<float>> complexRight(fftSize * 2, 0.0f);

        for (size_t i = 0; i < fftSize; ++i)
        {
            complexLeft[i] = { fftBufferL[i], 0.0f };
            complexRight[i] = { fftBufferR[i], 0.0f };
        }

        
        forwardFFT.perform(complexLeft.data(), complexLeft.data(), false);
        forwardFFT.perform(complexRight.data(), complexRight.data(), false);


        // Запис амплітуди та різниці фаз
        for (size_t i = 0; i < fftSize / 2; ++i)
        {
            
            // 1. Отримуємо чисту лінійну амплітуду та нормалізуємо її
            float magL = std::abs(complexLeft[i]) / (float)fftSize;
            float magR = std::abs(complexRight[i]) / (float)fftSize;

            // 2. ПЕРЕВОДИМО В ДЕЦИБЕЛИ (щоб функція mapDBToY у редакторі зрозуміла значення)
            // -100.0f — це мінімальний поріг тиші (дном екрана)
            leftFFTData[i] = juce::Decibels::gainToDecibels(magL, -100.0f);
            rightFFTData[i] = juce::Decibels::gainToDecibels(magR, -100.0f);
           
            float phaseL = std::arg(complexLeft[i]);
            float phaseR = std::arg(complexRight[i]);

            float phaseDiff = phaseL - phaseR;
            if (phaseDiff > juce::MathConstants<float>::pi)  phaseDiff -= juce::MathConstants<float>::twoPi;
            if (phaseDiff < -juce::MathConstants<float>::pi) phaseDiff += juce::MathConstants<float>::twoPi;

            
            float currentPhaseSample = phaseDiff / juce::MathConstants<float>::pi; // [-1.0, 1.0]

            // 3. ІНЕРЦІЯ (ЗГЛАДЖУВАННЯ):
            // Замість того, щоб миттєво змінювати значення, плавно підмішуємо його.
            // 0.85f - коефіцієнт плавности. Чим він більший, тим лінія стабільніша.
            phaseData[i] = phaseData[i] * 0.85f
                + currentPhaseSample * 0.15f;
        }
        // Заповнюємо решту масивів спектра нулями (на всякий випадок, бо їхній розмір fftSize)
        for (size_t i = fftSize / 2; i < fftSize; ++i)
        {
            leftFFTData[i] = -100.0f;
            rightFFTData[i] = -100.0f;
        }
        fifoIndex = 0;
    }
}

void AudioVisualizerAudioProcessor::storeInRingBuffer(float sampleL, float sampleR)
{
    waveformRingBuffer.setSample(0, ringBufferWriteIndex, sampleL);
    waveformRingBuffer.setSample(1, ringBufferWriteIndex, sampleR);

    ringBufferWriteIndex = (ringBufferWriteIndex + 1) % waveformRingBuffer.getNumSamples();
}

juce::AudioProcessorEditor* AudioVisualizerAudioProcessor::createEditor()
{
    return new AudioVisualizerAudioProcessorEditor(*this);
    //return new juce::GenericAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioVisualizerAudioProcessor();
}