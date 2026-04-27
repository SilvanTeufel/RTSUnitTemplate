#pragma once

#include "Modules/ModuleManager.h"

class FRTSUnitTemplateTestsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};
