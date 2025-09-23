// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/TaggedUnitButton.h"
#include "Components/Button.h"
#include "Components/Image.h"

void UTaggedUnitButton::NativeConstruct()
{
	Super::NativeConstruct();

	// Ensure the button is valid before binding to its click event.
	if (MainButton)
	{
		MainButton->OnClicked.AddDynamic(this, &UTaggedUnitButton::HandleButtonClicked);
	}
}

void UTaggedUnitButton::HandleButtonClicked()
{
	// When the internal button is clicked, broadcast our public delegate,
	// passing along the specific tag this button represents.
	OnClicked.Broadcast(UnitTagToSelect);
}

void UTaggedUnitButton::SetIcon(UTexture2D* NewIcon)
{
	if (IconImage && NewIcon)
	{
		IconImage->SetBrushFromTexture(NewIcon);
		IconImage->SetVisibility(ESlateVisibility::Visible);
	}
	else if (IconImage)
	{
		IconImage->SetVisibility(ESlateVisibility::Hidden);
	}
}