// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSGameModeBase.h"
#include "Core/WorkerData.h"
#include "ResourceGameMode.generated.h"

/**
 * GameMode class that manages resources for teams in a more generic way.
 */
UCLASS()
class RTSUNITTEMPLATE_API AResourceGameMode : public ARTSGameModeBase
{
	GENERATED_BODY()

public:
	// Constructor
	AResourceGameMode();

protected:
	virtual void BeginPlay() override; // Override BeginPlay
	
	// Array of structs to manage resources. Each struct contains an array for a specific resource type.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources")
	TArray<FResourceArray> TeamResources;

	int32 NumberOfTeams = 10;

	// Helper function to initialize resource arrays
	void InitializeResources(int32 NumberOfTeams);

public:
	// Function to modify a resource for a specific team
	void ModifyResource(EResourceType ResourceType, int32 TeamId, float Amount);

};