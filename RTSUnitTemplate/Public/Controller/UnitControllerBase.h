// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Characters/UnitBase.h"
#include "Hud/HUDBase.h"
#include "Hud/PathProviderHUD.h"
#include "UnitControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API AUnitControllerBase : public AAIController
{
	GENERATED_BODY()

public:
	AUnitControllerBase();

	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* Pawn) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	virtual void Tick(float DeltaSeconds) override;
	
	virtual FRotator GetControlRotation() const override;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "KillUnitBase", Keywords = "RTSUnitTemplate KillUnitBase"), Category = RTSUnitTemplate)
		void KillUnitBase(AUnitBase* UnitBase);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SightRadius", Keywords = "RTSUnitTemplate SightRadius"), Category = RTSUnitTemplate)
		float SightRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SightAge", Keywords = "RTSUnitTemplate SightAge"), Category = RTSUnitTemplate)
		float SightAge = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "LoseSightRadius", Keywords = "RTSUnitTemplate LoseSightRadius"), Category = RTSUnitTemplate)
		float LoseSightRadius = SightRadius + 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FieldOfView", Keywords = "RTSUnitTemplate FieldOfView"), Category = RTSUnitTemplate)
		float FieldOfView = 360.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DespawnTime", Keywords = "RTSUnitTemplate DespawnTime"), Category = RTSUnitTemplate)
		float DespawnTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "PauseDuration", Keywords = "RTSUnitTemplate PauseDuration"), Category = RTSUnitTemplate)
		float PauseDuration = 0.6f; // Duratin of the State Pause
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "AttackDuration", Keywords = "RTSUnitTemplate AttackDuration"), Category = RTSUnitTemplate)
		float AttackDuration = 0.6f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "IsAttackedDuration", Keywords = "RTSUnitTemplate IsAttackedDuration"), Category = RTSUnitTemplate)
		float IsAttackedDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "AttackAngleTolerance", Keywords = "RTSUnitTemplate AttackAngleTolerance"), Category = RTSUnitTemplate)
		float AttackAngleTolerance = 0.f;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SightConfig", Keywords = "RTSUnitTemplate SightConfig"), Category = RTSUnitTemplate)
		class UAISenseConfig_Sight* SightConfig;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "DistanceToUnitToChase", Keywords = "RTSUnitTemplate DistanceToUnitToChase"), Category = RTSUnitTemplate)
		float DistanceToUnitToChase = 0.0f;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "DetectFriendlyUnits", Keywords = "RTSUnitTemplate DetectFriendlyUnits"), Category = RTSUnitTemplate)
		bool DetectFriendlyUnits = false;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "OnUnitDetected", Keywords = "RTSUnitTemplate OnUnitDetected"), Category = RTSUnitTemplate)
		void OnUnitDetected(const TArray<AActor*>& DetectedUnits);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotateToAttackUnit", Keywords = "RTSUnitTemplate RotateToAttackUnit"), Category = RTSUnitTemplate)
		void RotateToAttackUnit(AUnitBase* AttackingUnit, AUnitBase* UnitToAttack);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "UnitControlStateMachine", Keywords = "RTSUnitTemplate UnitControlStateMachine"), Category = RTSUnitTemplate)
		void UnitControlStateMachine(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Dead", Keywords = "RTSUnitTemplate Dead"), Category = RTSUnitTemplate)
		bool IsUnitToChaseInRange(AUnitBase* UnitBase);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Dead", Keywords = "RTSUnitTemplate Dead"), Category = RTSUnitTemplate)
		void Dead(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Patrol", Keywords = "RTSUnitTemplate Patrol"), Category = RTSUnitTemplate)
		void Patrol(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Run", Keywords = "RTSUnitTemplate Run"), Category = RTSUnitTemplate)
		void Run(AUnitBase* UnitBase, float DeltaSeconds);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Chase", Keywords = "RTSUnitTemplate Chase"), Category = RTSUnitTemplate)
		void Chase(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Attack", Keywords = "RTSUnitTemplate Attack"), Category = RTSUnitTemplate)
		void Attack(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Pause", Keywords = "RTSUnitTemplate Pause"), Category = RTSUnitTemplate)
		void Pause(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "IsAttacked", Keywords = "RTSUnitTemplate IsAttacked"), Category = RTSUnitTemplate)
		void IsAttacked(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Idle", Keywords = "RTSUnitTemplate Idle"), Category = RTSUnitTemplate)
		void Idle(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void RunUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void PatrolUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetUEPathfinding(AUnitBase* UnitBase, float DeltaSeconds, FVector Location);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
		void SetUEPathfindingTo(AUnitBase* UnitBase, float DeltaSeconds, FVector Location);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "CreateProjectile", Keywords = "RTSUnitTemplate CreateProjectile"), Category = RTSUnitTemplate)
		void CreateProjectile (AUnitBase* UnitBase);

	UPROPERTY(BlueprintReadWrite,  Category = RTSUnitTemplate)
		bool ProjectileSpawned = false;
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DisableBaseConstructor", Keywords = "RTSUnitTemplate DisableBaseConstructor"), Category = RTSUnitTemplate)
	//bool DisableBaseConstructor = false;
};

