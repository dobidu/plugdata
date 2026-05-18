#include "Tests.h"
#include "ObjectFuzzTest.h"
#include "HelpfileFuzzTest.h"
#include "HelpfileErrorTest.h"
#include "RepentePdTests.h"

void runTests(PluginEditor* editor)
{
    // Battery B, Battery F, SpectralAnalyzer — pure computation, no GUI needed.
    // Run synchronously before spawning the async GUI test thread.
    {
        UnitTestRunner pdRunner;
        pdRunner.runTestsInCategory("RepentePd");
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
