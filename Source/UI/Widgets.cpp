#include "UI/Widgets.h"

namespace rr
{
void configureLabel (juce::Label& label, float height, juce::Colour colour, bool bold,
                     juce::Justification justification)
{
    label.setFont (theme::font (height, bold));
    label.setColour (juce::Label::textColourId, colour);
    label.setJustificationType (justification);
    label.setInterceptsMouseClicks (false, false);
}

Panel::Panel (juce::String panelTitle, juce::Colour accentColour)
    : title (std::move (panelTitle)), accent (accentColour)
{
    setPaintingIsUnclipped (true);
}

void Panel::setTitle (juce::String newTitle)
{
    title = std::move (newTitle);
    repaint();
}

void Panel::setAccentColour (juce::Colour newAccent)
{
    accent = newAccent;
    repaint();
}

juce::Rectangle<int> Panel::getContentBounds() const
{
    auto area = getLocalBounds().reduced (12);

    if (title.isNotEmpty())
        area.removeFromTop (26);

    return area;
}

void Panel::paint (juce::Graphics& g)
{
    theme::drawPanel (g, getLocalBounds().toFloat().reduced (0.5f), accent);

    if (title.isNotEmpty())
    {
        g.setColour (theme::text);
        g.setFont (theme::font (12.5f, true));
        g.drawText (title.toUpperCase(), 13, 8, getWidth() - 26, 18,
                    juce::Justification::centredLeft, true);

        g.setColour (accent.withAlpha (0.7f));
        g.fillRect (13.0f, 30.0f, 34.0f, 1.5f);
    }
}

//==============================================================================
LevelMeter::LevelMeter (Orientation meterOrientation)
    : orientation (meterOrientation)
{
    setInterceptsMouseClicks (false, false);
}

void LevelMeter::setLevelDb (float newLevelDb, float newPeakDb)
{
    const auto clampedLevel = juce::jmax (-100.0f, newLevelDb);
    const auto clampedPeak = juce::jmax (-100.0f, newPeakDb);

    if (! juce::approximatelyEqual (levelDb, clampedLevel)
        || ! juce::approximatelyEqual (peakDb, clampedPeak))
    {
        levelDb = clampedLevel;
        peakDb = clampedPeak;
        repaint();
    }
}

void LevelMeter::setClip (bool shouldShowClip)
{
    if (clip != shouldShowClip)
    {
        clip = shouldShowClip;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);

    if (area.getWidth() <= 2.0f || area.getHeight() <= 2.0f)
        return;

    constexpr float minDb = -60.0f;
    constexpr float maxDb = 0.0f;
    constexpr int numSegments = 22;

    if (orientation == Orientation::vertical)
    {
        auto ledArea = area.removeFromTop (5.0f);
        g.setColour (clip ? theme::red : theme::border.darker (0.1f));
        g.fillRoundedRectangle (ledArea, 1.5f);
        area.removeFromTop (2.0f);

        const auto segmentHeight = area.getHeight() / (float) numSegments;
        const auto levelPosition = juce::jlimit (0.0f, 1.0f, (levelDb - minDb) / (maxDb - minDb));

        for (int i = 0; i < numSegments; ++i)
        {
            const auto low = minDb + (float) i / (float) numSegments * (maxDb - minDb);
            const auto lit = levelPosition * (float) numSegments > (float) i;
            auto segment = juce::Rectangle<float> (area.getX(),
                                                   area.getBottom() - (float) (i + 1) * segmentHeight + 0.75f,
                                                   area.getWidth(),
                                                   juce::jmax (1.0f, segmentHeight - 1.5f));
            g.setColour (lit ? theme::meterColourForDb (low) : theme::panelAlt);
            g.fillRoundedRectangle (segment, 1.0f);
        }

        if (peakDb > minDb)
        {
            const auto y = area.getBottom() - juce::jlimit (0.0f, 1.0f, (peakDb - minDb) / (maxDb - minDb)) * area.getHeight();
            g.setColour (theme::text.withAlpha (0.85f));
            g.fillRect (area.getX(), y - 0.75f, area.getWidth(), 1.5f);
        }
    }
    else
    {
        auto ledArea = area.removeFromRight (5.0f);
        g.setColour (clip ? theme::red : theme::border.darker (0.1f));
        g.fillRoundedRectangle (ledArea, 1.5f);
        area.removeFromRight (2.0f);

        const auto segmentWidth = area.getWidth() / (float) numSegments;
        const auto levelPosition = juce::jlimit (0.0f, 1.0f, (levelDb - minDb) / (maxDb - minDb));

        for (int i = 0; i < numSegments; ++i)
        {
            const auto low = minDb + (float) i / (float) numSegments * (maxDb - minDb);
            const auto lit = levelPosition * (float) numSegments > (float) i;
            auto segment = juce::Rectangle<float> (area.getX() + (float) i * segmentWidth + 0.75f,
                                                   area.getY(),
                                                   juce::jmax (1.0f, segmentWidth - 1.5f),
                                                   area.getHeight());
            g.setColour (lit ? theme::meterColourForDb (low) : theme::panelAlt);
            g.fillRoundedRectangle (segment, 1.0f);
        }

        if (peakDb > minDb)
        {
            const auto x = area.getX() + juce::jlimit (0.0f, 1.0f, (peakDb - minDb) / (maxDb - minDb)) * area.getWidth();
            g.setColour (theme::text.withAlpha (0.85f));
            g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());
        }
    }
}

