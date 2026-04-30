#pragma once

#include "RepentePd/UI/PromptBar.h"

class PromptBarSmokeTest : public UnitTest {
public:
    PromptBarSmokeTest() : UnitTest("PromptBar smoke test", "RepentePd") {}

    void runTest() override
    {
        beginTest("PromptBar can be instantiated");
        PromptBar bar;
        expect(true, "PromptBar constructed without crash");

        beginTest("PromptBar input is accessible");
        bar.setSize(400, 36);
        expect(true, "PromptBar resized without crash");
    }
};

static PromptBarSmokeTest promptBarSmokeTest;
