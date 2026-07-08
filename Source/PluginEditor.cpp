#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==========================================
// РЕАЛІЗАЦІЯ ЕКРАНА 1 (SPECTRUM)
// ==========================================
SpectrumScreen::SpectrumScreen(AudioVisualizerAudioProcessor& p) : processor(p) { startTimerHz(60); }
void SpectrumScreen::setFreeze(bool shouldFreeze) { isFrozen = shouldFreeze; }

float SpectrumScreen::mapFreqToX(float freq) const {
    auto logMin = std::log10(minFreq); auto logMax = std::log10(maxFreq);
    return getWidth() * (std::log10(std::clamp(freq, minFreq, maxFreq)) - logMin) / (logMax - logMin);
}
float SpectrumScreen::mapDBToY(float db) const {
    return juce::jmap(std::clamp(db, minDB, maxDB), minDB, maxDB, (float)getHeight() - 16.0f, 0.0f);
}

juce::Path SpectrumScreen::createSpectrumPath(const std::array<float,
    AudioVisualizerAudioProcessor::fftSize>& fftData) {
    juce::Path p;
    bool first = true; 
    auto numBins = AudioVisualizerAudioProcessor::fftSize / 2;
    for (int i = 1; i < numBins; ++i) {
        float freq = (float)i * 44100.0f / (float)AudioVisualizerAudioProcessor::fftSize;
        if (freq < minFreq) continue; 
        if (freq > maxFreq) break;
        //float db = juce::Decibels::gainToDecibels(fftData[(size_t)i], minDB);
        // Замість твого рядка 24 встав цей чистий рядок:
        float db = fftData[(size_t)i];

        //// ТИМЧАСОВИЙ ТЕСТ: якщо індекс біна посередині, штучно піднімемо лінію вгору
        //if (i > numBins / 3 && i < (numBins / 3) * 2)
        //{
        //    db = -20.0f; // замість тиші робимо гучний сигнал на середніх частотах
        //}

        float x = mapFreqToX(freq);
        float y = mapDBToY(db);
        if (first) 
        { p.startNewSubPath(x, y); 
        first = false; }
        else { p.lineTo(x, y); }
    }
    return p;
}

void SpectrumScreen::drawFrequencyGrid(juce::Graphics& g) {
    g.setColour(juce::Colours::darkgrey.withAlpha(0.25f));
    std::vector<float> lines = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (auto f : lines) {
        float x = mapFreqToX(f); g.drawVerticalLine(juce::roundToInt(x), 0.0f, (float)getHeight() - 16.0f);
        g.setColour(juce::Colours::lightgrey.withAlpha(0.4f)); g.setFont(10.0f);
        g.drawText(f >= 1000 ? juce::String(f / 1000) + "kHz" : juce::String(f) + "Hz", juce::roundToInt(x) + 3, getHeight() - 28, 50, 12, juce::Justification::left);
        g.setColour(juce::Colours::darkgrey.withAlpha(0.25f));
    }
}

void SpectrumScreen::drawPhaseCurve(juce::Graphics& g) {
    //auto& phaseData = processor.getPhaseData();
    auto& phaseData = frozenPhase;
    float centerY = (getHeight() - 16.0f) * 0.5f; // Центр екрана по вертикалі

    // Тонка фонова лінія «нульового» фазового зсуву (моно)
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawHorizontalLine(juce::roundToInt(centerY), 0.0f, (float)getWidth());

    juce::Path p;
    bool first = true;

    for (int i = 1; i < AudioVisualizerAudioProcessor::fftSize / 2; ++i) {
        float freq = (float)i * 44100.0f / (float)AudioVisualizerAudioProcessor::fftSize;
        if (freq < minFreq) continue;
        if (freq > maxFreq) break;

        // 1. Беремо чисте значення фази [-1.0, 1.0]
        float phaseValue = phaseData[(size_t)i];

        // 2. Координата X залишається логарифмічною, як і для спектра
        float x = mapFreqToX(freq);

        // 3. ПРАВИЛЬНИЙ РОЗРАХУНОК Y ДЛЯ ФАЗИ:
        
        float maxDeviation = 60.0f; // Висота розмаху лінії в пікселях (налаштуй під себе)
        float y = centerY - (phaseValue * maxDeviation);

        if (first) {
            p.startNewSubPath(x, y);
            first = false;
        }
        else {
            p.lineTo(x, y);
        }
    }

    // Малюємо червону фазову лінію
    g.setColour(juce::Colours::red.withAlpha(0.75f));
    g.strokePath(p, juce::PathStrokeType(1.5f));
}


