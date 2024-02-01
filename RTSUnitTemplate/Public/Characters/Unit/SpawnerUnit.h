// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "PathSeekerBase.h"
#include "GameFramework/Character.h"
#include "Actors/Selectable.h"
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
	TSubclassOf<ASelectable> SelectableBlueprints;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float ProbabilityArray;
	
};

UCLASS()
class RTSUNITTEMPLATE_API ASpawnerUnit : public ACharacter
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spawn)
	UDataTable* SpawnDataTable;
	
	UPROPERTY(BlueprintReadWrite, Category = Spawn)
	TArray<FSpawnData> SpawnDataArray;

	UFUNCTION(BlueprintCallable, Category = Spawn)
	void CreateSpawnDataFromDataTable();
	
	UFUNCTION(BlueprintCallable, Category = Spawn)
	ASelectable* SpawnSelectable(FVector Location, TSubclassOf<ASelectable> SelectableClass);

	UFUNCTION(BlueprintCallable, Category = Spawn)
	bool SpawnSelectableWithProbability(FSpawnData Data, FVector Offset);

	UFUNCTION(BlueprintCallable, Category = Spawn)
	void SpawnSelectablesArray();

	UPROPERTY(BlueprintReadWrite, Category = Spawn)
	bool IsSpawned = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	int TeamId = 1;
};
