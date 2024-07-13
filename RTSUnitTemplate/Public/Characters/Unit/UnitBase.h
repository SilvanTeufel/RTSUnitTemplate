// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkingUnitBase.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "PathSeekerBase.h"
#include "UnitBase.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AUnitBase : public AWorkingUnitBase
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString Name = "Unit";
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool IsPlayer = false;
	
	AUnitBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION()
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* CollisionUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector CollisionLocation;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FGameplayTagContainer UnitTags;
	
protected:
// Called when the game starts or when spawned
	virtual void BeginPlay() override;



public:	
// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	FRotator ServerMeshRotation = FRotator(0.f, -90.f, 0.f);
	
	UPROPERTY(Replicated, BlueprintReadWrite, ReplicatedUsing=OnRep_MeshAssetPath, Category = RTSUnitTemplate)
	FString MeshAssetPath;

	UPROPERTY(Replicated, BlueprintReadWrite, ReplicatedUsing=OnRep_MeshMaterialPath, Category = RTSUnitTemplate)
	FString MeshMaterialPath;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnRep_MeshAssetPath();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnRep_MeshMaterialPath();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetMeshRotationServer();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "IsFlying", Keywords = "RTSUnitTemplate IsFlying"), Category = RTSUnitTemplate)
		bool IsFlying = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FlyHeight", Keywords = "RTSUnitTemplate FlyHeight"), Category = RTSUnitTemplate)
		float FlyHeight = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool UEPathfindingUsed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool UsingUEPathfindingPatrol = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool SetUEPathfinding = true;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		//bool ReCalcRandomLocation = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float AutoSetUEPathfindingTimer = 0.f;
	
// RTSHud related //////////////////////////////////////////
public:

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartAttackEvent();

	UFUNCTION(NetMulticast, Reliable)
	void MultiCastStartAttackEvent();
	
	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void StartAttackEvent();


	UFUNCTION(Server, Reliable, WithValidation)
	void ServerMeeleImpactEvent();

	UFUNCTION(NetMulticast, Reliable)
	void MultiCastMeeleImpactEvent();
	
	UFUNCTION(BlueprintImplementableEvent, Category="RTSUnitTemplate")
	void MeeleImpactEvent();
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void IsAttacked(AActor* AttackingCharacter); // AActor* SelectedCharacter

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetRunLocation(FVector Location);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void SetWalkSpeed(float Speed);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "StopRunTolerance", Keywords = "RTSUnitTemplate StopRunTolerance"), Category = RTSUnitTemplate)
		float StopRunTolerance = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "StopRunToleranceY", Keywords = "RTSUnitTemplate StopRunToleranceY"), Category = RTSUnitTemplate)
		float StopRunToleranceForFlying = 100.f;
///////////////////////////////////////////////////////////////////

// related to Animations  //////////////////////////////////////////
public:

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "UnitControlTimer", Keywords = "RTSUnitTemplate UnitControlTimer"), Category = RTSUnitTemplate)
	float UnitControlTimer = 0.0f; // This Timer is used in UnitControllerBase Statemachine
/*
	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ToggleUnitDetection = false;

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void SetToggleUnitDetection(bool ToggleTo);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool GetToggleUnitDetection();
	*/
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "UnitToChase", Keywords = "RTSUnitTemplate UnitToChase"), Category = RTSUnitTemplate)
	AUnitBase* UnitToChase;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "UnitsToChase", Keywords = "RTSUnitTemplate UnitsToChase"), Category = RTSUnitTemplate)
	TArray <AUnitBase*> UnitsToChase;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetNextUnitToChase", Keywords = "RTSUnitTemplate SetNextUnitToChase"), Category = RTSUnitTemplate)
	bool SetNextUnitToChase();

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "DistanceToUnitToChase", Keywords = "RTSUnitTemplate DistanceToUnitToChase"), Category = RTSUnitTemplate)
	float DistanceToUnitToChase;
///////////////////////////////////////////////////////

// related to Waypoints  //////////////////////////////////////////
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "NextWaypoint", Keywords = "RTSUnitTemplate NextWaypoint"), Category = RTSUnitTemplate)
	class AWaypoint* NextWaypoint;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetWaypoint", Keywords = "RTSUnitTemplate SetWaypoint"), Category = RTSUnitTemplate)
	void SetWaypoint(class AWaypoint* NewNextWaypoint);
///////////////////////////////////////////////////////////////////



// related to Healthbar  //////////////////////////////////////////
public:

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "SetHealth", Keywords = "RTSUnitTemplate SetHealth"), Category = RTSUnitTemplate)
	void SetHealth(float NewHealth);

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "SetHealth", Keywords = "RTSUnitTemplate SetHealth"), Category = RTSUnitTemplate)
	void SetShield(float NewHealth);
	
