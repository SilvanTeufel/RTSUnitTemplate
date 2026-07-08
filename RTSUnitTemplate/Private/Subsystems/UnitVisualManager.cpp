// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Subsystems/UnitVisualManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/MassUnitVisualFragments.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"
#include "Characters/Unit/MassUnitBase.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "TimerManager.h"

void UUnitVisualManager::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
}

void UUnitVisualManager::OnWorldBeginPlay(UWorld& InWorld) {
	Super::OnWorldBeginPlay(InWorld);
	// Low-frequency safety net: reclaim any pooled instance whose owning actor died without freeing it
	// (any teardown path). Complements the actor-cache release in AMassUnitBase::EndPlay.
	InWorld.GetTimerManager().SetTimer(SweepTimerHandle, this, &UUnitVisualManager::SweepStaleInstances, 7.0f, true);
}

void UUnitVisualManager::AssignUnitVisual(FMassEntityHandle Entity, UInstancedStaticMeshComponent* TemplateISM, AMassUnitBase* Unit) {
	if (!TemplateISM || !TemplateISM->GetStaticMesh()) {
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) {
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Entity)) return;
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);

	if (!VisualFrag) {
		return;
	}

	UStaticMesh* Mesh = TemplateISM->GetStaticMesh();
	UMaterialInterface* Material = TemplateISM->GetMaterial(0);
	bool bCastShadow = TemplateISM->CastShadow;

	// Check for reuse and cleanup stale instances
	for (int32 i = VisualFrag->VisualInstances.Num() - 1; i >= 0; --i) {
		FMassUnitVisualInstance& Existing = VisualFrag->VisualInstances[i];
		
		// Reuse if same template pointer OR if stale instance matches the new mesh/material/shadow settings
		bool bIsSameTemplate = (Existing.TemplateISM == TemplateISM);
		bool bIsStaleMatch = (!Existing.TemplateISM.IsValid() && Existing.TargetISM.IsValid() && 
							 Existing.TargetISM->GetStaticMesh() == Mesh && 
							 Existing.TargetISM->GetMaterial(0) == Material &&
							 Existing.TargetISM->CastShadow == bCastShadow);

		if (bIsSameTemplate || bIsStaleMatch) {
			Existing.TemplateISM = TemplateISM;
			Existing.BaseOffset = TemplateISM->GetRelativeTransform();
			Existing.CurrentRelativeTransform = Existing.BaseOffset;

			// Suche nach ProjectileSpawn Child unter dem TemplateISM
			const FName ProjectileSpawnTag = TEXT("ProjectileSpawn");
			TArray<USceneComponent*> Children;
			TemplateISM->GetChildrenComponents(true, Children);
			Existing.bHasMuzzle = false;
			for (USceneComponent* Child : Children) {
				if (Child->ComponentHasTag(ProjectileSpawnTag)) {
					Existing.MuzzleOffset = Child->GetRelativeTransform();
					Existing.bHasMuzzle = true;
					break;
				}
			}
			
			// Re-map the unit in the manager
			TArray<TWeakObjectPtr<AMassUnitBase>>& UnitArray = ISMToUnitMap.FindOrAdd(Existing.TargetISM.Get());
			if (UnitArray.IsValidIndex(Existing.InstanceIndex)) {
				UnitArray[Existing.InstanceIndex] = Unit;
			}
			if (Unit) Unit->CachePooledVisual(Existing.TargetISM.Get(), Existing.InstanceIndex);
				return; // Successfully reused/updated
		}
	}

	UInstancedStaticMeshComponent* ISM = GetOrCreateISM(Mesh, Material, bCastShadow);
	if (!ISM) {
		return;
	}

	FMeshMaterialKey Key;
	Key.Mesh = Mesh;
	Key.Material = Material;
	Key.bCastShadow = bCastShadow;

	int32 NewIndex = INDEX_NONE;
	if (FreeIndexPool.Contains(Key) && FreeIndexPool[Key].Num() > 0)
	{
		NewIndex = FreeIndexPool[Key].Pop();
		ISM->UpdateInstanceTransform(NewIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
	}
	else
	{
		// Create a new instance with zero scale to avoid flicker
		NewIndex = ISM->AddInstance(FTransform::Identity);
		ISM->UpdateInstanceTransform(NewIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
	}

	// Copy collision settings from template to the pooled ISM
	// Note: Since ISMs are pooled by mesh/material, the last one assigned will dictate collision for all instances.
	ISM->SetCollisionEnabled(TemplateISM->GetCollisionEnabled());
	ISM->SetCollisionResponseToChannels(TemplateISM->GetCollisionResponseToChannels());
	ISM->SetCollisionObjectType(TemplateISM->GetCollisionObjectType());
	if (ISM->GetCollisionObjectType() == ECC_WorldStatic) { ISM->SetCollisionObjectType(ECC_WorldDynamic); }
	ISM->SetReceivesDecals(TemplateISM->bReceivesDecals);
	ISM->SetGenerateOverlapEvents(TemplateISM->GetGenerateOverlapEvents());
	ISM->SetRenderCustomDepth(TemplateISM->bRenderCustomDepth);
	ISM->SetCustomDepthStencilValue(TemplateISM->CustomDepthStencilValue);
	// Never inherit a too-small value from the editor template. The animation processor needs 13
	// custom-data floats (indices 1..12); clamp up so a misconfigured template can't undersize the
	// pooled ISM and force a destructive runtime resize later (which would zero all instances).
	ISM->SetNumCustomDataFloats(FMath::Max(13, TemplateISM->NumCustomDataFloats));

	// Map ISM instance to MassUnitBase
	TArray<TWeakObjectPtr<AMassUnitBase>>& UnitArray = ISMToUnitMap.FindOrAdd(ISM);
	if (UnitArray.Num() <= NewIndex) {
		UnitArray.SetNum(NewIndex + 1);
	}
	UnitArray[NewIndex] = Unit;

	FMassUnitVisualInstance NewInstance;
	NewInstance.TemplateISM = TemplateISM;
	NewInstance.TargetISM = ISM;
	NewInstance.InstanceIndex = NewIndex;
	NewInstance.BaseOffset = TemplateISM->GetRelativeTransform();
	NewInstance.CurrentRelativeTransform = NewInstance.BaseOffset;
	NewInstance.bWasVisible = false;

	// Suche nach ProjectileSpawn Child unter dem TemplateISM
	const FName ProjectileSpawnTag = TEXT("ProjectileSpawn");
	TArray<USceneComponent*> Children;
	TemplateISM->GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children) {
		if (Child->ComponentHasTag(ProjectileSpawnTag)) {
			NewInstance.MuzzleOffset = Child->GetRelativeTransform();
			NewInstance.bHasMuzzle = true;
			break;
		}
	}

	VisualFrag->VisualInstances.Add(NewInstance);

	// Cache on the actor for entity-independent release in EndPlay.
	if (Unit) Unit->CachePooledVisual(ISM, NewIndex);
}