void SpectrumScreen::drawResonances(juce::Graphics& g, const std::array<float, AudioVisualizerAudioProcessor::fftSize>& fftData) {
    struct Peak { float freq, db; }; std::vector<Peak> peaks; const int range = 8;
    for (int i = range; i < AudioVisualizerAudioProcessor::fftSize / 2 - range; ++i) {
        float freq = (float)i * 44100.0f / (float)AudioVisualizerAudioProcessor::fftSize;
        if (freq < minFreq || freq > maxFreq) continue;
        float curDB = juce::Decibels::gainToDecibels(fftData[(size_t)i], minDB);
        if (curDB < -45.0f) continue;
        bool isPeak = true; float avg = 0.0f;
        for (int n = -range; n <= range; ++n) {
            if (n == 0) continue; float nDB = juce::Decibels::gainToDecibels(fftData[(size_t)(i + n)], minDB);
            avg += nDB; if (nDB >= curDB) { isPeak = false; break; }
        }
        if (isPeak && (curDB - (avg / (range * 2)) > 7.5f)) peaks.push_back({ freq, curDB });
    }
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) { return a.db > b.db; });
    int drawn = 0; g.setColour(juce::Colours::red.withAlpha(0.8f));
    for (const auto& p : peaks) {
        if (drawn++ >= 4) break; float x = mapFreqToX(p.freq); float y = mapDBToY(p.db);
        g.drawEllipse(x - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.2f); g.setColour(juce::Colours::white);
        g.fillEllipse(x - 1.5f, y - 1.5f, 3.0f, 3.0f); g.setColour(juce::Colours::red.withAlpha(0.8f));
    }
}

void SpectrumScreen::drawCompressionLines(juce::Graphics& g) {
    float lP = 0, lS = 0; int lC = 0; float mP = 0, mS = 0; int mC = 0; float hP = 0, hS = 0; int hC = 0;
    for (int i = 1; i < AudioVisualizerAudioProcessor::fftSize / 2; ++i) {
        float f = (float)i * 44100.0f / (float)AudioVisualizerAudioProcessor::fftSize;
        float amp = (frozenLeftFFT[(size_t)i] + frozenRightFFT[(size_t)i]) * 0.5f;
        if (f < 250.0f) { lP = std::max(lP, amp); lS += amp * amp; lC++; }
        else if (f < 4000.0f) { mP = std::max(mP, amp); mS += amp * amp; mC++; }
        else if (f <= 20000.0f) { hP = std::max(hP, amp); hS += amp * amp; hC++; }
    }
    float lCrest = juce::Decibels::gainToDecibels(lP) - juce::Decibels::gainToDecibels(std::sqrt(lS / (lC + 1e-5f)) + 1e-5f);
    float mCrest = juce::Decibels::gainToDecibels(mP) - juce::Decibels::gainToDecibels(std::sqrt(mS / (mC + 1e-5f)) + 1e-5f);
    float hCrest = juce::Decibels::gainToDecibels(hP) - juce::Decibels::gainToDecibels(std::sqrt(hS / (hC + 1e-5f)) + 1e-5f);

    auto getDrop = [](float c) { return c >= 10.0f ? 0.0f : juce::jmap(c, 3.0f, 10.0f, 38.0f, 0.0f); };
    float lD = getDrop(lCrest), mD = getDrop(mCrest), hD = getDrop(hCrest);

    juce::Path p; bool first = true;
    for (int i = 1; i < AudioVisualizerAudioProcessor::fftSize / 2; ++i) {
        float f = (float)i * 44100.0f / (float)AudioVisualizerAudioProcessor::fftSize;
        if (f < minFreq) continue; if (f > maxFreq) break;
        float x = mapFreqToX(f); float drop = (f < 250.0f) ? lD : ((f < 4000.0f) ? mD : hD);
        if (first) { p.startNewSubPath(x, 2.0f + drop); first = false; }
        else { p.lineTo(x, 2.0f + drop); }
    }
    juce::Path floor = p; floor.lineTo((float)getWidth(), 2.0f); floor.lineTo(0.0f, 2.0f); floor.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colours::orange.withAlpha(0.12f), 0, 0, juce::Colours::transparentBlack, 0, 40, false));
    g.fillPath(floor); g.setColour(juce::Colours::orange.withAlpha(0.8f)); g.strokePath(p, juce::PathStrokeType(1.5f));
}

