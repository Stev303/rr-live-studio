#include "UI/TopBar.h"
#include "UI/Widgets.h"

namespace rr
{
TopBar::TopBar (AppState& state, std::function<void()> onGoLive, std::function<void (int)> onNavigate)
    : app (state),
      goLiveCallback (std::move (onGoLive)),
      navigateCallback (std::move (onNavigate))
{
    setOpaque (false);

    configureLabel (titleLabel, 15.0f, theme::text, true);
    titleLabel.setText ("Dashboard", juce::dontSendNotification);

    configureLabel (modeCaption, 9.5f, theme::textDim, true, juce::Justification::centredRight);
    modeCaption.setText ("MODE", juce::dontSendNotification);

    for (int i = 0; i < AppState::getModeNames().size(); ++i)
        modeBox.addItem (AppState::getModeNames()[i], i + 1);

    modeBox.setSelectedItemIndex (juce::jmax (0, AppState::getModeNames().indexOf (app.getMode())),
                                  juce::dontSendNotification);
    modeBox.onChange = [this]
    {
        if (updatingMode)
            return;

        juce::String error;
        app.setMode (modeBox.getText(), error);

        if (error.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Mode buffer",
                                                    "Mode tidak dapat diterapkan:\n" + error);
        }

        updateStatus();
    };

    configureLabel (deviceLabel, 10.5f, theme::textDim);
    configureLabel (sampleRateLabel, 10.5f, theme::textDim, false, juce::Justification::centred);
    configureLabel (cpuLabel, 10.5f, theme::textDim, false, juce::Justification::centred);
    configureLabel (connectionLabel, 10.5f, theme::amber, true, juce::Justification::centred);

    goLiveButton.setColour (juce::TextButton::buttonColourId, theme::magenta.withAlpha (0.2f));
    goLiveButton.setColour (juce::TextButton::buttonOnColourId, theme::magenta.withAlpha (0.4f));
    goLiveButton.setColour (juce::TextButton::textColourOffId, theme::text);
    goLiveButton.setTooltip ("Buka checklist Go Live untuk HP / BIGO LIVE");
    goLiveButton.onClick = [this]
    {
        if (goLiveCallback != nullptr)
            goLiveCallback();
    };

    deviceLabel.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    deviceLabel.setTooltip ("Buka pengaturan perangkat audio");

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (modeCaption);
    addAndMakeVisible (modeBox);
    addAndMakeVisible (deviceLabel);
    addAndMakeVisible (sampleRateLabel);
    addAndMakeVisible (cpuLabel);
    addAndMakeVisible (connectionLabel);
    addAndMakeVisible (goLiveButton);

    updateStatus();
    startTimerHz (4);
}

TopBar::~TopBar()
{
    stopTimer();
}

void TopBar::mouseDown (const juce::MouseEvent& event)
{
    if (navigateCallback != nullptr && deviceLabel.getBounds().contains (event.getPosition()))
        navigateCallback (8);
}

void TopBar::setPageTitle (const juce::String& title)
{
    titleLabel.setText (title, juce::dontSendNotification);
}

void TopBar::timerCallback()
{
    updateStatus();
}

void TopBar::updateStatus()
{
    const auto modes = AppState::getModeNames();
    const auto modeIndex = modes.indexOf (app.getMode());

    if (! modeBox.isPopupActive() && modeBox.getSelectedItemIndex() != modeIndex)
    {
        updatingMode = true;
        modeBox.setSelectedItemIndex (juce::jmax (0, modeIndex), juce::dontSendNotification);
        updatingMode = false;
    }

    auto device = app.engine.getDeviceDescription();

    if (device.length() > 58)
        device = device.substring (0, 56) + "...";

    deviceLabel.setText ("DEVICE  " + device, juce::dontSendNotification);

    if (auto* current = app.engine.getDeviceManager().getCurrentAudioDevice())
    {
        sampleRateLabel.setText (juce::String (current->getCurrentSampleRate() / 1000.0, 1)
                                     + " kHz / " + juce::String (current->getCurrentBufferSizeSamples()) + " smp",
                                 juce::dontSendNotification);
    }
    else
    {
        sampleRateLabel.setText ("No device", juce::dontSendNotification);
    }

    const auto meters = app.engine.getMeters();
    cpuLabel.setText ("CPU " + juce::String (meters.cpuPercent, 1) + "%", juce::dontSendNotification);

    if (! app.engine.isRunning())
    {
        connectionLabel.setText ("AUDIO OFF", juce::dontSendNotification);
        connectionLabel.setColour (juce::Label::textColourId, theme::red);
    }
    else if (app.engine.isLoopbackCapturing())
    {
        connectionLabel.setText ("MUSIC CAPTURE", juce::dontSendNotification);
        connectionLabel.setColour (juce::Label::textColourId, theme::green);
    }
    else
    {
        connectionLabel.setText ("AUDIO READY", juce::dontSendNotification);
        connectionLabel.setColour (juce::Label::textColourId, theme::cyan);
    }

    repaint();
}

void TopBar::paint (juce::Graphics& g)
{
    g.setColour (theme::background.brighter (0.02f));
    g.fillRect (getLocalBounds());

    juce::ColourGradient line (theme::cyan.withAlpha (0.7f), 0.0f, (float) getHeight() - 1.0f,
                               theme::magenta.withAlpha (0.55f), (float) getWidth(), (float) getHeight() - 1.0f,
                               false);
    g.setGradientFill (line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    auto pill = connectionLabel.getBounds().toFloat().expanded (6.0f, 2.0f);
    g.setColour (connectionLabel.findColour (juce::Label::textColourId).withAlpha (0.12f));
    g.fillRoundedRectangle (pill, 4.0f);

    g.setColour (theme::border.withAlpha (0.65f));
    g.drawVerticalLine (188, 14.0f, (float) getHeight() - 14.0f);
}

void TopBar::resized()
{
    auto area = getLocalBounds().reduced (16, 9);

    titleLabel.setBounds (area.removeFromLeft (160));
    area.removeFromLeft (4);
    modeCaption.setBounds (area.removeFromLeft (40));
    modeBox.setBounds (area.removeFromLeft (122).reduced (0, 5));
    area.removeFromLeft (12);

    goLiveButton.setBounds (area.removeFromRight (118).reduced (0, 6));
    area.removeFromRight (12);
    connectionLabel.setBounds (area.removeFromRight (112));
    area.removeFromRight (8);
    cpuLabel.setBounds (area.removeFromRight (76));
    area.removeFromRight (8);
    sampleRateLabel.setBounds (area.removeFromRight (132));
    area.removeFromRight (8);
    deviceLabel.setBounds (area);
}

} // namespace rr
