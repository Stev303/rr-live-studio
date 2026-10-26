#include "App/AppState.h"
#include "App/MainComponent.h"
#include "UI/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace rr
{
class RrLiveStudioApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Rr Live Studio"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        lookAndFeel = std::make_unique<RrLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        appState.initialise();
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), appState);
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
            mainWindow.reset();

        appState.save();
        appState.engine.shutdown();

        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, AppState& state)
            : juce::DocumentWindow (name, theme::background, juce::DocumentWindow::allButtons),
              app (state)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (app), true);
            setResizable (true, true);
            setResizeLimits (1120, 700, 4096, 2160);
            setBackgroundColour (theme::background);

            restoreBounds();
            setVisible (true);
        }

        ~MainWindow() override
        {
            saveBounds();
        }

        void closeButtonPressed() override
        {
            saveBounds();
            app.save();

            if (auto* instance = juce::JUCEApplication::getInstance())
                instance->systemRequestedQuit();
        }

    private:
        void restoreBounds()
        {
            if (auto* props = app.getProperties())
            {
                const int width = props->getIntValue ("windowWidth", 0);
                const int height = props->getIntValue ("windowHeight", 0);
                const int x = props->getIntValue ("windowX", 0);
                const int y = props->getIntValue ("windowY", 0);

                if (width >= 1000 && height >= 640)
                {
                    auto saved = juce::Rectangle<int> (x, y, width, height);
                    const auto* display = juce::Desktop::getInstance().getDisplays()
                                              .getDisplayForRect (saved);

                    if (display != nullptr)
                    {
                        auto suitable = saved.constrainedWithin (display->userArea);

                        if (suitable.getWidth() >= 1000 && suitable.getHeight() >= 640)
                        {
                            setBounds (suitable);
                            return;
                        }
                    }
                }
            }

            auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();

            if (display != nullptr)
            {
                const auto screen = display->userArea;
                const int width = juce::jmin (1500, screen.getWidth() - 40);
                const int height = juce::jmin (940, screen.getHeight() - 60);
                centreWithSize (juce::jmax (1120, width), juce::jmax (700, height));
            }
            else
            {
                centreWithSize (1400, 880);
            }
        }

        void saveBounds()
        {
            if (auto* props = app.getProperties())
            {
                const auto bounds = getBounds();
                props->setValue ("windowX", bounds.getX());
                props->setValue ("windowY", bounds.getY());
                props->setValue ("windowWidth", bounds.getWidth());
                props->setValue ("windowHeight", bounds.getHeight());
                props->saveIfNeeded();
            }
        }

        AppState& app;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<RrLookAndFeel> lookAndFeel;
    AppState appState;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace rr

START_JUCE_APPLICATION (rr::RrLiveStudioApplication)
