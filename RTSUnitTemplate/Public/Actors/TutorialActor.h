// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Framework/Commands/InputChord.h"
#include "TutorialActor.generated.h"

class AUnitBase;
class AControllerBase;
class UTutorialWidget;

/** What the player has to do before the next tutorial text comes. */
UENUM(BlueprintType)
enum class ETutorialStepCondition : uint8
{
	/** Has at least Count units selected that fit the filter (UnitClasses / UnitTag / bWorkersOnly). */
	SelectUnits      UMETA(DisplayName = "Select Units"),

	/** Has placed Count buildings of UnitClasses since the tutorial started (construction site placed). */
	PlaceBuilding    UMETA(DisplayName = "Place Building"),

	/** Has finished Count buildings of UnitClasses since the tutorial started. */
	FinishBuilding   UMETA(DisplayName = "Finish Building"),

	/** Pressed Key (with its modifiers). */
	PressKey         UMETA(DisplayName = "Press Key"),

	/** Just waits WaitSeconds. */
	Wait             UMETA(DisplayName = "Wait"),

	/** Only ATutorialActor::CompleteCurrentStep (Blueprint, level script) goes on. */
	Manual           UMETA(DisplayName = "Manual"),
};

/** One text of the tutorial and what ends it. */
USTRUCT(BlueprintType)
struct FTutorialStep
{
	GENERATED_BODY()

	/**
	 * The hint. {Name} = the Name of the first of UnitClasses (as in the tooltips, e.g. "Core Tap"),
	 * {AName} = the same with "a" / "an" in front.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (MultiLine = "true"))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ETutorialStepCondition Condition = ETutorialStepCondition::SelectUnits;

	/** Units / buildings that count (or a parent class of them). Empty = any (SelectUnits only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial",
		meta = (EditCondition = "Condition == ETutorialStepCondition::SelectUnits || Condition == ETutorialStepCondition::PlaceBuilding || Condition == ETutorialStepCondition::FinishBuilding", EditConditionHides))
	TArray<TSubclassOf<AUnitBase>> UnitClasses;

	/** Only units with this tag in their UnitTags count (e.g. the tag of a control group key). None = no filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial",
		meta = (EditCondition = "Condition == ETutorialStepCondition::SelectUnits", EditConditionHides))
	FGameplayTag UnitTag;

	/** Only workers count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial",
		meta = (EditCondition = "Condition == ETutorialStepCondition::SelectUnits", EditConditionHides))
	bool bWorkersOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "1",
		EditCondition = "Condition == ETutorialStepCondition::SelectUnits || Condition == ETutorialStepCondition::PlaceBuilding || Condition == ETutorialStepCondition::FinishBuilding", EditConditionHides))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial",
		meta = (EditCondition = "Condition == ETutorialStepCondition::PressKey", EditConditionHides))
	FInputChord Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "0",
		EditCondition = "Condition == ETutorialStepCondition::Wait", EditConditionHides))
	float WaitSeconds = 3.f;

	/** The text stays at least this long, even when the player is already done. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "0"))
	float MinDisplayTime = 1.5f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTutorialStepEvent, int32, StepIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTutorialFinishedEvent);

/**
 * Place on a map to run a tutorial there: shows Steps[0].Text in WidgetClass, waits until the local player has
 * done what the step asks (Condition), then shows the next text, until all are done. Runs only on the machine
 * of the local player (not replicated, nothing on a dedicated server); reads the selection and the buildings
 * of the player's team. More steps: add them to Steps, or end Manual steps from Blueprint (CompleteCurrentStep).
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API ATutorialActor : public AActor
{
	GENERATED_BODY()

public:
	ATutorialActor();

	/** The panel (a Blueprint child of UTutorialWidget, e.g. WBP_Tutorial_AH). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	TSubclassOf<UTutorialWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	TArray<FTutorialStep> Steps;

	/** Starts by itself StartDelay seconds after BeginPlay; off = StartTutorial from Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bStartOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "0"))
	float StartDelay = 2.f;

	/**
	 * Starts by itself only the first N times this map is played on this machine (SaveGame "TutorialProgress").
	 * 0 = every time. Starting it by hand (ReplayTutorial, UTutorialReplayButton) always works and does not count.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "0", EditCondition = "bStartOnBeginPlay"))
	int32 MaxAutoStarts = 0;

	/** Key of the counter for MaxAutoStarts. Empty = name of the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (EditCondition = "MaxAutoStarts > 0"))
	FString ProgressKey;

	/** Pause between a done step and the next text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "0"))
	float StepDelay = 0.5f;

	/** Seconds between two checks of the conditions (the keys are checked every frame). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial", meta = (ClampMin = "0.02"))
	float CheckInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 WidgetZOrder = 20;

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StartTutorial();

	/** Ends the current step, as if the player had done it (for Manual steps). */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void CompleteCurrentStep();

	/** Hides the panel and stops. */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void StopTutorial();

	/** Starts the tutorial of this map again from the first step (first ATutorialActor in the world). False = none there. */
	UFUNCTION(BlueprintCallable, Category = "Tutorial", meta = (WorldContext = "WorldContextObject"))
	static bool ReplayTutorial(const UObject* WorldContextObject);

	/** True when the map has an ATutorialActor with steps. */
	UFUNCTION(BlueprintPure, Category = "Tutorial", meta = (WorldContext = "WorldContextObject"))
	static bool HasTutorial(const UObject* WorldContextObject);

	/** How often the tutorial of this actor already started by itself (MaxAutoStarts). */
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	int32 GetAutoStartCount() const;

	/** Index of the shown step, INDEX_NONE before the start and after the end. */
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	int32 GetCurrentStepIndex() const { return CurrentStep; }

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FTutorialStepEvent OnStepStarted;

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FTutorialStepEvent OnStepCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FTutorialFinishedEvent OnTutorialFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BeginStep(int32 Index);

	/** The X on the panel. */
	UFUNCTION()
	void HandleWidgetClosed();
	/** Step.Text with {Name} / {AName} filled in. */
	static FText MakeStepText(const FTutorialStep& Step);
	bool IsStepDone(const FTutorialStep& Step) const;
	bool KeyPressed(const FTutorialStep& Step) const;
	/** Buildings of Step.UnitClasses of the player's team: placed sites + buildings (bFinishedOnly: buildings). */
	int32 CountBuildings(const FTutorialStep& Step, bool bFinishedOnly) const;
	static bool MatchesClasses(const FTutorialStep& Step, const UClass* Class);
	AControllerBase* GetLocalController() const;
	FString GetProgressKey() const;
	/** Counts this auto start; false when MaxAutoStarts is reached. */
	bool ConsumeAutoStart();

	UPROPERTY(Transient)
	TObjectPtr<UTutorialWidget> Widget;

	/** Per step: CountBuildings at the tutorial start, so that only what the player builds counts. */
	TArray<int32> BuildingBaseline;

	int32 CurrentStep = INDEX_NONE;
	bool bRunning = false;
	bool bStepDone = false;
	float StepTime = 0.f;
	float DoneTime = 0.f;
	float CheckTimer = 0.f;
	float StartTimer = -1.f;
};
