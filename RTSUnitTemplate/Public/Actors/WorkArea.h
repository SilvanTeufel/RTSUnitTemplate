// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
//#include "Characters/Unit/UnitBase.h"
#include "TimerManager.h"
#include "Core/WorkerData.h"
#include "WorkArea.generated.h"


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
};


UCLASS()
class RTSUNITTEMPLATE_API AWorkArea : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWorkArea();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FString Tag;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	UStaticMeshComponent* Mesh;
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<UGameplayEffect> AreaEffect;

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleResourceExtractionArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBaseArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SwitchResourceArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SwitchBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void HandleBuildArea(AWorkingUnitBase* Worker, AUnitBase* UnitBase, AResourceGameMode* ResourceGameMode, bool CanAffordConstruction);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	EResourceType ConvertWorkAreaTypeToResourceType(WorkAreaData::WorkAreaType WorkAreaType);
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void DespawnWorkResource(AWorkResource* WorkResource);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TEnumAsByte<WorkAreaData::WorkAreaType> Type = WorkAreaData::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class ABuildingBase> BuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<class AAIController> BuildingController;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ABuildingBase* Building;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BuildTime = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	float BuildZOffset = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool StartedBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FBuildingCost ConstructionCost;
/*
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
	int32 LegendaryCost = 0;*/
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool CanAffordConstruction(int32 TeamId, int32 NumberOfTeams, TArray<FResourceArray> TeamResources);
	
};
