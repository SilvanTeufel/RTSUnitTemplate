// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "PathSeekerBase.h"
#include "Actors/AbilityIndicator.h"
#include "GameFramework/Character.h"
#include "Characters/Mass/MassActorBindingComponent.h"
#include "Actors/Pickup.h"
#include "Engine/World.h"
#include "SpawnerUnit.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FSpawnData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int Id;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	TSubclassOf<APickup> PickupBlueprint;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ProbabilityArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MaxLifeTime;
};

UCLASS()
class RTSUNITTEMPLATE_API ASpawnerUnit : public ACharacter
{
	GENERATED_BODY()
	
public:

	ASpawnerUnit(const FObjectInitializer& ObjectInitializer);
	
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spawn)
	UDataTable* SpawnDataTable;
	
	UPROPERTY(BlueprintReadWrite, Category = Spawn)
	TArray<FSpawnData> SpawnDataArray;

	UFUNCTION(BlueprintCallable, Category = Spawn)
	void CreateSpawnDataFromDataTable();
	
	UFUNCTION(BlueprintCallable, Category = Spawn)
	APickup* SpawnPickup(FVector Location, FSpawnData Data);

	UFUNCTION(BlueprintCallable, Category = Spawn)
	bool SpawnPickupWithProbability(FSpawnData Data, FVector Offset);

	UFUNCTION(BlueprintCallable, Category = Spawn)
	void SpawnPickupsArray();

	UPROPERTY(BlueprintReadWrite, Category = Spawn)
	bool IsSpawned = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int SquadId = 0;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = Ability)
	AAbilityIndicator* CurrentDraggedAbilityIndicator;

	UFUNCTION(BlueprintCallable, Category=Ability)
	void SpawnAbilityIndicator(TSubclassOf<AAbilityIndicator> AbilityIndicatorClass, FVector SpawnLocation);
	
	UFUNCTION(BlueprintCallable, Category=Ability)
	void DespawnCurrentAbilityIndicator();


	// The Mass Actor Binding Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass")
	UMassActorBindingComponent* MassActorBindingComponent;
};
