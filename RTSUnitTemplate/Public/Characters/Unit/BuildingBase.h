// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilityUnit.h"
#include "NavModifierVolume.h"
#include "GameFramework/Character.h"
#include "Components/WidgetComponent.h"
#include "Core/UnitData.h"
#include "Actors/WorkArea.h"
#include "Actors/WorkResource.h"
#include "PathSeekerBase.h"
#include "UnitBase.h"
#include "BuildingBase.generated.h"


UCLASS()
class RTSUNITTEMPLATE_API ABuildingBase : public AUnitBase
{
	GENERATED_BODY()
	
public:

	// Constructor declaration
	ABuildingBase(const FObjectInitializer& ObjectInitializer);
	
	// Rotate a Niagara component to face the Origin (AWorkingUnitBase) of the WorkArea, with an optional rotation offset.
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = Construction)
	void MulticastRotateNiagaraToOrigin(UNiagaraComponent* NiagaraToRotate, const FRotator& RotationOffset, float InRotateDuration, float InRotationEaseExponent, ERotationAxis AxisSelection = ERotationAxis::Full);

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool HasWaypoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool CancelsAbilityOnRightClick = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsBase = false;

	// Per-building adjustment to controller SnapGap (can be negative). Effective gap is clamped to >= 0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuildingSnap)
	float SnapGapAdjustment = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BeaconRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ExtensionOffset = FVector(20.f, 20.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ExtensionGroundTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool ExtensionDominantSideSelection = true;
	
	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	ABuildingBase* Extension = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = RTSUnitTemplate)
	ABuildingBase* Origin = nullptr;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetBeaconRange(float NewRange);
	
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;
	
	virtual void Destroyed() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComp, 
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp, 
		int32 OtherBodyIndex,
		bool bFromSweep, 
		const FHitResult& SweepResult
	);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBaseArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SwitchResourceArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, int32 RecursionCount = 0);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool SwitchBuildArea(AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DespawnWorkResource(AWorkResource* ResourceToDespawn);


	virtual void MulticastSetEnemyVisibility_Implementation(AActor* DetectingActor, bool bVisible);

	virtual void SetEnemyVisibility(AActor* DetectingActor, bool bVisible) override;

	// Returns true if this building's location is within range of any Beacon (any BuildingBase with BeaconRange > 0)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsInBeaconRange() const;

	// Utility: Returns true if the given world location is within range of any Beacon (any BuildingBase with BeaconRange > 0)
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	static bool IsLocationInBeaconRange(UWorld* World, const FVector& Location);
};








