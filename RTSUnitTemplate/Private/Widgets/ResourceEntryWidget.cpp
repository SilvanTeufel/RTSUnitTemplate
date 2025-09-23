// ResourceEntryWidget.cpp
#include "Widgets/ResourceEntryWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UResourceEntryWidget::UpdateWorkerCount(int32 AmountToAdd)
{
	// 1. Safety check to ensure the WorkerCountText widget is valid.
	if (!WorkerCountText)
	{
		return;
	}

	// 2. Get the current text and convert it to an integer.
	// FText -> FString -> int32
	const FString CurrentTextString = WorkerCountText->GetText().ToString();
	int32 CurrentWorkerCount = FCString::Atoi(*CurrentTextString);

	// 3. Calculate the new worker count.
	const int32 NewWorkerCount = CurrentWorkerCount + AmountToAdd;

	// 4. Set the new number back on the text block.
	// int32 -> FText
	WorkerCountText->SetText(FText::AsNumber(NewWorkerCount));
}

void UResourceEntryWidget::SetResourceData(EResourceType InResourceType, const FText& InResourceName, float InResourceAmount, int32 InWorkerCount, int32 PlayerTeamId)
{
	ResourceType = InResourceType;

	if (ResourceNameText)
	{
		ResourceNameText->SetText(InResourceName);
	}
	if (ResourceAmountText)
	{
		ResourceAmountText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), InResourceAmount)));
	}
	if (WorkerCountText)
	{
		WorkerCountText->SetText(FText::AsNumber(InWorkerCount));
	}

		TeamId = PlayerTeamId;	
}

