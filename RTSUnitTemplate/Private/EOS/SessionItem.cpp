// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

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