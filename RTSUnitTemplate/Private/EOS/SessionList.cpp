// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EOS/SessionList.h"

#include "EOS/EOS_GameInstance.h"
#include "EOS/SessionItem.h"
#include "TimerManager.h"

void USessionList::NativeConstruct()
{
	Super::NativeConstruct();
	
	 // Set up a repeating timer to update the ListView every 1 second
	GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &USessionList::UpdateListView, 1.0f, true);
}

void USessionList::UpdateListView()
{
	UEOS_GameInstance* GameInstance = Cast<UEOS_GameInstance>(GetWorld()->GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("GameInstance is null!"));
		return;
	}

	if (MyListView)
	{
		MyListView->ClearListItems();  // Clear existing items

		for (int i = 0; i < GameInstance->SearchNames.Num(); ++i)
		{
			USessionItem* Item = CreateWidget<USessionItem>(GetWorld(), ListItemClass);
			if (Item)
			{
				Item->SetItemName(FText::FromString(GameInstance->SearchNames[i]));
				MyListView->AddItem(Item);
				UE_LOG(LogTemp, Error, TEXT("Created List Item with SearchName: %s"), *GameInstance->SearchNames[i]);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MyListView is null!"));
	}
	
}
