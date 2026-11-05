// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"

/**
 * What counts as ground for clicks, building placement, the camera floor and the minimap: every landscape, and
 * every actor with the tag RTSGround::Tag - e.g. a terrain generated at runtime (ProceduralMesh) that is not an
 * ALandscape. Put the tag on such an actor and the controller treats it like the landscape.
 */
namespace RTSGround
{
	inline const FName Tag(TEXT("RTSGround"));

	inline bool IsGround(const AActor* Actor)
	{
		return Actor && (Actor->IsA<ALandscapeProxy>() || Actor->ActorHasTag(Tag));
	}
}
