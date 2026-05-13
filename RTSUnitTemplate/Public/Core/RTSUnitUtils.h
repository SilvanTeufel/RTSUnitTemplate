#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Mass/MassUnitVisualFragments.h"
#include "NavigationSystem.h"

namespace RTSUnitUtils
{
	inline FVector FindGroundLocationAtPosition(const UObject* WorldContextObject, FVector Position, TArray<AActor*> ActorsToIgnore, float TraceDistance = 10000.f)
	{
		if (!WorldContextObject)
		{
			return Position;
		}

		UWorld* World = WorldContextObject->GetWorld();
		if (!World)
		{
			return Position;
		}

		FVector StartTrace = Position + FVector(0.f, 0.f, TraceDistance);
		FVector EndTrace = Position - FVector(0.f, 0.f, TraceDistance);

		FHitResult HitResult;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActors(ActorsToIgnore);

		TArray<UPrimitiveComponent*> AllComponentsToIgnore;

		auto CollectUnitComponents = [&](AActor* InActor)
		{
			if (!InActor) return;
			if (AMassUnitBase* MassUnit = Cast<AMassUnitBase>(InActor))
			{
				if (MassUnit->ISMComponent) AllComponentsToIgnore.AddUnique(MassUnit->ISMComponent);
				for (UInstancedStaticMeshComponent* AdditionalISM : MassUnit->AdditionalISMComponents)
				{
					if (AdditionalISM) AllComponentsToIgnore.AddUnique(AdditionalISM);
				}

				if (const FMassUnitVisualFragment* VisualFrag = MassUnit->GetVisualFragment())
				{
					for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
					{
						if (Instance.TargetISM.IsValid())
						{
							AllComponentsToIgnore.AddUnique(Instance.TargetISM.Get());
						}
					}
				}
			}
		};

		for (AActor* IgnoreActor : ActorsToIgnore)
		{
			CollectUnitComponents(IgnoreActor);
		}

		CollisionParams.AddIgnoredComponents(AllComponentsToIgnore);

		if (World->LineTraceSingleByChannel(HitResult, StartTrace, EndTrace, ECC_Visibility, CollisionParams))
		{
			return FVector(Position.X, Position.Y, HitResult.Location.Z);
		}

		return Position;
	}

	inline FVector FindGroundLocationForActor(const UObject* WorldContextObject, AActor* TargetActor, TArray<AActor*> ActorsToIgnore, float TraceDistance = 10000.f)
	{
		if (!TargetActor)
		{
			return FVector::ZeroVector;
		}

		return FindGroundLocationAtPosition(WorldContextObject, TargetActor->GetActorLocation(), ActorsToIgnore, TraceDistance);
	}


	inline FVector ProjectLocationToNavMeshOnEdge(UWorld* World, const FVector& Center, const FVector& TargetPos, float TotalRadius)
	{
		if (!World) return TargetPos;

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!NavSys) return TargetPos;

		FNavLocation ProjectedLocation;
		// 1. Prüfen, ob der Punkt bereits auf dem NavMesh liegt (kleiner Radius)
		if (NavSys->ProjectPointToNavigation(TargetPos, ProjectedLocation, FVector(20.f, 20.f, 200.f)))
		{
			return ProjectedLocation.Location;
		}

		// 2. Wenn nicht, suche in einem größeren Umkreis
		if (NavSys->ProjectPointToNavigation(TargetPos, ProjectedLocation, FVector(500.f, 500.f, 500.f)))
		{
			FVector Dir = (ProjectedLocation.Location - Center);
			Dir.Z = 0.f;
			if (!Dir.IsNearlyZero())
			{
				Dir.Normalize();
				// 3. Auf den Rand zurückprojizieren
				return Center + Dir * TotalRadius;
			}
			return ProjectedLocation.Location;
		}

		return TargetPos;
	}

	inline float GetCombinedRadii(const FMassAgentCharacteristicsFragment& AttackerChar, const FTransform& AttackerTransform,
		const FMassAgentCharacteristicsFragment* TargetChar, const FTransform* TargetTransform, const FVector& TargetLocation)
	{
		float AttackerRadius = AttackerChar.CapsuleRadius;
		float TargetRadius = 0.f;

		if (TargetChar)
		{
			TargetRadius = TargetChar->CapsuleRadius;
			if (AttackerChar.bUseBoxComponent || TargetChar->bUseBoxComponent)
			{
				FVector Dir = (TargetLocation - AttackerTransform.GetLocation());
				Dir.Z = 0.f;
				if (!Dir.IsNearlyZero())
				{
					Dir.Normalize();
					AttackerRadius = AttackerChar.GetRadiusInDirection(Dir, AttackerTransform.GetRotation().Rotator());
                
					if (TargetTransform)
					{
						TargetRadius = TargetChar->GetRadiusInDirection(-Dir, TargetTransform->GetRotation().Rotator());
					}
				}
			}
		}
		return AttackerRadius + TargetRadius;
	}
}
