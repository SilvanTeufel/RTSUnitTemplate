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


USTRUCT(BlueprintType)
struct FResourceArray
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	EResourceType ResourceType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	TArray<float> Resources;

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
	}
};


UENUM()
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