#pragma once

#include "App/AppState.h"
#include "UI/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rr
{
/** Signal-routing diagram driven by the actual engine parameters and meters. */
class RoutingDiagram : public juce::Component,
                       private juce::Timer
{
public:
    explicit RoutingDiagram (AppState& state, bool compactView = false);
    ~RoutingDiagram() override;

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    AppState& app;
    bool compact;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingDiagram)
};

} // namespace rr
