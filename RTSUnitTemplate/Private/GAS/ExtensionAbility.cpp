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

void UExtensionAbility::PlaceAndSpawnAt(const FVector& TargetLoc)
{
	// Stop following preview if active and remove preview instance to avoid duplicates
	StopFollowTimer();
	AWorkArea* Preview = TrackedWorkArea.Get();
	AUnitBase* UnitPrev = TrackedUnit.Get();
	if (Preview)
	{
		// Clear unit's pointer if it points to the preview
		if (UnitPrev && UnitPrev->BuildArea == Preview)
		{
			UnitPrev->BuildArea = nullptr;
		}
		Preview->Destroy(false, true);
		TrackedWorkArea = nullptr;
	}
	AUnitBase* Unit = TrackedUnit.Get();
	if (!Unit && CurrentActorInfo && CurrentActorInfo->OwnerActor.IsValid())
	{
		Unit = Cast<AUnitBase>(CurrentActorInfo->OwnerActor.Get());
	}
	if (!Unit)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UWorld* World = Unit->GetWorld();
	if (!World)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Spawn WorkArea at computed spot (server-authoritative or local if called client-side)
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWorkArea* WA = World->SpawnActor<AWorkArea>(WorkAreaClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);
	if (!WA)
	{
		PlayOwnerLocalSound(CancelSound);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	WA->TeamId = Unit->TeamId;
	WA->Type = WorkAreaData::BuildArea;
	WA->ConstructionUnitClass = ConstructionUnitClass;
	WA->CurrentBuildTime = 0.f;
	WA->PlannedBuilding = false;
	WA->StartedBuilding = false;
	WA->IsPaid = true;
	WA->AllowAddingWorkers = AllowAddingWorkers;
	if (APerformanceUnit* PerfUnit = Cast<APerformanceUnit>(Unit))
	{
		PerfUnit->ShowWorkAreaIfNoFog(WA);
	}
	// Align bottom of WorkArea to ground with a small clearance so it doesn't sink
	{
		FHitResult GroundHit2;
		FCollisionQueryParams Params2;
		Params2.AddIgnoredActor(Unit);
		Params2.AddIgnoredActor(WA);
		const FVector XY = FVector(WA->GetActorLocation().X, WA->GetActorLocation().Y, 0.f);
		World->LineTraceSingleByChannel(GroundHit2, XY + FVector(0,0,2000.f), XY - FVector(0,0,2000.f), ECC_Visibility, Params2);
		const float GroundZ2 = GroundHit2.bBlockingHit ? GroundHit2.Location.Z : WA->GetActorLocation().Z;
		const FBox Bounds = WA->GetComponentsBoundingBox(true);
		const float BottomZ = Bounds.Min.Z;
		const float ActorZ = WA->GetActorLocation().Z;
		const float Clearance = 2.f;
		const float NewZ = ActorZ + ((GroundZ2 + Clearance) - BottomZ);
		WA->SetActorLocation(FVector(WA->GetActorLocation().X, WA->GetActorLocation().Y, NewZ));
	}
	Unit->BuildArea = WA;
	TrackedUnit = Unit;
	TrackedWorkArea = WA;

	// Immediately spawn the ConstructionUnit
	if (WA->ConstructionUnitClass)
	{
		const FTransform SpawnTM(FRotator::ZeroRotator, WA->GetActorLocation());
		AUnitBase* NewConstruction = World->SpawnActorDeferred<AUnitBase>(WA->ConstructionUnitClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (NewConstruction)
		{
			NewConstruction->TeamId = Unit->TeamId;
			if (WA->BuildingClass)
			{
				if (ABuildingBase* BuildingCDO = WA->BuildingClass->GetDefaultObject<ABuildingBase>())
				{
					NewConstruction->DefaultAttributeEffect = BuildingCDO->DefaultAttributeEffect;
				}
			}
			if (AConstructionUnit* CU = Cast<AConstructionUnit>(NewConstruction))
			{
				CU->Worker = Unit;
				CU->WorkArea = WA;
				CU->CastTime = WA->BuildTime;
			}
			NewConstruction->FinishSpawning(SpawnTM);
			if (NewConstruction->AbilitySystemComponent)
			{
				NewConstruction->AbilitySystemComponent->InitAbilityActorInfo(NewConstruction, NewConstruction);
				NewConstruction->InitializeAttributes();
			}
			// Fit/ground-align and visuals same as before
			{
				FBox AreaBox = WA->Mesh ? WA->Mesh->Bounds.GetBox() : WA->GetComponentsBoundingBox(true);
				const FVector AreaCenter = AreaBox.GetCenter();
				const FVector AreaSize = AreaBox.GetSize();
				FHitResult Hit;
				FVector Start = AreaCenter + FVector(0, 0, 10000.f);
				FVector End = AreaCenter - FVector(0, 0, 10000.f);
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(WA);
				Params.AddIgnoredActor(NewConstruction);
				bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
				float GroundZ = bHit ? Hit.Location.Z : NewConstruction->GetActorLocation().Z;
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
				FBox ScaledBox = NewConstruction->GetComponentsBoundingBox(true);
				const FVector UnitCenter = ScaledBox.GetCenter();
				const float BottomZ = ScaledBox.Min.Z;
				FVector FinalLoc = NewConstruction->GetActorLocation();
				FinalLoc.X += (AreaCenter.X - UnitCenter.X);
				FinalLoc.Y += (AreaCenter.Y - UnitCenter.Y);
				FinalLoc.Z += (GroundZ - BottomZ);
				NewConstruction->SetActorLocation(FinalLoc);
			}
			if (AConstructionUnit* CU_Anim = Cast<AConstructionUnit>(NewConstruction))
			{
				const float AnimDuration = WA->BuildTime * 0.95f;
				CU_Anim->MulticastStartRotateVisual(CU_Anim->DefaultRotateAxis, CU_Anim->DefaultRotateDegreesPerSecond, AnimDuration);
				CU_Anim->MulticastStartOscillateVisual(CU_Anim->DefaultOscOffsetA, CU_Anim->DefaultOscOffsetB, CU_Anim->DefaultOscillationCyclesPerSecond, AnimDuration);
				if (CU_Anim->bPulsateScaleDuringBuild)
				{
					CU_Anim->MulticastPulsateScale(CU_Anim->PulsateMinMultiplier, CU_Anim->PulsateMaxMultiplier, CU_Anim->PulsateTimeMinToMax, true);
				}
			}
			WA->ConstructionUnit = NewConstruction;
			WA->bConstructionUnitSpawned = true;
			NewConstruction->BuildArea = WA;
			if (AConstructionUnit* CU = Cast<AConstructionUnit>(NewConstruction))
			{
				CU->SwitchEntityTag(FMassStateCastingTag::StaticStruct());
			}
		}
	}

 EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UExtensionAbility::StartFollowTimer()
{
	if (bFollowingPreview)
	{
		return;
	}
	AUnitBase* Unit = TrackedUnit.Get();
	UWorld* World = Unit ? Unit->GetWorld() : GetWorld();
	if (!World)
	{
		return;
	}
	bFollowingPreview = true;
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UExtensionAbility::UpdatePreviewFollow);
	World->GetTimerManager().SetTimer(FollowMouseTimerHandle, Delegate, 0.02f, true);
}

void UExtensionAbility::StopFollowTimer()
{
	bFollowingPreview = false;
	UWorld* World = GetWorld();
	if (AUnitBase* Unit = TrackedUnit.Get())
	{
		World = Unit->GetWorld();
	}
	if (World)
	{
		World->GetTimerManager().ClearTimer(FollowMouseTimerHandle);
	}
}

void UExtensionAbility::UpdatePreviewFollow()
{
	AUnitBase* Unit = TrackedUnit.Get();
	AWorkArea* WA = TrackedWorkArea.Get();
	if (!Unit || !WA)
	{
		StopFollowTimer();
		return;
	}
	UWorld* World = Unit->GetWorld();
	if (!World)
	{
		StopFollowTimer();
		return;
	}
	// Get the current mouse ground hit for the owning player
	APlayerController* PC = nullptr;
	if (APerformanceUnit* Perf = Cast<APerformanceUnit>(Unit))
	{
		PC = Cast<APlayerController>(Perf->OwningPlayerController);
	}
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(World, 0);
	}
	if (!PC)
	{
		return;
	}
	FHitResult Hit;
	if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return;
	}
	const FVector UnitLoc = Unit->GetMassActorLocation();
	FVector TargetLoc = UnitLoc;
	const FVector2D Delta2D(Hit.Location.X - UnitLoc.X, Hit.Location.Y - UnitLoc.Y);
	const float AbsX = FMath::Abs(ExtensionOffset.X);
	const float AbsY = FMath::Abs(ExtensionOffset.Y);
	if (FMath::Abs(Delta2D.X) >= FMath::Abs(Delta2D.Y))
	{
		const float SignX = (Delta2D.X >= 0.f) ? 1.f : -1.f;
		TargetLoc.X += SignX * (AbsX > KINDA_SMALL_NUMBER ? AbsX : 100.f);
	}
	else
	{
		const float SignY = (Delta2D.Y >= 0.f) ? 1.f : -1.f;
		TargetLoc.Y += SignY * (AbsY > KINDA_SMALL_NUMBER ? AbsY : 100.f);
	}
	// Ground align always when using mouse-driven preview
	FHitResult GroundHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Unit);
	Params.AddIgnoredActor(WA);
	World->LineTraceSingleByChannel(GroundHit, TargetLoc + FVector(0,0,2000.f), TargetLoc - FVector(0,0,2000.f), ECC_Visibility, Params);
	if (GroundHit.bBlockingHit)
	{
		TargetLoc.Z = GroundHit.Location.Z;
	}
	// Adjust Z so the bottom of the WorkArea sits on ground + small offset
	{
		const FBox Bounds = WA->GetComponentsBoundingBox(true);
		const float BottomZ = Bounds.Min.Z;
		const float ActorZ = WA->GetActorLocation().Z;
		const float Clearance = 2.f;
		const float NewZ = ActorZ + ((TargetLoc.Z + Clearance) - BottomZ);
		TargetLoc.Z = NewZ;
	}
	WA->SetActorLocation(TargetLoc);
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

	// If mouse positioning is enabled, defer placement/spawn to OnAbilityMouseHit clicks
	if (UseMousePosition)
	{
		TrackedUnit = Unit; // track owner for click handling
		TrackedWorkArea = nullptr; // no WA yet
		// Ensure distance gating does not block second click: temporarily set Range=0
		if (!bRangeOverridden)
		{
			SavedRange = Range;
			Range = 0.f;
			bRangeOverridden = true;
		}
		return; // keep ability active; first click will place WA, second will spawn CU
	}

	// Legacy: immediate placement by offset/ground and immediate CU spawn
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
		bool bOnlyCreator = true;
		for (AActor* A : Blockers)
		{
			if (A && A != Unit) { bOnlyCreator = false; break; }
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

	// Defer to helper to place WA and spawn CU immediately
	PlaceAndSpawnAt(TargetLoc);
	return;
}

void UExtensionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Restore Range if we overrode it during interactive placement
	if (bRangeOverridden)
	{
		Range = SavedRange;
		bRangeOverridden = false;
	}
	// Cleanup preview follow and references
	StopFollowTimer();
	TrackedWorkArea = nullptr;
	TrackedUnit = nullptr;
	bEndedByWorkArea = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UExtensionAbility::MouseHitWorkAreaCreation(const FHitResult& InHitResult)
{
	// Only handle mouse flow if enabled and we have an owner
	if (!UseMousePosition)
	{
		return;
	}
	
	AUnitBase* Unit = TrackedUnit.Get();
	if (!Unit)
	{
		// Fallback: try to get from CurrentActorInfo
		if (CurrentActorInfo && CurrentActorInfo->OwnerActor.IsValid())
		{
			Unit = Cast<AUnitBase>(CurrentActorInfo->OwnerActor.Get());
			TrackedUnit = Unit;
		}
	}
	if (!Unit)
	{
		return;
	}
	ABuildingBase* AsBuilding = Cast<ABuildingBase>(Unit);
	if (!AsBuilding)
	{
		PlayOwnerLocalSound(CancelSound);
		return;
	}
	if (!WorkAreaClass)
	{
		PlayOwnerLocalSound(CancelSound);
		return;
	}

	UWorld* World = Unit->GetWorld();
	if (!World)
	{
		return;
	}

	// Compute side-based placement around the Unit based on mouse location relative to the unit
	const FVector UnitLoc = Unit->GetMassActorLocation();
	FVector TargetLoc = UnitLoc;
	// Determine dominant axis in XY plane
	const FVector2D Delta2D(InHitResult.Location.X - UnitLoc.X, InHitResult.Location.Y - UnitLoc.Y);
	const float AbsX = FMath::Abs(ExtensionOffset.X);
	const float AbsY = FMath::Abs(ExtensionOffset.Y);
	if (FMath::Abs(Delta2D.X) >= FMath::Abs(Delta2D.Y))
	{
		// Place on +X or -X side
		const float SignX = (Delta2D.X >= 0.f) ? 1.f : -1.f;
		TargetLoc.X += SignX * (AbsX > KINDA_SMALL_NUMBER ? AbsX : 100.f);
	}
	else
	{
		// Place on +Y or -Y side
		const float SignY = (Delta2D.Y >= 0.f) ? 1.f : -1.f;
		TargetLoc.Y += SignY * (AbsY > KINDA_SMALL_NUMBER ? AbsY : 100.f);
	}
	// If ExtensionOffset.Z is not zero, apply it; otherwise ground align
	TargetLoc.Z += ExtensionOffset.Z;
	if (FMath::IsNearlyZero(ExtensionOffset.Z))
	{
		FHitResult GroundHit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Unit);
		World->LineTraceSingleByChannel(GroundHit, TargetLoc + FVector(0,0,2000.f), TargetLoc - FVector(0,0,2000.f), ECC_Visibility, Params);
		if (GroundHit.bBlockingHit)
		{
			TargetLoc.Z = GroundHit.Location.Z;
		}
	}

	// Overlap check ignoring the creator
	TArray<AActor*> Blockers;
	const float Radius = FMath::Max(150.f, GetApproxPlacementRadius(Unit));
	if (IsPlacementBlocked(Unit, TargetLoc, Radius, Blockers))
	{
		bool bOnlyCreator = true;
		for (AActor* A : Blockers)
		{
			if (A && A != Unit) { bOnlyCreator = false; break; }
		}
		if (!bOnlyCreator)
		{
			PlayOwnerLocalSound(CancelSound);
			return;
		}
	}

	// If this is the first click (ClickCount becomes 1), place/replace the WorkArea only
	if (ClickCount <= 1)
	{
		// Destroy previous preview WorkArea if any
		if (TrackedWorkArea.IsValid())
		{
			TrackedWorkArea->Destroy(false, true);
			TrackedWorkArea = nullptr;
		}
		// Spawn new WorkArea at computed TargetLoc
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWorkArea* WA = World->SpawnActor<AWorkArea>(WorkAreaClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);
		if (!WA)
		{
			PlayOwnerLocalSound(CancelSound);
			return;
		}
		WA->TeamId = Unit->TeamId;
		WA->Type = WorkAreaData::BuildArea;
		WA->ConstructionUnitClass = ConstructionUnitClass;
		WA->AllowAddingWorkers = AllowAddingWorkers;
		WA->IsPaid = true;
		WA->CurrentBuildTime = 0.f;
		WA->PlannedBuilding = false;
		WA->StartedBuilding = false;
		if (APerformanceUnit* PerfUnit = Cast<APerformanceUnit>(Unit))
		{
			PerfUnit->ShowWorkAreaIfNoFog(WA);
		}
		// Align bottom of WorkArea to ground with a small clearance so it doesn't sink
		{
			FHitResult GroundHit2;
			FCollisionQueryParams Params2;
			Params2.AddIgnoredActor(Unit);
			Params2.AddIgnoredActor(WA);
			const FVector XY(TargetLoc.X, TargetLoc.Y, 0.f);
			World->LineTraceSingleByChannel(GroundHit2, XY + FVector(0,0,2000.f), XY - FVector(0,0,2000.f), ECC_Visibility, Params2);
			const float GroundZ2 = GroundHit2.bBlockingHit ? GroundHit2.Location.Z : TargetLoc.Z;
			const FBox Bounds = WA->GetComponentsBoundingBox(true);
			const float BottomZ = Bounds.Min.Z;
			const float ActorZ = WA->GetActorLocation().Z;
			const float Clearance = 2.f; // small lift above ground
			const float NewZ = ActorZ + ((GroundZ2 + Clearance) - BottomZ);
			WA->SetActorLocation(FVector(TargetLoc.X, TargetLoc.Y, NewZ));
		}
		Unit->BuildArea = WA;
		TrackedWorkArea = WA;
		// start preview follow timer so WA moves with mouse snap between 4 positions
		StartFollowTimer();
	}
	// For UseMousePosition flow, ConstructionUnit spawning will be triggered externally via BP by calling PlaceAndSpawnAt.
	return;
}
