/*
// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
*/

#include "Actors/RSVirtualTextureActor.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Engine/World.h"

ARSVirtualTextureActor::ARSVirtualTextureActor()
{
	PrimaryActorTick.bCanEverTick = false;

	VirtualTextureComponent = CreateDefaultSubobject<URuntimeVirtualTextureComponent>(TEXT("VirtualTextureComponent"));
	SetRootComponent(VirtualTextureComponent);

	bAutoSetBoundsFromNavMesh = true;
}

void ARSVirtualTextureActor::BeginPlay()
{
	Super::BeginPlay();

	if (VirtualTextureComponent && VirtualTexture)
	{
		VirtualTextureComponent->SetVirtualTexture(VirtualTexture);
	}

	if (bAutoSetBoundsFromNavMesh)
	{
		TArray<AActor*> NavBounds;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANavMeshBoundsVolume::StaticClass(), NavBounds);
		if (NavBounds.Num() > 0)
		{
			FBox CombinedBounds(ForceInit);
			for (AActor* NavActor : NavBounds)
			{
				if (NavActor)
				{
					CombinedBounds += NavActor->GetComponentsBoundingBox();
				}
			}

			if (CombinedBounds.IsValid)
			{
				FVector Center = CombinedBounds.GetCenter();
				FVector Extents = CombinedBounds.GetExtent();

				SetActorLocation(Center);
				SetActorRotation(FRotator::ZeroRotator);
				
				// URuntimeVirtualTextureComponent covers the area defined by its transform.
				// A unit scale typically results in a volume of 200x200x200 if it was a standard volume brush,
				// but for SceneComponents, the scale usually corresponds directly to the size if the base mesh is 1.0.
				// UE's RVT component volume is usually treated as a normalized projection space.
				// To cover the bounds, we set the scale to the full size (Diameter).
				SetActorScale3D(Extents * 2.0f);

				UE_LOG(LogTemp, Log, TEXT("ARSVirtualTextureActor: Auto-set bounds from %d NavMeshBoundsVolumes. Center: %s, Size: %s"),
					NavBounds.Num(), *Center.ToString(), *(Extents * 2.0f).ToString());
			}
		}
	}
}