void SpectrumScreen::drawTargetPinkNoiseCurve(juce::Graphics& g) {
    juce::Path p; bool first = true;
    for (float f = minFreq; f <= maxFreq; f *= 1.15f) {
        float x = mapFreqToX(f); float targetDB = -19.0f - (3.0f * std::log2(f / minFreq)); float y = mapDBToY(targetDB);
        if (first) { p.startNewSubPath(x, y); first = false; }
        else { p.lineTo(x, y); }
    }
    
    g.setColour(juce::Colours::white.withAlpha(0.2f));

    // Створюємо тип обводки і задаємо їй пунктир через вбудований метод
    juce::PathStrokeType stroke(1.0f);
    float dashedLengths[] = { 4.0f, 4.0f };
    stroke.createDashedStroke(p, p, dashedLengths, 2);

    // Тепер просто малюємо вже готовий змінений шлях p
    g.fillPath(p);
}

void SpectrumScreen::drawCorrelationMeter(juce::Graphics& g) {
    auto b = getLocalBounds().removeFromBottom(16); int w = 220, h = 6; int x = (getWidth() - w) / 2, y = b.getY() + 4;
    g.setColour(juce::Colours::darkgrey.withAlpha(0.25f)); g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 2.0f);
    g.setColour(juce::Colours::lightgrey.withAlpha(0.3f)); g.drawVerticalLine(x + w / 2, (float)y, (float)(y + h));
    float c = processor.getPhaseCorrelation();
    g.setColour(c >= 0.0f ? juce::Colours::blue.withAlpha(0.75f) : juce::Colours::red.withAlpha(0.75f));
    g.fillRoundedRectangle(x + w * 0.5f, (float)y, c * (w * 0.5f), (float)h, 1.5f);
}


void SpectrumScreen::paint(juce::Graphics& g) {
    // 1. Малюємо чорний фон та сітку
    g.fillAll(juce::Colours::black.withAlpha(0.95f));
    drawFrequencyGrid(g);
    drawTargetPinkNoiseCurve(g); // Твоя біла пунктирна лінія рожевого шуму

    // 2. Отримуємо живі дані спектра (які тепер у децибелах!)
    if (!isFrozen) {
        frozenLeftFFT = processor.getLeftFFTData();
        frozenRightFFT = processor.getRightFFTData();
        frozenPhase = processor.getPhaseData();
    }

    // Переводимо масиви децибел у графічні лінії (Paths)
    juce::Path lp = createSpectrumPath(frozenLeftFFT);
    juce::Path rp = createSpectrumPath(frozenRightFFT);

    // 3. МАЛЮЄМО КЛАСИЧНИЙ АУДІО-СПЕКТР
    // Лівий канал — Блакитний (Cyan)
    g.setColour(juce::Colours::cyan.withAlpha(0.85f));
    g.strokePath(lp, juce::PathStrokeType(1.4f));

    // Правий канал — Жовтий (Yellow)
    g.setColour(juce::Colours::yellow.withAlpha(0.85f));
    g.strokePath(rp, juce::PathStrokeType(1.4f));

    // 4. НАКЛАДАЄМО ІНШІ ІНДИКАТОРИ ПОВЕРХ
    drawPhaseCurve(g);        // Малюємо нашу тонку червону лінію фази
    drawCompressionLines(g);
    drawCorrelationMeter(g);
}

