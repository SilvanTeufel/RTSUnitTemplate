// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Blueprint/PCGClearingBlueprintLibrary.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

namespace
{
	/** Mirrors PCGHelpers::DefaultPCGTag. Duplicated as a literal so this module needs no PCG dependency. */
	const FName PCGGeneratedComponentTag(TEXT("PCG Generated Component"));
}

int32 UPCGClearingBlueprintLibrary::ClearPCGInstancesInRadius(
	const UObject* WorldContextObject,
	FVector Center,
	float Radius,
	bool bIncludeVertical)
{
	if (Radius <= 0.f)
	{
		return 0;
	}

	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!World)
	{
		return 0;
	}

	const float RadiusSq = Radius * Radius;
	int32 TotalRemoved = 0;

	// Iterating ISM components rather than actors: PCG partition actors carry no collision at all
	// (their root is a bare USceneComponent and their bounds box is editor-only), so a physics overlap
	// can never find them -- which is exactly why the old SphereOverlapActors approach silently did
	// nothing. Identifying the components by PCG's tag also avoids a PCG module dependency.
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* ISM = *It;
		if (!IsValid(ISM)
			|| ISM->GetWorld() != World
			|| !ISM->ComponentTags.Contains(PCGGeneratedComponentTag))
		{
			continue;
		}

		const FTransform ComponentToWorld = ISM->GetComponentTransform();

		// Collect first, remove afterwards in descending order: RemoveInstance shuffles indices, so
		// removing while scanning would skip instances.
		TArray<int32> ToRemove;
		const int32 InstanceCount = ISM->GetInstanceCount();
		for (int32 Index = 0; Index < InstanceCount; ++Index)
		{
			FTransform InstanceTransform;
			if (!ISM->GetInstanceTransform(Index, InstanceTransform, /*bWorldSpace=*/true))
			{
				// Older instance data can be component-space only; fall back explicitly.
				if (!ISM->GetInstanceTransform(Index, InstanceTransform, /*bWorldSpace=*/false))
				{
					continue;
				}
				InstanceTransform = InstanceTransform * ComponentToWorld;
			}

			FVector Delta = InstanceTransform.GetLocation() - Center;
			if (!bIncludeVertical)
			{
				Delta.Z = 0.f;
			}

			if (Delta.SizeSquared() <= RadiusSq)
			{
				ToRemove.Add(Index);
			}
		}

		if (ToRemove.Num() == 0)
		{
			continue;
		}

		ToRemove.Sort([](const int32 A, const int32 B) { return A > B; });
		for (const int32 Index : ToRemove)
		{
			ISM->RemoveInstance(Index);
		}

		ISM->MarkRenderStateDirty();
		TotalRemoved += ToRemove.Num();
	}

	return TotalRemoved;
}
