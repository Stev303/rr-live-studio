#pragma once

#include "App/AppState.h"
#include "UI/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

namespace rr
{
class Sidebar : public juce::Component,
                private juce::Timer
{
public:
    static constexpr int numPages = 9;

    explicit Sidebar (AppState& state);
    ~Sidebar() override;

    void setCurrentPage (int pageIndex);
    std::function<void (int)> onPageSelected;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectPage (int pageIndex);

    AppState& app;
    std::array<std::unique_ptr<juce::TextButton>, (size_t) numPages> buttons;
    int currentPage = 0;
    juce::String statusText;
    bool audioReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sidebar)
};

} // namespace rr
