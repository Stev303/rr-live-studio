#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rr::theme
{
    inline const juce::Colour background   { 0xff05080f };
    inline const juce::Colour panel        { 0xff0b1322 };
    inline const juce::Colour panelAlt     { 0xff111c2f };
    inline const juce::Colour border       { 0xff1d2c46 };
    inline const juce::Colour text         { 0xffe7f1ff };
    inline const juce::Colour textDim      { 0xff7d90aa };
    inline const juce::Colour cyan         { 0xff20e6ff };
    inline const juce::Colour magenta      { 0xffff3d9a };
    inline const juce::Colour green        { 0xff3dffb0 };
    inline const juce::Colour amber        { 0xffffc857 };
    inline const juce::Colour red          { 0xffff4d6d };

    juce::Font font (float height, bool bold = false);
    juce::Font monoFont (float height, bool bold = false);
    void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                    juce::Colour accent = border, float cornerSize = 10.0f);
    juce::Colour meterColourForDb (float db);
}

namespace rr
{
/** Dark neon look and feel used by the whole application. */
class RrLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RrLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override;
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RrLookAndFeel)
};
} // namespace rr