//==============================================================================
ParamSlider::ParamSlider (ParamStore& parameters, ParamId parameter,
                          juce::Slider::SliderStyle style, bool isCompact)
    : params (parameters), id (parameter), compact (isCompact)
{
    const auto& info = ParamStore::info (id);

    slider.setSliderStyle (style);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRange (info.minValue, info.maxValue,
                     info.decimals == 0 ? 1.0 : info.decimals == 1 ? 0.1 : 0.01);
    slider.setValue (params.get (id), juce::dontSendNotification);
    slider.setDoubleClickReturnValue (true, info.defaultValue);

    if (info.skew > 0.0f && ! juce::approximatelyEqual (info.skew, 1.0f))
        slider.setSkewFactor (info.skew);

    slider.onValueChange = [this] { sliderChanged(); };
    slider.setTooltip (juce::String (info.label) + " (" + info.key + ")");

    configureLabel (nameLabel, compact ? 10.0f : 11.0f, theme::textDim);
    nameLabel.setText (info.label, juce::dontSendNotification);

    configureLabel (valueLabel, compact ? 10.0f : 11.0f, theme::text, true, juce::Justification::centred);
    valueLabel.setFont (theme::monoFont (compact ? 10.0f : 11.0f, false));
    valueLabel.setText (params.getDisplayValue (id), juce::dontSendNotification);

    addAndMakeVisible (slider);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (valueLabel);

    params.addChangeListener (this);
}

ParamSlider::~ParamSlider()
{
    params.removeChangeListener (this);
}

void ParamSlider::refresh()
{
    if (! slider.isMouseButtonDown())
        slider.setValue (params.get (id), juce::dontSendNotification);

    valueLabel.setText (params.getDisplayValue (id), juce::dontSendNotification);
}

void ParamSlider::setAccentColour (juce::Colour colour)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setColour (juce::Slider::thumbColourId, colour);
    slider.setColour (juce::Slider::trackColourId, colour);
}

void ParamSlider::resized()
{
    auto area = getLocalBounds();

    if (slider.getSliderStyle() == juce::Slider::Rotary
        || slider.getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag)
    {
        nameLabel.setBounds (area.removeFromTop (compact ? 14 : 17));
        valueLabel.setBounds (area.removeFromBottom (compact ? 14 : 17));
        slider.setBounds (area.reduced (compact ? 1 : 3));
    }
    else if (slider.getSliderStyle() == juce::Slider::LinearVertical)
    {
        nameLabel.setBounds (area.removeFromTop (compact ? 13 : 16));
        valueLabel.setBounds (area.removeFromBottom (compact ? 13 : 16));
        slider.setBounds (area.reduced (3, 1));
    }
    else
    {
        nameLabel.setBounds (area.removeFromLeft (compact ? 66 : 92));
        valueLabel.setBounds (area.removeFromRight (compact ? 48 : 62));
        slider.setBounds (area.reduced (2, 0));
    }
}

