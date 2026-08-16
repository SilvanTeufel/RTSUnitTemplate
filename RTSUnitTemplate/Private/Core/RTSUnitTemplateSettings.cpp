// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Core/RTSUnitTemplateSettings.h"

URTSUnitTemplateSettings::URTSUnitTemplateSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("RTS Unit Template");
}

const URTSUnitTemplateSettings* URTSUnitTemplateSettings::Get()
{
	return GetDefault<URTSUnitTemplateSettings>();
}
