// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
//#include "Characters/Unit/UnitBase.h"3
#include "Components/SceneComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "Core/WorkerData.h"
#include "Components/StaticMeshComponent.h"
#include "WorkArea.generated.h"

class AUnitBase;

USTRUCT(BlueprintType)
struct FBuildingCost
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 PrimaryCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 SecondaryCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 TertiaryCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 RareCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 EpicCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 LegendaryCost = 0;


	FString ToFormattedString() const {
		TArray<FString> parts;
        
		if(PrimaryCost > 0) parts.Add(FString::Printf(TEXT("Primary: %d"), PrimaryCost));
		if(SecondaryCost > 0) parts.Add(FString::Printf(TEXT("Secondary: %d"), SecondaryCost));
		if(TertiaryCost > 0) parts.Add(FString::Printf(TEXT("Tertiary: %d"), TertiaryCost));
		if(RareCost > 0) parts.Add(FString::Printf(TEXT("Rare: %d"), RareCost));
		if(EpicCost > 0) parts.Add(FString::Printf(TEXT("Epic: %d"), EpicCost));
		if(LegendaryCost > 0) parts.Add(FString::Printf(TEXT("Legendary: %d"), LegendaryCost));

		if (parts.Num())
		return FString::Join(parts, TEXT("\n"));
		else
		return FString("");
		
	}
};


UCLASS()
class RTSUNITTEMPLATE_API AWorkArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWorkArea();

	FTimerHandle HideWorkAreaTimerHandle;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// Optional: if provided, a construction site will be spawned and tracked during build
 UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	TSubclassOf<class AUnitBase> ConstructionUnitClass;
	
	// Pointer to the active construction site actor (if any)
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Construction)
	class AUnitBase* ConstructionUnit = nullptr;
	
	UFUNCTION(Server, Reliable,BlueprintCallable, Category = RTSUnitTemplate)
	void RemoveAreaFromGroup();
	
	UFUNCTION(Server, Reliable, Category = RTSUnitTemplate)
	void AddAreaToGroup();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USceneComponent* SceneRoot;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* Mesh;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 0;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsNoBuildZone = false;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = RTSUnitTemplate)
	class UCapsuleComponent* TriggerCapsule;
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleResourceExtractionArea(AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBaseArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SwitchResourceArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool SwitchBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	EResourceType ConvertWorkAreaTypeToResourceType(WorkAreaData::WorkAreaType WorkAreaType);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DespawnWorkResource(AWorkResource* WorkResource);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<WorkAreaData::WorkAreaType> Type = WorkAreaData::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AWorkResource> WorkResourceClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class ABuildingBase> BuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AAIController> BuildingController;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ABuildingBase* Building;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BuildTime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	float CurrentBuildTime = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float AvailableResourceAmount = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float MaxAvailableResourceAmount = AvailableResourceAmount;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BuildZOffset = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool PlannedBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool StartedBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool DestroyAfterBuild = true;

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StartedBuild();

	UFUNCTION(BlueprintImplementableEvent, Category = RTSUnitTemplate)
	void StartedResourceExtraction();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FBuildingCost ConstructionCost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ResetStartBuildTime = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float ControlTimer = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool IsPaid = false;
	//UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	//bool CanAffordConstruction(int32 TeamId, int32 NumberOfTeams, TArray<FResourceArray> TeamResources);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class AWaypoint* NextWaypoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Worker)
	FRotator ServerMeshRotationBuilding = FRotator (0.f, -90.f, 0.f);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetScale(FVector NewScale);

	// -- NEW TIMER HANDLE & PROPERTIES --
	/** Timer handle for reverting the material change. */
	FTimerHandle ChangeMaterialTimerHandle;

	/** Material to apply temporarily for 3 seconds when TemporarilyChangeMaterial is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* TemporaryHighlightMaterial;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void TemporarilyChangeMaterial();
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UMaterialInterface* OriginalMaterial;
    
	/** Called by a timer to revert the material back to its original state. */
	void RevertMaterial();
};
