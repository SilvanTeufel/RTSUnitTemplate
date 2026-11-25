// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "PathSeekerBase.h"
#include "Actors/AbilityIndicator.h"
#include "GameFramework/Character.h"
#include "Mass/MassActorBindingComponent.h"
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
	int Id = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	TSubclassOf<APickup> PickupBlueprint;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ProbabilityArray = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float MaxLifeTime = 0.f;
};

UCLASS()
class RTSUNITTEMPLATE_API ASpawnerUnit : public ACharacter
{
	GENERATED_BODY()
	
public:

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTagContainer UnitTags;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag AbilitySelectionTag;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FGameplayTag TalentTag;

	/**
	 * Position of this unit in a tag selection (Ctrl+Q and the other key tags) and therefore in the order the
	 * ability pages scroll through. Lower comes first; equal values fall back to the actor name, which keeps
	 * units of one class together. Production buildings use their tech tier: T1 = 1 ... T4 = 4.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 SelectionOrder = 0;

	/** Sort predicate for selections: SelectionOrder, then actor name. */
	static bool IsBeforeInSelection(const ASpawnerUnit& A, const ASpawnerUnit& B)
	{
		if (A.SelectionOrder != B.SelectionOrder)
		{
			return A.SelectionOrder < B.SelectionOrder;
		}
		return A.GetName() < B.GetName();
	}

	ASpawnerUnit(const FObjectInitializer& ObjectInitializer);
	
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadWrite, meta = (DisplayName = "UnitControlTimer", Keywords = "RTSUnitTemplate UnitControlTimer"), Category = RTSUnitTemplate)
	float UnitControlTimer = 0.0f; // This Timer is used in UnitControllerBase Statemachine
	
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

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int32 ActiveRotationPlayerId = -1;
};
