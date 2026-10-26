#include "UI/Sidebar.h"

namespace rr
{
namespace
{
    const char* const pageNames[] =
    {
        "Dashboard",
        "YouTube",
        "Vocal Chain",
        "Effects",
        "VST3 Plugins",
        "Sampler Pads",
        "Mixer",
        "Presets",
        "Settings"
    };
}

Sidebar::Sidebar (AppState& state)
    : app (state)
{
    for (int i = 0; i < numPages; ++i)
    {
        auto button = std::make_unique<juce::TextButton> (pageNames[i]);
        button->setClickingTogglesState (true);
        button->setRadioGroupId (0x52524c);
        button->getProperties().set ("alignLeft", true);
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button->setColour (juce::TextButton::buttonOnColourId, theme::cyan.withAlpha (0.14f));
        button->setTooltip (juce::String (pageNames[i]) + " - halaman " + juce::String (i + 1));
        button->onClick = [this, i]
        {
            if (onPageSelected != nullptr)
                onPageSelected (i);
            else
                setCurrentPage (i);
        };
        addAndMakeVisible (*button);
        buttons[(size_t) i] = std::move (button);
    }

    setCurrentPage (app.getCurrentPage());
    startTimerHz (2);
}

Sidebar::~Sidebar()
{
    stopTimer();
}

void Sidebar::setCurrentPage (int pageIndex)
{
    currentPage = juce::jlimit (0, numPages - 1, pageIndex);

    for (int i = 0; i < numPages; ++i)
        if (buttons[(size_t) i] != nullptr)
            buttons[(size_t) i]->setToggleState (i == currentPage, juce::dontSendNotification);

    repaint();
}

void Sidebar::selectPage (int pageIndex)
{
    if (onPageSelected != nullptr)
        onPageSelected (pageIndex);
    else
        setCurrentPage (pageIndex);
}

void Sidebar::timerCallback()
{
    const auto ready = app.engine.isRunning();
    const auto capturing = app.engine.isLoopbackCapturing();

    juce::String newStatus = ready ? (capturing ? "Music capture aktif" : "Engine audio siap")
                                   : "Audio belum aktif";

    if (newStatus != statusText || ready != audioReady)
    {
        statusText = newStatus;
        audioReady = ready;
        repaint();
    }
}

void Sidebar::paint (juce::Graphics& g)
{
    juce::ColourGradient gradient (theme::panel, 0.0f, 0.0f,
                                   theme::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (gradient);
    g.fillAll();

    g.setColour (theme::border.withAlpha (0.8f));
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    auto logoBox = juce::Rectangle<float> (18.0f, 18.0f, 44.0f, 44.0f);
    g.setColour (theme::cyan.withAlpha (0.14f));
    g.fillRoundedRectangle (logoBox, 11.0f);
    g.setColour (theme::cyan);
    g.drawRoundedRectangle (logoBox.reduced (0.5f), 11.0f, 1.4f);
    g.setFont (theme::font (20.0f, true));
    g.setColour (theme::text);
    g.drawText ("RR", logoBox, juce::Justification::centred, false);

    g.setColour (theme::text);
    g.setFont (theme::font (14.5f, true));
    g.drawText ("LIVE STUDIO", 72, 22, getWidth() - 84, 18, juce::Justification::centredLeft, true);
    g.setColour (theme::textDim);
    g.setFont (theme::font (9.5f));
    g.drawText ("REAL-TIME VOCAL ENGINE", 72, 41, getWidth() - 84, 14,
                juce::Justification::centredLeft, true);

    g.setColour (theme::border.withAlpha (0.7f));
    g.drawHorizontalLine (80, 16.0f, (float) getWidth() - 16.0f);

    const auto bottomTop = getHeight() - 74;
    g.setColour (theme::border.withAlpha (0.7f));
    g.drawHorizontalLine (bottomTop, 16.0f, (float) getWidth() - 16.0f);

    g.setColour (audioReady ? theme::green : theme::red);
    g.fillEllipse (18.0f, (float) bottomTop + 15.0f, 7.0f, 7.0f);

    g.setColour (theme::textDim);
    g.setFont (theme::font (10.5f));
    g.drawText (statusText, 32, bottomTop + 10, getWidth() - 46, 16,
                juce::Justification::centredLeft, true);

    g.setFont (theme::font (9.5f));
    g.setColour (theme::textDim.withAlpha (0.75f));
    g.drawText ("v1.0.0  |  Rr Studio", 18, bottomTop + 32, getWidth() - 36, 14,
                juce::Justification::centredLeft, true);
}

void Sidebar::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (76);

    const auto buttonHeight = 38;
    const auto gap = 3;

    for (int i = 0; i < numPages; ++i)
    {
        auto row = area.removeFromTop (buttonHeight);
        buttons[(size_t) i]->setBounds (row);
        area.removeFromTop (gap);
    }
}

} // namespace rr
