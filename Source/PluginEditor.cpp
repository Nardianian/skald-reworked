#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
// MusicKnob implementation (inspired by mx-knob)
void MusicKnob::paint(juce::Graphics& g)
{
    if (!knobSprite.isValid())
        return;

    auto bounds = getLocalBounds().toFloat();

    // Sprite is 80 x (80 * numFrames) - vertical strip of knob rotation frames
    // Simple Gray sprite: 80x8080 = 101 frames of 80x80 each
    const int spriteFrameSize = 80;

    // Calculate which frame to display based on slider value
    double normalizedValue = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
    int frameIndex = static_cast<int>(normalizedValue * (spriteFrameCount - 1));
    frameIndex = juce::jlimit(0, spriteFrameCount - 1, frameIndex);

    // Calculate source Y position for this frame
    int sourceY = frameIndex * spriteFrameSize;

    // Scale the knob to fit the component bounds (with a small margin)
    auto margin = 4.0f;
    auto destSize = juce::jmin(bounds.getWidth(), bounds.getHeight()) - (margin * 2);
    auto destX = bounds.getCentreX() - destSize / 2.0f;
    auto destY = bounds.getCentreY() - destSize / 2.0f;

    // Draw the appropriate frame from the sprite, scaled to fit
    g.drawImage(knobSprite,
                destX, destY, destSize, destSize,              // destination (scaled to fit)
                0, sourceY, spriteFrameSize, spriteFrameSize); // source (80x80 frame)
}

//==============================================================================
// MusicToggle implementation (inspired by mx-switch)
void MusicToggle::paint(juce::Graphics& g)
{
    if (!toggleSprite.isValid())
        return;

    auto bounds = getLocalBounds().toFloat();

    // Sprite is 56x112 (2 frames of 56x56 each, stacked vertically)
    // Top frame (0-56) = OFF state
    // Bottom frame (56-112) = ON state
    const int frameHeight = 56;
    const int frameWidth = 56;

    // Determine which frame to use based on toggle state
    int sourceY = getToggleState() ? frameHeight : 0;

    // Center the sprite in the component bounds
    auto destX = bounds.getCentreX() - frameWidth / 2.0f;
    auto destY = bounds.getCentreY() - frameHeight / 2.0f;

    // Draw the appropriate frame from the sprite
    // Source rectangle: which part of the sprite to draw (the specific frame)
    // Destination rectangle: where to draw it on the component
    g.drawImage(toggleSprite,
                destX, destY, frameWidth, frameHeight,  // destination
                0, sourceY, frameWidth, frameHeight);    // source
}

