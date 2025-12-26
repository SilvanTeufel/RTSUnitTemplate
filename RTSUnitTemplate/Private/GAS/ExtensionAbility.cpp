// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "GAS/ExtensionAbility.h"

#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Actors/WorkArea.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h" // FOverlapResult, FHitResult, FCollisionQueryParams, FCollisionObjectQueryParams
#include "CollisionShape.h"     // FCollisionShape
#include "CollisionQueryParams.h" // FCollisionQueryParams
#include "Engine/OverlapResult.h" // FOverlapResult
#include "GameFramework/PlayerController.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Characters/Unit/ConstructionUnit.h"

UExtensionAbility::UExtensionAbility()
{
	AbilityName = TEXT("Build Extension\n\n");
	KeyboardKey = TEXT("X");
	Type = FText::FromString(TEXT("Build"));
	Description = FText::FromString(TEXT("Place an extension work area next to this building and start casting to construct it."));
}

static float GetApproxPlacementRadius(AActor* Actor)
{
	if (!Actor) return 100.f;
	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	return FVector2D(Extent.X, Extent.Y).Size();
}

bool UExtensionAbility::ComputePlacementLocation(AUnitBase* Unit, FVector& OutLocation) const
{
	if (!Unit) return false;

	// If non-zero offset, simply use Mass location + offset
	if (!ExtensionOffset.IsNearlyZero())
	{
		OutLocation = Unit->GetMassActorLocation() + ExtensionOffset;
		return true;
	}

	// Otherwise, trace to ground under unit forward
	UWorld* World = Unit->GetWorld();
	if (!World) return false;
	FVector Start = Unit->GetMassActorLocation() + FVector(0,0,2000);
	FVector End = Unit->GetMassActorLocation() - FVector(0,0,2000);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExtensionAbilityTrace), /*bTraceComplex*/ true);
	Params.AddIgnoredActor(Unit);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutLocation = Hit.Location;
		return true;
	}
	OutLocation = Unit->GetMassActorLocation();
	return true;
}

bool UExtensionAbility::IsPlacementBlocked(AActor* IgnoredActor, const FVector& TestLocation, float TestRadius, TArray<AActor*>& OutBlockingActors) const
{
	UWorld* World = IgnoredActor ? IgnoredActor->GetWorld() : nullptr;
	if (!World) return true; // conservative

	// Sphere overlap against pawns/world dynamic to catch WorkAreas (Actor) and Buildings
	FCollisionShape Sphere = FCollisionShape::MakeSphere(TestRadius);
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ExtensionAbilityOverlap), false);
	if (IgnoredActor) Params.AddIgnoredActor(IgnoredActor);

	bool bAny = World->OverlapMultiByObjectType(
		Hits,
		TestLocation,
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		Sphere,
		Params
	);
	if (!bAny) return false;

	for (const FOverlapResult& R : Hits)
	{
		AActor* A = R.GetActor();
		if (!A || A==IgnoredActor) continue;
		if (A->IsA(AWorkArea::StaticClass()) || A->IsA(ABuildingBase::StaticClass()))
		{
			OutBlockingActors.Add(A);
		}
	}
	return OutBlockingActors.Num() > 0;
}

void UExtensionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AUnitBase* Unit = ActorInfo && ActorInfo->OwnerActor.IsValid() ? Cast<AUnitBase>(ActorInfo->OwnerActor.Get()) : nullptr;
	if (!Unit)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Only buildings can create extensions (but we do not require IsWorker)
	ABuildingBase* AsBuilding = Cast<ABuildingBase>(Unit);
	if (!AsBuilding)
	{
		PlayOwnerLocalSound(CancelSound);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!WorkAreaClass)
	{
		PlayOwnerLocalSound(CancelSound);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector TargetLoc;
	if (!ComputePlacementLocation(Unit, TargetLoc))
	{
		PlayOwnerLocalSound(CancelSound);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Overlap checks: ignore the creator; sometimes the extension will overlap the creator -> allow that by ignoring it
	TArray<AActor*> Blockers;
	const float Radius = FMath::Max(150.f, GetApproxPlacementRadius(Unit));
	if (IsPlacementBlocked(Unit, TargetLoc, Radius, Blockers))
	{
		// If the only blocker is the creator itself, allow; otherwise cancel
		bool bOnlyCreator = true;
		for (AActor* A : Blockers)
		{
			if (A && A != Unit)
			{
				// ignore if it is the creator itself; otherwise, it's blocked
				bOnlyCreator = false;
				break;
			}
		}
		if (!bOnlyCreator)
		{
			PlayOwnerLocalSound(CancelSound);
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	UWorld* World = Unit->GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Spawn WorkArea at computed spot (server-authoritative)
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWorkArea* WA = World->SpawnActor<AWorkArea>(WorkAreaClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);
	if (!WA)
	{
		PlayOwnerLocalSound(CancelSound);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Configure WorkArea as build area for this team; leave BuildingClass as defined on the WorkArea asset
	WA->TeamId = Unit->TeamId;
	WA->Type = WorkAreaData::BuildArea;
	// Optional: assign a construction site unit to visualize progress
	WA->ConstructionUnitClass = ConstructionUnitClass;
	WA->CurrentBuildTime = 0.f;
	WA->PlannedBuilding = false;
	WA->StartedBuilding = false;
	WA->IsPaid = true; // assume paid via ability cost or resource gate in UI
	WA->AllowAddingWorkers = AllowAddingWorkers;
	// Make the area visible for the owning team like in ExtendedControllerBase
	if (APerformanceUnit* PerfUnit = Cast<APerformanceUnit>(Unit))
	{
		PerfUnit->ShowWorkAreaIfNoFog(WA);
	}

	// Assign as BuildArea on the creator (keep for legacy queries)
	Unit->BuildArea = WA;

	// Track for potential cleanup
	TrackedUnit = Unit;
	TrackedWorkArea = WA;

	// Immediately spawn the ConstructionUnit for this WorkArea (server-authoritative)
	if (WA->ConstructionUnitClass)
	{
		const FTransform SpawnTM(FRotator::ZeroRotator, WA->GetActorLocation());
		AUnitBase* NewConstruction = World->SpawnActorDeferred<AUnitBase>(WA->ConstructionUnitClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (NewConstruction)
		{
			// Team and attributes setup
			NewConstruction->TeamId = Unit->TeamId;
			if (WA->BuildingClass)
			{
				if (ABuildingBase* BuildingCDO = WA->BuildingClass->GetDefaultObject<ABuildingBase>())
				{
					NewConstruction->DefaultAttributeEffect = BuildingCDO->DefaultAttributeEffect;
				}
			}
			// Wire up references when using our ConstructionUnit type
			if (AConstructionUnit* CU = Cast<AConstructionUnit>(NewConstruction))
			{
				CU->Worker = Unit;
				CU->WorkArea = WA;
				CU->CastTime = WA->BuildTime;
			}

			// Finish spawn and initialize ability system/attributes
			NewConstruction->FinishSpawning(SpawnTM);
			if (NewConstruction->AbilitySystemComponent)
			{
				NewConstruction->AbilitySystemComponent->InitAbilityActorInfo(NewConstruction, NewConstruction);
				NewConstruction->InitializeAttributes();
			}

			// Fit, center, and ground-align construction unit to WorkArea footprint
			{
				FBox AreaBox = WA->Mesh ? WA->Mesh->Bounds.GetBox() : WA->GetComponentsBoundingBox(true);
				const FVector AreaCenter = AreaBox.GetCenter();
				const FVector AreaSize = AreaBox.GetSize();
				// Ground trace at the area center to find floor Z
				FHitResult Hit;
				FVector Start = AreaCenter + FVector(0, 0, 10000.f);
				FVector End = AreaCenter - FVector(0, 0, 10000.f);
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(WA);
				Params.AddIgnoredActor(NewConstruction);
				bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
				float GroundZ = bHit ? Hit.Location.Z : NewConstruction->GetActorLocation().Z;
				// Compute uniform scale to fit XY footprint
				FBox PreBox = NewConstruction->GetComponentsBoundingBox(true);
				FVector UnitSize = PreBox.GetSize();
				if (!UnitSize.IsNearlyZero(1e-3f) && AreaSize.X > KINDA_SMALL_NUMBER && AreaSize.Y > KINDA_SMALL_NUMBER)
				{
					const float Margin = 0.98f;
					const float ScaleX = (AreaSize.X * Margin) / FMath::Max(KINDA_SMALL_NUMBER, UnitSize.X);
					const float ScaleY = (AreaSize.Y * Margin) / FMath::Max(KINDA_SMALL_NUMBER, UnitSize.Y);
					const float ScaleZComp = (AreaSize.Z * Margin) / FMath::Max(KINDA_SMALL_NUMBER, UnitSize.Z);
					const float Uniform = FMath::Max(FMath::Min(ScaleX, ScaleY), 0.1f);
					FVector NewScale = NewConstruction->GetActorScale3D();
					NewScale.X = Uniform;
					NewScale.Y = Uniform;
					NewScale.Z = Uniform;
					if (AConstructionUnit* CU_ScaleCheck = Cast<AConstructionUnit>(NewConstruction))
					{
						if (CU_ScaleCheck->ScaleZ)
						{
							NewScale.Z = ScaleZComp;
						}
					}
					NewConstruction->SetActorScale3D(NewScale * 2.f);
				}
				// Recompute bounds after scale and compute single final location
				FBox ScaledBox = NewConstruction->GetComponentsBoundingBox(true);
				const FVector UnitCenter = ScaledBox.GetCenter();
				const float BottomZ = ScaledBox.Min.Z;
				FVector FinalLoc = NewConstruction->GetActorLocation();
				FinalLoc.X += (AreaCenter.X - UnitCenter.X);
				FinalLoc.Y += (AreaCenter.Y - UnitCenter.Y);
				FinalLoc.Z += (GroundZ - BottomZ);
				NewConstruction->SetActorLocation(FinalLoc);
			}

			// Start construction visuals (rotate + oscillate + optional pulsate) for remaining build time
			if (AConstructionUnit* CU_Anim = Cast<AConstructionUnit>(NewConstruction))
			{
				const float AnimDuration = WA->BuildTime * 0.95f; // BuildTime minus 5%
				CU_Anim->MulticastStartRotateVisual(CU_Anim->DefaultRotateAxis, CU_Anim->DefaultRotateDegreesPerSecond, AnimDuration);
				CU_Anim->MulticastStartOscillateVisual(CU_Anim->DefaultOscOffsetA, CU_Anim->DefaultOscOffsetB, CU_Anim->DefaultOscillationCyclesPerSecond, AnimDuration);
				if (CU_Anim->bPulsateScaleDuringBuild)
				{
					CU_Anim->MulticastPulsateScale(CU_Anim->PulsateMinMultiplier, CU_Anim->PulsateMaxMultiplier, CU_Anim->PulsateTimeMinToMax, true);
				}
			}

			// Link to WorkArea and prevent later respawn in SyncCastTime
			WA->ConstructionUnit = NewConstruction;
			WA->bConstructionUnitSpawned = true;
			// Drive building from the construction unit
			NewConstruction->BuildArea = WA;
			
			// Switch the construction unit entity to Casting so Mass processors will tick it
			if (AConstructionUnit* CU = Cast<AConstructionUnit>(NewConstruction))
			{
				CU->SwitchEntityTag(FMassStateCastingTag::StaticStruct());
			}
		}
	}

	// Done: ability ends immediately after placement/spawn
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UExtensionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Unbind delegate if still bound and area exists
	if (TrackedWorkArea.IsValid())
	{
		// No delegate bound here; just clear reference
	}
	TrackedWorkArea = nullptr;
	TrackedUnit = nullptr;
	bEndedByWorkArea = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
