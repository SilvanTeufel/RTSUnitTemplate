#include "Widgets/MapMarkerWidget.h"
#include "Components/TextBlock.h"

void UMapMarkerWidget::SetMarkerText(const FText& InText)
{
	if (MarkerText)
	{
		MarkerText->SetText(InText);
	}
}
