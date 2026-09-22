// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Core/UnitData.h"
#include "WinLoseConfigActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWinLoseEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWinConditionChanged, AWinLoseConfigActor*, Config, EWinLoseCondition, NewCondition);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTagProgressUpdated, AWinLoseConfigActor*, Config);

USTRUCT(BlueprintType)
struct FTagProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|WinLose")
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|WinLose")
	int32 AliveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|WinLose")
	int32 TotalCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|WinLose")
	int32 TargetCount = 0;
};

USTRUCT(BlueprintType)
struct FGameplayTagCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct FWinConditionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	EWinLoseCondition Condition = EWinLoseCondition::None;

	/**
	 * Freier Beschreibungstext fuer genau dieses Ziel. Ist er gesetzt, zeigt das
	 * WinConditionWidget ihn ANSTELLE des automatisch gebauten Satzes.
	 *
	 * Gedacht fuer Ziele, die sich nicht aus Zahlen erklaeren lassen: TeamReachedLocation hatte
	 * im Widget gar keinen Zweig und landete deshalb bei "Unknown". Leer gelassen bleibt alles
	 * wie bisher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose", meta = (MultiLine = "true"))
	FText CustomDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FBuildingCost TargetResourceCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	float TargetGameTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FGameplayTagContainer WinLoseTargetTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TArray<FGameplayTagCount> TargetTagCounts;

	/** Zielort fuer TeamReachedLocation (Weltkoordinate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FVector TargetLocation = FVector::ZeroVector;

	/** Wie nah eine Einheit dem Zielort kommen muss. Gemessen wird in X/Y, damit die Hoehe
	 *  keine Rolle spielt - sonst zaehlt ein Ziel auf einem Berg erst bei exakter Hoehe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	float TargetLocationRadius = 900.f;
};

UCLASS()
class RTSUNITTEMPLATE_API AWinLoseConfigActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AWinLoseConfigActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|WinLose")
	FOnWinLoseEvent OnYouWonTheGame;

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|WinLose")
	FOnWinLoseEvent OnYouLostTheGame;

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|WinLose")
	FOnWinConditionChanged OnWinConditionChanged;

	UPROPERTY(BlueprintAssignable, Category = "RTSUnitTemplate|WinLose")
	FOnTagProgressUpdated OnTagProgressUpdated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	float WinConditionDisplayDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	float GameStartDisplayDuration = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	float InitialDisplayDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	float WinDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	float LoseDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	TArray<FWinConditionData> WinConditions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWinConditionIndex, Category = "RTSUnitTemplate|WinLose")
	int32 CurrentWinConditionIndex = 0;

	UFUNCTION()
	void OnRep_CurrentWinConditionIndex();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	EWinLoseCondition LoseCondition = EWinLoseCondition::AllBuildingsDestroyed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	int32 TeamId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	FBuildingCost TargetResourceCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	float TargetGameTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "RTSUnitTemplate|WinLose")
	FGameplayTagContainer WinLoseTargetTags;

	/**
	 * Traegt die ueberlebte Zeit am Spielende in die Survival-Bestenliste ein
	 * (USurvivalScoreSubsystem). Nur fuer Endlos-Karten sinnvoll.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	bool bReportSurvivalScore = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSubclassOf<class UWinLoseWidget> WinLoseWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSoftObjectPtr<UWorld> WinLoseTargetMapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FName DestinationSwitchTagToEnable;

	/**
	 * Further tags switched on for WinLoseTargetMapName besides the one above.
	 *
	 * One level often opens more than one door: beating Level_3 both reveals the next planet and
	 * adds a second mission to the planet it was played from. With a single FName the second of
	 * those had to borrow the first one's tag, which quietly ties two unlocks together that have
	 * nothing to do with each other. Empty by default, so levels that open exactly one door are
	 * unaffected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FName> AdditionalSwitchTagsToEnable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_TagProgress, Category = "RTSUnitTemplate|WinLose")
	TArray<FTagProgress> TagProgress;

	UFUNCTION()
	void OnRep_TagProgress();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	EWinLoseCondition GetCurrentWinCondition() const;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	FWinConditionData GetCurrentWinConditionData() const;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	bool IsLastWinCondition() const;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	void AdvanceToNextWinCondition();

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose", meta = (WorldContext = "WorldContextObject"))
	static AWinLoseConfigActor* GetWinLoseConfigForTeam(const UObject* WorldContextObject, int32 MyTeamId);

protected:
	virtual void BeginPlay() override;

};
