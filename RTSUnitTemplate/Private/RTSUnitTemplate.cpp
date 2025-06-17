// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "RTSUnitTemplate.h"

#define LOCTEXT_NAMESPACE "FRTSUnitTemplateModule"

void FRTSUnitTemplateModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FRTSUnitTemplateModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRTSUnitTemplateModule, RTSUnitTemplate)