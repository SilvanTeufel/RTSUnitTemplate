// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Waypoint.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstance.h"
#include "Core/UnitData.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayEffect.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Developer/GraphColor/Private/appconst.h"
#include "GameplayTagContainer.h"
#include "RTSGameModeBase.generated.h"

class ARLAgent;
class ACameraControllerBase;
class APlayerStartBase;
class ARTSBTController;
class UBehaviorTree;
class UWorld;
class AUnitBase;
class AWinLoseConfigActor;
class ULoadingWidget;

USTRUCT(BlueprintType)
struct FTimerHandleMapping
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Timer")
	int32 Id;

	UPROPERTY(VisibleAnywhere, Category = "Timer")
	FTimerHandle Timer;

	UPROPERTY(VisibleAnywhere, Category = "Timer")
	bool SkipTimer = false;
};
UCLASS()
class RTSUNITTEMPLATE_API ARTSGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	// Pawn class to use when spawning AI players for AI PlayerStarts
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TSubclassOf<ARLAgent> AIPlayerPawnClass;

 // PlayerController class to use when spawning AI players (must derive from ACameraControllerBase)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TSubclassOf<ACameraControllerBase> AIPlayerControllerClass;

	// Optional: AI orchestrator controller class (non-possessing) that runs the Behavior Tree
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TSubclassOf<ARTSBTController> AIOrchestratorClass;

	// Optional: Behavior Tree to assign to the orchestrator if its StrategyBehaviorTree is not set
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTSUnitTemplate|AI")
	TObjectPtr<UBehaviorTree> AIBehaviorTree;
	
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate")
	void ApplyCustomizationsFromPlayerStart(APlayerController* PC, const APlayerStartBase* CustomStart);
		
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FTimerHandleMapping> SpawnTimerHandleMap;
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TimerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DisableSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float DelaySpawnTableTime = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int GatherControllerTimer = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|UI")
	TSubclassOf<class ULoadingWidget> LoadingWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "RTSUnitTemplate|WinLose")
	AWinLoseConfigActor* WinLoseConfigActor;

	bool bWinLoseTriggered = false;

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|WinLose")
	virtual void CheckWinLoseCondition(AUnitBase* DestroyedUnit = nullptr);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_TriggerWinLoseUI(bool bWon, TSubclassOf<class UWinLoseWidget> InWidgetClass, const FString& InMapName, FName DestinationSwitchTagToEnable);
	
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DataTableTimerStart();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool PathfindingIsRdy = false;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsPathfindingRdy(){ return PathfindingIsRdy; };
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void NavInitialisation();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamIdAndDefaultWaypoint(int Id, AWaypoint* Waypoint, ACameraControllerBase* CameraControllerBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void FillUnitArrays();
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamIdsAndWaypoints();
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetupTimerFromDataTable(FVector Location, AUnitBase* UnitToChase);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetupUnitsFromDataTable(FVector Location, AUnitBase* UnitToChase, const TArray<class UDataTable*>& UnitTable); // , int TeamId, const FString& WaypointTag, int32 UnitIndex = 0, AUnitBase* SummoningUnit = nullptr, int SummonIndex = -1
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FTimerHandleMapping GetTimerHandleMappingById(int32 SearchId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetSkipTimerMappingById(int32 SearchId, bool Value);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase); // , int TeamId, AWaypoint* Waypoint = nullptr

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* SpawnSingleUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsUnitWithIndexDead(int32 UnitIndex);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool RemoveDeadUnitWithIndexFromDataSet(int32 UnitIndex);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 CheckAndRemoveDeadUnits(int32 SpawnParaId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* SpawnSingleUnit(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnUnits(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase); // , int TeamId, AWaypoint* Waypoint = nullptr, int32 UnitIndex = 0, AUnitBase* SummoningUnit = nullptr, int SummonIndex = -1

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int AddUnitIndexAndAssignToAllUnitsArray(AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int AssignNewHighestIndex(AUnitBase* Unit);

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 HighestUnitIndex = 0;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddUnitIndexAndAssignToAllUnitsArrayWithIndex(AUnitBase* UnitBase, int32 Index, FUnitSpawnParameter SpawnParameter);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AssignWaypointToUnit(AUnitBase* UnitBase, const FString& WaypointTag);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector CalcLocation(FVector Offset, FVector MinRange, FVector MaxRange);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FUnitSpawnData> UnitSpawnDataSets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UDataTable*> UnitSpawnParameters;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 HighestSquadId = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AActor*> AllUnits;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AActor*> CameraUnits;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AWorkingUnitBase*> WorkingUnits;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <ASpeakingUnit*> SpeakingUnits;


	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	
};