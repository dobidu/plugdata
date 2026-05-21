#include "Tests.h"
#include "ObjectFuzzTest.h"
#include "HelpfileFuzzTest.h"
#include "HelpfileErrorTest.h"
#include "RepentePdTests.h"

#include <cstdlib>
#include <iostream>

void runTests(PluginEditor* editor)
{
    // Battery B, Battery F, SpectralAnalyzer, OpenAIProvider, AnthropicProvider,
    // PresetLoader — pure computation, no GUI needed. Run synchronously.
    int totalFailures = 0;
    {
        UnitTestRunner pdRunner;
        pdRunner.runTestsInCategory("RepentePd");
        for (int i = 0; i < pdRunner.getNumResults(); ++i)
            if (auto const* r = pdRunner.getResult(i))
                totalFailures += r->failures;
    }

    // CI mode: skip the GUI test thread (which needs a real display and
    // working audio device) and exit with a code reflecting the sync results.
    // Triggered by PLUGDATA_CI_TESTS_ONLY=1 in the environment.
    auto const ciOnly = juce::SystemStats::getEnvironmentVariable(
        "PLUGDATA_CI_TESTS_ONLY", "");
    if (ciOnly.isNotEmpty()) {
        std::cerr << "PLUGDATA_CI_TESTS_ONLY: RepentePd category done, "
                  << totalFailures << " failure(s); exiting." << std::endl;
        std::_Exit(totalFailures == 0 ? 0 : 1);
    }

    // GUI tests need a separate thread because they block until the message
    // thread has processed each test case.
    std::thread testRunnerThread([editor] {
        ObjectFuzzTest objectFuzzer(editor);
        HelpFileFuzzTest helpfileFuzzer(editor);
        HelpFileErrorTest helpfileErrorTest(editor);

        UnitTestRunner runner;
        runner.runTests({&helpfileFuzzer, &objectFuzzer, &helpfileErrorTest}, 23);
    });
    testRunnerThread.detach();
}