void UUnitVisualManager::RemoveUnitVisual(FMassEntityHandle Entity) {
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Entity)) return;
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);

	// The actor that owns this entity, used as the reuse-guard identity in the shared free primitive.
	AMassUnitBase* OwnerActor = nullptr;
	if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity)) {
		OwnerActor = Cast<AMassUnitBase>(ActorFrag->GetMutable());
	}

	if (VisualFrag) {
		for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances) {
			ReleasePooledInstanceInternal(Instance.TargetISM.Get(), Instance.InstanceIndex, OwnerActor);
		}
		VisualFrag->VisualInstances.Empty();
	}
}

void UUnitVisualManager::ReleasePooledInstanceInternal(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex, AMassUnitBase* ExpectedOwner) {
	if (!ISM || InstanceIndex == INDEX_NONE) return;

	// Reuse guard: if the slot currently belongs to a DIFFERENT live unit, pooling already reassigned this
	// index (indices are popped from FreeIndexPool and remapped on reuse). Freeing it would blank a live
	// unit's instance AND double-hand the index. Only proceed when the slot is our own, stale, or null.
	if (TArray<TWeakObjectPtr<AMassUnitBase>>* UnitArray = ISMToUnitMap.Find(ISM)) {
		if (UnitArray->IsValidIndex(InstanceIndex)) {
			AMassUnitBase* Cur = (*UnitArray)[InstanceIndex].Get();
			if (Cur != nullptr && Cur != ExpectedOwner) return;
			(*UnitArray)[InstanceIndex] = nullptr;
		}
	}

	// Hide (origin + zero scale) and return the index to the pool. AddUnique = idempotent double-free guard.
	const FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
	ISM->UpdateInstanceTransform(InstanceIndex, HiddenTransform, true, true, true);

	FMeshMaterialKey Key;
	Key.Mesh = ISM->GetStaticMesh();
	Key.Material = ISM->GetMaterial(0);
	Key.bCastShadow = ISM->CastShadow;
	FreeIndexPool.FindOrAdd(Key).AddUnique(InstanceIndex);
}

