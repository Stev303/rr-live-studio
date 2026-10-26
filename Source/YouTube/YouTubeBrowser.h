#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace rr
{
/** Embedded official YouTube browser (WebView2 on Windows).

    Only youtube.com, youtu.be and Google sign-in pages are allowed to load in
    the embedded view.  There is no ad, DRM or login bypass: the page is the
    normal YouTube web player, and external links are blocked instead of being
    opened silently.
*/
class YouTubeBrowser : public juce::Component
{
public:
    explicit YouTubeBrowser (juce::File webViewDataFolder);
    ~YouTubeBrowser() override;

    static bool isRuntimeAvailable (juce::String* version = nullptr);
    static juce::String runtimeInstructions();

    bool isBrowserAvailable() const noexcept { return browser != nullptr; }
    juce::String getRuntimeVersion() const { return runtimeVersion; }

    void loadHome();
    void searchFor (const juce::String& query);
    void goBack();
    void goForward();
    void reload();
    void setVideoVolumePercent (int percent);
    int getVideoVolumePercent() const noexcept { return volumePercent; }

    std::function<void()> onNavigationChanged;
    std::function<void (const juce::String&)> onBlockedNavigation;
    std::function<void (const juce::String&)> onLoadError;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Browser;

    void attemptCreateBrowser();
    void updateUnavailableUi();

    juce::File dataFolder;
    std::unique_ptr<Browser> browser;
    juce::String runtimeVersion;

    juce::Label unavailableTitle;
    juce::Label unavailableBody;
    juce::TextButton copyCommandButton { "Salin Perintah Instal" };
    juce::TextButton retryButton { "Periksa Ulang" };

    int volumePercent = 85;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouTubeBrowser)
};

} // namespace rr
