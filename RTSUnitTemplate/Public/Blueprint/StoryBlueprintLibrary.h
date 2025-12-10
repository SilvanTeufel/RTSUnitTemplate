#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "StoryBlueprintLibrary.generated.h"

class UDataTable;
class UStoryWidgetBase;
class USoundBase;

struct FStoryQueueItem;

UCLASS()
class RTSUNITTEMPLATE_API UStoryBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Enqueue a story with hard references
	UFUNCTION(BlueprintCallable, Category="Story", meta=(WorldContext="WorldContextObject"))
	static void EnqueueStory(UObject* WorldContextObject,
		TSubclassOf<UStoryWidgetBase> WidgetClass,
		FText Text,
		UTexture2D* Image,
		UMaterialInterface* Material,
		float OffsetX = 0.f,
		float OffsetY = 0.f,
		float LifetimeSeconds = 0.f,
		USoundBase* Sound = nullptr);

	// Enqueue a story with soft references (preferred for reducing cook size)
	UFUNCTION(BlueprintCallable, Category="Story", meta=(WorldContext="WorldContextObject"))
	static void EnqueueStorySoft(UObject* WorldContextObject,
		TSubclassOf<UStoryWidgetBase> WidgetClass,
		FText Text,
		TSoftObjectPtr<UTexture2D> ImageSoft,
		TSoftObjectPtr<UMaterialInterface> MaterialSoft,
		float OffsetX = 0.f,
		float OffsetY = 0.f,
		float LifetimeSeconds = 0.f,
		USoundBase* Sound = nullptr);

	// Enqueue directly from a DataTable row of type FStoryWidgetTable
	UFUNCTION(BlueprintCallable, Category="Story", meta=(WorldContext="WorldContextObject"))
	static void EnqueueFromDataTableRow(UObject* WorldContextObject,
		UDataTable* Table,
		FName RowName,
		bool bRandomIfNone = false);
};
