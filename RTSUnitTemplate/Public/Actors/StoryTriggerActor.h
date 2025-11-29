#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "StoryTriggerActor.generated.h"

class UCapsuleComponent;
class UPrimitiveComponent;
class UStoryWidgetBase;
class USoundBase;
class UTexture2D;
class AUnitBase;

UCLASS()
class RTSUNITTEMPLATE_API AStoryTriggerActor : public AActor
{
	GENERATED_BODY()
public:
	AStoryTriggerActor();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Overlap capsule used as the lightweight trigger
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Story)
	TObjectPtr<UCapsuleComponent> TriggerCapsule;

	// Widget class to create (choose a BP subclass of StoryWidgetBase)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story)
	TSubclassOf<UStoryWidgetBase> StoryWidgetClass;

	// Sound to play upon first overlap
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story)
	TObjectPtr<USoundBase> TriggerSound = nullptr;

	// Text shown in the story widget
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story, meta=(MultiLine=true))
	FText StoryText;

	// Optional image shown in the story widget
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story)
	TObjectPtr<UTexture2D> StoryImage = nullptr;

	// If true, the trigger will only fire once and then disable itself
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story)
	bool bTriggerOnce = true;

	// Screen-space offset from center (X=right, Y=down)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story)
	float ScreenOffsetX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story)
	float ScreenOffsetY = 0.f;

	// Only units with this TeamId will trigger the story
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story)
	int32 TeamId = 0;

	// Auto-remove the widget after this many seconds (<=0 disables auto-remove)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Story, meta=(ClampMin="0.0"))
	float WidgetLifetimeSeconds = 10.f;

	// Internal flag to ensure single execution
	UPROPERTY(VisibleInstanceOnly, Category = Story)
	bool bHasTriggered = false;

private:
	// Track the spawned widget to allow timed removal
	UPROPERTY(Transient)
	TObjectPtr<UStoryWidgetBase> ActiveWidget = nullptr;

	FTimerHandle RemoveWidgetTimer;

	void RemoveActiveWidget();
};
