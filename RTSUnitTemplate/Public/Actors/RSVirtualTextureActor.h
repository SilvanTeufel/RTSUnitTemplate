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
 *
 * Setup is entirely edit-time. On VirtualTextureComponent set Virtual Texture, Bounds Align Actor
 * and Expand Bounds, then press "Set Bounds". Nothing about this actor may be configured at runtime:
 * changing the RVT asset or the transform during play forces a full VT recreate and drops every
 * resident page. See the comment in RSVirtualTextureActor.cpp.
 */
UCLASS()
class RTSUNITTEMPLATE_API ARSVirtualTextureActor : public AActor
{
	GENERATED_BODY()

public:
	ARSVirtualTextureActor();

	/** The component that manages the RVT volume and rendering. Configure it in the level, not at runtime. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RVT")
	URuntimeVirtualTextureComponent* VirtualTextureComponent;

	/**
	 * The RVT asset this actor's volume renders into.
	 *
	 * Reads it off VirtualTextureComponent, which is the single source of truth. There used to be a
	 * separate VirtualTexture UPROPERTY on this actor as well; it was left unset on every placed
	 * instance while the component's was set, so anything reading the actor's copy silently got null.
	 */
	UFUNCTION(BlueprintPure, Category = "RVT")
	URuntimeVirtualTexture* GetVirtualTexture() const;
};
