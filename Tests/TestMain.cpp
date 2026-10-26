#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

namespace
{
class ConsoleLogger final : public juce::Logger
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("RrLiveStudio");

    int tests = 0;
    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* result = runner.getResult (i))
        {
            ++tests;
            failures += result->failures;

            std::cout << (result->failures == 0 ? "[PASS] " : "[FAIL] ")
                      << result->unitTestName << " / " << result->subcategoryName
                      << " (" << result->passes << " checks)" << std::endl;

            for (const auto& message : result->messages)
                std::cout << "       " << message << std::endl;
        }
    }

    juce::Logger::setCurrentLogger (nullptr);

    if (tests == 0)
    {
        std::cerr << "Tidak ada test yang berjalan." << std::endl;
        return 2;
    }

    std::cout << (failures == 0 ? "SEMUA TEST LULUS" : "TEST GAGAL") << std::endl;
    return failures == 0 ? 0 : 1;
}
