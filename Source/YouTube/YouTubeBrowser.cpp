#include "YouTube/YouTubeBrowser.h"
#include "UI/Theme.h"
#include "UI/Widgets.h"

#if JUCE_WINDOWS && JUCE_USE_WIN_WEBVIEW2
 #include <windows.h>
 #include <WebView2.h>
#endif

namespace rr
{
namespace
{
    bool isAllowedYouTubeUrl (const juce::String& urlText)
    {
        if (urlText.isEmpty() || urlText.startsWithIgnoreCase ("about:")
            || urlText.startsWithIgnoreCase ("data:") || urlText.startsWithIgnoreCase ("blob:"))
            return true;

        const juce::URL url (urlText);
        const auto scheme = url.getScheme().toLowerCase();

        if (scheme != "https" && scheme != "http")
            return false;

        const auto domain = url.getDomain().toLowerCase();

        return domain == "youtube.com"
            || domain.endsWith (".youtube.com")
            || domain == "youtu.be"
            || domain == "google.com"
            || domain.endsWith (".google.com");
    }
}

class YouTubeBrowser::Browser : public juce::WebBrowserComponent
{
public:
    explicit Browser (YouTubeBrowser& ownerToUse)
        : juce::WebBrowserComponent (juce::WebBrowserComponent::Options {}
              .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
              .withKeepPageLoadedWhenBrowserIsHidden()
              .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                  .withUserDataFolder (ownerToUse.dataFolder)
                  .withStatusBarDisabled())),
          owner (ownerToUse)
    {
    }

    bool pageAboutToLoad (const juce::String& newURL) override
    {
        if (isAllowedYouTubeUrl (newURL))
            return true;

        if (owner.onBlockedNavigation != nullptr)
            owner.onBlockedNavigation (newURL);

        return false;
    }

    void pageFinishedLoading (const juce::String&) override
    {
        owner.setVideoVolumePercent (owner.volumePercent);

        if (owner.onNavigationChanged != nullptr)
            owner.onNavigationChanged();
    }

    bool pageLoadHadNetworkError (const juce::String& errorInfo) override
    {
        if (owner.onLoadError != nullptr)
            owner.onLoadError (errorInfo);

        return true;
    }

    void windowCloseRequest() override {}

    void newWindowAttemptingToLoad (const juce::String& newURL) override
    {
        if (isAllowedYouTubeUrl (newURL))
            goToURL (newURL);
        else if (owner.onBlockedNavigation != nullptr)
            owner.onBlockedNavigation (newURL);
    }

private:
    YouTubeBrowser& owner;
};

//==============================================================================
YouTubeBrowser::YouTubeBrowser (juce::File webViewDataFolder)
    : dataFolder (std::move (webViewDataFolder))
{
    dataFolder.createDirectory();

    configureLabel (unavailableTitle, 18.0f, theme::text, true, juce::Justification::centred);
    unavailableTitle.setText ("WebView2 Runtime belum terpasang", juce::dontSendNotification);

    configureLabel (unavailableBody, 12.0f, theme::textDim, false, juce::Justification::centred);
    unavailableBody.setJustificationType (juce::Justification::centred);
    unavailableBody.setText (runtimeInstructions(), juce::dontSendNotification);

    styleActionButton (copyCommandButton, theme::cyan);
    copyCommandButton.onClick = []
    {
        juce::SystemClipboard::copyTextToClipboard ("winget install --id Microsoft.EdgeWebView2Runtime");
        showInfoMessage ("Perintah disalin",
                         "Tempel perintah tersebut di PowerShell atau Command Prompt, lalu jalankan. "
                         "Setelah instalasi selesai, tekan Periksa Ulang.");
    };

    styleActionButton (retryButton, theme::green);
    retryButton.onClick = [this] { attemptCreateBrowser(); };

    addAndMakeVisible (unavailableTitle);
    addAndMakeVisible (unavailableBody);
    addAndMakeVisible (copyCommandButton);
    addAndMakeVisible (retryButton);

    attemptCreateBrowser();
}

YouTubeBrowser::~YouTubeBrowser()
{
    browser.reset();
}

bool YouTubeBrowser::isRuntimeAvailable (juce::String* version)
{
#if JUCE_WINDOWS && JUCE_USE_WIN_WEBVIEW2
    LPWSTR rawVersion = nullptr;
    const auto result = GetAvailableCoreWebView2BrowserVersionString (nullptr, &rawVersion);

    if (SUCCEEDED (result) && rawVersion != nullptr)
    {
        if (version != nullptr)
            *version = juce::String (rawVersion);

        CoTaskMemFree (rawVersion);
        return true;
    }

    if (rawVersion != nullptr)
        CoTaskMemFree (rawVersion);
#endif

    if (version != nullptr)
        version->clear();

    return false;
}

