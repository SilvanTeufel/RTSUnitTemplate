#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "StoryTriggerQueueSubsystem.generated.h"

class UStoryWidgetBase;
class USoundBase;
class UTexture2D;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FStoryQueueItem
{
	GENERATED_BODY()

	// Widget class to instantiate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TSubclassOf<UStoryWidgetBase> WidgetClass = nullptr;

	// Text to show
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	FText Text;

	// Optional image (hard reference)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<UTexture2D> Image = nullptr;

	// Optional material (hard reference; takes precedence over Image if set)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	// Optional soft references to reduce cook size. If set, they take precedence over hard refs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TSoftObjectPtr<UTexture2D> ImageSoft;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TSoftObjectPtr<UMaterialInterface> MaterialSoft;

	// Center offsets in pixels
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	// float OffsetX = 0.f;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	// float OffsetY = 0.f;

	// Lifetime in seconds; <= 0 means persist indefinitely
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	float LifetimeSeconds = 0.f;

	// Sound to play when this item becomes active
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "StoryQueue")
	TWeakObjectPtr<UObject> TriggeringSource = nullptr;
};

UCLASS()
class RTSUNITTEMPLATE_API UStoryTriggerQueueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UStoryTriggerQueueSubsystem();

	// Enqueue a story item to be displayed sequentially for the local player
	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void EnqueueStory(const FStoryQueueItem& Item);

	// Force remove the currently active widget, if any
	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void ClearActive();

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	float GetGlobalSoundMultiplier() const { return GlobalSoundMultiplier; }

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	float GetDefaultSoundVolume() const { return DefaultSoundVolume; }

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void SetDefaultSoundVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	float GetMasterVolume() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<class USoundClass> MasterSoundClass;

protected:
	virtual void Deinitialize() override;

private:
	// Queue of pending items
	UPROPERTY(Transient)
	TArray<FStoryQueueItem> Pending;

	UPROPERTY(Transient)
	FStoryQueueItem CurrentItem;

	bool bIsStoryActive = false;

	float GlobalSoundMultiplier = 1.0f;
	float DefaultSoundVolume = 1.0f;

	// Currently active widget
	UPROPERTY(Transient)
	TObjectPtr<UStoryWidgetBase> ActiveWidget = nullptr;

	FTimerHandle ActiveTimerHandle;
	FTimerHandle NextStoryTimerHandle;

	void TryPlayNext();
	void OnActiveLifetimeFinished();
};
