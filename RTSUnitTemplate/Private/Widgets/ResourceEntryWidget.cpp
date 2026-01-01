// ResourceEntryWidget.cpp
#include "Widgets/ResourceEntryWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Internationalization/Text.h"

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

void UResourceEntryWidget::SetResourceData(EResourceType InResourceType, const FText& InResourceName, float InResourceAmount, int32 InWorkerCount, int32 PlayerTeamId, UTexture2D* InIconTexture, float InMaxResourceAmount, bool bInIsSupplyLike, bool bInCollapseWorkerUI)
{
	ResourceType = InResourceType;
	bCollapseWorkerUI = bInCollapseWorkerUI;

	if (ResourceNameText)
	{
		ResourceNameText->SetText(InResourceName);
	}
	if (ResourceAmountText)
	{
		FNumberFormattingOptions FormatOptions;
		const int32 Decimals = FMath::Clamp(ResourceAmountDecimalPlaces, 0, 6);
		FormatOptions.MinimumFractionalDigits = Decimals;
		FormatOptions.MaximumFractionalDigits = Decimals;

		if (bInIsSupplyLike)
		{
			FText AmountText = FText::AsNumber(InResourceAmount, &FormatOptions);
			FText MaxAmountText = FText::AsNumber(InMaxResourceAmount, &FormatOptions);
			ResourceAmountText->SetText(FText::Format(FText::FromString("{0} / {1}"), AmountText, MaxAmountText));
		}
		else
		{
			ResourceAmountText->SetText(FText::AsNumber(InResourceAmount, &FormatOptions));
		}
	}
	if (WorkerCountText)
	{
		WorkerCountText->SetText(FText::AsNumber(InWorkerCount));
	}

	if (ResourceIcon)
	{
		if (InIconTexture)
		{
			ResourceIcon->SetBrushFromTexture(InIconTexture, true);
		}
		else
		{
			// If no texture is provided, you may clear or leave existing brush
			// ResourceIcon->SetBrush(FSlateBrush());
		}
	}

	TeamId = PlayerTeamId;

	// Handle collapse
	ESlateVisibility WorkerVisibility = bCollapseWorkerUI ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	if (WorkerCountText) WorkerCountText->SetVisibility(WorkerVisibility);
	if (AddWorkerButton) AddWorkerButton->SetVisibility(WorkerVisibility);
	if (RemoveWorkerButton) RemoveWorkerButton->SetVisibility(WorkerVisibility);
}