//==============================================================================
// HardwareButtonLookAndFeel implementations
void HardwareButtonLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                     const juce::Colour& backgroundColour,
                                                     bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    auto baseColour = backgroundColour;

    if (isButtonDown)
    {
        // Pressed state - darker, inset look
        g.setColour(baseColour.darker(0.3f));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Inner shadow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 1.5f);
    }
    else
    {
        // Normal state - 3D raised button
        g.setGradientFill(juce::ColourGradient(
            baseColour.brighter(0.2f), bounds.getX(), bounds.getY(),
            baseColour.darker(0.2f), bounds.getX(), bounds.getBottom(),
            false));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Top edge highlight
        g.setColour(baseColour.brighter(0.4f).withAlpha(0.6f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 1.5f);

        // Bottom shadow
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(bounds.getX(), bounds.getBottom() - 3, bounds.getWidth(), 3, 4.0f);
        g.fillPath(shadowPath);
    }

    // Border
    g.setColour(baseColour.darker(0.6f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void HardwareButtonLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                               bool isMouseOverButton, bool isButtonDown)
{
    // No text or icons - just colored buttons
    (void)g;
    (void)button;
    (void)isMouseOverButton;
    (void)isButtonDown;
}

//==============================================================================
SkaldEditor::SkaldEditor (SkaldProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), hardwareLookAndFeel(this)
{
    setSize (1350, 850);  // Wider and taller for more room

    // Speed display (LED screen style - cyan to match turntable)
    speedDisplay.setText("1x", juce::dontSendNotification);
    speedDisplay.setJustificationType(juce::Justification::centred);
    speedDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0a0a0a));
    speedDisplay.setColour(juce::Label::textColourId, juce::Colour(0xff00d9ff));
    speedDisplay.setColour(juce::Label::outlineColourId, juce::Colour(0xff1a3a3a));
    speedDisplay.setFont(juce::FontOptions("Courier New", 14.0f, juce::Font::bold));
    addAndMakeVisible(speedDisplay);

    // Speed tap button (small, no text)
    speedTapButton.setButtonText("");
    speedTapButton.setLookAndFeel(&hardwareLookAndFeel);
    speedTapButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    speedTapButton.onClick = [this]()
    {
        const juce::String speedOptions[] = { "0.25x", "0.5x", "1x", "2x", "4x" };
        const float speeds[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };

        currentSpeedIndex = (currentSpeedIndex + 1) % 5;
        speedDisplay.setText(speedOptions[currentSpeedIndex], juce::dontSendNotification);
        audioProcessor.setSpeed(speeds[currentSpeedIndex]);
    };
    addAndMakeVisible(speedTapButton);

    speedLabel.setText("DIV", juce::dontSendNotification);
    speedLabel.setJustificationType(juce::Justification::centred);
    speedLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    speedLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(speedLabel);

    // Load icons from embedded binary data
    // TODO: Re-add icon files
    // addIcon = juce::ImageCache::getFromMemory(BinaryData::add_png,
    //                                           BinaryData::add_pngSize);
    // clearIcon = juce::ImageCache::getFromMemory(BinaryData::clear_png,
    //                                             BinaryData::clear_pngSize);

    // Load toggle switch sprite (2-frame vertical sprite: 56x56 per frame)
    auto toggleSprite = juce::ImageCache::getFromMemory(BinaryData::switch_toggle_png,
                                                         BinaryData::switch_toggle_pngSize);

    // Load knob sprite (101-frame vertical sprite: 80x80 per frame, 80x8080 total)
    auto knobSprite = juce::ImageCache::getFromMemory(BinaryData::knob_simplegray_png,
                                                       BinaryData::knob_simplegray_pngSize);

    // Clear button (hardware style - red for destructive action)
    clearButton.setName("clearButton");
    clearButton.setButtonText("");
    clearButton.setLookAndFeel(&hardwareLookAndFeel);
    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    clearButton.onClick = [this]()
    {
        audioProcessor.clearAllDots();
    };
    addAndMakeVisible(clearButton);

    clearLabel.setText("CLR", juce::dontSendNotification);
    clearLabel.setJustificationType(juce::Justification::centred);
    clearLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    clearLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(clearLabel);

    // Add dot button (hardware style - cyan for adding/creating)
    addDotButton.setName("addButton");
    addDotButton.setButtonText("");
    addDotButton.setLookAndFeel(&hardwareLookAndFeel);
    addDotButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    addDotButton.onClick = [this]()
    {
        juce::Random random;
        float angle = random.nextFloat() * 360.0f;
        int numRings = audioProcessor.getNumRings();
        int ringIndex = random.nextInt(juce::jmax(1, numRings)); // Random ring based on scale

        // Color based on ring
        juce::Colour color = juce::Colour(0xffff6b35);

        audioProcessor.addDot(angle, ringIndex, color);
    };
    addAndMakeVisible(addDotButton);

    addLabel.setText("ADD", juce::dontSendNotification);
    addLabel.setJustificationType(juce::Justification::centred);
    addLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    addLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(addLabel);

    // Randomize button (hardware style - orange for randomizing/shuffle)
    randomizeButton.setName("randomizeButton");
    randomizeButton.setButtonText("");
    randomizeButton.setLookAndFeel(&hardwareLookAndFeel);
    randomizeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    randomizeButton.onClick = [this]()
    {
        juce::Random random;
        audioProcessor.clearAllDots();

        int numRings = audioProcessor.getNumRings();
        int numDots = random.nextInt(8) + 4; // 4-11 dots

        for (int i = 0; i < numDots; ++i)
        {
            float angle = random.nextFloat() * 360.0f;
            int ringIndex = random.nextInt(juce::jmax(1, numRings));
            juce::Colour color = juce::Colour(0xffff6b35);

            audioProcessor.addDot(angle, ringIndex, color);
        }
    };
    addAndMakeVisible(randomizeButton);

    randomizeLabel.setText("RAND", juce::dontSendNotification);
    randomizeLabel.setJustificationType(juce::Justification::centred);
    randomizeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    randomizeLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(randomizeLabel);

    // Play/Stop button (big vintage transport button) - Only show in Standalone
    playStopButton.setButtonText("PLAY");
    playStopButton.setClickingTogglesState(true);
    playStopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3a2a));
    playStopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4a2a2a));
    playStopButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff88cc88));
    playStopButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffff6b35));
    playStopButton.onClick = [this]()
    {
        bool isPlaying = playStopButton.getToggleState();
        audioProcessor.setPlaying(isPlaying);
        playStopButton.setButtonText(isPlaying ? "STOP" : "PLAY");
    };

    // Only show in standalone mode - will set visibility in resized()
    addAndMakeVisible(playStopButton);
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(bpmSlider);

    // BPM slider (horizontal vintage style) - Only in Standalone
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setRange(60.0, 200.0, 1.0);
    bpmSlider.setValue(120.0);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    bpmSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a3a3a));
    bpmSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff6b35));
    bpmSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffddbb));
    bpmSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1a1410));
    bpmSlider.onValueChange = [this]()
    {
        audioProcessor.setStandaloneBPM(bpmSlider.getValue());
    };

    bpmLabel.setText("TEMPO", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    bpmLabel.setFont(juce::FontOptions("Arial", 11.0f, juce::Font::bold));

    // Velocity knob
    velocityKnob.setRange(1.0, 127.0, 1.0);
    velocityKnob.setValue(100.0);
    velocityKnob.onValueChange = [this]() {
        audioProcessor.setGlobalVelocity(static_cast<int>(velocityKnob.getValue()));
    };
    velocityKnob.setSpriteImage(knobSprite, 101);
    addAndMakeVisible(velocityKnob);

    velocityLabel.setText("VEL", juce::dontSendNotification);
    velocityLabel.setJustificationType(juce::Justification::centred);
    velocityLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    velocityLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(velocityLabel);

    // Gate time knob
    gateTimeKnob.setRange(10.0, 2000.0, 1.0);
    gateTimeKnob.setValue(100.0);
    gateTimeKnob.setSkewFactorFromMidPoint(250.0); // Make lower values easier to adjust
    gateTimeKnob.onValueChange = [this]() {
        audioProcessor.setGateTime(static_cast<float>(gateTimeKnob.getValue()));
    };
    gateTimeKnob.setSpriteImage(knobSprite, 101);
    addAndMakeVisible(gateTimeKnob);

    gateTimeLabel.setText("GATE", juce::dontSendNotification);
    gateTimeLabel.setJustificationType(juce::Justification::centred);
    gateTimeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    gateTimeLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(gateTimeLabel);

    // Reverse toggle
    reverseToggle.onClick = [this]() {
        audioProcessor.setReverse(reverseToggle.getToggleState());
    };
    reverseToggle.setSpriteImage(toggleSprite);
    addAndMakeVisible(reverseToggle);

    reverseLabel.setText("REV", juce::dontSendNotification);
    reverseLabel.setJustificationType(juce::Justification::centred);
    reverseLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    reverseLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(reverseLabel);

    // Start/Stop toggle (record player style)
    startStopToggle.onClick = [this]() {
        audioProcessor.setMotorRunning(startStopToggle.getToggleState());
    };
    startStopToggle.setToggleState(true, juce::dontSendNotification);  // Default to running
    startStopToggle.setSpriteImage(toggleSprite);
    addAndMakeVisible(startStopToggle);

    startStopLabel.setText("MOTOR", juce::dontSendNotification);
    startStopLabel.setJustificationType(juce::Justification::centred);
    startStopLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    startStopLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(startStopLabel);

    // Probability knob
    probabilityKnob.setRange(0.0, 100.0, 1.0);
    probabilityKnob.setValue(100.0);
    probabilityKnob.onValueChange = [this]() {
        audioProcessor.setProbability(static_cast<float>(probabilityKnob.getValue()));
    };
    probabilityKnob.setSpriteImage(knobSprite, 101);
    addAndMakeVisible(probabilityKnob);

    probabilityLabel.setText("PROB", juce::dontSendNotification);
    probabilityLabel.setJustificationType(juce::Justification::centred);
    probabilityLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    probabilityLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(probabilityLabel);

    // Velocity variation knob
    velocityVariationKnob.setRange(0.0, 100.0, 1.0);
    velocityVariationKnob.setValue(0.0);
    velocityVariationKnob.onValueChange = [this]() {
        audioProcessor.setVelocityVariation(static_cast<float>(velocityVariationKnob.getValue()));
    };
    velocityVariationKnob.setSpriteImage(knobSprite, 101);
    addAndMakeVisible(velocityVariationKnob);

    velocityVariationLabel.setText("VVAR", juce::dontSendNotification);
    velocityVariationLabel.setJustificationType(juce::Justification::centred);
    velocityVariationLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    velocityVariationLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(velocityVariationLabel);

    // Swing knob
    swingKnob.setRange(0.0, 100.0, 1.0);
    swingKnob.setValue(0.0);
    swingKnob.onValueChange = [this]() {
        audioProcessor.setSwing(static_cast<float>(swingKnob.getValue()));
    };
    swingKnob.setSpriteImage(knobSprite, 101);
    addAndMakeVisible(swingKnob);

    swingLabel.setText("SWING", juce::dontSendNotification);
    swingLabel.setJustificationType(juce::Justification::centred);
    swingLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    swingLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(swingLabel);

    // Save pattern button (green for safe save action)
    savePatternButton.setButtonText("");
    savePatternButton.setLookAndFeel(&hardwareLookAndFeel);
    savePatternButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    savePatternButton.onClick = [this]() {
        // Save pattern to file
        auto chooser = std::make_shared<juce::FileChooser>("Save Pattern", juce::File(), "*.ttp");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser&) {
                auto file = chooser->getResult();
                if (file != juce::File())
                {
                    juce::MemoryBlock data;
                    audioProcessor.getStateInformation(data);
                    file.replaceWithData(data.getData(), data.getSize());
                }
            });
    };
    addAndMakeVisible(savePatternButton);

    saveLabel.setText("SAVE", juce::dontSendNotification);
    saveLabel.setJustificationType(juce::Justification::centred);
    saveLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    saveLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(saveLabel);

    // Load pattern button (blue for loading action)
    loadPatternButton.setButtonText("");
    loadPatternButton.setLookAndFeel(&hardwareLookAndFeel);
    loadPatternButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    loadPatternButton.onClick = [this]() {
        // Load pattern from file
        auto chooser = std::make_shared<juce::FileChooser>("Load Pattern", juce::File(), "*.ttp");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser&) {
                auto file = chooser->getResult();
                if (file != juce::File())
                {
                    juce::MemoryBlock data;
                    if (file.loadFileAsData(data))
                    {
                        audioProcessor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));

                        // Update UI to reflect loaded values
                        velocityKnob.setValue(audioProcessor.getGlobalVelocity(), juce::dontSendNotification);
                        gateTimeKnob.setValue(audioProcessor.getGateTime(), juce::dontSendNotification);
                        reverseToggle.setToggleState(audioProcessor.getReverse(), juce::dontSendNotification);
                        probabilityKnob.setValue(audioProcessor.getProbability(), juce::dontSendNotification);
                        velocityVariationKnob.setValue(audioProcessor.getVelocityVariation(), juce::dontSendNotification);
                        swingKnob.setValue(audioProcessor.getSwing(), juce::dontSendNotification);

                        repaint();
                    }
                }
            });
    };
    addAndMakeVisible(loadPatternButton);

    loadLabel.setText("LOAD", juce::dontSendNotification);
    loadLabel.setJustificationType(juce::Justification::centred);
    loadLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    loadLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(loadLabel);

    // About button (shows help/about screen)
    aboutButton.setButtonText("");
    aboutButton.setLookAndFeel(&hardwareLookAndFeel);
    aboutButton.onClick = [this]()
    {
        showingHelpScreen = true;
        setControlsVisible(false);  // Hide controls when showing help
        backButton.setVisible(true);
        backLabel.setVisible(true);
        repaint();
    };
    addAndMakeVisible(aboutButton);

    aboutLabel.setText("HELP", juce::dontSendNotification);
    aboutLabel.setJustificationType(juce::Justification::centred);
    aboutLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    aboutLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(aboutLabel);

    // Back button (for help screen)
    backButton.setButtonText("");
    backButton.setLookAndFeel(&hardwareLookAndFeel);
    backButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));  // Match tap button gray
    backButton.onClick = [this]()
    {
        showingHelpScreen = false;
        setControlsVisible(true);  // Show controls when leaving help screen
        backButton.setVisible(false);
        backLabel.setVisible(false);
        repaint();
    };
    addAndMakeVisible(backButton);
    backButton.setVisible(false);  // Hidden by default

    backLabel.setText("BACK", juce::dontSendNotification);
    backLabel.setJustificationType(juce::Justification::centred);
    backLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    backLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(backLabel);
    backLabel.setVisible(false);  // Hidden by default

    // Scale display (LED screen style - cyan to match turntable)
    scaleDisplay.setText("Penta", juce::dontSendNotification);
    scaleDisplay.setJustificationType(juce::Justification::centred);
    scaleDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0a0a0a));
    scaleDisplay.setColour(juce::Label::textColourId, juce::Colour(0xff00d9ff));
    scaleDisplay.setColour(juce::Label::outlineColourId, juce::Colour(0xff1a3a3a));
    scaleDisplay.setFont(juce::FontOptions("Courier New", 14.0f, juce::Font::bold));
    addAndMakeVisible(scaleDisplay);

    // Scale tap button (small, no text)
    scaleTapButton.setButtonText("");
    scaleTapButton.setLookAndFeel(&hardwareLookAndFeel);
    scaleTapButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    scaleTapButton.onClick = [this]()
    {
        const juce::String scaleNames[] = { "Major", "Minor", "HarmM", "MelM", "Penta",
                                            "PentM", "Blues", "Doria", "Phryg", "Lydia",
                                            "Mixol", "Locri", "Chrom" };

        currentScaleIndex = (currentScaleIndex + 1) % 13;
        scaleDisplay.setText(scaleNames[currentScaleIndex], juce::dontSendNotification);
        audioProcessor.setScale(static_cast<ScaleType>(currentScaleIndex));
    };
    addAndMakeVisible(scaleTapButton);

    scaleLabel.setText("SCALE", juce::dontSendNotification);
    scaleLabel.setJustificationType(juce::Justification::centred);
    scaleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    scaleLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(scaleLabel);

    // Key display (LED screen style - cyan to match turntable)
    keyDisplay.setText("C", juce::dontSendNotification);
    keyDisplay.setJustificationType(juce::Justification::centred);
    keyDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0a0a0a));
    keyDisplay.setColour(juce::Label::textColourId, juce::Colour(0xff00d9ff));
    keyDisplay.setColour(juce::Label::outlineColourId, juce::Colour(0xff1a3a3a));
    keyDisplay.setFont(juce::FontOptions("Courier New", 14.0f, juce::Font::bold));
    addAndMakeVisible(keyDisplay);

    // Key tap button (small, no text)
    keyTapButton.setButtonText("");
    keyTapButton.setLookAndFeel(&hardwareLookAndFeel);
    keyTapButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    keyTapButton.onClick = [this]()
    {
        const juce::String keyNames[] = { "C", "C#", "D", "D#", "E", "F",
                                          "F#", "G", "G#", "A", "A#", "B" };

        currentKeyIndex = (currentKeyIndex + 1) % 12;
        keyDisplay.setText(keyNames[currentKeyIndex], juce::dontSendNotification);
        audioProcessor.setRootNote(currentKeyIndex);
    };
    addAndMakeVisible(keyTapButton);

    keyLabel.setText("KEY", juce::dontSendNotification);
    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    keyLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(keyLabel);

    // Octave display (LED screen style - cyan to match turntable)
    octaveDisplay.setText("0", juce::dontSendNotification);
    octaveDisplay.setJustificationType(juce::Justification::centred);
    octaveDisplay.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0a0a0a));
    octaveDisplay.setColour(juce::Label::textColourId, juce::Colour(0xff00d9ff));
    octaveDisplay.setColour(juce::Label::outlineColourId, juce::Colour(0xff1a3a3a));
    octaveDisplay.setFont(juce::FontOptions("Courier New", 14.0f, juce::Font::bold));
    addAndMakeVisible(octaveDisplay);

    // Octave tap button (small, no text)
    octaveTapButton.setButtonText("");
    octaveTapButton.setLookAndFeel(&hardwareLookAndFeel);
    octaveTapButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15253a));
    octaveTapButton.onClick = [this]()
    {
        const juce::String octaveLabels[] = { "-2", "-1", "0", "+1", "+2" };
        const int octaveShifts[] = { -2, -1, 0, +1, +2 };

        currentOctaveIndex = (currentOctaveIndex + 1) % 5;
        octaveDisplay.setText(octaveLabels[currentOctaveIndex], juce::dontSendNotification);
        audioProcessor.setOctaveShift(octaveShifts[currentOctaveIndex]);
    };
    addAndMakeVisible(octaveTapButton);

    octaveLabel.setText("OCT", juce::dontSendNotification);
    octaveLabel.setJustificationType(juce::Justification::centred);
    octaveLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    octaveLabel.setFont(juce::FontOptions("Arial", 9.0f, juce::Font::bold));
    addAndMakeVisible(octaveLabel);

    // Load Viking logo for help/about screen
    vikingFullImage = juce::ImageCache::getFromMemory(BinaryData::viking_full_png,
                                                       BinaryData::viking_full_pngSize);

    // Load custom fonts (all legally licensed for distribution)
    // Cinzel Bold - SIL OFL 1.1
    auto headerFontData = juce::Typeface::createSystemTypefaceFor(BinaryData::header_ttf,
                                                                   BinaryData::header_ttfSize);
    if (headerFontData)
        csArthemisFont = juce::FontOptions(headerFontData);

    // UnifrakturMaguntia - SIL OFL 1.1
    auto subHeaderFontData = juce::Typeface::createSystemTypefaceFor(BinaryData::subheader_ttf,
                                                                      BinaryData::subheader_ttfSize);
    if (subHeaderFontData)
        distropiaxFont = juce::FontOptions(subHeaderFontData);

    // IM Fell English - SIL OFL 1.1
    auto paragraphFontData = juce::Typeface::createSystemTypefaceFor(BinaryData::paragraph_ttf,
                                                                      BinaryData::paragraph_ttfSize);
    if (paragraphFontData)
        wonderworldFont = juce::FontOptions(paragraphFontData);

    // Load wallpaper background texture
    wallpaperImage = juce::ImageCache::getFromMemory(BinaryData::wallpaper_jpg,
                                                      BinaryData::wallpaper_jpgSize);

    // Scale down the wallpaper for proper tiling (original is 3000x3000, too large for 1:1 tiling)
    if (wallpaperImage.isValid())
    {
        const int tileSize = 1024;  // Scale to 1024x1024 to reduce visible tiling
        juce::Image scaledWallpaper(juce::Image::ARGB, tileSize, tileSize, true);
        juce::Graphics g(scaledWallpaper);
        g.drawImage(wallpaperImage, 0, 0, tileSize, tileSize,
                    0, 0, wallpaperImage.getWidth(), wallpaperImage.getHeight());
        wallpaperImage = scaledWallpaper;
    }

    versionLabel.setText("v1.1.0", juce::dontSendNotification);
    versionLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold)); 
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.6f)); 
    versionLabel.setJustificationType(juce::Justification::centredRight); 
    addAndMakeVisible(versionLabel); 

    algoTitleLabel.setText("ALGORITHMIC GEN", juce::dontSendNotification);
    algoTitleLabel.setJustificationType(juce::Justification::centred);
    algoTitleLabel.setFont(csArthemisFont.withHeight(18.0f));
    algoTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00d9ff)); // Cyan come i LED
    addAndMakeVisible(algoTitleLabel);

    ringSelector.setEditableText(false);
    ringSelector.setJustificationType(juce::Justification::centred);

    for (int i = 0; i < 12; ++i) {
        ringSelector.addItem("Ring " + juce::String(i + 1), i + 1);
    }

    ringSelector.setSelectedId(1); // Default on the first ring
    ringSelector.onChange = [this]() {
        selectedRingForAlgo = ringSelector.getSelectedId() - 1;
        };
    addAndMakeVisible(ringSelector);

    ringSelectorLabel.setText("TARGET RING", juce::dontSendNotification);
    addAndMakeVisible(ringSelectorLabel);

        // Slider Pulses & Steps (Euclidean Base)
    pulsesSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    pulsesSlider.setRange(1, 32, 1);
    pulsesSlider.setValue(audioProcessor.getLastPulses(), juce::dontSendNotification);
    pulsesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    addAndMakeVisible(pulsesSlider);
    pulsesLabel.setText("PULSES", juce::dontSendNotification);
    addAndMakeVisible(pulsesLabel);

    stepsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    stepsSlider.setRange(1, 32, 1);
    stepsSlider.setValue(audioProcessor.getLastSteps(), juce::dontSendNotification);
    stepsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    addAndMakeVisible(stepsSlider);
    stepsLabel.setText("STEPS", juce::dontSendNotification);
    addAndMakeVisible(stepsLabel);

    // Slider Depth & Shift
    depthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    depthSlider.setRange(1, 4, 1);
    depthSlider.setValue(audioProcessor.getLastDepth(), juce::dontSendNotification);
    depthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    addAndMakeVisible(depthSlider);
    depthLabel.setText("HYPER-DEPTH", juce::dontSendNotification);
    addAndMakeVisible(depthLabel);

    shiftSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    shiftSlider.setRange(0, 32, 1);
    shiftSlider.setValue(audioProcessor.getLastShift(), juce::dontSendNotification);
    shiftSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    addAndMakeVisible(shiftSlider);
    shiftLabel.setText("SHIFT", juce::dontSendNotification);
    addAndMakeVisible(shiftLabel);

    generateButton.setButtonText("GENERATE");
    generateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a3a3a));
    generateButton.onClick = [this]()
        {
            // Invio tutti i parametri alla funzione algoritmica nel processore
            audioProcessor.generateHyperPattern(selectedRingForAlgo,
                (int)pulsesSlider.getValue(),
                (int)stepsSlider.getValue(),
                (int)depthSlider.getValue(),
                (int)shiftSlider.getValue());
            repaint();
        };
    addAndMakeVisible(generateButton);

    // Start timer for animation (30 FPS)
    startTimerHz(30);

    setResizable(true, true);
    setResizeLimits(600, 450, 2000, 1500); 
}

