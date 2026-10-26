#include "UI/Theme.h"

namespace rr::theme
{
juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font monoFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Consolas", height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent, float cornerSize)
{
    juce::ColourGradient gradient (panel.brighter (0.025f), bounds.getTopLeft(),
                                   panel.darker (0.12f), bounds.getBottomRight(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, cornerSize);

    g.setColour (accent.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);
}

juce::Colour meterColourForDb (float db)
{
    if (db >= -0.5f)
        return red;

    if (db >= -3.0f)
        return amber;

    if (db >= -12.0f)
        return cyan;

    return green;
}
} // namespace rr::theme

namespace rr
{
RrLookAndFeel::RrLookAndFeel()
{
    setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme());

    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::DocumentWindow::textColourId, theme::text);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, theme::panelAlt);
    setColour (juce::TextButton::buttonOnColourId, theme::cyan.withAlpha (0.28f));
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::text);
    setColour (juce::ToggleButton::textColourId, theme::text);
    setColour (juce::ToggleButton::tickColourId, theme::cyan);
    setColour (juce::ToggleButton::tickDisabledColourId, theme::border);
    setColour (juce::Slider::thumbColourId, theme::cyan);
    setColour (juce::Slider::rotarySliderFillColourId, theme::cyan);
    setColour (juce::Slider::rotarySliderOutlineColourId, theme::border);
    setColour (juce::Slider::trackColourId, theme::cyan);
    setColour (juce::Slider::backgroundColourId, theme::panelAlt);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, theme::panelAlt);
    setColour (juce::ComboBox::outlineColourId, theme::border);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::arrowColourId, theme::cyan);
    setColour (juce::ComboBox::buttonColourId, theme::panelAlt);
    setColour (juce::PopupMenu::backgroundColourId, theme::panel);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::cyan.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, theme::text);
    setColour (juce::TextEditor::backgroundColourId, theme::panelAlt);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::outlineColourId, theme::border);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::cyan);
    setColour (juce::TextEditor::highlightColourId, theme::cyan.withAlpha (0.3f));
    setColour (juce::ScrollBar::thumbColourId, theme::border.brighter (0.3f));
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, theme::border);
    setColour (juce::ListBox::textColourId, theme::text);
    setColour (juce::GroupComponent::outlineColourId, theme::border);
    setColour (juce::GroupComponent::textColourId, theme::textDim);
    setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedComponent::outlineColourId, theme::border);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, theme::border);
    setColour (juce::TabbedButtonBar::tabTextColourId, theme::textDim);
    setColour (juce::TabbedButtonBar::frontTextColourId, theme::text);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, theme::cyan);
    setColour (juce::AlertWindow::backgroundColourId, theme::panel);
    setColour (juce::AlertWindow::textColourId, theme::text);
    setColour (juce::AlertWindow::outlineColourId, theme::border);
}

void RrLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPosProportional, float rotaryStartAngle,
                                      float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto arcRadius = radius - 3.0f;

    g.setColour (theme::panelAlt.darker (0.2f));
    g.fillEllipse (juce::Rectangle<float> (radius * 1.45f, radius * 1.45f).withCentre (centre));

    g.setColour (theme::border);
    g.drawEllipse (juce::Rectangle<float> (radius * 1.45f, radius * 1.45f).withCentre (centre), 1.0f);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (theme::border);
    g.strokePath (backgroundArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, angle, true);

        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (valueArc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    const auto pointerLength = radius * 0.62f;
    const auto pointerThickness = juce::jmax (1.5f, radius * 0.07f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength,
                                 pointerThickness, pointerLength, pointerThickness * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (theme::text);
    g.fillPath (pointer);

    g.setColour (theme::background.withAlpha (0.7f));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.42f, radius * 0.42f).withCentre (centre));
}

void RrLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float minSliderPos, float maxSliderPos,
                                      juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);

    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    const auto vertical = style == juce::Slider::LinearVertical
                       || style == juce::Slider::LinearBarVertical
                       || style == juce::Slider::IncDecButtons;

    if (vertical)
    {
        const auto centreX = (float) x + (float) width * 0.5f;
        auto track = juce::Rectangle<float> (centreX - 2.0f, (float) y + 7.0f,
                                             4.0f, (float) height - 14.0f);
        g.setColour (theme::panelAlt);
        g.fillRoundedRectangle (track, 2.0f);

        auto filled = track.withTop (juce::jlimit (track.getY(), track.getBottom(), sliderPos));
        g.setColour (accent.withAlpha (0.35f));
        g.fillRoundedRectangle (filled.expanded (2.0f, 0.0f), 3.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (filled, 2.0f);

        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (14.0f, 10.0f)
                           .withCentre ({ centreX, juce::jlimit ((float) y + 6.0f,
                                                                 (float) y + height - 6.0f, sliderPos) }));
    }
    else
    {
        const auto centreY = (float) y + (float) height * 0.5f;
        auto track = juce::Rectangle<float> ((float) x + 7.0f, centreY - 2.0f,
                                             (float) width - 14.0f, 4.0f);
        g.setColour (theme::panelAlt);
        g.fillRoundedRectangle (track, 2.0f);

        auto filled = track.withRight (juce::jlimit (track.getX(), track.getRight(), sliderPos));
        g.setColour (accent.withAlpha (0.35f));
        g.fillRoundedRectangle (filled.expanded (0.0f, 2.0f), 3.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (filled, 2.0f);

        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 14.0f)
                           .withCentre ({ juce::jlimit ((float) x + 6.0f,
                                                        (float) x + width - 6.0f, sliderPos), centreY }));
    }
}

void RrLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                          const juce::Colour& backgroundColour,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = juce::jmin (8.0f, bounds.getHeight() * 0.3f);
    auto base = backgroundColour;

    if (base.isTransparent())
        base = theme::panelAlt;

    if (button.getToggleState())
        base = base.overlaidWith (theme::cyan.withAlpha (0.22f));

    if (shouldDrawButtonAsDown)
        base = base.darker (0.25f);
    else if (shouldDrawButtonAsHighlighted)
        base = base.brighter (0.1f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);

    const auto outline = button.getToggleState() ? theme::cyan.withAlpha (0.8f)
                                                 : theme::border;
    g.setColour (outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

void RrLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                  int buttonX, int buttonY, int buttonW, int buttonH,
                                  juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown);
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    juce::Path arrow;
    const auto centre = juce::Point<float> ((float) buttonX + (float) buttonW * 0.5f,
                                            (float) buttonY + (float) buttonH * 0.5f);
    arrow.startNewSubPath (centre.x - 4.0f, centre.y - 2.0f);
    arrow.lineTo (centre.x, centre.y + 2.5f);
    arrow.lineTo (centre.x + 4.0f, centre.y - 2.0f);
    g.setColour (theme::cyan);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void RrLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto boxSize = juce::jmin (18.0f, bounds.getHeight() - 4.0f);
    auto boxBounds = juce::Rectangle<float> (0.0f, 0.0f, boxSize, boxSize)
                         .withCentre ({ bounds.getX() + boxSize * 0.5f + 2.0f, bounds.getCentreY() });

    g.setColour (button.getToggleState() ? theme::cyan.withAlpha (0.22f) : theme::panelAlt);
    g.fillRoundedRectangle (boxBounds, 4.0f);
    g.setColour (button.getToggleState() ? theme::cyan : theme::border.brighter (0.2f));
    g.drawRoundedRectangle (boxBounds, 4.0f, 1.2f);

    if (button.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (boxBounds.getX() + 4.0f, boxBounds.getCentreY());
        tick.lineTo (boxBounds.getX() + 7.5f, boxBounds.getBottom() - 5.0f);
        tick.lineTo (boxBounds.getRight() - 3.5f, boxBounds.getY() + 4.5f);
        g.setColour (theme::cyan);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    g.setColour (button.findColour (juce::ToggleButton::textColourId)
                       .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.setFont (theme::font (12.0f, shouldDrawButtonAsDown));
    g.drawText (button.getButtonText(),
                bounds.withTrimmedLeft (boxSize + 8.0f).toNearestInt(),
                juce::Justification::centredLeft, true);

    if (shouldDrawButtonAsHighlighted)
    {
        g.setColour (theme::cyan.withAlpha (0.12f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    }
}

void RrLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                    bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId)
                       .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));

    const auto alignLeft = (bool) button.getProperties().getWithDefault ("alignLeft", false);
    const auto area = button.getLocalBounds();

    if (alignLeft)
    {
        if (button.getToggleState())
        {
            g.setColour (theme::cyan);
            g.fillRoundedRectangle (7.0f, (float) area.getHeight() * 0.28f, 3.0f,
                                    (float) area.getHeight() * 0.44f, 1.5f);
        }

        g.drawText (button.getButtonText(), area.withTrimmedLeft (18).withTrimmedRight (8),
                    juce::Justification::centredLeft, true);
    }
    else
    {
        g.drawText (button.getButtonText(), area.reduced (6, 0),
                    juce::Justification::centred, true);
    }
}

juce::Font RrLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return theme::font (juce::jlimit (12.0f, 16.0f, (float) buttonHeight * 0.38f), true);
}

juce::Font RrLookAndFeel::getPopupMenuFont()
{
    return theme::font (13.0f);
}

juce::Font RrLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return theme::font (12.5f);
}

juce::Font RrLookAndFeel::getLabelFont (juce::Label&)
{
    return theme::font (12.5f);
}

} // namespace rr
