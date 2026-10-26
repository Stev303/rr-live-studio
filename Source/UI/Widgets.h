#pragma once

#include "Audio/ParamStore.h"
#include "DSP/SmartEQ.h"
#include "UI/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

namespace rr
{
void configureLabel (juce::Label& label, float height = 12.0f,
                     juce::Colour colour = theme::textDim, bool bold = false,
                     juce::Justification justification = juce::Justification::centredLeft);

void styleActionButton (juce::TextButton& button, juce::Colour accent = theme::cyan);
void promptForText (const juce::String& title, const juce::String& message,
                    const juce::String& initialText, std::function<void (juce::String)> onAccept);
void showInfoMessage (const juce::String& title, const juce::String& message);
void showWarningMessage (const juce::String& title, const juce::String& message);
void showConfirm (const juce::String& title, const juce::String& message,
                  const juce::String& confirmText, std::function<void()> onConfirm);

/** Rounded panel with an optional title. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String title = {}, juce::Colour accent = theme::border);
    ~Panel() override = default;

    void setTitle (juce::String newTitle);
    void setAccentColour (juce::Colour newAccent);
    juce::Rectangle<int> getContentBounds() const;
    void paint (juce::Graphics& g) override;

private:
    juce::String title;
    juce::Colour accent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panel)
};

/** Segmented peak meter. */
class LevelMeter : public juce::Component
{
public:
    enum class Orientation { vertical, horizontal };

    explicit LevelMeter (Orientation orientation = Orientation::vertical);
    ~LevelMeter() override = default;

    void setLevelDb (float newLevelDb, float newPeakDb = -100.0f);
    void setClip (bool shouldShowClip);
    void paint (juce::Graphics& g) override;

private:
    Orientation orientation;
    float levelDb = -100.0f;
    float peakDb = -100.0f;
    bool clip = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

/** Rotary or linear slider bound to a ParamStore parameter. */
class ParamSlider : public juce::Component,
                    private juce::ChangeListener
{
public:
    ParamSlider (ParamStore& parameters, ParamId parameter,
                 juce::Slider::SliderStyle style = juce::Slider::Rotary,
                 bool compact = false);
    ~ParamSlider() override;

    void refresh();
    void setAccentColour (juce::Colour colour);
    juce::Slider& getSlider() noexcept { return slider; }

    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void sliderChanged();

    ParamStore& params;
    ParamId id;
    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    bool compact = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamSlider)
};

/** Toggle button bound to an on/off parameter. */
class ParamToggle : public juce::Component,
                    private juce::ChangeListener
{
public:
    ParamToggle (ParamStore& parameters, ParamId parameter, juce::String overrideText = {});
    ~ParamToggle() override;

    void refresh();
    void resized() override;
    juce::ToggleButton& getButton() noexcept { return button; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    ParamStore& params;
    ParamId id;
    juce::ToggleButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamToggle)
};

/** Combo box bound to an integer parameter. */
class ParamCombo : public juce::Component,
                   private juce::ChangeListener
{
public:
    ParamCombo (ParamStore& parameters, ParamId parameter,
                juce::StringArray items, juce::String labelText = {});
    ~ParamCombo() override;

    void refresh();
    void resized() override;
    juce::ComboBox& getComboBox() noexcept { return combo; }
    void setLabelText (juce::String newText);

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    ParamStore& params;
    ParamId id;
    juce::Label label;
    juce::ComboBox combo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamCombo)
};

/** Spectrum + EQ curve display for the Smart EQ. */
class SmartEqDisplay : public juce::Component,
                       private juce::Timer
{
public:
    SmartEqDisplay (ParamStore& parameters, SmartEQ& equaliser);
    ~SmartEqDisplay() override;

    void setPreview (const SmartEqAnalysis& analysis);
    void clearPreview();
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    float frequencyToX (float hz) const noexcept;
    float responseDb (float hz) const noexcept;
    float previewResponseDb (float hz) const noexcept;

    ParamStore& params;
    SmartEQ& eq;
    std::array<float, SmartEQ::numSpectrumBands> spectrum {};
    SmartEqAnalysis preview;
    bool hasPreview = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartEqDisplay)
};

} // namespace rr
