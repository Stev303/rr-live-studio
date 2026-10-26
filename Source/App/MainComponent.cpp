#include "App/MainComponent.h"
#include "UI/Pages/DashboardPage.h"
#include "UI/Pages/EffectsPage.h"
#include "UI/Pages/MixerPage.h"
#include "UI/Pages/PresetsPage.h"
#include "UI/Pages/SamplerPage.h"
#include "UI/Pages/SettingsPage.h"
#include "UI/Pages/VST3Page.h"
#include "UI/Pages/VocalChainPage.h"
#include "UI/Pages/YouTubePage.h"

namespace rr
{
namespace
{
    constexpr int sidebarWidth = 196;
    constexpr int topBarHeight = 58;

    const char* const pageTitles[] =
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

MainComponent::MainComponent (AppState& state)
    : app (state),
      sidebar (app),
      topBar (app,
              [this] { openGoLive(); },
              [this] (int pageIndex) { showPage (pageIndex); }),
      goLiveDialog (app)
{
    sidebar.onPageSelected = [this] (int pageIndex) { showPage (pageIndex); };
    app.onNavigate = [this] (int pageIndex) { showPage (pageIndex); };

    addAndMakeVisible (sidebar);
    addAndMakeVisible (topBar);
    addAndMakeVisible (goLiveDialog);
    goLiveDialog.setVisible (false);

    showPage (juce::jlimit (0, Sidebar::numPages - 1, app.getCurrentPage()));

    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::Timer::callAfterDelay (600, [safeThis]
    {
        if (safeThis != nullptr)
            safeThis->showStartupWarnings();
    });
}

MainComponent::~MainComponent()
{
    app.onNavigate = nullptr;
    app.save();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    sidebar.setBounds (area.removeFromLeft (sidebarWidth));
    topBar.setBounds (area.removeFromTop (topBarHeight));

    for (auto& page : pages)
        if (page != nullptr)
            page->setBounds (area);

    goLiveDialog.setBounds (getLocalBounds());

    if (goLiveDialog.isVisible())
        goLiveDialog.toFront (false);
}

void MainComponent::showPage (int pageIndex)
{
    if (! juce::isPositiveAndBelow (pageIndex, Sidebar::numPages))
        return;

    if (pages[(size_t) pageIndex] == nullptr)
    {
        std::unique_ptr<juce::Component> newPage;

        switch (pageIndex)
        {
            case 0:
                newPage = std::make_unique<DashboardPage> (app, [this] { openGoLive(); });
                break;
            case 1:
                newPage = std::make_unique<YouTubePage> (app);
                break;
            case 2:
                newPage = std::make_unique<VocalChainPage> (app);
                break;
            case 3:
                newPage = std::make_unique<EffectsPage> (app);
                break;
            case 4:
                newPage = std::make_unique<VST3Page> (app);
                break;
            case 5:
                newPage = std::make_unique<SamplerPage> (app);
                break;
            case 6:
                newPage = std::make_unique<MixerPage> (app);
                break;
            case 7:
                newPage = std::make_unique<PresetsPage> (app);
                break;
            case 8:
                newPage = std::make_unique<SettingsPage> (app);
                break;
            default:
                break;
        }

        if (newPage == nullptr)
            return;

        addAndMakeVisible (*newPage);
        pages[(size_t) pageIndex] = std::move (newPage);
    }

    currentPage = pageIndex;

    auto pageArea = getLocalBounds();
    pageArea.removeFromLeft (sidebarWidth);
    pageArea.removeFromTop (topBarHeight);

    for (int i = 0; i < Sidebar::numPages; ++i)
        if (pages[(size_t) i] != nullptr)
        {
            pages[(size_t) i]->setBounds (pageArea);
            pages[(size_t) i]->setVisible (i == currentPage);
        }

    sidebar.setCurrentPage (currentPage);
    topBar.setPageTitle (pageTitles[(size_t) currentPage]);
    app.setCurrentPage (currentPage);

    if (goLiveDialog.isVisible())
        goLiveDialog.toFront (false);
}

void MainComponent::openGoLive()
{
    if (! goLiveDialog.isVisible())
    {
        goLiveDialog.setBounds (getLocalBounds());
        goLiveDialog.open();
    }
    else
    {
        goLiveDialog.toFront (true);
    }
}

void MainComponent::showStartupWarnings()
{
    if (warningShown)
        return;

    warningShown = true;

    auto warning = app.getStartupWarning();

    if (app.getDeviceInitError().isNotEmpty() && ! warning.contains (app.getDeviceInitError()))
        warning = (warning.isNotEmpty() ? warning + "\n\n" : "") + "Audio: " + app.getDeviceInitError();

    if (warning.isEmpty())
        return;

    showWarningMessage ("Peringatan Startup", warning);
}

} // namespace rr
