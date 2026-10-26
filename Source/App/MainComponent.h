#pragma once

#include "App/AppState.h"
#include "UI/GoLiveDialog.h"
#include "UI/Sidebar.h"
#include "UI/TopBar.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

namespace rr
{
class MainComponent : public juce::Component
{
public:
    explicit MainComponent (AppState& state);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void showPage (int pageIndex);
    void openGoLive();
    void showStartupWarnings();

    AppState& app;
    Sidebar sidebar;
    TopBar topBar;
    GoLiveDialog goLiveDialog;
    std::array<std::unique_ptr<juce::Component>, Sidebar::numPages> pages;
    int currentPage = 0;
    bool warningShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace rr
