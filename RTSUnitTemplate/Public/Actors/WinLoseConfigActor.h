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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FBuildingCost TargetResourceCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	float TargetGameTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	FGameplayTagContainer WinLoseTargetTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TArray<FGameplayTagCount> TargetTagCounts;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSubclassOf<class UWinLoseWidget> WinLoseWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|WinLose")
	TSoftObjectPtr<UWorld> WinLoseTargetMapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FName DestinationSwitchTagToEnable;

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
