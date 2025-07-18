// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetourCrowdAIController.h"

#include "Navigation/PathFollowingComponent.h"

#include "Characters/Unit/UnitBase.h"
#include "Hud/HUDBase.h"
#include "Hud/PathProviderHUD.h"
#include "NavigationSystem.h"
#include "Engine.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "NavMesh/NavMeshPath.h"
#include "Widgets/UnitBaseHealthBar.h"


#include "UnitControllerBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoveCompletedDelegate, FAIRequestID, RequestID, EPathFollowingResult::Type, Result);
/**
 * 
 */

UCLASS()
class RTSUNITTEMPLATE_API AUnitControllerBase : public  ADetourCrowdAIController
{
	GENERATED_BODY()

private:

	// Workaround helper functions
	//bool MoveToUEPathFindingAvoidance(AUnitBase* Unit, const FVector& DestinationLocation);
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool PerformLineTrace(AUnitBase* Unit, const FVector& DestinationLocation, FHitResult& HitResult);
	//bool OnLineTraceHit(AUnitBase* Unit, const FVector& DestinationLocation);
	//FVector CalculateAlternateLocation(AUnitBase* Unit, const FVector& DestinationLocation);

	// Timer handle for delayed movement
	//FTimerHandle MoveRetryTimerHandle;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool DisableUnitControllerDetection = false;
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	AUnitBase* PendingUnit = nullptr;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	FVector PendingDestination;
	
	//FTimerHandle TimerHandle_DetectAndLoseUnits;
	//float TimerInterval = 0.25f;

public:
	AUnitControllerBase();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DetectAndLoseUnits();
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	AUnitBase* MyUnitBase;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	ARTSGameModeBase* RTSGameMode;
	
	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	AExtendedControllerBase* ControllerBase;

