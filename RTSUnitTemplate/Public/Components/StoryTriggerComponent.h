#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StoryTriggerComponent.generated.h"

class UDataTable;
class UStoryTriggerQueueSubsystem;

// Forward declare struct from StoryTriggerActor to avoid duplication
struct FStoryWidgetTable;
struct FStoryQueueItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryComponentTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryComponentFinished);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UStoryTriggerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UStoryTriggerComponent();

	UPROPERTY(BlueprintAssignable, Category = Story)
	FOnStoryComponentTriggered OnStoryTriggered;

	UPROPERTY(BlueprintAssignable, Category = Story)
	FOnStoryComponentFinished OnStoryFinished;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Story)
	float LowerVolume = 0.4f;

	// Data table that contains rows of FStoryTriggerRow
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Data")
	TObjectPtr<UDataTable> StoryDataTable = nullptr;

	// Attempt to open a random story from the DataTable based on ChancePercent (0..100)
	// 100 = always show, 0 = never show.
	UFUNCTION(BlueprintCallable, Category = "Story")
	void TryTriggerRandom(float ChancePercent = 100.f);

	// Attempt to open a specific story from the DataTable by Row Name (ID)
	UFUNCTION(BlueprintCallable, Category = "Story")
	void TriggerSpecific(FName RowName);

private:
	bool ShouldTrigger(float ChancePercent) const;
	bool BuildQueueItemFromRow(FName RowName, struct FStoryQueueItem& OutItem) const;
	bool BuildQueueItemFromRandomRow(struct FStoryQueueItem& OutItem) const;
};
