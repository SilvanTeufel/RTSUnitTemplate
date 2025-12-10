#include "Blueprint/StoryBlueprintLibrary.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "System/StoryTriggerQueueSubsystem.h"
#include "Actors/StoryTriggerActor.h" // for FStoryWidgetTable

void UStoryBlueprintLibrary::EnqueueStory(UObject* WorldContextObject,
	TSubclassOf<UStoryWidgetBase> WidgetClass,
	FText Text,
	UTexture2D* Image,
	UMaterialInterface* Material,
	float OffsetX,
	float OffsetY,
	float LifetimeSeconds,
	USoundBase* Sound)
{
	if (!WorldContextObject)
	{
		return;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
		{
			FStoryQueueItem Item;
			Item.WidgetClass = WidgetClass;
			Item.Text = Text;
			Item.Image = Image;
			Item.Material = Material;
			Item.OffsetX = OffsetX;
			Item.OffsetY = OffsetY;
			Item.LifetimeSeconds = LifetimeSeconds;
			Item.Sound = Sound;
			Queue->EnqueueStory(Item);
		}
	}
}

void UStoryBlueprintLibrary::EnqueueStorySoft(UObject* WorldContextObject,
	TSubclassOf<UStoryWidgetBase> WidgetClass,
	FText Text,
	TSoftObjectPtr<UTexture2D> ImageSoft,
	TSoftObjectPtr<UMaterialInterface> MaterialSoft,
	float OffsetX,
	float OffsetY,
	float LifetimeSeconds,
	USoundBase* Sound)
{
	if (!WorldContextObject)
	{
		return;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
		{
			FStoryQueueItem Item;
			Item.WidgetClass = WidgetClass;
			Item.Text = Text;
			Item.ImageSoft = ImageSoft;
			Item.MaterialSoft = MaterialSoft;
			Item.OffsetX = OffsetX;
			Item.OffsetY = OffsetY;
			Item.LifetimeSeconds = LifetimeSeconds;
			Item.Sound = Sound;
			Queue->EnqueueStory(Item);
		}
	}
}

void UStoryBlueprintLibrary::EnqueueFromDataTableRow(UObject* WorldContextObject,
	UDataTable* Table,
	FName RowName,
	bool bRandomIfNone)
{
	if (!WorldContextObject || !Table)
	{
		return;
	}

	static const FString Context(TEXT("StoryBlueprintLibrary_DT"));
	const FStoryWidgetTable* Row = nullptr;

	if (RowName.IsNone() && bRandomIfNone)
	{
		const TArray<FName> RowNames = Table->GetRowNames();
		if (RowNames.Num() > 0)
		{
			const int32 Index = FMath::RandRange(0, RowNames.Num() - 1);
			Row = Table->FindRow<FStoryWidgetTable>(RowNames[Index], Context, true);
		}
	}
	else if (!RowName.IsNone())
	{
		Row = Table->FindRow<FStoryWidgetTable>(RowName, Context, true);
	}

	if (!Row)
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (UStoryTriggerQueueSubsystem* Queue = GI->GetSubsystem<UStoryTriggerQueueSubsystem>())
		{
			FStoryQueueItem Item;
			Item.WidgetClass = Row->StoryWidgetClass;
			Item.Text = Row->StoryText;
			Item.Image = Row->StoryImage;
			Item.Material = Row->StoryMaterial;
			Item.ImageSoft = Row->StoryImageSoft;
			Item.MaterialSoft = Row->StoryMaterialSoft;
			Item.OffsetX = Row->ScreenOffsetX;
			Item.OffsetY = Row->ScreenOffsetY;
			Item.LifetimeSeconds = Row->WidgetLifetimeSeconds;
			Item.Sound = Row->TriggerSound;
			Queue->EnqueueStory(Item);
		}
	}
}