	UPROPERTY(VisibleAnywhere, Category = RTSUnitTemplate)
	UUnitBaseHealthBar* HealthBarWidget;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool RotateWhileCasting = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool RotateToUnitToChase = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float TickInterval = 0.25f; 
	
	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* PawN) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	virtual void Tick(float DeltaSeconds) override;
	
	virtual FRotator GetControlRotation() const override;
	
	FOnMoveCompletedDelegate OnMoveCompleted;
	
	void OnAdjustedMoveCompleted(FAIRequestID RequestID, const EPathFollowingResult::Type Result)
	{
		if(PendingUnit &&  Result == EPathFollowingResult::Success) //  Result.IsSuccess()
		{
			MoveToLocationUEPathFinding(PendingUnit, PendingDestination);
			// Reset the PendingUnit and PendingDestination to avoid reusing them incorrectly
			PendingUnit = nullptr;
		}
	};

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "KillUnitBase", Keywords = "RTSUnitTemplate KillUnitBase"), Category = RTSUnitTemplate)
		void KillUnitBase(AUnitBase* UnitBase);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SightRadius", Keywords = "RTSUnitTemplate SightRadius"), Category = RTSUnitTemplate)
		float SightRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SightAge", Keywords = "RTSUnitTemplate SightAge"), Category = RTSUnitTemplate)
		float SightAge = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "LoseSightRadius", Keywords = "RTSUnitTemplate LoseSightRadius"), Category = RTSUnitTemplate)
		float LoseSightRadius = SightRadius + 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FieldOfView", Keywords = "RTSUnitTemplate FieldOfView"), Category = RTSUnitTemplate)
		float FieldOfView = 360.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DespawnTime", Keywords = "RTSUnitTemplate DespawnTime"), Category = RTSUnitTemplate)
		float DespawnTime = 4.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "IsAttackedDuration", Keywords = "RTSUnitTemplate IsAttackedDuration"), Category = RTSUnitTemplate)
		float IsAttackedDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float IsRootedDuration = 5.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "AttackAngleTolerance", Keywords = "RTSUnitTemplate AttackAngleTolerance"), Category = RTSUnitTemplate)
		float AttackAngleTolerance = 0.f;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SightConfig", Keywords = "RTSUnitTemplate SightConfig"), Category = RTSUnitTemplate)
		class UAISenseConfig_Sight* SightConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DistanceToUnitToChase", Keywords = "RTSUnitTemplate DistanceToUnitToChase"), Category = RTSUnitTemplate)
		float DistanceToUnitToChase = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DetectFriendlyUnits", Keywords = "RTSUnitTemplate DetectFriendlyUnits"), Category = RTSUnitTemplate)
		bool DetectFriendlyUnits = false;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OnUnitDetected", Keywords = "RTSUnitTemplate OnUnitDetected"), Category = RTSUnitTemplate)
		void OnUnitDetected(const TArray<AActor*>& DetectedUnits, bool SetState);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
		float RotationSpeed = 5.0f;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotateToAttackUnit", Keywords = "RTSUnitTemplate RotateToAttackUnit"), Category = RTSUnitTemplate)
		void RotateToAttackUnit(AUnitBase* AttackingUnit, AUnitBase* UnitToAttackk, float DeltaSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool Debug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool DebugDetection = false;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnitControlStateMachine", Keywords = "RTSUnitTemplate UnitControlStateMachine"), Category = RTSUnitTemplate)
		void UnitControlStateMachine(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void Rooted(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void Casting(AUnitBase* UnitBase, float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Dead", Keywords = "RTSUnitTemplate Dead"), Category = RTSUnitTemplate)
		bool IsUnitToChaseInRange(AUnitBase* UnitBase);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Dead", Keywords = "RTSUnitTemplate Dead"), Category = RTSUnitTemplate)
		void Dead(AUnitBase* UnitBase, float DeltaSeconds);

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float UnitDetectionTimer = 0.0f;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float NewDetectionTime = 3.f;
	
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool IsUnitDetected = false;

	UFUNCTION(BlueprintCallable,  Category = RTSUnitTemplate)
		void CheckUnitDetectionTimer(float DeltaSeconds);

	UFUNCTION(BlueprintCallable,  Category = RTSUnitTemplate)
		void DetectUnitsFromGameMode(AUnitBase* DetectingUnit, TArray<AActor*>& DetectedUnits, float Sight);
	
	UFUNCTION(BlueprintCallable,  Category = RTSUnitTemplate)
		void DetectUnitsAndSetState(AUnitBase* UnitBase, float DeltaSeconds, bool SetState);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Patrol", Keywords = "RTSUnitTemplate Patrol"), Category = RTSUnitTemplate)
		void Patrol(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Run", Keywords = "RTSUnitTemplate Run"), Category = RTSUnitTemplate)
		void Run(AUnitBase* UnitBase, float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Chase", Keywords = "RTSUnitTemplate Chase"), Category = RTSUnitTemplate)
		void Chase(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void ActivateCombatAbilities(AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		FVector CalculateChaseLocation(AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void LoseUnitToChase(AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void ResetPath(AUnitBase* UnitBase);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Attack", Keywords = "RTSUnitTemplate Attack"), Category = RTSUnitTemplate)
		void Attack(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Pause", Keywords = "RTSUnitTemplate Pause"), Category = RTSUnitTemplate)
		void Pause(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsAttacked", Keywords = "RTSUnitTemplate IsAttacked"), Category = RTSUnitTemplate)
		void IsAttacked(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Idle", Keywords = "RTSUnitTemplate Idle"), Category = RTSUnitTemplate)
		void Idle(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void EvasionChase(AUnitBase* UnitBase, float DeltaSeconds, FVector CollisionLocation);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void EvasionIdle(AUnitBase* UnitBase, FVector CollisionLocation);

	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetUnitBackToPatrol(AUnitBase* UnitBase, float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void RunUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool SetUnitsBackToPatrol = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SetUnitsBackToPatrolTime = 10.f;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void PatrolUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	FVector GetCloseLocation(FVector ToLocation, float Distance);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetPatrolCloseLocation(AUnitBase* UnitBase);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetUEPathfindingRandomLocation(AUnitBase* UnitBase, float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		bool SetUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds, FVector Location, AUnitBase* UnitToIgnore = nullptr);

	//UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		//void SetUEPathfindingTo(AUnitBase* UnitBase, float DeltaSeconds, FVector Location);

	//UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	//bool MoveToLocationUEPathFinding(AUnitBase* Unit, const FVector& DestinationLocation);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool DirectMoveToLocation(AUnitBase* Unit, const FVector& DestinationLocation);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool MoveToLocationUEPathFindingAvoidance(AUnitBase* Unit, const FVector& DestinationLocation, AUnitBase* UnitToIgnore = nullptr);

	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool MoveToLocationUEPathFinding(AUnitBase* Unit, const FVector& DestinationLocation, AUnitBase* UnitToIgnore = nullptr);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopMovementCommand(AUnitBase* Unit);
		//void OnAdjustedMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "CreateProjectile", Keywords = "RTSUnitTemplate CreateProjectile"), Category = RTSUnitTemplate)
		void CreateProjectile (AUnitBase* UnitBase);

	UPROPERTY(BlueprintReadWrite,  Category = RTSUnitTemplate)
		bool ProjectileSpawned = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
		float AcceptanceRadius = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
		bool DebugPatrolRandom = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
		UClass* SurfaceClass = AStaticMeshActor::StaticClass();
};