// ==========================================
// РЕАЛІЗАЦІЯ ЕКРАНА 2 (WAVEFORM)
// ==========================================
WaveformScreen::WaveformScreen(AudioVisualizerAudioProcessor& p) : processor(p) { startTimerHz(60); localBufferL.resize(512, 0); localBufferR.resize(512, 0); }
void WaveformScreen::setFreeze(bool shouldFreeze) { isFrozen = shouldFreeze; }
void WaveformScreen::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.95f)); float midY = getHeight() * 0.5f;
    g.setColour(juce::Colours::darkgrey.withAlpha(0.2f)); g.drawHorizontalLine(juce::roundToInt(midY), 0.0f, (float)getWidth());
    const int numSamples = 512;
    if (!isFrozen) {
        auto& rb = processor.getWaveformBuffer(); int len = rb.getNumSamples();
        auto* rL = rb.getReadPointer(0); auto* rR = rb.getNumChannels() > 1 ? rb.getReadPointer(1) : rL;
        int tIdx = 0;
        for (int i = 0; i < len - numSamples; ++i) { if (rL[i] < 0.0f && rL[i + 1] >= 0.0f) { tIdx = i; break; } }
        for (int i = 0; i < numSamples; ++i) { int idx = (tIdx + i) % len; localBufferL[i] = rL[idx]; localBufferR[i] = rR[idx]; }
    }
    juce::Path pL, pR; bool first = true;
    for (int i = 0; i < numSamples; ++i) {
        float x = juce::jmap((float)i, 0.0f, (float)numSamples, 0.0f, (float)getWidth());
        float yL = midY - (localBufferL[i] * midY * 0.88f), yR = midY - (localBufferR[i] * midY * 0.88f);
        if (first) { pL.startNewSubPath(x, yL); pR.startNewSubPath(x, yR); first = false; }
        else { pL.lineTo(x, yL); pR.lineTo(x, yR); }
    }
    g.setColour(juce::Colours::green.withAlpha(0.7f)); g.strokePath(pL, juce::PathStrokeType(1.8f));
    g.setColour(juce::Colours::purple.withAlpha(0.7f)); g.strokePath(pR, juce::PathStrokeType(1.8f));
}

// ==========================================
// РЕАЛІЗАЦІЯ ЕКРАНА 3 (STEREO / GONIOMETER)
// ==========================================
StereoScreen::StereoScreen(AudioVisualizerAudioProcessor& p) : processor(p) { startTimerHz(60); }
void StereoScreen::setFreeze(bool shouldFreeze) { isFrozen = shouldFreeze; }
void StereoScreen::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.95f));
    float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f, r = std::min(cx, cy) * 0.88f;
    g.setColour(juce::Colours::darkgrey.withAlpha(0.2f)); g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.0f);
    g.drawVerticalLine(juce::roundToInt(cx), cy - r, cy + r); g.drawHorizontalLine(juce::roundToInt(cy), cx - r, cx + r);
    if (!isFrozen) {
        frozenGoniometerPath.clear(); auto& rb = processor.getWaveformBuffer();
        auto* rL = rb.getReadPointer(0); auto* rR = rb.getNumChannels() > 1 ? rb.getReadPointer(1) : rL;
        int head = rb.getNumSamples() - 257; if (head < 0) head = 0; bool first = true;
        for (int i = 0; i < 256; i += 2) {
            float mid = (rL[head + i] + rR[head + i]) * 0.7071f, side = (rL[head + i] - rR[head + i]) * 0.7071f;
            float x = cx + (side * r), y = cy - (mid * r), d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            if (d > r) { x = cx + (x - cx) * (r / d); y = cy + (y - cy) * (r / d); }
            if (first) { frozenGoniometerPath.startNewSubPath(x, y); first = false; }
            else { frozenGoniometerPath.lineTo(x, y); }
        }
    }
    g.setColour(juce::Colours::cyan.withAlpha(0.12f)); g.strokePath(frozenGoniometerPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::JointStyle::curved));
    g.setColour(juce::Colours::blue.withAlpha(0.8f)); g.strokePath(frozenGoniometerPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::JointStyle::curved));
}

