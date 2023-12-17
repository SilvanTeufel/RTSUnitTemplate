// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PathSeekerBase.h"
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
class RTSUNITTEMPLATE_API ASpawnerUnit : public APathSeekerBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TopDownRTSTemplate")
	UDataTable* SpawnDataTable;
	
	UPROPERTY(BlueprintReadWrite, Category = "TopDownRTSTemplate")
	TArray<FSpawnData> SpawnDataArray;

	UFUNCTION(BlueprintCallable, Category = "TopDownRTSTemplate")
	void CreateSpawnDataFromDataTable();
	
	UFUNCTION(BlueprintCallable, Category = "TopDownRTSTemplate")
	ASelectable* SpawnSelectable(FVector Location, TSubclassOf<ASelectable> SelectableClass);

	UFUNCTION(BlueprintCallable, Category = "TopDownRTSTemplate")
	bool SpawnSelectableWithProbability(FSpawnData Data, FVector Offset);

	UFUNCTION(BlueprintCallable, Category = "TopDownRTSTemplate")
	void SpawnSelectablesArray();
};
