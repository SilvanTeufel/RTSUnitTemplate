// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DijkstraMatrix.generated.h"

USTRUCT(BlueprintType)
struct FGridData : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int RowCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int ColCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float Delta = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	FVector Offset = FVector::ZeroVector;
	
};
USTRUCT()
struct FPathMatrix
{
	GENERATED_USTRUCT_BODY()
	
	TArray<FPathMatrixRow> Matrix;
};

USTRUCT()
struct FPathMatrixRow
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 Id_A = 0; // Initialize
    
	UPROPERTY()
	FVector3d Point_A = FVector3d::Zero(); // Initialize
    
	UPROPERTY()
	int32 Id_B = 0; // Initialize
    
	UPROPERTY()
	FVector3d Point_B = FVector3d::Zero(); // Initialize
    
	UPROPERTY()
	float Distance = 0.0f; // Initialize

	bool Processed = false; // Initialize
};


USTRUCT()
struct FDijkstraRow
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    int32 Id_End = 0;
    
    UPROPERTY()
    FVector3d End_Point = FVector3d::Zero();
    
    UPROPERTY()
    int32 Id_Previous = 0;
    
    UPROPERTY()
    FVector3d Previous_Point = FVector3d::Zero();

    UPROPERTY()
    uint64 Costs = 0;
};

USTRUCT()
struct FDijkstraMatrix
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 Id;
	
	UPROPERTY()
	TArray<FDijkstraRow> Matrix;
	
	UPROPERTY()
	FVector3d CenterPoint;

};


USTRUCT()
struct FPathPoint
{
	GENERATED_USTRUCT_BODY()
		
	UPROPERTY()
	int32 Id;
	
	UPROPERTY()
	FVector3d Point;
		
};