SkaldEditor::~SkaldEditor()
{
    stopTimer();
}

//==============================================================================
void SkaldEditor::paint (juce::Graphics& g)
{
    // Background - textured wallpaper
    if (wallpaperImage.isValid())
    {
        // Tile the wallpaper to fill the background
        g.setTiledImageFill(wallpaperImage, 0, 0, 1.0f);
        g.fillAll();
    }
    else
    {
        // Fallback to gradient if wallpaper fails to load
        juce::ColourGradient bgGradient(
            juce::Colour(0xff1a1a1a), 0, 0,
            juce::Colour(0xff0d0d0d), 0, static_cast<float>(getHeight()),
            false
        );
        g.setGradientFill(bgGradient);
        g.fillAll();
    }

    // If showing help screen, paint it and return
    if (showingHelpScreen)
    {
        paintHelpScreen(g);
        return;
    }

    // Draw turntable
    turntableCenter = turntableArea.getCentre();

    // Main outer ring with modern gradient - thin outer ring
    juce::ColourGradient metalGradient(
        juce::Colour(0xff404040), turntableCenter.x - turntableRadius, turntableCenter.y - turntableRadius,
        juce::Colour(0xff202020), turntableCenter.x + turntableRadius, turntableCenter.y + turntableRadius,
        false
    );
    g.setGradientFill(metalGradient);
    g.fillEllipse(turntableCenter.x - turntableRadius,
                  turntableCenter.y - turntableRadius,
                  turntableRadius * 2.0f,
                  turntableRadius * 2.0f);

    // LED indicator ring (shows active dots)
    float ledRingRadius = turntableRadius * 0.96f;

    // Draw LED ring segments (like Orbita's LED circle)
    int numLEDs = 60;
    for (int i = 0; i < numLEDs; ++i)
    {
        float angle = (i * 360.0f / numLEDs - audioProcessor.getCurrentRotation()) *
                     juce::MathConstants<float>::pi / 180.0f;

        // Check if this LED is near a dot
        bool isActive = false;
        auto& dots = audioProcessor.getDots();
        for (const auto& dot : dots)
        {
            float ledAngleDeg = std::fmod(i * 360.0f / numLEDs + 360.0f, 360.0f);
            float dotAngleDeg = std::fmod(dot.angle + 360.0f, 360.0f);
            float diff = std::abs(ledAngleDeg - dotAngleDeg);
            if (diff > 180.0f) diff = 360.0f - diff;

            if (diff < 360.0f / numLEDs * 2)
            {
                isActive = true;
                break;
            }
        }

        auto ledPos = juce::Point<float>(
            turntableCenter.x + std::cos(angle) * ledRingRadius,
            turntableCenter.y + std::sin(angle) * ledRingRadius
        );

        if (isActive)
        {
            // Active LED with glow
            g.setColour(juce::Colour(0xff00d9ff).withAlpha(0.9f));
            g.fillEllipse(ledPos.x - 3, ledPos.y - 3, 6, 6);
            g.setColour(juce::Colour(0xff00d9ff).withAlpha(0.3f));
            g.fillEllipse(ledPos.x - 5, ledPos.y - 5, 10, 10);
        }
        else
        {
            // Inactive LED
            g.setColour(juce::Colour(0xff333333).withAlpha(0.4f));
            g.fillEllipse(ledPos.x - 1.5f, ledPos.y - 1.5f, 3, 3);
        }
    }

    // Inner surface - much larger, taking up more space
    float innerRadius = turntableRadius * 0.90f;

    // Draw artistic concentric ring tracks with organic variations
    // Alternating shades with subtle variations
    juce::Colour ringColors[] = {
        juce::Colour(0xff1a1a1a),  // Ring 0 - darker grey
        juce::Colour(0xff252525),  // Ring 1 - lighter grey
        juce::Colour(0xff1a1a1a),  // Ring 2 - darker grey
        juce::Colour(0xff252525),  // Ring 3 - lighter grey
        juce::Colour(0xff1a1a1a),  // Ring 4 - darker grey
        juce::Colour(0xff252525),  // Ring 5 - lighter grey
        juce::Colour(0xff1a1a1a),  // Ring 6 - darker grey
        juce::Colour(0xff252525),  // Ring 7 - lighter grey
        juce::Colour(0xff1a1a1a),  // Ring 8 - darker grey
        juce::Colour(0xff252525),  // Ring 9 - lighter grey
        juce::Colour(0xff1a1a1a),  // Ring 10 - darker grey
        juce::Colour(0xff252525)   // Ring 11 - lighter grey
    };

    int numRings = audioProcessor.getNumRings();
    float ringSpacing = numRings > 0 ? 0.80f / numRings : 0.15f;

    for (int ring = 0; ring < numRings; ++ring)
    {
        // Add stronger organic spacing variation with multiple wave patterns for artistic effect
        float spacingVariation = 0.018f * std::sin(ring * 1.3f) + 0.008f * std::cos(ring * 2.1f);
        float ringOuterRadius = innerRadius * (0.95f - ring * ringSpacing + spacingVariation);
        float ringInnerRadius = innerRadius * (0.95f - (ring + 1) * ringSpacing + spacingVariation);

        // Draw ring track with subtle colored tint
        g.setColour(ringColors[ring]);
        juce::Path ringPath;
        ringPath.addEllipse(turntableCenter.x - ringOuterRadius,
                           turntableCenter.y - ringOuterRadius,
                           ringOuterRadius * 2, ringOuterRadius * 2);
        juce::Path innerHole;
        innerHole.addEllipse(turntableCenter.x - ringInnerRadius,
                            turntableCenter.y - ringInnerRadius,
                            ringInnerRadius * 2, ringInnerRadius * 2);
        ringPath.addPath(innerHole);
        ringPath.setUsingNonZeroWinding(false);
        g.fillPath(ringPath);

        // Draw artistic ring separator with more dramatic thickness variation and multiple overlays
        float lineThickness = 1.2f + (ring % 4) * 0.6f + 0.3f * std::sin(ring * 0.7f);  // More dramatic varying thickness

        // Draw multiple slightly offset circles for hand-drawn organic feel
        for (int layer = 0; layer < 2; ++layer)
        {
            float offset = layer * 0.15f;
            float alpha = layer == 0 ? 0.6f : 0.3f;
            g.setColour(juce::Colour(0xff2a2a2a).withAlpha(alpha));
            g.drawEllipse(turntableCenter.x - ringOuterRadius + offset,
                         turntableCenter.y - ringOuterRadius + offset,
                         ringOuterRadius * 2 - offset * 2, ringOuterRadius * 2 - offset * 2,
                         lineThickness);
        }
    }

    // Draw crossbar sensor arm (connects center to edge)
    // Arm positioned at -90 degrees (top)
    float armAngle = -90.0f * juce::MathConstants<float>::pi / 180.0f;

    // Arm starts at center
    auto armStart = juce::Point<float>(
        turntableCenter.x,
        turntableCenter.y
    );

    // Arm extends to outer edge
    auto armEnd = juce::Point<float>(
        turntableCenter.x + std::cos(armAngle) * (turntableRadius + 10),
        turntableCenter.y + std::sin(armAngle) * (turntableRadius + 10)
    );

    float armWidth = 8.0f; // Wider arm

    // Draw main metal arm with gradient
    juce::ColourGradient armGradient(
        juce::Colour(0xff5a5a5a), armStart.x - armWidth, armStart.y,
        juce::Colour(0xff3a3a3a), armStart.x + armWidth, armStart.y,
        false
    );
    g.setGradientFill(armGradient);

    juce::Path armPath;
    armPath.startNewSubPath(armStart.x - armWidth, armStart.y);
    armPath.lineTo(armEnd.x - armWidth, armEnd.y);
    armPath.lineTo(armEnd.x + armWidth, armEnd.y);
    armPath.lineTo(armStart.x + armWidth, armStart.y);
    armPath.closeSubPath();
    g.fillPath(armPath);

    // Arm edge highlights
    g.setColour(juce::Colour(0xff6a6a6a).withAlpha(0.5f));
    g.strokePath(armPath, juce::PathStrokeType(1.0f));

    // Mounting bracket at edge (larger)
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillEllipse(armStart.x - 8, armStart.y - 8, 16, 16);
    g.setColour(juce::Colour(0xff6a6a6a));
    g.drawEllipse(armStart.x - 8, armStart.y - 8, 16, 16, 2.0f);

    // Check which dots are passing under the arm (at visual angle 0 degrees - top)
    auto& dots = audioProcessor.getDots();
    float armVisualAngle = 0.0f; // The arm is at the top (0 degrees) visually
    float currentRotation = audioProcessor.getCurrentRotation();

    // Draw glow on arm where dots are passing under it (only for triggered notes)
    auto triggeredDotsForArm = audioProcessor.getRecentlyTriggeredDots();
    auto currentTimeForArm = juce::Time::currentTimeMillis();

    for (size_t i = 0; i < dots.size(); ++i)
    {
        if (!dots[i].active)
            continue;

        // Find trigger info for this dot
        const SkaldProcessor::TriggeredDotInfo* triggerInfo = nullptr;
        for (const auto& info : triggeredDotsForArm)
        {
            if (info.dotIndex == static_cast<int>(i) && (currentTimeForArm - info.timestamp) <= 200)
            {
                triggerInfo = &info;
                break;
            }
        }

        // Only show glow if dot was actually triggered (probability passed)
        if (triggerInfo == nullptr || !triggerInfo->wasTriggered)
            continue;

        // Calculate the visual angle of this dot (where it appears after rotation)
        float visualAngle = dots[i].angle - currentRotation;

        // Normalize to 0-360
        visualAngle = std::fmod(visualAngle + 360.0f, 360.0f);

        // Apply swing offset to arm glow position
        float swingOffsetAngle = getSwingOffset(triggerInfo->beatCount, audioProcessor.getSwing());
        float effectiveArmAngle = armVisualAngle + swingOffsetAngle;

        // Check if dot is near the (possibly swing-offset) arm position
        float angleDiff = std::abs(visualAngle - effectiveArmAngle);
        if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;

        // If dot is within ~20 degrees of the arm, add glow
        if (angleDiff < 20.0f)
        {
            float glowIntensity = 1.0f - (angleDiff / 20.0f); // Fade with distance

            // Apply velocity-based brightness
            float velocityBrightness = calculateGlowBrightness(triggerInfo->velocity);
            glowIntensity *= velocityBrightness;

            int ringIndex = dots[i].ringIndex;
            float ringOuterRadius = innerRadius * (0.95f - ringIndex * ringSpacing);
            float ringInnerRadius = innerRadius * (0.95f - (ringIndex + 1) * ringSpacing);
            float ringMidRadius = (ringOuterRadius + ringInnerRadius) / 2.0f;

            // Glow position on the arm (with swing offset)
            float effectiveArmAngleRad = (effectiveArmAngle - 90.0f) * juce::MathConstants<float>::pi / 180.0f;
            auto glowPos = juce::Point<float>(
                turntableCenter.x + std::cos(effectiveArmAngleRad) * ringMidRadius,
                turntableCenter.y + std::sin(effectiveArmAngleRad) * ringMidRadius
            );

            // Draw smooth cyan glow with velocity-based brightness (more rings for smoother fade)
            for (int glow = 6; glow >= 0; --glow)
            {
                float glowSize = (6.0f + glow * 3.0f) * glowIntensity;
                float alpha = (0.35f - glow * 0.045f) * glowIntensity;  // Higher alpha for more visible glow
                g.setColour(juce::Colour(0xff00d9ff).withAlpha(alpha));
                g.fillEllipse(glowPos.x - glowSize, glowPos.y - glowSize,
                            glowSize * 2, glowSize * 2);
            }

            // Bright glowing center
            g.setColour(juce::Colour(0xff00d9ff).withAlpha(1.0f * glowIntensity));
            g.fillEllipse(glowPos.x - 5, glowPos.y - 5, 10, 10);
        }
    }

    // Get recently triggered dots for visual feedback
    auto triggeredDots = audioProcessor.getRecentlyTriggeredDots();
    auto currentTime = juce::Time::currentTimeMillis();

    // Helper to find triggered info for a dot
    auto findTriggeredInfo = [&triggeredDots, currentTime](int index) -> const SkaldProcessor::TriggeredDotInfo* {
        for (const auto& info : triggeredDots)
        {
            if (info.dotIndex == index && (currentTime - info.timestamp) <= 200)
                return &info;
        }
        return nullptr;
    };

    // Draw gate time tracer effect (fading glow trail)
    for (const auto& triggerInfo : triggeredDots)
    {
        if (!triggerInfo.wasTriggered)
            continue; // Only show tracers for actually triggered notes

        int dotIndex = triggerInfo.dotIndex;
        if (dotIndex < 0 || dotIndex >= static_cast<int>(dots.size()) || !dots[dotIndex].active)
            continue;

        // Calculate age of this trigger
        float ageMs = static_cast<float>(currentTime - triggerInfo.timestamp);
        float fadeProgress = ageMs / triggerInfo.gateTimeMs;  // 0.0 = just triggered, 1.0 = gate expired

        if (fadeProgress >= 1.0f)
            continue; // Gate time expired, don't draw tracer

        // Calculate how far the dot has rotated since trigger
        // Rotation continues during gate time
        float bpm = audioProcessor.getBPM();
        float speed = audioProcessor.getSpeed();
        bool isReversed = audioProcessor.getReverse();
        float rotationsPerSecond = ((bpm / 60.0) / 4.0) * speed;
        float rotationSinceTrigger = rotationsPerSecond * (ageMs / 1000.0f) * 360.0f;
        if (isReversed) rotationSinceTrigger = -rotationSinceTrigger;

        // Dot's original trigger position (where it was when triggered)
        float triggerRotation = currentRotation - rotationSinceTrigger;

        // Calculate ring positions
        int ringIndex = dots[dotIndex].ringIndex;
        float spacing = getRingSpacing();
        float ringOuterRadius = innerRadius * (0.95f - ringIndex * spacing);
        float ringInnerRadius = innerRadius * (0.95f - (ringIndex + 1) * spacing);
        float ringMidRadius = (ringOuterRadius + ringInnerRadius) / 2.0f;

        // Draw tracer at the trigger position (where dot was when it triggered)
        float angleInOurSystem = dots[dotIndex].angle - triggerRotation;
        float angleInStandardMath = angleInOurSystem - 90.0f;
        float visualAngle = angleInStandardMath * juce::MathConstants<float>::pi / 180.0f;
        auto tracerPos = juce::Point<float>(
            turntableCenter.x + std::cos(visualAngle) * ringMidRadius,
            turntableCenter.y + std::sin(visualAngle) * ringMidRadius
        );

        // Fade out smoothly over gate time
        float tracerAlpha = 1.0f - fadeProgress;  // 1.0 -> 0.0
        float velocityBrightness = calculateGlowBrightness(triggerInfo.velocity);

        // Draw fading glow (like it's "burning out")
        float tracerSize = 4.0f;
        for (int glowRing = 2; glowRing >= 0; --glowRing)
        {
            float glowSize = tracerSize * (1.5f + glowRing * 0.4f);
            float alpha = (0.08f + glowRing * 0.02f) * tracerAlpha * velocityBrightness;
            g.setColour(dots[dotIndex].color.withAlpha(alpha));
            g.fillEllipse(tracerPos.x - glowSize, tracerPos.y - glowSize, glowSize * 2, glowSize * 2);
        }

        // Fading core
        g.setColour(dots[dotIndex].color.withAlpha(0.4f * tracerAlpha * velocityBrightness));
        g.fillEllipse(tracerPos.x - tracerSize / 2, tracerPos.y - tracerSize / 2, tracerSize, tracerSize);
    }

    // Draw dots as lights shining from underneath (Drum Buddy style!)
    for (size_t i = 0; i < dots.size(); ++i)
    {
        if (!dots[i].active)
            continue;

        // Calculate position on the appropriate ring
        int ringIndex = dots[i].ringIndex;
        float spacing = getRingSpacing();
        float ringOuterRadius = innerRadius * (0.95f - ringIndex * spacing);
        float ringInnerRadius = innerRadius * (0.95f - (ringIndex + 1) * spacing);
        float ringMidRadius = (ringOuterRadius + ringInnerRadius) / 2.0f;

        // Convert angle to position (accounting for current rotation)
        // Angles are stored in our system where 0° = top
        // cos/sin expect standard math where 0° = right
        // So we subtract 90° to convert: standard = our - 90°
        float angleInOurSystem = dots[i].angle - audioProcessor.getCurrentRotation();
        float angleInStandardMath = angleInOurSystem - 90.0f;
        float visualAngle = angleInStandardMath * juce::MathConstants<float>::pi / 180.0f;
        auto dotPos = juce::Point<float>(
            turntableCenter.x + std::cos(visualAngle) * ringMidRadius,
            turntableCenter.y + std::sin(visualAngle) * ringMidRadius
        );

        // Check if this dot was recently triggered and get trigger info
        const auto* triggerInfo = findTriggeredInfo(static_cast<int>(i));
        bool isPulsing = (triggerInfo != nullptr && triggerInfo->wasTriggered);

        // Calculate brightness based on velocity (only for triggered dots)
        float velocityBrightness = 1.0f;
        if (isPulsing)
        {
            velocityBrightness = calculateGlowBrightness(triggerInfo->velocity);
        }

        // Smooth pulse fade based on time since trigger (not binary on/off)
        float pulseAmount = 1.0f;
        if (isPulsing && triggerInfo)
        {
            float timeSinceTrigger = (currentTime - triggerInfo->timestamp) / 1000.0f;
            // Smooth fade from 2.5x to 1.0x over 200ms
            pulseAmount = 1.0f + (1.5f * velocityBrightness * juce::jmax(0.0f, 1.0f - (timeSinceTrigger * 5.0f)));
        }

        float velocityFactor = dots[i].velocity / 127.0f;
        float dotSize = ((selectedDotIndex == static_cast<int>(i)) ? 6.0f : 4.5f) * (0.7f + velocityFactor * 0.6f);
        if (isPulsing) dotSize *= 1.2f;

        // LIGHT FROM UNDERNEATH EFFECT - Smooth glowing with more rings for softer edges
        for (int glowRing = 6; glowRing >= 0; --glowRing)
        {
            float glowSize = dotSize * (1.5f + glowRing * 0.4f) * pulseAmount;
            float velAlpha = 0.4f + (dots[i].velocity / 127.0f) * 0.6f;
            float alpha = (isPulsing ? 0.25f : 0.08f) * (1.0f - (glowRing / 7.0f)) * velAlpha;  // Smooth gradient + vel
            g.setColour(dots[i].color.withAlpha(alpha));
            g.fillEllipse(dotPos.x - glowSize, dotPos.y - glowSize, glowSize * 2, glowSize * 2);
        }

        // Bright center (the actual light hole)
        float brightnessBoost = isPulsing ? 0.8f : 0.4f;
        juce::ColourGradient lightGradient(
            dots[i].color.brighter(brightnessBoost).withAlpha(isPulsing ? 1.0f : 0.9f), dotPos.x, dotPos.y,
            dots[i].color.withAlpha(isPulsing ? 0.8f : 0.5f), dotPos.x, dotPos.y + dotSize,
            true
        );
        g.setGradientFill(lightGradient);
        g.fillEllipse(dotPos.x - dotSize / 2, dotPos.y - dotSize / 2, dotSize, dotSize);

        // Small bright core (like looking at a small bulb through hole)
        float coreAlpha = (selectedDotIndex == static_cast<int>(i) || isPulsing) ? 0.9f : 0.4f;
        g.setColour(juce::Colours::white.withAlpha(coreAlpha));
        if (selectedDotIndex == static_cast<int>(i) || isPulsing)
        {
            g.fillEllipse(dotPos.x - dotSize / 3, dotPos.y - dotSize / 3, dotSize / 1.5f, dotSize / 1.5f);
        }
        else
        {
            g.fillEllipse(dotPos.x - 1, dotPos.y - 1, 2, 2);
        }

        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.setFont(10.0f);
        g.drawText(juce::String(dots[i].midiChannel),
            static_cast<int>(dotPos.x - 10),
            static_cast<int>(dotPos.y - 10),
            20, 20,
            juce::Justification::centred);

        // Note label when selected
        if (selectedDotIndex == static_cast<int>(i))
        {
            if (isDraggingDot && juce::ModifierKeys::getCurrentModifiers().isAnyModifierKeyDown())
            {
                juce::String statusText;
                if (juce::ModifierKeys::getCurrentModifiers().isShiftDown())
                    statusText = "VELOCITY: " + juce::String(dots[i].velocity);
                else if (juce::ModifierKeys::getCurrentModifiers().isAltDown())
                    statusText = "MIDI CH: " + juce::String(dots[i].midiChannel);

                if (statusText.isNotEmpty())
                {
                    auto hudArea = juce::Rectangle<int>(getWidth() - 160, 115, 145, 28);

                    g.setColour(juce::Colours::black.withAlpha(0.7f));
                    g.fillRoundedRectangle(hudArea.toFloat(), 4.0f);
                    g.setColour(juce::Colour(0xff00d9ff).withAlpha(0.8f));
                    g.drawRoundedRectangle(hudArea.toFloat(), 4.0f, 1.5f);

                    g.setColour(juce::Colours::white);
                    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
                    g.drawText(statusText, hudArea, juce::Justification::centred);
                }
            }

            g.setColour(juce::Colour(0xff00d9ff));
            g.setFont(juce::FontOptions("Arial", 10.0f, juce::Font::bold));
            int midiNote = audioProcessor.ringToMidiNote(ringIndex);
            juce::String noteText = midiNoteToString(midiNote);
            g.drawText(noteText,
                      static_cast<int>(dotPos.x - 40),
                      static_cast<int>(dotPos.y + dotSize + 2),
                      80, 14,
                      juce::Justification::centred);
        }
    }

    // Draw center spindle (like vinyl record center)
    juce::ColourGradient spindleGradient(
        juce::Colour(0xff6a6a6a), turntableCenter.x - 20, turntableCenter.y - 20,
        juce::Colour(0xff2a2a2a), turntableCenter.x + 20, turntableCenter.y + 20,
        false
    );
    g.setGradientFill(spindleGradient);
    g.fillEllipse(turntableCenter.x - 20, turntableCenter.y - 20, 40, 40);

    // Spindle center hole
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillEllipse(turntableCenter.x - 8, turntableCenter.y - 8, 16, 16);

    // Metallic highlight on spindle
    g.setColour(juce::Colour(0xffaaaaaa).withAlpha(0.5f));
    g.fillEllipse(turntableCenter.x - 15, turntableCenter.y - 15, 12, 12);
}