void ParamSlider::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);
    refresh();
}

void ParamSlider::sliderChanged()
{
    params.set (id, (float) slider.getValue());
    valueLabel.setText (params.getDisplayValue (id), juce::dontSendNotification);
}

//==============================================================================
ParamToggle::ParamToggle (ParamStore& parameters, ParamId parameter, juce::String overrideText)
    : params (parameters), id (parameter)
{
    const auto& info = ParamStore::info (id);
    button.setButtonText (overrideText.isNotEmpty() ? overrideText : info.label);
    button.setToggleState (params.getBool (id), juce::dontSendNotification);
    button.setTooltip (info.key);
    button.onClick = [this]
    {
        params.setBool (id, button.getToggleState());
    };

    addAndMakeVisible (button);
    params.addChangeListener (this);
}

ParamToggle::~ParamToggle()
{
    params.removeChangeListener (this);
}

void ParamToggle::refresh()
{
    button.setToggleState (params.getBool (id), juce::dontSendNotification);
}

void ParamToggle::resized()
{
    button.setBounds (getLocalBounds());
}

void ParamToggle::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);

    if (! button.isMouseButtonDown())
        button.setToggleState (params.getBool (id), juce::dontSendNotification);
}

//==============================================================================
ParamCombo::ParamCombo (ParamStore& parameters, ParamId parameter,
                        juce::StringArray items, juce::String labelText)
    : params (parameters), id (parameter)
{
    const auto& info = ParamStore::info (id);

    configureLabel (label, 11.5f, theme::textDim);
    label.setText (labelText.isNotEmpty() ? labelText : info.label, juce::dontSendNotification);

    for (int i = 0; i < items.size(); ++i)
        combo.addItem (items[i], i + 1);

    combo.setSelectedItemIndex (juce::jlimit (0, juce::jmax (0, items.size() - 1), params.getInt (id)),
                                juce::dontSendNotification);
    combo.onChange = [this]
    {
        params.set (id, (float) juce::jmax (0, combo.getSelectedItemIndex()));
    };

    addAndMakeVisible (label);
    addAndMakeVisible (combo);
    params.addChangeListener (this);
}

ParamCombo::~ParamCombo()
{
    params.removeChangeListener (this);
}

void ParamCombo::setLabelText (juce::String newText)
{
    label.setText (std::move (newText), juce::dontSendNotification);
}

void ParamCombo::refresh()
{
    if (! combo.isPopupActive())
        combo.setSelectedItemIndex (juce::jlimit (0, juce::jmax (0, combo.getNumItems() - 1), params.getInt (id)),
                                    juce::dontSendNotification);
}

void ParamCombo::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromLeft (juce::jmin (90, area.getWidth() / 2)));
    combo.setBounds (area);
}

void ParamCombo::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);
    refresh();
}

//==============================================================================
SmartEqDisplay::SmartEqDisplay (ParamStore& parameters, SmartEQ& equaliser)
    : params (parameters), eq (equaliser)
{
    setOpaque (false);
    startTimerHz (12);
}

SmartEqDisplay::~SmartEqDisplay()
{
    stopTimer();
}

void SmartEqDisplay::setPreview (const SmartEqAnalysis& analysis)
{
    preview = analysis;
    hasPreview = analysis.hasData;
    repaint();
}

void SmartEqDisplay::clearPreview()
{
    hasPreview = false;
    preview = {};
    repaint();
}

void SmartEqDisplay::timerCallback()
{
    if (! isShowing())
        return;

    eq.getSpectrum (spectrum);
    repaint();
}

