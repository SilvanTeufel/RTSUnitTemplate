// Fill out your copyright notice in the Description page of Project Settings.


#include "EOS/SessionItem.h"


void USessionItem::NativeConstruct()
{
	Super::NativeConstruct();

	// Update the text block with the item name
	if (MyTextBlock)
	{
		MyTextBlock->SetText(ItemName);
	}
}

void USessionItem::SetItemName(FText NewItemName)
{
	ItemName = NewItemName;
	if (MyTextBlock)
	{
		MyTextBlock->SetText(ItemName);
	}
}