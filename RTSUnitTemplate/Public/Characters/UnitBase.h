// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "PathSeekerBase.h"
#include "UnitBase.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API AUnitBase : public APathSeekerBase
{
	GENERATED_BODY()
	
public:
	AUnitBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION()
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	AUnitBase* CollisionUnit;
	
protected:
// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	
public:	
// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	class USkeletalMeshComponent* CharacterMesh;
	
	UPROPERTY(Replicated, EditAnywhere, Category = RTSUnitTemplate)
	FRotator MeshRotation = FRotator(0.f, -90.f, 0.f);
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
		void SetMeshRotation(FRotator NewRotation);
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "TeamId", Keywords = "RTSUnitTemplate IsFriendly"), Category = RTSUnitTemplate)
		int TeamId = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "IsFlying", Keywords = "RTSUnitTemplate IsFlying"), Category = RTSUnitTemplate)
		bool IsFlying = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FlyHeight", Keywords = "RTSUnitTemplate FlyHeight"), Category = RTSUnitTemplate)
		float FlyHeight = 500.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Range", Keywords = "RTSUnitTemplate Range"), Category = RTSUnitTemplate)
		float Range = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "StopChaseAtDistance", Keywords = "RTSUnitTemplate StopChaseAtDistance"), Category = RTSUnitTemplate)
		float StopChaseAtDistance = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool UEPathfindingUsed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool UsingUEPathfindingPatrol = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		bool SetUEPathfinding = true;
// RTSHud related //////////////////////////////////////////
public:
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void IsAttacked(AActor* AttackingCharacter); // AActor* SelectedCharacter

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = RTSUnitTemplate)
	void SetRunLocation(FVector Location);
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void SetWalkSpeed(float Speed);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "MaxRunSpeed", Keywords = "RTSUnitTemplate MaxRunSpeed"), Category = RTSUnitTemplate)
		float MaxRunSpeed = 400.f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "IsAttackedSpeed", Keywords = "RTSUnitTemplate IsAttackedSpeed"), Category = RTSUnitTemplate)
		float IsAttackedSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "RunSpeedScale", Keywords = "RTSUnitTemplate RunSpeedScale"), Category = RTSUnitTemplate)
		float RunSpeedScale = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "StopRunTolerance", Keywords = "RTSUnitTemplate StopRunTolerance"), Category = RTSUnitTemplate)
		float StopRunTolerance = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "StopRunToleranceY", Keywords = "RTSUnitTemplate StopRunToleranceY"), Category = RTSUnitTemplate)
		float StopRunToleranceForFlying = 100.f;
///////////////////////////////////////////////////////////////////

// related to Animations  //////////////////////////////////////////
public:

	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "UnitControlTimer", Keywords = "RTSUnitTemplate UnitControlTimer"), Category = RTSUnitTemplate)
	float UnitControlTimer = 0.0f; // This Timer is used in UnitControllerBase Statemachine

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "AttackDamage", Keywords = "RTSUnitTemplate AttackDamage"), Category = RTSUnitTemplate)
	float AttackDamage = 40.0f;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ToggleUnitDetection = false;

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "CreateCameraComp", Keywords = "RTSUnitTemplate CreateCameraComp"), Category = RTSUnitTemplate)
	void SetToggleUnitDetection(bool ToggleTo);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool GetToggleUnitDetection();
	
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

// Set Unit States  //////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState);
	//void SetUnitState(TEnumAsByte<UnitData::EState> NewUnitState);
	//virtual void SetUnitState_Implementation(TEnumAsByte<UnitData::EState> NewUnitState);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> GetUnitState();

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UnitState", Keywords = "RTSUnitTemplate UnitState"), Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitState = UnitData::Idle;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UnitStatePlaceholder", Keywords = "RTSUnitTemplate UnitStatePlaceholder"), Category = RTSUnitTemplate)
	TEnumAsByte<UnitData::EState> UnitStatePlaceholder = UnitData::Patrol;
///////////////////////////////////////////////////////////////////

// related to Healthbar  //////////////////////////////////////////
public:
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetHealth", Keywords = "RTSUnitTemplate GetHealth"), Category = RTSUnitTemplate)
	float GetHealth();

	UFUNCTION(Server, Reliable, BlueprintCallable, meta = (DisplayName = "SetHealth", Keywords = "RTSUnitTemplate SetHealth"), Category = RTSUnitTemplate)
	void SetHealth(float NewHealth);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetMaxHealth", Keywords = "RTSUnitTemplate GetMaxHealth"), Category = RTSUnitTemplate)
	float GetMaxHealth();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	float GetShield();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetShield(float NewShield);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	float GetMaxShield();
	
protected:
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "HealthWidgetComp", Keywords = "RTSUnitTemplate HealthWidgetComp"), Category = RTSUnitTemplate)
		class UWidgetComponent* HealthWidgetComp;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "Health", Keywords = "RTSUnitTemplate Health"), Category = RTSUnitTemplate)
		float Shield;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float MaxShield = 200.f;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
		float SpawnShield = 60.f;
	
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "Health", Keywords = "RTSUnitTemplate Health"), Category = RTSUnitTemplate)
		float Health;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "MaxHealth", Keywords = "RTSUnitTemplate MaxHealth"), Category = RTSUnitTemplate)
		float MaxHealth = 120;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SpawnHealth", Keywords = "RTSUnitTemplate SpawnHealth"), Category = RTSUnitTemplate)
		float SpawnHealth = 120;
	
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

	UPROPERTY( BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector RandomPatrolLocation;
/////////////////////////////

// SelectedIcon related /////////
protected:
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "SelectedCharacterIcon", Keywords = "RTSUnitTemplate SelectedCharacterIcon"), Category = TopDownRTSTemplate)
	class ASelectedIcon* SelectedIcon;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UseProjectile", Keywords = "RTSUnitTemplate UseProjectile"), Category = RTSUnitTemplate)
	bool UseProjectile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileSpawnOffset", Keywords = "RTSUnitTemplate ProjectileSpawnOffset"), Category = RTSUnitTemplate)
	FVector ProjectileSpawnOffset = FVector(0.f,0.f,0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileScaleActorDirectionOffset", Keywords = "RTSUnitTemplate ProjectileScaleActorDirectionOffset"), Category = RTSUnitTemplate)
	float ProjectileScaleActorDirectionOffset = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileSpeed", Keywords = "RTSUnitTemplate ProjectileSpeed"), Category = RTSUnitTemplate)
	float ProjectileSpeed = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ProjectileScale", Keywords = "RTSUnitTemplate ProjectileScale"), Category = RTSUnitTemplate)
	FVector ProjectileScale = FVector(1.f,1.f,1.f);
	//////////////////////////////////////
	
	
// Used for Despawn  //////////////////////////////////////////
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "DestroyAfterDeath", Keywords = "RTSUnitTemplate DestroyAfterDeath"), Category = RTSUnitTemplate)
	bool DestroyAfterDeath = true;
///////////////////////////////////////////////////////////////////
	
	/*
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (DisplayName = "TriggerCapsule", Keywords = "RTSUnitTemplate TriggerCapsule"), Category = RTSUnitTemplate)
	class UCapsuleComponent* TriggerCapsule;

	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);*/
};






