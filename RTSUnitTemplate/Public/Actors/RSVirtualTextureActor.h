/*
// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSVirtualTextureActor.generated.h"

class URuntimeVirtualTexture;
class URuntimeVirtualTextureComponent;

/**
 * An Actor that manages a Runtime Virtual Texture (RVT) in the world.
 * Place this in the level to provide the RVT that AreaDecalComponents can write into.
 */
UCLASS()
class RTSUNITTEMPLATE_API ARSVirtualTextureActor : public AActor
{
	GENERATED_BODY()

public:
	ARSVirtualTextureActor();

	/** If true, automatically sets bounds to match the NavMeshBoundsVolume(s) found in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	bool bAutoSetBoundsFromNavMesh = true;

protected:
	virtual void BeginPlay() override;

public:
	/** The component that manages the RVT volume and rendering. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RVT")
	URuntimeVirtualTextureComponent* VirtualTextureComponent;

	/** The Virtual Texture asset to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVT")
	URuntimeVirtualTexture* VirtualTexture;
};
