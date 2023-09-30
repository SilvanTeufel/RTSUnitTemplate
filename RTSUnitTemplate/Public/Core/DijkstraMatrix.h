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
	int RowCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	int ColCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	float Delta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TopDownRTSTemplate)
	FVector Offset;
	
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
	int32 Id_A;
	
	UPROPERTY()
	FVector3d Point_A;
	
	UPROPERTY()
	int32 Id_B;
	
	UPROPERTY()
	FVector3d Point_B;
	
	UPROPERTY()
	float Distance;

	bool Processed;
};


USTRUCT()
struct FDijkstraRow
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 Id_End;
	
	UPROPERTY()
	FVector3d End_Point;
	
	UPROPERTY()
	int32 Id_Previous ;
	
	UPROPERTY()
	FVector3d Previous_Point;

	UPROPERTY()
	uint64 Costs;
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
