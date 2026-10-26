#include "UI/RoutingDiagram.h"

namespace rr
{
RoutingDiagram::RoutingDiagram (AppState& state, bool compactView)
    : app (state), compact (compactView)
{
    startTimerHz (10);
}

RoutingDiagram::~RoutingDiagram()
{
    stopTimer();
}

void RoutingDiagram::timerCallback()
{
    if (isShowing())
        repaint();
}

void RoutingDiagram::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::background.darker (0.15f));
    g.fillRoundedRectangle (bounds, 8.0f);

    if (bounds.getWidth() < 120.0f || bounds.getHeight() < 90.0f)
        return;

    const auto meters = app.engine.getMeters();
    const auto width = bounds.getWidth();
    const auto height = bounds.getHeight();

    auto makeRect = [width, height] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (width * x, height * y, width * w, height * h);
    };

    auto drawNode = [&] (juce::Rectangle<float> area, const juce::String& title,
                         const juce::String& subtitle, juce::Colour accent,
                         float levelDb, bool active)
    {
        auto r = area.reduced (1.0f);
        g.setColour (theme::panelAlt.withAlpha (0.94f));
        g.fillRoundedRectangle (r, 5.0f);

        if (active && levelDb > -60.0f)
        {
            g.setColour (accent.withAlpha (0.10f));
            g.fillRoundedRectangle (r, 5.0f);
        }

        g.setColour (accent.withAlpha (active ? 0.75f : 0.32f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

        auto textArea = r.reduced (5.0f, 2.0f);
        auto titleArea = textArea.removeFromTop (compact ? 11.0f : 14.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (compact ? 8.5f : 10.0f, true));
        g.drawText (title, titleArea.toNearestInt(), juce::Justification::centredLeft, true);

        if (r.getHeight() > 26.0f)
        {
            g.setColour (theme::textDim);
            g.setFont (theme::font (compact ? 7.2f : 8.2f));
            auto subArea = textArea.removeFromTop (compact ? 9.0f : 12.0f);
            g.drawText (subtitle, subArea.toNearestInt(), juce::Justification::centredLeft, true);
        }

        auto bar = r.removeFromBottom (3.0f).reduced (4.0f, 0.0f);
        bar.setY (bar.getY() - 2.0f);
        g.setColour (accent.withAlpha (0.14f));
        g.fillRoundedRectangle (bar, 1.0f);
        const auto norm = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
        g.setColour (accent.withAlpha (0.8f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * norm), 1.0f);
    };

    auto drawConnection = [&] (juce::Point<float> from, juce::Point<float> to,
                               juce::Colour colour, bool active)
    {
        juce::Path path;
        path.startNewSubPath (from);
        const auto midX = (from.x + to.x) * 0.5f;
        path.cubicTo (midX, from.y, midX, to.y, to.x, to.y);

        g.setColour (colour.withAlpha (active ? 0.7f : 0.22f));
        g.strokePath (path, juce::PathStrokeType (active ? 1.5f : 1.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        juce::Path arrow;
        arrow.addTriangle (to.x - 5.0f, to.y - 3.5f, to.x, to.y, to.x - 5.0f, to.y + 3.5f);
        g.setColour (colour.withAlpha (active ? 0.8f : 0.28f));
        g.fillPath (arrow);
    };

    auto rightPort = [] (juce::Rectangle<float> r, float yFraction = 0.5f)
    {
        return juce::Point<float> (r.getRight(), r.getY() + r.getHeight() * yFraction);
    };
    auto leftPort = [] (juce::Rectangle<float> r, float yFraction = 0.5f)
    {
        return juce::Point<float> (r.getX(), r.getY() + r.getHeight() * yFraction);
    };

    const auto micGain = app.params.getDisplayValue (ParamId::micGainDb);
    const auto micMuted = app.params.getBool (ParamId::micMute);
    const auto musicMuted = app.params.getBool (ParamId::musicMute);
    const auto samplerMuted = app.params.getBool (ParamId::samplerMute);
    const auto correction = app.params.getBool (ParamId::pitchCorrectionEnabled);
    const auto harmony = app.params.getBool (ParamId::harmonyEnabled) && ! app.params.getBool (ParamId::harmonyBypass);
    const auto balanceOn = app.params.getBool (ParamId::sbEnabled);
    const auto boostBypassed = app.params.getBool (ParamId::boosterBypass);
    const auto pitchBypassed = app.params.getBool (ParamId::pitchBypass);
    const auto limiterOn = app.params.getBool (ParamId::masterLimiterEnabled);
    const auto testOn = app.params.getBool (ParamId::testToneEnabled);

    if (! compact)
    {
        const auto mic = makeRect (0.010f, 0.06f, 0.110f, 0.22f);
        const auto chain = makeRect (0.145f, 0.06f, 0.130f, 0.22f);
        const auto vocalVst = makeRect (0.300f, 0.06f, 0.130f, 0.22f);
        const auto sum = makeRect (0.460f, 0.06f, 0.090f, 0.88f);
        const auto masterVst = makeRect (0.585f, 0.36f, 0.125f, 0.28f);
        const auto limiter = makeRect (0.735f, 0.36f, 0.125f, 0.28f);
        const auto output = makeRect (0.885f, 0.36f, 0.105f, 0.28f);

        const auto loopback = makeRect (0.010f, 0.39f, 0.110f, 0.22f);
        const auto pitch = makeRect (0.145f, 0.39f, 0.130f, 0.22f);
        const auto balance = makeRect (0.300f, 0.39f, 0.130f, 0.22f);

        const auto samplerNode = makeRect (0.010f, 0.72f, 0.110f, 0.22f);
        const auto samplerFader = makeRect (0.145f, 0.72f, 0.130f, 0.22f);

        const auto vocalVstCount = app.pluginHost.getChainSize (false);
        const auto masterVstCount = app.pluginHost.getChainSize (true);
        const auto endpointShort = app.engine.isLoopbackCapturing()
                                     ? "WASAPI CAPTURE"
                                     : "WASAPI OFF";

        drawNode (mic, "MIC INPUT",
                  micMuted ? "MUTED" : "Input " + micGain,
                  theme::cyan, meters.micPeakDb, ! micMuted);

        drawNode (chain, "VOCAL CHAIN",
                  juce::String (correction ? "Koreksi ON" : "Koreksi OFF") + " / "
                      + (harmony ? "Harmony ON" : "Harmony OFF"),
                  theme::cyan, meters.vocalChainPeakDb, meters.micPeakDb > -60.0f);

        drawNode (vocalVst, "VOCAL VST3",
                  juce::String (vocalVstCount) + " plugin / "
                      + juce::String (app.pluginHost.getChainLatencySamples (false)) + " smp",
                  theme::cyan, meters.vocalBusPeakDb, meters.vocalBusPeakDb > -60.0f);

        drawNode (loopback, "YOUTUBE LOOPBACK", endpointShort,
                  theme::magenta, meters.loopbackPeakDb, app.engine.isLoopbackCapturing());

        drawNode (pitch, "PITCH + BOOSTER",
                  (pitchBypassed ? "Pitch bypass" : "Pitch " + app.params.getDisplayValue (ParamId::transpose))
                      + (boostBypassed ? " / Booster OFF" : " / Booster ON"),
                  theme::magenta, meters.musicBusPeakDb, app.engine.isLoopbackCapturing());

        drawNode (balance, "SMART BALANCE",
                  (balanceOn ? "ON" : "OFF") + juce::String (" / duck ")
                      + juce::String (meters.smartBalanceGrDb, 1) + " dB",
                  theme::magenta, meters.musicBusPeakDb, balanceOn);

        drawNode (samplerNode, "SAMPLER PADS",
                  juce::String (app.sampler.getActiveVoiceCount()) + " aktif",
                  theme::green, meters.samplerPeakDb, app.sampler.getActiveVoiceCount() > 0);

        drawNode (samplerFader, "SAMPLER FADER",
                  samplerMuted ? "MUTED" : app.params.getDisplayValue (ParamId::samplerMixDb),
                  theme::green, meters.samplerPeakDb, ! samplerMuted);

        juce::String sumSubtitle = "Gain " + app.params.getDisplayValue (ParamId::masterGainDb);

        if (testOn)
            sumSubtitle << " / TEST";

        drawNode (sum, "MIX BUS", sumSubtitle, theme::amber, meters.masterPeakDb, true);

        drawNode (masterVst, "MASTER VST3",
                  juce::String (masterVstCount) + " plugin",
                  theme::amber, meters.masterPeakDb, true);

        drawNode (limiter, "LIMITER",
                  limiterOn ? (juce::String (meters.masterLimiterGrDb, 1) + " dB GR")
                            : "BYPASS",
                  theme::amber, meters.masterPeakDb, limiterOn);

        drawNode (output, "OUTPUT",
                  micMuted && musicMuted && samplerMuted ? "SILENT" : "USB / HP / VCABLE",
                  theme::cyan, meters.masterPeakDb, true);

        drawConnection (rightPort (mic), leftPort (chain), theme::cyan, meters.micPeakDb > -60.0f);
        drawConnection (rightPort (chain), leftPort (vocalVst), theme::cyan, meters.vocalChainPeakDb > -60.0f);
        drawConnection (rightPort (vocalVst), leftPort (sum, 0.18f), theme::cyan, meters.vocalBusPeakDb > -60.0f);

        drawConnection (rightPort (loopback), leftPort (pitch), theme::magenta, app.engine.isLoopbackCapturing());
        drawConnection (rightPort (pitch), leftPort (balance), theme::magenta, meters.musicBusPeakDb > -60.0f);
        drawConnection (rightPort (balance), leftPort (sum, 0.48f), theme::magenta, meters.musicBusPeakDb > -60.0f);

        drawConnection (rightPort (samplerNode), leftPort (samplerFader), theme::green, meters.samplerPeakDb > -60.0f);
        drawConnection (rightPort (samplerFader), leftPort (sum, 0.78f), theme::green, meters.samplerPeakDb > -60.0f);

        drawConnection (rightPort (sum), leftPort (masterVst), theme::amber, meters.masterPeakDb > -60.0f);
        drawConnection (rightPort (masterVst), leftPort (limiter), theme::amber, meters.masterPeakDb > -60.0f);
        drawConnection (rightPort (limiter), leftPort (output), theme::cyan, meters.masterPeakDb > -60.0f);

        if (width > 560.0f)
        {
            g.setFont (theme::font (7.0f));
            g.setColour (theme::textDim.withAlpha (0.65f));
            g.drawText ("VOCAL", (int) (sum.getX() - 44), (int) mic.getCentreY() - 5, 38, 10,
                        juce::Justification::centredRight, false);
            g.drawText ("MUSIC", (int) (sum.getX() - 44), (int) loopback.getCentreY() - 5, 38, 10,
                        juce::Justification::centredRight, false);
            g.drawText ("SAMPLER", (int) (sum.getX() - 44), (int) samplerNode.getCentreY() - 5, 38, 10,
                        juce::Justification::centredRight, false);
        }
    }
    else
    {
        const auto mic = makeRect (0.012f, 0.05f, 0.190f, 0.22f);
        const auto vocal = makeRect (0.245f, 0.05f, 0.270f, 0.22f);
        const auto loopback = makeRect (0.012f, 0.39f, 0.190f, 0.22f);
        const auto music = makeRect (0.245f, 0.39f, 0.270f, 0.22f);
        const auto samplerNode = makeRect (0.012f, 0.73f, 0.190f, 0.22f);
        const auto sum = makeRect (0.555f, 0.05f, 0.120f, 0.90f);
        const auto master = makeRect (0.715f, 0.33f, 0.150f, 0.34f);
        const auto output = makeRect (0.890f, 0.37f, 0.100f, 0.26f);

        drawNode (mic, "MIC", micMuted ? "MUTED" : micGain,
                  theme::cyan, meters.micPeakDb, ! micMuted);

        drawNode (vocal, "VOCAL + VST",
                  juce::String (app.pluginHost.getChainSize (false)) + " plugin / "
                      + (correction ? "C" : "-") + (harmony ? "H" : "-"),
                  theme::cyan, meters.vocalBusPeakDb, meters.micPeakDb > -60.0f);

        drawNode (loopback, "LOOPBACK",
                  app.engine.isLoopbackCapturing() ? "WASAPI ON" : "WASAPI OFF",
                  theme::magenta, meters.loopbackPeakDb, app.engine.isLoopbackCapturing());

        drawNode (music, "PITCH / BOOST / BAL",
                  (pitchBypassed ? "Pitch -" : app.params.getDisplayValue (ParamId::transpose))
                      + (balanceOn ? " / Duck ON" : " / Duck OFF"),
                  theme::magenta, meters.musicBusPeakDb, app.engine.isLoopbackCapturing());

        drawNode (samplerNode, "SAMPLER",
                  juce::String (app.sampler.getActiveVoiceCount()) + " aktif / "
                      + app.params.getDisplayValue (ParamId::samplerMixDb),
                  theme::green, meters.samplerPeakDb, app.sampler.getActiveVoiceCount() > 0);

        drawNode (sum, "MIX", app.params.getDisplayValue (ParamId::masterGainDb),
                  theme::amber, meters.masterPeakDb, true);

        drawNode (master, "MASTER + LIMIT",
                  juce::String (app.pluginHost.getChainSize (true)) + " plugin / "
                      + (limiterOn ? "LIMIT" : "BYPASS"),
                  theme::amber, meters.masterPeakDb, true);

        drawNode (output, "OUTPUT", "USB / HP", theme::cyan, meters.masterPeakDb, true);

        drawConnection (rightPort (mic), leftPort (vocal), theme::cyan, meters.micPeakDb > -60.0f);
        drawConnection (rightPort (vocal), leftPort (sum, 0.18f), theme::cyan, meters.vocalBusPeakDb > -60.0f);
        drawConnection (rightPort (loopback), leftPort (music), theme::magenta, app.engine.isLoopbackCapturing());
        drawConnection (rightPort (music), leftPort (sum, 0.48f), theme::magenta, meters.musicBusPeakDb > -60.0f);
        drawConnection (rightPort (samplerNode), leftPort (sum, 0.78f), theme::green, meters.samplerPeakDb > -60.0f);
        drawConnection (rightPort (sum), leftPort (master), theme::amber, meters.masterPeakDb > -60.0f);
        drawConnection (rightPort (master), leftPort (output), theme::cyan, meters.masterPeakDb > -60.0f);
    }

    if (meters.clip)
    {
        g.setColour (theme::red);
        g.setFont (theme::font (compact ? 7.5f : 9.0f, true));
        g.drawText ("CLIP", getLocalBounds().reduced (10).removeFromTop (12),
                    juce::Justification::centredRight, false);
    }
}

} // namespace rr