void UUnitVisualManager::ReleaseInstanceDirect(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex, AMassUnitBase* Owner) {
	ReleasePooledInstanceInternal(ISM, InstanceIndex, Owner);
}

void UUnitVisualManager::SweepStaleInstances() {
	// Reclaim any pooled instance whose owning actor was destroyed without a prior successful free.
	for (TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, TArray<TWeakObjectPtr<AMassUnitBase>>>& Pair : ISMToUnitMap) {
		UInstancedStaticMeshComponent* ISM = Pair.Key.Get();
		if (!ISM) continue;
		TArray<TWeakObjectPtr<AMassUnitBase>>& UnitArray = Pair.Value;
		for (int32 Index = 0; Index < UnitArray.Num(); ++Index) {
			// IsStale(true) is true ONLY for a slot that pointed at a now-destroyed actor — false for a
			// live unit (IsValid) and false for an explicitly-nulled/freed slot (plain null). So this only
			// ever reclaims genuine orphans and is fully idempotent.
			if (UnitArray[Index].IsStale(true)) {
				ReleasePooledInstanceInternal(ISM, Index, nullptr);
			}
		}
	}
}

void UUnitVisualManager::SetUnitVisualVisible(FMassEntityHandle Entity, bool bVisible) {
	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Entity)) return;
	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);
	FMassVisualEffectFragment* EffectFrag = EntityManager.GetFragmentDataPtr<FMassVisualEffectFragment>(Entity);

	if (EffectFrag) {
		EffectFrag->bForceHidden = !bVisible;
	}

	if (VisualFrag) {
		if (!bVisible) {
			for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances) {
				if (Instance.TargetISM.IsValid() && Instance.InstanceIndex != INDEX_NONE) {
					// Move to origin and zero scale to "hide" it while staying in the pool
					FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
					Instance.TargetISM->UpdateInstanceTransform(Instance.InstanceIndex, HiddenTransform, true, true, true);
				}
			}
		}
	} 
}

