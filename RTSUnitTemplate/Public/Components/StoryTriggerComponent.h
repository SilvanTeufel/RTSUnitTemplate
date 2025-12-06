#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StoryTriggerComponent.generated.h"

class UDataTable;
class UStoryTriggerQueueSubsystem;

// Forward declare struct from StoryTriggerActor to avoid duplication
struct FStoryWidgetTable;
struct FStoryQueueItem;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API UStoryTriggerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UStoryTriggerComponent();

	// Data table that contains rows of FStoryTriggerRow
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Data")
	TObjectPtr<UDataTable> StoryDataTable = nullptr;

	// Attempt to open a random story from the DataTable based on ChancePercent (0..100)
	// 100 = always show, 0 = never show.
	UFUNCTION(BlueprintCallable, Category = "Story")
	void TryTriggerRandom(float ChancePercent = 100.f);

private:
	bool ShouldTrigger(float ChancePercent) const;
	bool BuildQueueItemFromRandomRow(struct FStoryQueueItem& OutItem) const;
};
