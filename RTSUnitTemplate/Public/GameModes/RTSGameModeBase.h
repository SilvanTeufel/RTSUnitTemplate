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
#include "RTSGameModeBase.generated.h"


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
//protected:
	public:

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	// Timer handle for spawning units
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<FTimerHandleMapping> SpawnTimerHandleMap;
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TimerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DisableSpawn = true;
//public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int GatherUnitsTimer = 5.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int GatherControllerTimer = 10.f;
	
	virtual void BeginPlay() override;

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
	AUnitBase* SpawnSingleUnits(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<int32> AvailableUnitIndexArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<int32> SpawnParameterIdArray;
	
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

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <AWorkingUnitBase*> WorkingUnits;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray <ASpeakingUnit*> SpeakingUnits;
};