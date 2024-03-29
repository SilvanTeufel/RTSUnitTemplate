// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Actors/Waypoint.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstance.h"
#include "Core/UnitData.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayEffect.h"
#include "RTSGameModeBase.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FUnitSpawnParameter : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int Id;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AUnitBase> UnitBaseClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AAIController> UnitControllerBaseClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int UnitCount = 3;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector UnitOffset = FVector(0.f,0.f,1.f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector UnitMinRange = FVector(0.f,0.f,0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector UnitMaxRange = FVector(100.f,100.f,0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> State;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> StatePlaceholder;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float LoopTime = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ShouldLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString WaypointTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int MaxUnitSpawnCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	USkeletalMesh* CharacterMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInstance* Material;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FRotator ServerMeshRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RTSUnitTemplate)
	TSubclassOf<class UGameplayEffect> Attributes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool LoadLevelAfterSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool SkipTimerAfterDeath = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool SpawnAtWaypoint = false;
};

USTRUCT(BlueprintType)
struct FUnitSpawnData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int Id;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintReadWrite, Category = RTSUnitTemplate)
	class AUnitBase* UnitBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintReadWrite, Category = RTSUnitTemplate)
	FUnitSpawnParameter SpawnParameter;
};

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
	virtual void BeginPlay() override;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamId(int Id, ACameraControllerBase* CameraControllerBase);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetTeamIds();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetupTimerFromDataTable(FVector Location, AUnitBase* UnitToChase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FTimerHandleMapping GetTimerHandleMappingById(int32 SearchId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetSkipTimerMappingById(int32 SearchId, bool Value);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* SpawnSingleUnitFromDataTable(int id, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	int32 CheckAndRemoveDeadUnits(int32 SpawnParaId);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AUnitBase* SpawnSingleUnits(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<int32> AvailableUnitIndexArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<int32> SpawnParameterIdArray;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnUnits(FUnitSpawnParameter SpawnParameter, FVector Location, AUnitBase* UnitToChase, int TeamId, AWaypoint* Waypoint = nullptr);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void AddUnitIndexAndAssignToAllUnitsArray(AUnitBase* UnitBase);

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

};