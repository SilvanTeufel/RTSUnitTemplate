// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MapMarkerWidget.h"
#include "Components/TextBlock.h"

void UMapMarkerWidget::SetMarkerText(const FText& InText)
{
	if (MarkerText)
	{
		MarkerText->SetText(InText);
	}
}