protected:
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "HealthWidgetComp", Keywords = "RTSUnitTemplate HealthWidgetComp"), Category = RTSUnitTemplate)
		class UWidgetComponent* HealthWidgetComp;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "HealthWidgetCompLocation", Keywords = "RTSUnitTemplate HealthWidgetCompLocation"), Category = RTSUnitTemplate)
		FVector HealthWidgetCompLocation = FVector (0.f, 0.f, 180.f);
///////////////////////////////////////////////////////////////////


// HUDBase related ///////////
public:
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetSelected", Keywords = "TopDownRTSTemplate SetSelected"), Category = RTSUnitTemplate)
	void SetSelected();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetDeselected", Keywords = "TopDownRTSTemplate SetDeselected"), Category = RTSUnitTemplate)
	void SetDeselected();

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "RunLocationArray", Keywords = "RTSUnitTemplate RunLocationArray"), Category = RTSUnitTemplate)
	TArray <FVector> RunLocationArray;

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "RunLocationArrayIterator", Keywords = "RTSUnitTemplate RunLocationArrayIterator"), Category = RTSUnitTemplate)
	int32 RunLocationArrayIterator = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector RunLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector RandomPatrolLocation;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float LineTraceZDistance = 80.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float EvadeDistance = 70.f;
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	float EvadeDistanceChase = 150.f;
/////////////////////////////

// SelectedIcon related /////////
protected:
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SelectedCharacterIcon", Keywords = "RTSUnitTemplate SelectedCharacterIcon"), Category = TopDownRTSTemplate)
	class ASelectedIcon* SelectedIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class ASelectedIcon> SelectedIconBaseClass;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SpawnSelectedIcon", Keywords = "RTSUnitTemplate SpawnSelectedIcon"), Category = TopDownRTSTemplate)
	void SpawnSelectedIcon();
//////////////////////////////////////


// Projectile related /////////
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileBaseClass", Keywords = "TopDownRTSTemplate ProjectileBaseClass"), Category = RTSUnitTemplate)
	TSubclassOf<class AProjectile> ProjectileBaseClass;
	
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Projectile", Keywords = "RTSUnitTemplate Projectile"), Category = RTSUnitTemplate)
	class AProjectile* Projectile;

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "SpawnProjectile", Keywords = "RTSUnitTemplate SpawnProjectile"), Category = RTSUnitTemplate)
	void SpawnProjectile(AActor* Target, AActor* Attacker);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SpawnProjectileFromClass(AActor* Aim, AActor* Attacker, TSubclassOf<class AProjectile> ProjectileClass, int MaxPiercedTargets, bool FollowTarget, int ProjectileCount, float Spread, bool IsBouncingNext, bool IsBouncingBack, bool DisableAutoZOffset, float ZOffset, float Scale = 1.f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UseProjectile", Keywords = "RTSUnitTemplate UseProjectile"), Category = RTSUnitTemplate)
	bool UseProjectile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileSpawnOffset", Keywords = "RTSUnitTemplate ProjectileSpawnOffset"), Category = RTSUnitTemplate)
	FVector ProjectileSpawnOffset = FVector(0.f,0.f,0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileScale", Keywords = "RTSUnitTemplate ProjectileScale"), Category = RTSUnitTemplate)
	FVector ProjectileScale = FVector(1.f,1.f,1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FRotator ProjectileRotationOffset = FRotator(0.f,90.f,-90.f);
	//////////////////////////////////////
	
	
// Used for Despawn  //////////////////////////////////////////
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DestroyAfterDeath", Keywords = "RTSUnitTemplate DestroyAfterDeath"), Category = RTSUnitTemplate)
		bool DestroyAfterDeath = true;
///////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "PauseDuration", Keywords = "RTSUnitTemplate PauseDuration"), Category = RTSUnitTemplate)
		float PauseDuration = 0.6f;
	
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float CastTime = 5.f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ReduceCastTime = 0.5f;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
		float ReduceRootedTime = 0.1f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		class UWidgetComponent* TimerWidgetComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		FVector TimerWidgetCompLocation = FVector (0.f, 0.f, -180.f);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetupTimerWidget();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetTimerWidgetCastingColor(FLinearColor Color);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	TArray<FUnitSpawnData> SummonedUnitsDataSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ability)
	TArray<int> SummonedUnitIndexes;
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SpawnUnitsFromParameters(
		TSubclassOf<class AAIController> AIControllerBaseClass,
		TSubclassOf<class AUnitBase> UnitBaseClass, UMaterialInstance* Material, USkeletalMesh* CharacterMesh, FRotator HostMeshRotation, FVector Location,
		TEnumAsByte<UnitData::EState> UState,
		TEnumAsByte<UnitData::EState> UStatePlaceholder,
		int NewTeamId, AWaypoint* Waypoint, int UnitCount = 1, bool SummonContinuously = true);

	UFUNCTION(BlueprintCallable, Category = Ability)
	bool IsSpawnedUnitDead(int UIndex);
	
	UFUNCTION(BlueprintCallable, Category = Ability)
	void SetUnitBase(int UIndex, AUnitBase* NewUnit);


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsOnPlattform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsDragged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float EnergyCost = 20.f;
};