void UUnitVisualManager::SwapUnitVisualToRuin(FMassEntityHandle Entity, AMassUnitBase* Unit, UStaticMesh* RuinMesh,
	UMaterialInterface* RuinMaterial, bool bCastShadow, const FVector& StretchFactor, float YawDegrees)
{
	if (!RuinMesh || !Unit) return;

	UMassEntitySubsystem* EntitySubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem) return;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(Entity)) return;

	FMassUnitVisualFragment* VisualFrag = EntityManager.GetFragmentDataPtr<FMassUnitVisualFragment>(Entity);
	FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
	if (!VisualFrag || !CharFrag) return;

	// --- 1. Size the ruin to the building's ACTUAL on-screen footprint, then StretchFactor on top. ---
	// Measure from the visual instance(s) the building is CURRENTLY using (the "previous ISM including
	// scaling"): mesh bounds * the instance's relative scale * the actor scale. Buildings scale via
	// SetActorScale3D, so the actor scale (PositionedTransform scale) carries the runtime size. This is
	// robust when the template ISMComponent is empty and honours multi-part buildings.
	const FVector ActorScale = CharFrag->PositionedTransform.GetScale3D();
	FVector BuildingWorldSize(0.f);
	for (const FMassUnitVisualInstance& Inst : VisualFrag->VisualInstances)
	{
		const UInstancedStaticMeshComponent* SrcISM = Inst.TargetISM.Get();
		if (!SrcISM || !SrcISM->GetStaticMesh()) continue;
		const FVector InstWorldSize = SrcISM->GetStaticMesh()->GetBoundingBox().GetSize()
			* (Inst.BaseOffset.GetScale3D() * ActorScale).GetAbs();
		BuildingWorldSize = BuildingWorldSize.ComponentMax(InstWorldSize);
	}
	if (BuildingWorldSize.IsNearlyZero())
	{
		BuildingWorldSize = (Unit->ISMComponent && Unit->ISMComponent->GetStaticMesh())
			? Unit->ISMComponent->GetStaticMesh()->GetBoundingBox().GetSize() * Unit->ISMComponent->GetRelativeScale3D().GetAbs() * ActorScale.GetAbs()
			: FVector(100.f);
	}

	const FBox RuinBox = RuinMesh->GetBoundingBox();
	const FVector RuinLocalSize = RuinBox.GetSize();

	auto SafeDiv = [](float A, float B) { return (FMath::Abs(B) > KINDA_SMALL_NUMBER) ? (A / B) : 1.f; };
	// UNIFORM fit from the X/Y footprint so the ruin keeps its natural proportions and Z scales ALONG WITH
	// X/Y (no pancaking). Per-axis Z-fitting made ruins flat whenever the ruin mesh was taller in local
	// bounds than the building (and the 0.01 clamp could floor it). StretchFactor is applied per-axis on
	// top for mesh-specific tuning (incl. deliberate Z squash/stretch).
	const float FitXY = 0.5f * (SafeDiv(BuildingWorldSize.X, RuinLocalSize.X) + SafeDiv(BuildingWorldSize.Y, RuinLocalSize.Y));
	FVector FinalScale = FVector(FitXY) * StretchFactor;
	FinalScale.X = FMath::Clamp(FinalScale.X, 0.01f, 100.f);
	FinalScale.Y = FMath::Clamp(FinalScale.Y, 0.01f, 100.f);
	FinalScale.Z = FMath::Clamp(FinalScale.Z, 0.01f, 100.f);

	// --- 2. Desired WORLD transform: building footprint X/Y, base seated on the ground, world yaw. ---
	const FTransform& BaseTransform = CharFrag->PositionedTransform;
	const FVector RuinCenter = RuinBox.GetCenter();
	const FVector RuinExtent = RuinBox.GetExtent();
	const float GroundZ = CharFrag->LastGroundLocation;
	// Seat the ruin's scaled mesh bottom (Center.Z - Extent.Z) exactly on GroundZ.
	const float SeatZ = GroundZ + (RuinExtent.Z - RuinCenter.Z) * FinalScale.Z;

	const FVector RuinWorldLoc(BaseTransform.GetLocation().X, BaseTransform.GetLocation().Y, SeatZ);
	const FQuat RuinWorldRot = FRotator(0.f, YawDegrees, 0.f).Quaternion();
	const FTransform RuinWorld(RuinWorldRot, RuinWorldLoc, FinalScale);

	// The placement processor renders each instance as CurrentRelativeTransform * BaseTransform, but for a
	// dead non-flying unit it first flattens BaseTransform's rotation to yaw-only. Convert the desired world
	// transform against the SAME yaw-only base, otherwise a ground-aligned (slope-pitched) building would
	// tilt the ruin and lift its base off the ground.
	FTransform SeatBase = BaseTransform;
	SeatBase.SetRotation(FRotator(0.f, BaseTransform.GetRotation().Rotator().Yaw, 0.f).Quaternion());
	const FTransform RuinRelative = RuinWorld.GetRelativeTransform(SeatBase);

	// --- 3. Release the current (building) visual instances and drop the ruin in their place. ---
	for (const FMassUnitVisualInstance& Instance : VisualFrag->VisualInstances)
	{
		ReleasePooledInstanceInternal(Instance.TargetISM.Get(), Instance.InstanceIndex, Unit);
	}
	VisualFrag->VisualInstances.Empty();

	UInstancedStaticMeshComponent* RuinISM = GetOrCreateISM(RuinMesh, RuinMaterial, bCastShadow);
	if (!RuinISM) return;

	// Key off the ACTUAL pooled ISM (not the requested material), so the pop bucket here matches the push
	// bucket in ReleasePooledInstanceInternal — otherwise a null RuinMaterial (ISM keeps the mesh's default
	// material, so GetMaterial(0) != null) would leak indices into a different bucket than we pop from.
	FMeshMaterialKey Key;
	Key.Mesh = RuinISM->GetStaticMesh();
	Key.Material = RuinISM->GetMaterial(0);
	Key.bCastShadow = RuinISM->CastShadow;

	int32 NewIndex = INDEX_NONE;
	if (FreeIndexPool.Contains(Key) && FreeIndexPool[Key].Num() > 0)
	{
		NewIndex = FreeIndexPool[Key].Pop();
	}
	else
	{
		NewIndex = RuinISM->AddInstance(FTransform::Identity);
	}
	// Start hidden (zero scale); the placement processor reveals it next tick from RuinRelative.
	RuinISM->UpdateInstanceTransform(NewIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);

	// Ruins are pure decoration — never collide or affect navigation.
	RuinISM->SetCanEverAffectNavigation(false);
	RuinISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TArray<TWeakObjectPtr<AMassUnitBase>>& UnitArray = ISMToUnitMap.FindOrAdd(RuinISM);
	if (UnitArray.Num() <= NewIndex) { UnitArray.SetNum(NewIndex + 1); }
	UnitArray[NewIndex] = Unit;

	FMassUnitVisualInstance RuinInstance;
	RuinInstance.TemplateISM = RuinISM; // must be valid or the tween/placement loops skip the instance
	RuinInstance.TargetISM = RuinISM;
	RuinInstance.InstanceIndex = NewIndex;
	RuinInstance.BaseOffset = RuinRelative;
	RuinInstance.CurrentRelativeTransform = RuinRelative;
	RuinInstance.bWasVisible = false;
	VisualFrag->VisualInstances.Add(RuinInstance);

	// Free the ruin instance on actor teardown (ruin despawns with the unit).
	Unit->CachePooledVisual(RuinISM, NewIndex);

	// --- 4. Un-hide (HideActorTime set bForceHidden) and force one placement pass to reveal the ruin. ---
	if (FMassVisualEffectFragment* EffectFrag = EntityManager.GetFragmentDataPtr<FMassVisualEffectFragment>(Entity))
	{
		EffectFrag->bForceHidden = false;
	}
	// The visibility processor excludes dead entities, so FMassVisibilityFragment.bIsOnViewport is frozen at
	// whatever it was at death — a building that died while off-screen would keep the ruin hidden even after
	// the camera pans back. Force it on; the placement gate also needs bIsVisibleEnemy (already true for own
	// buildings / enemies seen at death) and the ISM renderer still frustum-culls the instance per frame.
	if (FMassVisibilityFragment* VisFrag = EntityManager.GetFragmentDataPtr<FMassVisibilityFragment>(Entity))
	{
		VisFrag->bIsOnViewport = true;
	}
	CharFrag->bTransformDirty = true;
}

