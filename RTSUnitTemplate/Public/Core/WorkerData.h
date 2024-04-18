// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/DataTable.h"
#include "WorkerData.generated.h"

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	Primary,   
	Secondary,
	Tertiary,  
	Rare,
	Epic, 
	Legendary,
	MAX
};

//ENUM_RANGE_BY_FIRST_AND_LAST(EResourceType, EResourceType::Primary, EResourceType::Legendary);

USTRUCT(BlueprintType)
struct FResourceArray
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	EResourceType ResourceType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	TArray<float> Resources;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	TArray<int> CurrentWorkers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	TArray<int> MaxWorkers;
	// Default constructor
	FResourceArray()
		: ResourceType(EResourceType::Primary) // Default value, adjust as needed
	{
	}

	// Constructor for setting resource type and size
	FResourceArray(EResourceType InResourceType, int32 Size)
		: ResourceType(InResourceType)
	{
		Resources.Init(0.0f, Size);
		CurrentWorkers.Init(0.0f, Size);
		MaxWorkers.Init(0.0f, Size);
	}
};


UENUM(BlueprintType)
namespace WorkAreaData
{
	enum WorkAreaType
	{
		Primary     UMETA(DisplayName = "Primary"),
		Secondary   UMETA(DisplayName = "Secondary "),
		Tertiary UMETA(DisplayName = "Tertiary"),
		Rare     UMETA(DisplayName = "Rare"),
		Epic   UMETA(DisplayName = "Epic "),
		Legendary UMETA(DisplayName = "Legendary"),
		Base UMETA(DisplayName = "Base"),
		BuildArea UMETA(DisplayName = "BuildArea"),
	};
}

inline EResourceType ConvertToResourceType(WorkAreaData::WorkAreaType workAreaType)
{
	switch (workAreaType)
	{
	case WorkAreaData::Primary:
		return EResourceType::Primary;
	case WorkAreaData::Secondary:
		return EResourceType::Secondary;
	case WorkAreaData::Tertiary:
		return EResourceType::Tertiary;
	case WorkAreaData::Rare:
		return EResourceType::Rare;
	case WorkAreaData::Epic:
		return EResourceType::Epic;
	case WorkAreaData::Legendary:
		return EResourceType::Legendary;
		// Add other cases as necessary
		default:
			// Handle the default case or assert
			break;
	}
    
	// Return a default value or assert if the input value is unexpected
	return EResourceType::MAX; // Example of a default return value, adjust as needed
}