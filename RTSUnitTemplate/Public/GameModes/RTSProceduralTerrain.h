// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RTSProceduralTerrain.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class URTSProceduralTerrain : public UInterface
{
	GENERATED_BODY()
};

/**
 * A level actor that builds the terrain of a full procedural match (UFullProceduralMapComponent). RTSUnitTemplate
 * only knows this interface - the terrain itself comes from the game (e.g. a landscape generator plugin).
 *
 * Called on the server at match start and on every client once the seed has replicated (AResourceGameState).
 * The same seed must give the same terrain everywhere.
 */
class RTSUNITTEMPLATE_API IRTSProceduralTerrain
{
	GENERATED_BODY()

public:
	/**
	 * Build the terrain for Seed, synchronously, with collision, and take the old ground (the map's landscape and
	 * whatever was generated on it) out of collision and navigation. Tag the new ground actor RTSGround::Tag.
	 * Server: the navmesh is built right after this returns. Returns the seed that was really used - the server
	 * may skip seeds that give a bad map (e.g. plateaus without a way up); 0 = failed, the map stays as it is.
	 * Clients get the server's result and must build exactly that one.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RTSUnitTemplate|Procedural")
	int32 GenerateProceduralTerrain(int32 Seed, bool bIsServer);

	/** Bases, resources and spawn points stand (server: right after; client: a moment after the seed came in). Decoration (PCG) goes here. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RTSUnitTemplate|Procedural")
	void OnProceduralMapReady();
};
