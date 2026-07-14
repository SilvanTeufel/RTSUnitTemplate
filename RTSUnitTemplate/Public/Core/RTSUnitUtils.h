// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
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

	inline FVector CalculateFollowPosition(FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const FMassAITargetFragment& TargetFrag, const FMassAgentCharacteristicsFragment* CharacteristicsFrag, const FVector& CurrentLocation, const FVector& FriendlyLoc, UWorld* World)
	{
		uint64 Seed = (uint64)Entity.Index | ((uint64)Entity.SerialNumber << 32);
		Seed += 0x9E3779B97F4A7C15ull;
		Seed = (Seed ^ (Seed >> 30)) * 0xBF58476D1CE4E5B9ull;
		Seed = (Seed ^ (Seed >> 27)) * 0x94D049BB133111EBull;
		Seed ^= (Seed >> 31);
		const float UnitOffset = (float)((double)(Seed >> 11) * (1.0 / 9007199254740992.0));
		const float UnitRadiusVariation = (float)((double)(Seed & 0xFFFFFFFF) / 4294967296.0);

		float FollowRadius = FMath::Max(0.f, TargetFrag.FollowRadius);
		if (CharacteristicsFrag)
		{
			// Vary the radius based on capsule size so they don't all stand on the same line
			FollowRadius += CharacteristicsFrag->CapsuleRadius * (1.0f + UnitRadiusVariation * 0.5f);
		}
    
		// --- Formation: half-circle / wedge BEHIND the friendly target ---
		// Determine the direction the target is facing; the formation forms on the opposite side
		// (behind the target). Fall back to the unit's own relative direction if no facing is available.
		FVector Forward2D = FVector::ZeroVector;
		if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
		{
			Forward2D = TargetXform->GetTransform().GetRotation().GetForwardVector();
			Forward2D.Z = 0.f;
		}

		FVector BackDir2D;
		if (!Forward2D.IsNearlyZero())
		{
			BackDir2D = -Forward2D.GetSafeNormal();
		}
		else
		{
			// No facing available: keep the unit roughly on its current side of the target.
			FVector ToSelf2D = (CurrentLocation - FriendlyLoc);
			ToSelf2D.Z = 0.f;
			BackDir2D = ToSelf2D.IsNearlyZero() ? FVector::XAxisVector : ToSelf2D.GetSafeNormal();
		}

		// Spread the followers across a half-circle (-90deg..+90deg) centered on the "behind" direction.
		// The per-entity hash (UnitOffset) gives each follower a stable angular slot so they fan out into
		// an arc instead of stacking on a single point.
		const float ArcHalfAngleDeg = 90.f;
		const float AngleDeg = (UnitOffset * 2.0f - 1.0f) * ArcHalfAngleDeg;
		const FVector Dir2D = BackDir2D.RotateAngleAxis(AngleDeg, FVector::UpVector);

		// Stagger the radius per unit so larger groups form a filled half-disc (depth) rather than a thin
		// arc, giving a triangle/wedge-like silhouette behind the target.
		float Radius = FollowRadius;
		const float ExtraSpread = FMath::Max(TargetFrag.FollowOffset, CharacteristicsFrag ? CharacteristicsFrag->CapsuleRadius * 2.0f : 0.f);
		Radius += ExtraSpread * UnitRadiusVariation;

		FVector DesiredPos = FriendlyLoc + Dir2D * Radius;

		// Use grounded Z if the target has characteristic data (buildings)
		if (const FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetFrag.FriendlyTargetEntity))
		{
			DesiredPos.Z = TargetCharFrag->LastGroundLocation;
		}
		else
		{
			DesiredPos.Z = FriendlyLoc.Z;
		}

		// Ensure DesiredPos is not in a dirty area
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
		{
			FNavLocation DesiredNav;
			if (NavSys->ProjectPointToNavigation(DesiredPos, DesiredNav, FVector(500.f, 500.f, 500.f)))
			{
				bool bDesiredDirty = false;
				const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
				if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
				{
					const uint32 PolyAreaID = Recast->GetPolyAreaID(DesiredNav.NodeRef);
					const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
					bDesiredDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
				}

				if (bDesiredDirty)
				{
					// Shift outward until clean
					static const float ShiftRadii[] = { 100.f, 250.f, 500.f };
					for (float Shift : ShiftRadii)
					{
						FVector Candidate = DesiredPos + Dir2D * Shift;
						FNavLocation CandNav;
						if (NavSys->ProjectPointToNavigation(Candidate, CandNav, FVector(500.f, 500.f, 500.f)))
						{
							if (const ARecastNavMesh* RM = Cast<ARecastNavMesh>(NavData))
							{
								const uint32 AID = RM->GetPolyAreaID(CandNav.NodeRef);
								const UClass* AC = RM->GetAreaClass(AID);
								if (!(AC && AC->IsChildOf(UNavArea_Obstacle::StaticClass())))
								{
									DesiredPos = CandNav.Location;
									break;
								}
							}
						}
					}
				}
				else
				{
					DesiredPos = DesiredNav.Location;
				}
			}
		}
		return DesiredPos;
	}

	inline bool IsWithinFollowThreshold(FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const FMassAITargetFragment& TargetFrag, const FMassAgentCharacteristicsFragment* CharacteristicsFrag, const FVector& CurrentLocation, const FMassMoveTargetFragment& MoveTarget, UWorld* World, float AcceptanceMultiplier)
	{
		if (!EntityManager.IsEntityActive(TargetFrag.FriendlyTargetEntity))
		{
			return true;
		}

		FVector FriendlyLoc = TargetFrag.LastKnownFriendlyLocation;
		if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag.FriendlyTargetEntity))
		{
			FriendlyLoc = FriendlyXform->GetTransform().GetLocation();
		}

		FVector DesiredPos = CalculateFollowPosition(EntityManager, Entity, TargetFrag, CharacteristicsFrag, CurrentLocation, FriendlyLoc, World);
		const float Dist2D = FVector::Dist2D(CurrentLocation, DesiredPos);
		const float Threshold = MoveTarget.SlackRadius * AcceptanceMultiplier;

		return Dist2D <= Threshold;
	}
}