juce::String YouTubeBrowser::runtimeInstructions()
{
    return "Browser YouTube memakai Microsoft Edge WebView2 Runtime. "
           "Runtime ini biasanya sudah terpasang di Windows 10/11.\n\n"
           "Jika belum ada, jalankan di PowerShell:\n"
           "winget install --id Microsoft.EdgeWebView2Runtime\n\n"
           "Atau unduh Evergreen Runtime (x64) dari halaman resmi Microsoft:\n"
           "https://developer.microsoft.com/microsoft-edge/webview2/\n\n"
           "Setelah instalasi, tekan Periksa Ulang. Tidak ada audio YouTube yang "
           "diambil tanpa WASAPI loopback pada halaman YouTube.";
}

void YouTubeBrowser::attemptCreateBrowser()
{
    if (browser != nullptr)
        return;

    juce::String version;

    if (! isRuntimeAvailable (&version))
    {
        runtimeVersion.clear();
        updateUnavailableUi();
        return;
    }

    runtimeVersion = version;
    browser = std::make_unique<Browser> (*this);
    addAndMakeVisible (*browser);
    browser->setBounds (getLocalBounds());
    updateUnavailableUi();
    resized();
    browser->goToURL ("https://www.youtube.com/");
}

void YouTubeBrowser::updateUnavailableUi()
{
    const auto unavailable = browser == nullptr;

    unavailableTitle.setVisible (unavailable);
    unavailableBody.setVisible (unavailable);
    copyCommandButton.setVisible (unavailable);
    retryButton.setVisible (unavailable);

    if (unavailable)
    {
        if (runtimeVersion.isEmpty())
            unavailableTitle.setText ("WebView2 Runtime belum terpasang", juce::dontSendNotification);
        else
            unavailableTitle.setText ("WebView2 belum dapat dibuka", juce::dontSendNotification);

        unavailableBody.setText (runtimeInstructions(), juce::dontSendNotification);
    }

    repaint();
}

void YouTubeBrowser::loadHome()
{
    if (browser != nullptr)
        browser->goToURL ("https://www.youtube.com/");
}

void YouTubeBrowser::searchFor (const juce::String& query)
{
    const auto trimmed = query.trim();

    if (browser == nullptr || trimmed.isEmpty())
        return;

    const auto url = juce::URL ("https://www.youtube.com/results")
                         .withParameter ("search_query", trimmed);
    browser->goToURL (url.toString (true));
}

void YouTubeBrowser::goBack()
{
    if (browser != nullptr)
        browser->goBack();
}

void YouTubeBrowser::goForward()
{
    if (browser != nullptr)
        browser->goForward();
}

void YouTubeBrowser::reload()
{
    if (browser != nullptr)
        browser->refresh();
}

void YouTubeBrowser::setVideoVolumePercent (int percent)
{
    volumePercent = juce::jlimit (0, 100, percent);

    if (browser != nullptr)
    {
        const auto volume = juce::String ((double) volumePercent / 100.0, 3);
        const auto script = "(() => { const list = document.querySelectorAll('video');"
                            " for (const v of list) { v.volume = " + volume + "; }"
                            " return list.length; })()";
        browser->evaluateJavascript (script);
    }
}

void YouTubeBrowser::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);

    if (browser == nullptr)
    {
        const auto bounds = getLocalBounds().toFloat();

        juce::ColourGradient gradient (theme::panelAlt, bounds.getTopLeft(),
                                       theme::background, bounds.getBottomRight(), false);
        g.setGradientFill (gradient);
        g.fillRect (bounds);
    }
}

void YouTubeBrowser::resized()
{
    if (browser != nullptr)
    {
        browser->setBounds (getLocalBounds());
        return;
    }

    auto area = getLocalBounds().reduced (28);
    auto buttonRow = area.removeFromBottom (38);

    retryButton.setBounds (buttonRow.removeFromRight (132));
    buttonRow.removeFromRight (10);
    copyCommandButton.setBounds (buttonRow.removeFromRight (180));

    area.removeFromBottom (12);
    unavailableBody.setBounds (area.removeFromBottom (juce::jmin (250, area.getHeight() / 2)));
    area.removeFromBottom (8);
    unavailableTitle.setBounds (area.removeFromBottom (34));
}

} // namespace rr