void SkaldEditor::paintHelpScreen(juce::Graphics& g)
{
    const int margin = 40;       // Tighter (was 50)
    const int logoHeight = 100;  // Smaller (was 120)
    const int logoWidth = 92;    // Smaller (was 110)
    const int logoPadding = 8;

    // Draw full Viking warrior at top center
    if (vikingFullImage.isValid())
    {
        int logoX = (getWidth() - logoWidth) / 2;
        int logoY = 10;  // Moved up more (was 15)
        g.setOpacity(1.0f);
        g.drawImageWithin(vikingFullImage,
                         logoX + logoPadding,
                         logoY + logoPadding,
                         logoWidth - (logoPadding * 2),
                         logoHeight - (logoPadding * 2),
                         juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

    // Title - using header font (even larger)
    g.setColour(juce::Colour(0xffE67E22));
    if (csArthemisFont.getTypefaceName().isNotEmpty())
        g.setFont(juce::Font(juce::FontOptions(csArthemisFont.getTypefacePtr()).withHeight(72.0f)));
    else
        g.setFont(juce::FontOptions("Arial", 64.0f, juce::Font::bold));
    g.drawText("SKALD", 0, 10 + logoHeight, getWidth(), 70,
               juce::Justification::centred);

    // Subtitle - using Wonderworld font (with more padding below title)
    g.setColour(juce::Colour(0xff888888));
    if (wonderworldFont.getTypefaceName().isNotEmpty())
        g.setFont(juce::Font(juce::FontOptions(wonderworldFont.getTypefacePtr()).withHeight(16.0f)));
    else
        g.setFont(juce::FontOptions("Arial", 16.0f, juce::Font::plain));
    g.drawText("Viking MIDI Warrior", 0, 10 + logoHeight + 72, getWidth(), 22,
               juce::Justification::centred);

    // Main description with shoutouts - using Wonderworld font (larger!)
    int textY = 10 + logoHeight + 102;  // More space after subtitle
    g.setColour(juce::Colour(0xffcccccc));
    if (wonderworldFont.getTypefaceName().isNotEmpty())
        g.setFont(juce::Font(juce::FontOptions(wonderworldFont.getTypefacePtr()).withHeight(15.0f)));
    else
        g.setFont(juce::FontOptions("Arial", 15.0f, juce::Font::plain));

    juce::String description =
        "Behold Skald, a warrior's weapon for forging rhythmic sagas! Born from the ancient "
        "crafts of Quintron's Drum Buddy and Playtonica's Orbita, this mechanical shieldmaiden "
        "merges the old ways with new sorcery. Command thy notes upon rings of fate, scratch "
        "thy vinyl like a battle axe upon shield, and let chance and motor drive shape thy destiny!";

    g.drawFittedText(description, margin, textY, getWidth() - (margin * 2), 85,
                    juce::Justification::centred, 4);

    // Single column layout
    textY += 85;
    const int contentWidth = getWidth() - (margin * 2);
    const int centerX = margin;

    // How To section - using sub-header font (larger and closer)
    g.setColour(juce::Colour(0xffE67E22));
    if (distropiaxFont.getTypefaceName().isNotEmpty())
        g.setFont(juce::Font(juce::FontOptions(distropiaxFont.getTypefacePtr()).withHeight(40.0f)));
    else
        g.setFont(juce::FontOptions("Arial", 36.0f, juce::Font::bold));
    g.drawText("How To", centerX, textY, contentWidth, 45,
               juce::Justification::centredLeft);

    textY += 50;  // Space after header
    g.setColour(juce::Colour(0xffaaaaaa));
    if (wonderworldFont.getTypefaceName().isNotEmpty())
        g.setFont(juce::Font(juce::FontOptions(wonderworldFont.getTypefacePtr()).withHeight(13.0f)));
    else
        g.setFont(juce::FontOptions("Arial", 13.0f, juce::Font::plain));
    const int bulletSpacing = 18;  // Tighter line spacing

    g.drawText("- Insert Skald on its own MIDI track (leave track empty, no instruments)",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("- Create a separate MIDI track with your synth/instrument",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("- Route MIDI from Skald's track to your synth track (check DAW routing settings)",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("- Add dots, adjust parameters, and let Skald generate MIDI for your synth!",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);

    // Features section - using sub-header font (larger and closer)
    textY += 32;  // Space before next section
    g.setColour(juce::Colour(0xffE67E22));
    if (distropiaxFont.getTypefaceName().isNotEmpty())
        g.setFont(juce::Font(juce::FontOptions(distropiaxFont.getTypefacePtr()).withHeight(40.0f)));
    else
        g.setFont(juce::FontOptions("Arial", 36.0f, juce::Font::bold));
    g.drawText("Features", centerX, textY, contentWidth, 45,
               juce::Justification::centredLeft);

    textY += 50;  // Space after header
    g.setColour(juce::Colour(0xffaaaaaa));
    if (wonderworldFont.getTypefaceName().isNotEmpty())
        g.setFont(juce::Font(juce::FontOptions(wonderworldFont.getTypefacePtr()).withHeight(13.0f)));
    else
        g.setFont(juce::FontOptions("Arial", 13.0f, juce::Font::plain));

    // Feature bullets - Single column
    g.drawText("-  MOTOR ON/OFF: Toggle between motorized playback and manual scrub mode",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  TURNTABLE CONTROL: Click outer ring to scratch, drag to spin, throw for momentum",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  DOUBLE-CLICK: Add or remove dots anywhere on the turntable surface",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  DRAG DOTS: Move dots between rings to change pitch, rotate to change timing",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  REVERSE: Flip playback direction for creative variations and fills",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  SPEED: Control rotation speed from quarter to quad speed (relative to BPM)",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  SCALE & KEY: Shape melodies with musical scales in any key",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  VELOCITY & GATE TIME: Control note dynamics and length",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  PROBABILITY: Add randomness - notes trigger based on percentage chance",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  VELOCITY VARIATION: Humanize patterns with random velocity changes",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  SWING: Add groove with adjustable swing timing (straight to triplet feel)",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  RANDOMIZE: Generate instant patterns for creative starting points",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);
    textY += bulletSpacing;

    g.drawText("-  SAVE/LOAD: Store and recall your favorite patterns",
               centerX + 10, textY, contentWidth - 20, 20, juce::Justification::centredLeft);

    // Version/Credits at bottom
    g.setColour(juce::Colour(0xff666666));
    g.setFont(juce::FontOptions("Arial", 11.0f, juce::Font::plain));
    g.drawText("Beowulf Audio | v1.0.0", 0, getHeight() - 30,
               getWidth(), 20, juce::Justification::centred);
}

void SkaldEditor::setControlsVisible(bool visible)
{
    // Top controls
    speedDisplay.setVisible(visible);
    speedLabel.setVisible(visible);
    speedTapButton.setVisible(visible);
    scaleDisplay.setVisible(visible);
    scaleLabel.setVisible(visible);
    scaleTapButton.setVisible(visible);
    keyDisplay.setVisible(visible);
    keyLabel.setVisible(visible);
    keyTapButton.setVisible(visible);
    octaveDisplay.setVisible(visible);
    octaveLabel.setVisible(visible);
    octaveTapButton.setVisible(visible);

    // Knobs and their labels
    velocityKnob.setVisible(visible);
    velocityLabel.setVisible(visible);
    gateTimeKnob.setVisible(visible);
    gateTimeLabel.setVisible(visible);
    probabilityKnob.setVisible(visible);
    probabilityLabel.setVisible(visible);
    velocityVariationKnob.setVisible(visible);
    velocityVariationLabel.setVisible(visible);
    swingKnob.setVisible(visible);
    swingLabel.setVisible(visible);

    // Toggles and their labels
    reverseToggle.setVisible(visible);
    reverseLabel.setVisible(visible);
    startStopToggle.setVisible(visible);
    startStopLabel.setVisible(visible);

    // Action buttons and their labels
    clearButton.setVisible(visible);
    clearLabel.setVisible(visible);
    addDotButton.setVisible(visible);
    addLabel.setVisible(visible);
    randomizeButton.setVisible(visible);
    randomizeLabel.setVisible(visible);
    savePatternButton.setVisible(visible);
    saveLabel.setVisible(visible);
    loadPatternButton.setVisible(visible);
    loadLabel.setVisible(visible);
    aboutButton.setVisible(visible);
    aboutLabel.setVisible(visible);

    // Standalone controls (if visible)
    playStopButton.setVisible(visible);
    bpmLabel.setVisible(visible);
    bpmSlider.setVisible(visible);
}

void SkaldEditor::resized()
{
    auto area = getLocalBounds();

    if (juce::JUCEApplication::isStandaloneApp())
        area.removeFromTop(45);

    // Top controls - single row with LED displays, tap buttons, and knobs
    auto topControls = area.removeFromTop(100);  // Increased height for knobs
    topControls.reduce(25, 15);  // Generous margins from edges

    // Check if we're running as standalone (runtime detection)
    bool isStandalone = (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    if (isStandalone)
    {
        // Only show transport controls in Standalone
        playStopButton.setVisible(true);
        bpmLabel.setVisible(true);
        bpmSlider.setVisible(true);

        auto playArea = topControls.removeFromLeft(60);
        playStopButton.setBounds(playArea.getX(), playArea.getY() + 10, playArea.getWidth(), 30);
        topControls.removeFromLeft(8);

        bpmLabel.setBounds(topControls.removeFromLeft(45));
        topControls.removeFromLeft(4);
        bpmSlider.setBounds(topControls.removeFromLeft(100));
        topControls.removeFromLeft(12);
    }
    else
    {
        // Hide in VST/AU mode
        playStopButton.setVisible(false);
        bpmLabel.setVisible(false);
        bpmSlider.setVisible(false);
        playStopButton.setBounds(0, 0, 0, 0);
        bpmLabel.setBounds(0, 0, 0, 0);
        bpmSlider.setBounds(0, 0, 0, 0);
    }

    // LED display constants
    const int elementHeight = 32;
    const int labelHeight = 12;
    const int displayWidth = 50;
    const int tapButtonWidth = 32;
    const int displayMargin = 8;
    const int edgeMargin = 25;  // Generous edge margin for professional spacing

    // Knob constants
    const int knobSize = 65;
    const int knobLabelHeight = 15;
    const int knobSpacing = 10;

    // Position LED displays in top-left with proper margins
    int xPos = edgeMargin;  // Proper left margin
    int yPos = 25;  // Top margin matching bottom margin (25px)
    const int ledLabelHeight = 12;  // Height for LED labels (same as knob labels)

    // Scale LED display + tap button + label (label centered under LED only)
    int scaleLEDX = xPos;
    scaleDisplay.setBounds(xPos, yPos, displayWidth, elementHeight);
    xPos += displayWidth + displayMargin;
    scaleTapButton.setBounds(xPos, yPos, tapButtonWidth, elementHeight);
    scaleLabel.setBounds(scaleLEDX, yPos + elementHeight, displayWidth, ledLabelHeight);
    xPos += tapButtonWidth + displayMargin * 2;

    // Key LED display + tap button + label (label centered under LED only)
    int keyLEDX = xPos;
    keyDisplay.setBounds(xPos, yPos, displayWidth, elementHeight);
    xPos += displayWidth + displayMargin;
    keyTapButton.setBounds(xPos, yPos, tapButtonWidth, elementHeight);
    keyLabel.setBounds(keyLEDX, yPos + elementHeight, displayWidth, ledLabelHeight);
    xPos += tapButtonWidth + displayMargin * 2;

    // Speed/Division LED display + tap button + label (label centered under LED only)
    int speedLEDX = xPos;
    speedDisplay.setBounds(xPos, yPos, displayWidth, elementHeight);
    xPos += displayWidth + displayMargin;
    speedTapButton.setBounds(xPos, yPos, tapButtonWidth, elementHeight);
    speedLabel.setBounds(speedLEDX, yPos + elementHeight, displayWidth, ledLabelHeight);
    xPos += tapButtonWidth + displayMargin * 2;

    // Octave LED display + tap button + label (label centered under LED only)
    int octaveLEDX = xPos;
    octaveDisplay.setBounds(xPos, yPos, displayWidth, elementHeight);
    xPos += displayWidth + displayMargin;
    octaveTapButton.setBounds(xPos, yPos, tapButtonWidth, elementHeight);
    octaveLabel.setBounds(octaveLEDX, yPos + elementHeight, displayWidth, ledLabelHeight);

    // Position knobs floating right with small right margin
    int knobY = 15;  // Top margin for knobs (slightly less than LED to align nicely)
    int knobRightMargin = 10;  // Small right margin - sweet spot between flush and too much space
    int totalKnobsWidth = (knobSize * 5) + (knobSpacing * 4);
    int knobStartX = getWidth() - totalKnobsWidth - knobRightMargin;

    // Probability knob (1st)
    probabilityKnob.setBounds(knobStartX, knobY, knobSize, knobSize);
    probabilityLabel.setBounds(knobStartX, knobY + knobSize, knobSize, knobLabelHeight);
    knobStartX += knobSize + knobSpacing;

    // Swing knob (2nd)
    swingKnob.setBounds(knobStartX, knobY, knobSize, knobSize);
    swingLabel.setBounds(knobStartX, knobY + knobSize, knobSize, knobLabelHeight);
    knobStartX += knobSize + knobSpacing;

    // Velocity knob (3rd)
    velocityKnob.setBounds(knobStartX, knobY, knobSize, knobSize);
    velocityLabel.setBounds(knobStartX, knobY + knobSize, knobSize, knobLabelHeight);
    knobStartX += knobSize + knobSpacing;

    // Velocity variation knob (4th)
    velocityVariationKnob.setBounds(knobStartX, knobY, knobSize, knobSize);
    velocityVariationLabel.setBounds(knobStartX, knobY + knobSize, knobSize, knobLabelHeight);
    knobStartX += knobSize + knobSpacing;

    // Gate time knob (5th)
    gateTimeKnob.setBounds(knobStartX, knobY, knobSize, knobSize);
    gateTimeLabel.setBounds(knobStartX, knobY + knobSize, knobSize, knobLabelHeight);

    // Calculate turntable area to be perfectly centered vertically
    const int topControlsHeight = 100;  // Height of top controls area
    const int bottomControlsSpace = 56 + 12 + 25;  // toggle height + label height + margin

    const int sidebarWidthRight = 220;
    const int layoutMargin = 25; // Definiamo qui il margine per evitare E0020

    int availableWidth = getWidth() - sidebarWidthRight - (layoutMargin * 2);
    int availableHeight = getHeight() - topControlsHeight - bottomControlsSpace;

    int turntableSize = juce::jmin(availableWidth, availableHeight);

    int turntableX = layoutMargin + (availableWidth - turntableSize) / 2;
    int turntableY = topControlsHeight + (availableHeight - turntableSize) / 2;

    turntableArea = juce::Rectangle<float>((float)turntableX, (float)turntableY, (float)turntableSize, (float)turntableSize);
    turntableRadius = turntableSize / 2.0f * 0.92f;

    turntableCenter = turntableArea.getCentre();

    // Position action buttons in bottom right
    const int buttonSize = 32;
    const int buttonSpacing = 5;
    const int buttonLabelHeight = 12;
    const int margin = 25;  // Generous bottom/side margin
    const int instructionTextHeight = 15;

    // Calculate action button Y position (align with toggles)
    // Buttons + labels should have same bottom margin as toggles
    int buttonStartX = getWidth() - margin - (buttonSize * 6 + buttonSpacing * 5);
    int buttonY = getHeight() - margin - buttonSize - buttonLabelHeight;

    // Add button
    addDotButton.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    addLabel.setBounds(buttonStartX, buttonY + buttonSize, buttonSize, buttonLabelHeight);
    buttonStartX += buttonSize + buttonSpacing;

    // Randomize button
    randomizeButton.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    randomizeLabel.setBounds(buttonStartX, buttonY + buttonSize, buttonSize, buttonLabelHeight);
    buttonStartX += buttonSize + buttonSpacing;

    // Clear button
    clearButton.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    clearLabel.setBounds(buttonStartX, buttonY + buttonSize, buttonSize, buttonLabelHeight);
    buttonStartX += buttonSize + buttonSpacing;

    // Save button
    savePatternButton.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    saveLabel.setBounds(buttonStartX, buttonY + buttonSize, buttonSize, buttonLabelHeight);
    buttonStartX += buttonSize + buttonSpacing;

    // Load button
    loadPatternButton.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    loadLabel.setBounds(buttonStartX, buttonY + buttonSize, buttonSize, buttonLabelHeight);
    buttonStartX += buttonSize + buttonSpacing;

    // About button
    aboutButton.setBounds(buttonStartX, buttonY, buttonSize, buttonSize);
    aboutLabel.setBounds(buttonStartX, buttonY + buttonSize, buttonSize, buttonLabelHeight);

    // Back button (for help screen) - centered at top of help screen
    const int backButtonSize = 32;
    backButton.setBounds(margin, margin, backButtonSize, backButtonSize);
    backLabel.setBounds(margin, margin + backButtonSize, backButtonSize, buttonLabelHeight);

    // Position toggles in bottom left corner - aligned with action button labels
    const int toggleWidth = 60;
    const int toggleHeight = 56;
    const int toggleLabelHeight = 12;
    const int toggleSpacing = 5;  // Reduced from 10 to bring switches closer together

    // Calculate Y position to ensure toggle + label fit with proper bottom margin
    auto toggleX = margin;
    auto toggleY = getHeight() - margin - toggleHeight - toggleLabelHeight;

    // Reverse toggle
    reverseToggle.setBounds(toggleX, toggleY, toggleWidth, 56);
    reverseLabel.setBounds(toggleX, toggleY + 56, toggleWidth, toggleLabelHeight);
    toggleX += toggleWidth + toggleSpacing;

    // Start/Stop (Motor) toggle
    startStopToggle.setBounds(toggleX, toggleY, toggleWidth, 56);
    startStopLabel.setBounds(toggleX, toggleY + 56, toggleWidth, toggleLabelHeight);

    auto sidebarArea = getLocalBounds().removeFromRight(220).reduced(20);

    sidebarArea.removeFromTop(160);

    int ctrlH = 25;
    int gap = 12;

    // Title Sidebar
    algoTitleLabel.setBounds(sidebarArea.removeFromTop(30));
    sidebarArea.removeFromTop(20);

    // Target Ring
    ringSelectorLabel.setBounds(sidebarArea.removeFromTop(15));
    ringSelector.setBounds(sidebarArea.removeFromTop(ctrlH));
    sidebarArea.removeFromTop(gap);

    // Pulses
    pulsesLabel.setBounds(sidebarArea.removeFromTop(15));
    pulsesSlider.setBounds(sidebarArea.removeFromTop(ctrlH));
    sidebarArea.removeFromTop(gap);

    // Steps
    stepsLabel.setBounds(sidebarArea.removeFromTop(15));
    stepsSlider.setBounds(sidebarArea.removeFromTop(ctrlH));
    sidebarArea.removeFromTop(gap);

    // Depth
    depthLabel.setBounds(sidebarArea.removeFromTop(15));
    depthSlider.setBounds(sidebarArea.removeFromTop(ctrlH));
    sidebarArea.removeFromTop(gap);

    // Shift
    shiftLabel.setBounds(sidebarArea.removeFromTop(15));
    shiftSlider.setBounds(sidebarArea.removeFromTop(ctrlH));
    sidebarArea.removeFromTop(gap * 2);

    generateButton.setBounds(sidebarArea.removeFromTop(45).reduced(10, 0));

    versionLabel.setBounds(getWidth() - 120, getHeight() - 30, 100, 20);
}

void SkaldEditor::mouseDown (const juce::MouseEvent& event)
{
    auto clickPos = event.position;

    // If on help screen, ignore interactions
    if (showingHelpScreen)
        return;

    // Check if clicking on outer ring for scratching or stopping
    auto delta = clickPos - turntableCenter;
    float distanceFromCenter = delta.getDistanceFromOrigin();
    float outerRingRadius = turntableRadius * 0.95f;
    float outerRingInner = turntableRadius * 0.80f;

    // Check if clicking on outer ring area
    if (distanceFromCenter >= outerRingInner && distanceFromCenter <= outerRingRadius)
    {
        // Check if turntable is already moving (from motor OR scratch momentum)
        bool hasMotorMotion = audioProcessor.getMotorRunning() &&
                              audioProcessor.getCurrentSpeedMultiplier() > 0.01f;
        bool hasScratchMomentum = std::abs(audioProcessor.getScratchVelocity()) > 0.1f;

        if (hasMotorMotion || hasScratchMomentum)
        {
            // STOP the turntable immediately (like putting hand on record)
            audioProcessor.setScratchVelocity(0.0f);
            audioProcessor.setCurrentSpeedMultiplier(0.0f);
            return;  // Don't start scratching yet - just stop
        }
        else if (!audioProcessor.getMotorRunning())
        {
            // Turntable is stationary and motor is off - start scratching
            isScratching = true;
            lastScratchAngle = angleFromPoint(clickPos);
            lastScratchPos = clickPos;  // For tangential velocity calculation
            lastScratchTime = juce::Time::currentTimeMillis();
            scratchVelocity = 0.0f;
            audioProcessor.setBeingScratched(true);
            return;  // Don't process as regular click
        }
    }

    // Check if we clicked on an existing dot
    selectedDotIndex = findDotAtPoint(clickPos);

    // Check if we clicked on an existing dot
    selectedDotIndex = findDotAtPoint(clickPos);

    auto& dots = audioProcessor.getDots();
    if (selectedDotIndex >= 0 && selectedDotIndex < static_cast<int>(dots.size()))
    {
        initialChannelOnDrag = dots[selectedDotIndex].midiChannel;
        initialVelocityOnDrag = dots[selectedDotIndex].velocity;
    }
    else
    {
        selectedDotIndex = -1; // Reset di sicurezza se l'indice è sporco
    }
    // Double-click handling
    if (event.getNumberOfClicks() == 2)
    {
        if (selectedDotIndex >= 0)
        {
            // Double-click on existing dot = delete
            audioProcessor.removeDot(selectedDotIndex);
            selectedDotIndex = -1;
            repaint();
            return;
        }
        else
        {
            // Double-click on empty space = add dot at that location
            float innerRadius = turntableRadius * 0.90f;
            auto delta = clickPos - turntableCenter;
            float distanceFromCenter = delta.getDistanceFromOrigin();

            // Check if click is within the turntable area
            if (distanceFromCenter > innerRadius * 0.95f || distanceFromCenter < innerRadius * 0.05f)
                return; // Outside playable area

            // Calculate which ring was clicked
            int numRings = audioProcessor.getNumRings();
            float ringSpacing = numRings > 0 ? 0.80f / numRings : 0.15f;

            int ringIndex = -1;
            for (int ring = 0; ring < numRings; ++ring)
            {
                float ringOuterRadius = innerRadius * (0.95f - ring * ringSpacing);
                float ringInnerRadius = innerRadius * (0.95f - (ring + 1) * ringSpacing);

                if (distanceFromCenter <= ringOuterRadius && distanceFromCenter >= ringInnerRadius)
                {
                    ringIndex = ring;
                    break;
                }
            }

            if (ringIndex >= 0)
            {
                // Calculate visual angle from click position
                auto delta = clickPos - turntableCenter;
                float angleRadians = std::atan2(delta.y, delta.x);
                float visualAngle = angleRadians * 180.0f / juce::MathConstants<float>::pi;

                // Adjust so 0 degrees is at the top
                visualAngle += 90.0f;

                // Normalize to 0-360
                if (visualAngle < 0)
                    visualAngle += 360.0f;

                // Convert visual angle to absolute angle by adding current rotation
                float absoluteAngle = visualAngle + audioProcessor.getCurrentRotation();
                absoluteAngle = std::fmod(absoluteAngle, 360.0f);

                // Add dot at this position
                audioProcessor.addDot(absoluteAngle, ringIndex, juce::Colour(0xffff6b35));

                // Trigger preview note
                audioProcessor.triggerPreviewNote(ringIndex);

                repaint();
            }
            return;
        }
    }

    if (selectedDotIndex >= 0)
    {
        // Start dragging existing dot
        isDraggingDot = true;

        auto& currentDots = audioProcessor.getDots();
        if (selectedDotIndex >= 0 && selectedDotIndex < static_cast<int>(currentDots.size()))
        {
            audioProcessor.triggerPreviewNote(currentDots[selectedDotIndex].ringIndex);
        }
    }
    else
    {
        // Just deselect if clicking on empty space
        repaint();
    }
}

void SkaldEditor::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    // Handle scratch release (apply momentum/"throw")
    if (isScratching)
    {
        isScratching = false;
        audioProcessor.setBeingScratched(false);
        // Velocity is already set in mouseDrag, will decay in processBlock
        return;
    }

    isDraggingDot = false;
}

void SkaldEditor::mouseDrag (const juce::MouseEvent& event)
{
    // Handle scratching (manual turntable control)
    if (isScratching)
    {
        auto currentTime = juce::Time::currentTimeMillis();
        float deltaTime = (currentTime - lastScratchTime) / 1000.0f;  // Convert to seconds

        if (deltaTime > 0.0f)
        {
            // Use tangential velocity instead of angular to support linear motion
            // Calculate mouse velocity in pixels/second
            auto currentPos = event.position;
            auto deltaPos = currentPos - lastScratchPos;
            auto mouseVelocity = deltaPos / deltaTime;

            // Get position relative to turntable center
            auto delta = currentPos - turntableCenter;
            float radius = delta.getDistanceFromOrigin();

            if (radius > 1.0f)  // Avoid division by zero at center
            {
                // Calculate tangent direction (perpendicular to radius, pointing counter-clockwise)
                juce::Point<float> tangent(-delta.y, delta.x);
                float tangentLength = tangent.getDistanceFromOrigin();
                if (tangentLength > 0.01f)
                {
                    tangent = tangent / tangentLength;  // Normalize

                    // Project mouse velocity onto tangent direction
                    float tangentialSpeed = mouseVelocity.getDotProduct(tangent);  // pixels/sec

                    // Convert tangential speed to angular velocity (degrees/sec)
                    // tangentialSpeed = radius * angularVelocity_radians
                    // angularVelocity_degrees = (tangentialSpeed / radius) * (180/pi)
                    float angularVelocityDeg = (tangentialSpeed / radius) * 57.2958f;

                    // Negate for intuitive direction (clockwise drag = clockwise rotation)
                    scratchVelocity = -angularVelocityDeg;

                    // Clamp velocity to realistic limits (2 rotations per second max)
                    const float MAX_SCRATCH_VELOCITY = 720.0f;
                    scratchVelocity = juce::jlimit(-MAX_SCRATCH_VELOCITY, MAX_SCRATCH_VELOCITY, scratchVelocity);

                    // Calculate rotation change for this frame
                    float deltaAngle = scratchVelocity * deltaTime;

                    // Update rotation directly
                    float newRotation = audioProcessor.getCurrentRotation() + deltaAngle;
                    if (newRotation < 0.0f)
                        newRotation += 360.0f;
                    else if (newRotation >= 360.0f)
                        newRotation = std::fmod(newRotation, 360.0f);

                    audioProcessor.setRotationDirect(newRotation);
                    audioProcessor.setScratchVelocity(scratchVelocity);
                }
            }

            lastScratchPos = currentPos;
            lastScratchTime = currentTime;
        }

        repaint();
        return;
    }

    {
        auto& dots = audioProcessor.getDots();

        if (selectedDotIndex >= (int)dots.size()) return;

        if (event.mods.isShiftDown())
        {
            int dragDelta = -event.getDistanceFromDragStartY() / 2;
            int newVelocity = juce::jlimit(1, 127, initialVelocityOnDrag + dragDelta);

            if (newVelocity != dots[selectedDotIndex].velocity)
            {
                dots[selectedDotIndex].velocity = newVelocity;
                repaint();
            }
            return;
        }

        if (event.mods.isAltDown())
        {
            int dragDelta = -event.getDistanceFromDragStartY() / 15;
            int newChannel = juce::jlimit(1, 16, initialChannelOnDrag + dragDelta);

            if (newChannel != dots[selectedDotIndex].midiChannel)
            {
                dots[selectedDotIndex].midiChannel = newChannel;
                dots[selectedDotIndex].color = channelColors[(newChannel - 1) % channelColors.size()];
                repaint();
            }
            return;
        }

        auto delta = event.position - turntableCenter;
        float distanceFromCenter = delta.getDistanceFromOrigin();
        float innerRadius = turntableRadius * 0.90f;

        dots[selectedDotIndex].angle = angleFromPoint(event.position);

        int numRings = audioProcessor.getNumRings();
        float ringSpacing = numRings > 0 ? 0.80f / numRings : 0.15f;
        int newRingIndex = dots[selectedDotIndex].ringIndex;

        for (int ring = 0; ring < numRings; ++ring)
        {
            float ringOuterRadius = innerRadius * (0.95f - ring * ringSpacing);
            float ringInnerRadius = innerRadius * (0.95f - (ring + 1) * ringSpacing);

            if (distanceFromCenter <= ringOuterRadius && distanceFromCenter >= ringInnerRadius)
            {
                newRingIndex = ring;
                break;
            }
        }

        if (newRingIndex != dots[selectedDotIndex].ringIndex)
        {
            dots[selectedDotIndex].ringIndex = newRingIndex;
            audioProcessor.triggerPreviewNote(newRingIndex);
        }

        repaint();
    }
}

void SkaldEditor::timerCallback()
{
    // Repaint to update the rotating turntable
    repaint();
}

//==============================================================================
// Helper methods

float SkaldEditor::angleFromPoint(juce::Point<float> point)
{
    auto delta = point - turntableCenter;
    float angleRadians = std::atan2(delta.y, delta.x);
    float angleDegrees = angleRadians * 180.0f / juce::MathConstants<float>::pi;

    // Adjust so 0 degrees is at the top
    angleDegrees += 90.0f;

    // Normalize to 0-360
    if (angleDegrees < 0)
        angleDegrees += 360.0f;

    // Add current rotation to get absolute angle
    angleDegrees += audioProcessor.getCurrentRotation();
    angleDegrees = std::fmod(angleDegrees, 360.0f);

    return angleDegrees;
}

juce::Point<float> SkaldEditor::pointFromAngle(float angle, float radius)
{
    float angleRadians = (angle - 90.0f) * juce::MathConstants<float>::pi / 180.0f;
    return juce::Point<float>(
        turntableCenter.x + std::cos(angleRadians) * radius,
        turntableCenter.y + std::sin(angleRadians) * radius
    );
}

int SkaldEditor::findDotAtPoint(juce::Point<float> point)
{
    auto& dots = audioProcessor.getDots();
    float innerRadius = turntableRadius * 0.90f;

    for (size_t i = 0; i < dots.size(); ++i)
    {
        if (!dots[i].active)
            continue;

        // Calculate position on the appropriate ring
        int ringIndex = dots[i].ringIndex;
        float spacing = getRingSpacing();
        float ringOuterRadius = innerRadius * (0.95f - ringIndex * spacing);
        float ringInnerRadius = innerRadius * (0.95f - (ringIndex + 1) * spacing);
        float ringMidRadius = (ringOuterRadius + ringInnerRadius) / 2.0f;

        // Convert dot angle to visual position
        // Angles are stored in our system where 0° = top
        // cos/sin expect standard math where 0° = right
        // So we subtract 90° to convert: standard = our - 90°
        float angleInOurSystem = dots[i].angle - audioProcessor.getCurrentRotation();
        float angleInStandardMath = angleInOurSystem - 90.0f;
        float visualAngle = angleInStandardMath * juce::MathConstants<float>::pi / 180.0f;
        auto dotPos = juce::Point<float>(
            turntableCenter.x + std::cos(visualAngle) * ringMidRadius,
            turntableCenter.y + std::sin(visualAngle) * ringMidRadius
        );

        // Check if point is within dot bounds (with larger tolerance for easier clicking)
        if (point.getDistanceFrom(dotPos) <= 12.0f)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

float SkaldEditor::getRingSpacing() const
{
    int numRings = audioProcessor.getNumRings();
    return numRings > 0 ? 0.80f / numRings : 0.15f;
}

juce::String SkaldEditor::midiNoteToString(int midiNote) const
{
    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    int noteInOctave = midiNote % 12;
    int octave = (midiNote / 12) - 1;

    return juce::String(noteNames[noteInOctave]) + juce::String(octave);
}

float SkaldEditor::calculateGlowBrightness(int velocity) const
{
    // Map velocity (1-127) to brightness multiplier (0.3 - 1.0)
    // Low velocity (1-40): 0.3 - 0.5
    // Medium velocity (41-90): 0.5 - 0.75
    // High velocity (91-127): 0.75 - 1.0
    float normalized = juce::jlimit(1, 127, velocity) / 127.0f;
    return 0.3f + (normalized * 0.7f);
}

float SkaldEditor::getSwingOffset(int beatCount, float swingAmount) const
{
    // Calculate angle offset for swing visualization (matches audio swing timing)
    // Even beats (0, 2, 4...): No offset
    // Odd beats (1, 3, 5...): Offset forward based on swing amount
    if (beatCount % 2 == 0)
    {
        // Straight beat - no offset
        return 0.0f;
    }
    else
    {
        // Swung beat - offset proportional to swing amount (more dramatic)
        // Swing at 50% = 0° offset (straight/no swing)
        // Swing at 66% = ~22.5° offset (triplet feel)
        // Swing at 100% = 45° offset (full 16th note delay - dramatic!)
        float swingRatio = swingAmount / 100.0f;  // 0.0 to 1.0
        float delayRatio = (swingRatio - 0.5f) * 2.0f;  // -1.0 to 1.0, centered at 0.5
        delayRatio = juce::jlimit(0.0f, 1.0f, delayRatio);  // Clamp to 0.0-1.0

        // One full rotation = 8 beats, so 16th note = 360°/32 = 11.25°
        // But make it more visually dramatic: use 45° max
        return delayRatio * 45.0f;
    }
}
