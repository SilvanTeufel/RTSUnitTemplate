// Copyright 2025 Teufel-Engineering
#pragma once

#include "CoreMinimal.h"
#include "MassReplicationTypes.h"
#include "UObject/WeakObjectPtr.h"

class UWorld;

namespace RTSReplicationBootstrap
{
	// Called early for each world to ensure bubble classes are registered before any clients are added
	RTSUNITTEMPLATE_API void RegisterForWorld(UWorld& World);

	// Retrieve the previously registered Unit bubble handle for the given world (may be invalid if registration failed)
	RTSUNITTEMPLATE_API FMassBubbleInfoClassHandle GetUnitBubbleHandle(const UWorld* World);
}