// ==========================================
// РЕАЛІЗАЦІЯ ГОЛОВНОГО EDITOR
// ==========================================
AudioVisualizerAudioProcessorEditor::AudioVisualizerAudioProcessorEditor(AudioVisualizerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), spectrumView(p), waveformView(p), stereoView(p)
{
    addAndMakeVisible(spectrumView); addAndMakeVisible(waveformView); addAndMakeVisible(stereoView);

    juce::TextButton* buttons[] = { &freezeSpectrumButton, &freezeWaveformButton, &freezeStereoButton, &freezeAllButton };
    for (auto* b : buttons) {
        addAndMakeVisible(b); b->setClickingTogglesState(true);
        b->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red.withAlpha(0.8f));
        b->setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.withAlpha(0.4f));
    }
    freezeAllButton.setColour(juce::TextButton::textColourOffId, juce::Colours::orange);

    // Логіка Master/Slave
    freezeSpectrumButton.onClick = [this]() {
        bool s = freezeSpectrumButton.getToggleState(); spectrumView.setFreeze(s);
        if (!s) freezeAllButton.setToggleState(false, juce::NotificationType::dontSendNotification); else checkIfAllAreFrozen();
        };
    freezeWaveformButton.onClick = [this]() {
        bool s = freezeWaveformButton.getToggleState(); waveformView.setFreeze(s);
        if (!s) freezeAllButton.setToggleState(false, juce::NotificationType::dontSendNotification); else checkIfAllAreFrozen();
        };
    freezeStereoButton.onClick = [this]() {
        bool s = freezeStereoButton.getToggleState(); stereoView.setFreeze(s);
        if (!s) freezeAllButton.setToggleState(false, juce::NotificationType::dontSendNotification); else checkIfAllAreFrozen();
        };
    freezeAllButton.onClick = [this]() {
        bool s = freezeAllButton.getToggleState();
        freezeSpectrumButton.setToggleState(s, juce::NotificationType::dontSendNotification);
        freezeWaveformButton.setToggleState(s, juce::NotificationType::dontSendNotification);
        freezeStereoButton.setToggleState(s, juce::NotificationType::dontSendNotification);
        spectrumView.setFreeze(s); waveformView.setFreeze(s); stereoView.setFreeze(s);
        };

    setSize(850, 540);
    setResizable(true, true);
    setResizeLimits(400, 300, 900, 600);
}

AudioVisualizerAudioProcessorEditor::~AudioVisualizerAudioProcessorEditor() {}
void AudioVisualizerAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::black); }

void AudioVisualizerAudioProcessorEditor::checkIfAllAreFrozen() {
    if (freezeSpectrumButton.getToggleState() && freezeWaveformButton.getToggleState() && freezeStereoButton.getToggleState())
        freezeAllButton.setToggleState(true, juce::NotificationType::dontSendNotification);
}

void AudioVisualizerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto topBar = bounds.removeFromTop(30);

    freezeAllButton.setBounds(topBar.removeFromLeft(105).reduced(2));
    freezeStereoButton.setBounds(topBar.removeFromRight(80).reduced(2));
    freezeWaveformButton.setBounds(topBar.removeFromRight(80).reduced(2));
    freezeSpectrumButton.setBounds(topBar.removeFromRight(80).reduced(2));

    spectrumView.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.56));
    auto halfW = bounds.getWidth() / 2;
    waveformView.setBounds(bounds.removeFromLeft(halfW));
    stereoView.setBounds(bounds);
}