float SmartEqDisplay::frequencyToX (float hz) const noexcept
{
    auto plot = getLocalBounds().toFloat().reduced (2.0f).withTrimmedLeft (28.0f)
                    .withTrimmedRight (8.0f).withTrimmedTop (8.0f).withTrimmedBottom (18.0f);
    const auto normalised = std::log10 (juce::jmax (20.0f, hz) / 20.0f) / std::log10 (1000.0f);
    return plot.getX() + juce::jlimit (0.0f, 1.0f, normalised) * plot.getWidth();
}

float SmartEqDisplay::responseDb (float hz) const noexcept
{
    double total = 0.0;

    for (int band = 0; band < 5; ++band)
    {
        const auto frequency = params.get (SmartEQ::eqParamIds[(size_t) band * 3]);
        const auto gain = params.get (SmartEQ::eqParamIds[(size_t) band * 3 + 1]);
        const auto q = params.get (SmartEQ::eqParamIds[(size_t) band * 3 + 2]);

        const auto octaves = std::log2 (hz / juce::jmax (1.0f, frequency));
        const auto width = 0.75 / juce::jmax (0.25f, q);
        total += gain * std::exp (-0.5 * std::pow (octaves / width, 2.0));
    }

    return (float) juce::jlimit (-24.0, 24.0, total);
}

float SmartEqDisplay::previewResponseDb (float hz) const noexcept
{
    double total = 0.0;

    for (const auto& band : preview.bands)
    {
        const auto octaves = std::log2 (hz / juce::jmax (1.0f, band.frequencyHz));
        const auto width = 0.75 / juce::jmax (0.25f, band.q);
        total += band.gainDb * std::exp (-0.5 * std::pow (octaves / width, 2.0));
    }

    return (float) juce::jlimit (-24.0, 24.0, total);
}

void SmartEqDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto plot = bounds.reduced (2.0f).withTrimmedLeft (28.0f).withTrimmedRight (8.0f)
                     .withTrimmedTop (8.0f).withTrimmedBottom (18.0f);

    g.setColour (theme::background.darker (0.2f));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (theme::border.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    g.setFont (theme::font (9.0f));
    const int frequencyTicks[] = { 50, 100, 500, 1000, 5000, 10000 };

    for (const auto frequency : frequencyTicks)
    {
        const auto x = frequencyToX ((float) frequency);
        g.setColour (theme::border.withAlpha (0.55f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (theme::textDim);
        juce::String text = frequency >= 1000 ? juce::String (frequency / 1000) + "k"
                                             : juce::String (frequency);
        g.drawText (text, (int) x - 12, (int) plot.getBottom() + 2, 24, 12,
                    juce::Justification::centred, false);
    }

    for (const auto db : { -12, 0, 12 })
    {
        const auto y = plot.getBottom() - (float) (db + 18) / 36.0f * plot.getHeight();
        g.setColour (db == 0 ? theme::border.brighter (0.15f) : theme::border.withAlpha (0.4f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (theme::textDim);
        g.drawText (juce::String (db) + " dB", 2, (int) y - 6, 24, 12,
                    juce::Justification::centredRight, false);
    }

    juce::Path spectrumPath;
    const auto numBands = (int) spectrum.size();

    for (int i = 0; i < numBands; ++i)
    {
        const auto x = plot.getX() + (float) i / (float) juce::jmax (1, numBands - 1) * plot.getWidth();
        const auto normalised = juce::jlimit (0.0f, 1.0f, (spectrum[(size_t) i] + 80.0f) / 80.0f);
        const auto y = plot.getBottom() - normalised * plot.getHeight();

        if (i == 0)
            spectrumPath.startNewSubPath (x, y);
        else
            spectrumPath.lineTo (x, y);
    }

    if (numBands > 0)
    {
        auto fillPath = spectrumPath;
        fillPath.lineTo (plot.getRight(), plot.getBottom());
        fillPath.lineTo (plot.getX(), plot.getBottom());
        fillPath.closeSubPath();

        g.setColour (theme::cyan.withAlpha (0.14f));
        g.fillPath (fillPath);
        g.setColour (theme::cyan.withAlpha (0.5f));
        g.strokePath (spectrumPath, juce::PathStrokeType (1.1f));
    }

    auto drawCurve = [&] (bool previewCurve)
    {
        juce::Path path;

        for (int x = 0; x <= (int) plot.getWidth(); x += 2)
        {
            const auto normalised = (float) x / juce::jmax (1.0f, plot.getWidth());
            const auto hz = 20.0f * std::pow (1000.0f, normalised);
            const auto db = previewCurve ? previewResponseDb (hz) : responseDb (hz);
            const auto y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 18.0f) / 36.0f) * plot.getHeight();

            if (x == 0)
                path.startNewSubPath (plot.getX(), y);
            else
                path.lineTo (plot.getX() + (float) x, y);
        }

        g.setColour (previewCurve ? theme::amber.withAlpha (0.8f) : theme::green);
        g.strokePath (path, juce::PathStrokeType (previewCurve ? 1.4f : 2.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    };

    if (hasPreview)
        drawCurve (true);

    drawCurve (false);

    for (int band = 0; band < 5; ++band)
    {
        const auto frequency = params.get (SmartEQ::eqParamIds[(size_t) band * 3]);
        const auto gain = params.get (SmartEQ::eqParamIds[(size_t) band * 3 + 1]);
        const auto x = frequencyToX (frequency);
        const auto y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, (gain + 18.0f) / 36.0f) * plot.getHeight();
        g.setColour (theme::green.withAlpha (0.85f));
        g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
    }

    if (hasPreview)
    {
        for (const auto& band : preview.bands)
        {
            const auto x = frequencyToX (band.frequencyHz);
            const auto y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, (band.gainDb + 18.0f) / 36.0f) * plot.getHeight();
            g.setColour (theme::amber.withAlpha (0.85f));
            g.drawEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.2f);
        }
    }

    g.setFont (theme::font (9.5f, true));
    g.setColour (theme::green.withAlpha (0.9f));
    g.drawText ("EQ", (int) plot.getRight() - 72, (int) plot.getY() + 2, 30, 12,
                juce::Justification::centredRight, false);

    if (hasPreview)
    {
        g.setColour (theme::amber.withAlpha (0.9f));
        g.drawText ("SEBELUM", (int) plot.getRight() - 74, (int) plot.getY() + 14, 72, 12,
                    juce::Justification::centredRight, false);
    }
}

//==============================================================================
void styleActionButton (juce::TextButton& button, juce::Colour accent)
{
    button.setColour (juce::TextButton::buttonColourId, theme::panelAlt);
    button.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.24f));
    button.setColour (juce::TextButton::textColourOffId, theme::text);
    button.setColour (juce::TextButton::textColourOnId, theme::text);
}

void promptForText (const juce::String& title, const juce::String& message,
                    const juce::String& initialText, std::function<void (juce::String)> onAccept)
{
    auto* window = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("value", initialText, "Nama");
    window->addButton ("Simpan", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Batal", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true,
                             juce::ModalCallbackFunction::create (
                                 [window, accept = std::move (onAccept)] (int result)
                                 {
                                     if (result == 1 && accept != nullptr)
                                         accept (window->getTextEditorContents ("value").trim());
                                 }),
                             true);
}

void showInfoMessage (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, title, message);
}

void showWarningMessage (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, message);
}

void showConfirm (const juce::String& title, const juce::String& message,
                  const juce::String& confirmText, std::function<void()> onConfirm)
{
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, title, message,
                                        confirmText, "Batal", nullptr,
                                        juce::ModalCallbackFunction::create (
                                            [confirm = std::move (onConfirm)] (int result)
                                            {
                                                if (result == 1 && confirm != nullptr)
                                                    confirm();
                                            }));
}

} // namespace rr