UInstancedStaticMeshComponent* UUnitVisualManager::GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow) {
    FMeshMaterialKey Key;
    Key.Mesh = Mesh;
    Key.Material = Material;
    Key.bCastShadow = bCastShadow;

    if (ISMPool.Contains(Key)) {
        return ISMPool[Key];
    }

    AActor* ManagerActor = nullptr;
    for (TObjectIterator<AActor> It; It; ++It) {
        if (It->GetWorld() == GetWorld() && (It->ActorHasTag(TEXT("UnitVisualISMManager")) || It->GetName().Contains(TEXT("UnitVisualISMManagerActor")))) {
            ManagerActor = *It;
            break;
        }
    }

    if (!ManagerActor) {
        ManagerActor = GetWorld()->SpawnActor<AActor>();
        ManagerActor->Tags.Add(TEXT("UnitVisualISMManager"));
        ManagerActor->SetFlags(RF_Transient);
        USceneComponent* Root = NewObject<USceneComponent>(ManagerActor, TEXT("Root"));
        ManagerActor->SetRootComponent(Root);
        Root->RegisterComponent();
#if WITH_EDITOR
        ManagerActor->SetActorLabel(TEXT("UnitVisualISMManagerActor"));
#endif
    }

    UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(ManagerActor);
    NewISM->SetStaticMesh(Mesh);
    // Pre-size custom data ONCE, before any instance is added (so nothing is wiped). The animation
    // processor writes custom-data indices 1..12, i.e. it needs 13 floats. Doing this here means it
    // never has to resize a live, shared ISM at runtime (which would zero EVERY instance's data).
    NewISM->SetNumCustomDataFloats(13);
    if (Material) {
        NewISM->SetMaterial(0, Material);
    }
    
    NewISM->SetMobility(EComponentMobility::Movable);
    NewISM->SetCastShadow(bCastShadow);
    // Collision will be set in AssignUnitVisual from Template
    NewISM->SetCanEverAffectNavigation(false);

    NewISM->AttachToComponent(ManagerActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    NewISM->RegisterComponent();
    
	ISMPool.Add(Key, NewISM);

	return NewISM;
}

AMassUnitBase* UUnitVisualManager::GetUnitFromInstance(UInstancedStaticMeshComponent* ISM, int32 InstanceIndex) {
	if (!ISM) return nullptr;
	if (TArray<TWeakObjectPtr<AMassUnitBase>>* UnitArray = ISMToUnitMap.Find(ISM)) {
		if (UnitArray->IsValidIndex(InstanceIndex)) {
			return (*UnitArray)[InstanceIndex].Get();
		}
	}
	return nullptr;
}
