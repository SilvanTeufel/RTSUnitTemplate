// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Controller/PlayerController/CustomControllerBase.h"
#include "GameModes/RTSGameModeBase.h"
#include "System/AbilityTemplateSubsystem.h"
// LUX-ANPASSUNG (16.08.2026): fuer die eng gefasste Direktsteuerungs-Ausnahme in
// CorrectSetUnitMoveTarget_Implementation (Schiessen waehrend des Laufens).
#include "Controller/PlayerController/CameraControllerBase.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"
#include "MassEntitySubsystem.h"     // Needed for FMassEntityManager, UMassEntitySubsystem
#include "MassNavigationFragments.h"
#include "ProfilingDebugging/CsvProfiler.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassMovementFragments.h"  // Needed for EMassMovementAction, FMassVelocityFragment
#include "MassEntityManager.h"  // For FMassEntityManager::FlushCommands
#include "MassExecutor.h"          // Provides Defer() method context typically
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "MassCommonFragments.h"
#include "Mass/UnitNavigationFragments.h"  // For FUnitNavigationPathFragment reset on client prediction
#include "Core/RTSUnitUtils.h"      // IsEntityUsable - guards the fragment access in RunUnitsAndSetWaypointsMass
#include "MassReplicationFragments.h" // For FMassNetworkIDFragment
#include "Mass/Replication/RTSWorldCacheSubsystem.h" // For MarkSkipMoveForNetID
#include "NavModifierVolume.h"
#include "Actors/FogActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h" // UGameViewportClient, for the drag-release focus check
#include "UnrealClient.h"              // FViewport::HasFocus
#include "Mass/Signals/MySignals.h"
#include "Mass/UnitMassTag.h"
#include "Actors/MinimapActor.h" 
#include "Actors/EffectArea.h"
#include "Characters/Unit/BuildingBase.h"
#include "NavigationSystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavMesh/RecastNavMesh.h"
#include "System/PlayerTeamSubsystem.h"
#include "TimerManager.h"
#include "Templates/Function.h"
#include "GAS/GameplayAbilityBase.h"
#include "AbilitySystemComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "Subsystems/UnitVisualManager.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "GameplayTagContainer.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Characters/Unit/GASUnit.h"
#include "Actors/WorkArea.h"
#include "GAS/AttributeSetBase.h"
#include "Blueprint/UserWidget.h"


void ACustomControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		MinimapSearchEndTime = World->GetTimeSeconds() + 10.0f;
	}
}

void ACustomControllerBase::Multi_SetMyTeamUnits_Implementation(const TArray<AActor*>& AllUnits)
{
	UE_LOG(LogTemp, Log, TEXT("Multi_SetMyTeamUnits: Starte Filterung fuer %d Einheiten (TeamId: %d)"), AllUnits.Num(), SelectableTeamId);

	if (!IsLocalController()) return;

	if (!HUDBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("Multi_SetMyTeamUnits: HUDBase ist nicht valide!"));
		return;
	}

	// Aktuelle Selektion leeren, bevor wir neu filtern
	HUDBase->DeselectAllUnits();
	int32 FilteredCount = 0;

	if (!bSelectOwnUnitsOnMatchStart)
	{
		SelectedUnits = HUDBase->SelectedUnits;
		return;
	}

	for (int32 i = 0; i < AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(AllUnits[i]);
		// !IsForeignCameraUnit: die Startauswahl nimmt sonst die CameraUnits der Mitspieler mit,
		// weil sie im selben Team stehen (siehe AControllerBase::IsForeignCameraUnit).
		if (Unit && Unit->GetUnitState() != UnitData::Dead && Unit->TeamId == SelectableTeamId
			&& !IsForeignCameraUnit(Unit))
		{
			// Direkt zum HUD-Array hinzufügen, da SetUnitSelected die Liste leeren würde
			HUDBase->SelectedUnits.AddUnique(Unit);
			HUDBase->SelectedUnitsSet.Add(Unit);
			Unit->SetSelected();
			FilteredCount++;
		}
	}

	// Synchronisiere die Controller-Liste mit der HUD-Liste
	SelectedUnits = HUDBase->SelectedUnits;

	UE_LOG(LogTemp, Log, TEXT("Multi_SetMyTeamUnits: Filterung abgeschlossen. %d Einheiten selektiert."), FilteredCount);

	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(GetPawn());
	if (Camera)
	{
		Camera->SetupResourceWidget(this);
	}
}

void ACustomControllerBase::Multi_SetCamLocation_Implementation(FVector NewLocation)
{
	//if (!IsLocalController()) return;
	
	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(CameraBase);
	if (Camera)
		Camera->SetActorLocation(NewLocation);
}

void ACustomControllerBase::Multi_HideEnemyWaypoints_Implementation()
{
	// Only execute for the local controller
	if (!IsLocalController())
	{
		return;
	}


	// Retrieve all waypoints from the world
	TArray<AActor*> FoundWaypoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWaypoint::StaticClass(), FoundWaypoints);

	// Iterate over each waypoint
	for (AActor* Actor : FoundWaypoints)
	{
		if (AWaypoint* Waypoint = Cast<AWaypoint>(Actor))
		{
			// Hide the waypoint if its TeamId is different from our team AND it isn't neutral (TeamId == 0)
			if (Waypoint->TeamId != SelectableTeamId && Waypoint->TeamId != 0)
			{
				Waypoint->SetActorHiddenInGame(true);
			}
		}
	}
}


void ACustomControllerBase::Multi_InitFogOfWar_Implementation()
{
	for (TActorIterator<AFogActor> It(GetWorld()); It; ++It)
	{
		AFogActor* FogActor = *It;
		if (FogActor)
		{
			// We call this now to ensure all Player Controllers have their Team Ids assigned first.
			FogActor->InitializeFogPostProcess();
		}
	}
}

void ACustomControllerBase::AgentInit_Implementation()
{
	// Only execute for the local controller (Client RPC). Server-spawned AI controllers without owning client will skip here.
	if (!IsLocalController())
	{
		return;
	}

	ARLAgent* Camera = Cast<ARLAgent>(CameraBase);
	if (Camera)
	{
		Camera->AgentInitialization();
	}
	else
	{
	}
}


void ACustomControllerBase::CorrectSetUnitMoveTarget_Implementation(UObject* WorldContextObject, AUnitBase* Unit, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius, bool AttackT)
{
	if (!Unit) { return; }
		
	if (!Unit->IsInitialized) { return; }
		
	if (!Unit->CanMove) { return; }
		
	// Do not accept move orders for dead units
	if (Unit->UnitState == UnitData::Dead) { return; }
		
	if (Unit->CurrentSnapshot.AbilityClass)
	{
		// ============================================================================================
		// LUX-ANPASSUNG (16.08.2026) â€” Schiessen waehrend des Laufens.
		// Bisher brach JEDER Laufbefehl die laufende Faehigkeit ab (bzw. wurde bei
		// AbilityCanBeCanceled == false ganz verworfen). Die WASD-Direktsteuerung schickt alle
		// UnitDirectMoveUpdateInterval (0,03 s) ein Update - der Dauerfeuer-Schuss wurde also
		// ~33x pro Sekunde gecancelt und kam nie zustande.
		//
		// AUSNAHME ist bewusst eng gefasst (Silvans Vorgabe: Aenderungen im RTSUnitTemplate nur
		// fuer die CameraUnit mit MouseFollow == false):
		//   - nur dieser Controller ist ein ACameraControllerBase in Direktsteuerung,
		//   - nur fuer genau dessen CameraUnitWithTag,
		//   - und nur wenn die laufende Faehigkeit Bewegung ausdruecklich erlaubt.
		// Letzteres kommt bei der Waffe aus FWeaponData::bCanFireWhileMoving, das
		// UShootAbility::ActivateAbility auf bStopMovementOnActivation der INSTANZ uebertraegt -
		// deshalb wird die Instanz bevorzugt vor dem CDO gelesen. Alle anderen Units und der
		// Maus-Folgen-Modus laufen unveraendert durch den Original-Zweig.
		//
		// Original:
		//   UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		//   if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;
		//   CancelCurrentAbility(Unit);
		// ============================================================================================
		bool bLuxKeepAbilityWhileMoving = false;
		if (const ACameraControllerBase* LuxDirectPC = Cast<ACameraControllerBase>(this))
		{
			if (LuxDirectPC->bUnitDirectControl && !LuxDirectPC->CameraUnitMouseFollow
				&& LuxDirectPC->CameraUnitWithTag == Unit)
			{
				const UGameplayAbilityBase* RunningAbility = Unit->ActivatedAbilityInstance
					? Unit->ActivatedAbilityInstance
					: Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();

				bLuxKeepAbilityWhileMoving = RunningAbility && !RunningAbility->bStopMovementOnActivation;
			}
		}

		if (!bLuxKeepAbilityWhileMoving)
		{
			UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();

			if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

			CancelCurrentAbility(Unit);
		}
		// ===================== ENDE LUX-ANPASSUNG ===================================================
	}

	Unit->bHoldPosition = false;
	// Bridge race: mark owner-name skip immediately to cover NetID==0 or pending
	
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: WorldContextObject is invalid or could not provide World."));
        return;
    }

    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: MassEntitySubsystem not found. Is Mass enabled?"));
        return;
    }

    FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	// Start with a mutable final target copy (param may be const)
	FVector FinalTargetLocation = NewTargetLocation;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (NavSys)
	{
		FNavLocation NavLoc;
		const bool bOnNav = NavSys->ProjectPointToNavigation(FinalTargetLocation, NavLoc, NavMeshProjectionExtent);
		bool bDirty = false;
		if (bOnNav)
		{
			const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
			if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
			{
				const uint32 PolyAreaID = Recast->GetPolyAreaID(NavLoc.NodeRef);
				const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
				bDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
			}
		}
		if (!bOnNav || bDirty)
		{
			// Adjust using formation-style validation for a single unit
			TArray<AUnitBase*> Single;
			Single.Add(Unit);
			TArray<FVector> OutOffsets;
			float UsedSpacing = 0.f;
			FVector Center = FinalTargetLocation;
			ValidateAndAdjustGridLocation(Single, Center, OutOffsets, UsedSpacing);
			FinalTargetLocation = (OutOffsets.Num() > 0) ? Center + OutOffsets[0] : Center;
		}
	}

	if (!Unit->MassActorBindingComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("CorrectSetUnitMoveTarget: Unit %s has no MassActorBindingComponent."), *GetNameSafe(Unit));
		return;
	}

	FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent->GetMassEntityHandle();

	if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(Unit))
	{
		if (IsValid(Worker->BuildArea))
		{
			Worker->BuildArea->StartedBuilding = false;
			Worker->BuildArea->PlannedBuilding = false;
			Worker->BuildArea->RemoveWorkerFromArray(Worker);
			Worker->BuildArea = nullptr;
		}
		if (IsValid(Worker->ResourcePlace))
		{
			Worker->ResourcePlace->RemoveWorkerFromArray(Worker);
			Worker->ResourcePlace = nullptr;
		}
	}

	if (!EntityManager.IsEntityActive(MassEntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is not active."), *MassEntityHandle.DebugGetDescription());
		return;
	}
    if (DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateDeadTag::StaticStruct()))
    {
        return;
    }

    // --- Access the PER-ENTITY fragment ---
    FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	FMassCombatStatsFragment* CombatStatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MassEntityHandle);
	
    if (!MoveTargetFragmentPtr || !AiStatePtr)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *MassEntityHandle.DebugGetDescription());
        return;
    }
	
	AiStatePtr->StoredLocation = FinalTargetLocation;
	
	bool bIsAttackingOrPausing = DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateAttackTag::StaticStruct()) || DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStatePauseTag::StaticStruct());
	bool bIsMovingWhileAttacking = CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking && bIsAttackingOrPausing;
	
	AiStatePtr->PlaceholderSignal = UnitSignals::Run;
	
	UpdateMoveTarget(*MoveTargetFragmentPtr, FinalTargetLocation, DesiredSpeed, World);
	MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;
	
	if (!bIsMovingWhileAttacking) EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	
	if (AttackT || (CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking))
	{
		if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
	}else
	{
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
	}

	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
	
	if (!bIsMovingWhileAttacking)
	{
		EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
	}
	
	//EntityManager.Defer().RemoveTag<FMassStateDeadTag>(MassEntityHandle); 
	EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateCastingTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(MassEntityHandle);

	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);
	// Ensure movement is not blocked by a lingering stop movement tag
	EntityManager.Defer().RemoveTag<FMassStateStopMovementTag>(MassEntityHandle);

	// Inform every client to predict locally for this single unit
	if (UWorld* PCWorld = GetWorld())
	{
		TArray<int32> IndicesArr;
		TArray<FVector> LocationsArr;
		TArray<float> SpeedsArr;
		TArray<float> RadiiArr;
		IndicesArr.Add(Unit ? Unit->UnitIndex : INDEX_NONE);
		LocationsArr.Add(FinalTargetLocation);
		SpeedsArr.Add(DesiredSpeed);
		RadiiArr.Add(AcceptanceRadius);
		for (FConstPlayerControllerIterator It = PCWorld->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
			{
				PC->Client_Predict_Batch_CorrectSetUnitMoveTargets(nullptr, IndicesArr, LocationsArr, SpeedsArr, RadiiArr, AttackT, true, true);
			}
		}
	}
}

TArray<FVector> ACustomControllerBase::AdjustBatchTargetsForNav(const TArray<AUnitBase*>& Units, const TArray<FVector>& InTargets)
{
	TArray<FVector> Result = InTargets;

	UWorld* World = GetWorld();
	if (!World) return Result;
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys || InTargets.Num() == 0) return Result;

	// Trigger the (relatively expensive) re-projection only when at least one target is off-navmesh or
	// in a dirty (UNavArea_Obstacle) area. The common case (all points valid) returns the input unchanged.
	bool bNeedsFormationAdjust = false;
	FVector FormationCenter = InTargets[0];

	// EINMAL vor der Schleife, nicht je Punkt: GetNavDataForProps durchsucht die Liste der
	// Navigationsdaten und der Cast kostet eine Typpruefung. Bei 510 Zielen lief beides 510-mal,
	// obwohl das Ergebnis fuer alle Punkte dasselbe ist.
	const ANavigationData* NavDataEinmal = NavSys->GetNavDataForProps(FNavAgentProperties());
	const ARecastNavMesh* RecastEinmal = Cast<ARecastNavMesh>(NavDataEinmal);

	for (const FVector& Loc : InTargets)
	{
		FNavLocation NavLoc;
		const bool bOnNav = NavSys->ProjectPointToNavigation(Loc, NavLoc, NavMeshProjectionExtent);
		bool bDirty = false;
		if (bOnNav)
		{
			if (const ARecastNavMesh* Recast = RecastEinmal)
			{
				const uint32 PolyAreaID = Recast->GetPolyAreaID(NavLoc.NodeRef);
				const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
				bDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
			}
		}
		if (!bOnNav || bDirty)
		{
			bNeedsFormationAdjust = true;
			break;
		}
	}
	if (!bNeedsFormationAdjust) return Result;

	// ValidateAndAdjustGridLocation snaps the whole grid (shift first, per-point fallback) onto valid,
	// non-dirty navmesh and returns per-slot offsets. It sorts its LocalUnits by radius, so match the
	// offsets back to the input order via the same stable sort.
	TArray<FVector> OutOffsets;
	float UsedSpacing = 0.f;
	ValidateAndAdjustGridLocation(Units, FormationCenter, OutOffsets, UsedSpacing);

	TArray<AUnitBase*> SortedUnits = Units;
	SortedUnits.Sort([](const AUnitBase& A, const AUnitBase& B) {
		float RA = 50.0f;
		if (A.GetCapsuleComponent()) RA = A.GetCapsuleComponent()->GetScaledCapsuleRadius();
		float RB = 50.0f;
		if (B.GetCapsuleComponent()) RB = B.GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (FMath::IsNearlyEqual(RA, RB)) return A.GetName() > B.GetName();
		return RA > RB;
	});

	TMap<AUnitBase*, FVector> TempOffsets;
	for (int32 i = 0; i < SortedUnits.Num(); ++i)
	{
		if (OutOffsets.IsValidIndex(i))
		{
			TempOffsets.Add(SortedUnits[i], OutOffsets[i]);
		}
	}

	for (int32 i = 0; i < Result.Num(); ++i)
	{
		if (Units.IsValidIndex(i) && Units[i])
		{
			Result[i] = FormationCenter + TempOffsets.FindRef(Units[i]);
		}
	}

	return Result;
}

/**
 * Entry point for every batched move order.
 *
 * Verteilt die Ziele in EINEM Durchgang. Eine frueher hier eingebaute Stueckelung (100 Einheiten
 * je halbe Sekunde) ist wieder entfernt: gemessen am 17.09.2026 auf LevelSix kostete der Befehl
 * selbst nur 0,26 ms je 100 Einheiten, also rund 1,5 ms fuer 510. Der spuerbare Klick-Ruckler kam
 * aus RecalculateFormation (die kubische Assignment in SolveHungarian, ~238 ms) und hatte mit
 * diesem Befehl nichts zu tun. Die Stueckelung kaufte also nichts, kostete aber einen
 * Sekunden-Nachlauf und zerlegte das Ergebnis in mehrere Netzwerkpakete.
 */
void ACustomControllerBase::Batch_CorrectSetUnitMoveTargets(UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	const TArray<float>& AcceptanceRadii,
	bool AttackT,
	bool bResetHoldPosition,
	bool bResetFollowTarget)
{
	ExecuteBatchMove(WorldContextObject, Units, NewTargetLocations, DesiredSpeeds, AcceptanceRadii,
		AttackT, bResetHoldPosition, bResetFollowTarget, /*bTargetsAlreadyValidated=*/false);
}

void ACustomControllerBase::ExecuteBatchMove(UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	const TArray<float>& AcceptanceRadii,
	bool AttackT,
	bool bResetHoldPosition,
	bool bResetFollowTarget,
	bool bTargetsAlreadyValidated)
{

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[BatchMove] Invalid WorldContextObject (World == nullptr)."));
		return;
	}

	const TArray<AUnitBase*>& Einheiten = Units;
	
	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[BatchMove] MassEntitySubsystem not found. Is Mass enabled?"));
		return;
	}
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

	// Nav-validate the per-unit targets via the shared single-source-of-truth helper. When the commanding
	// client already adjusted (right-click move) these are already valid -> no-op here; other callers
	// (attack-move/transporter/path) get validated here. Server_Batch_... forwards these same UsedTargets
	// to every client, so the server and all clients steer toward identical destinations (the off-nav /
	// dirty-area divergence that left client units stuck while the server moved them).
	// Die Pruefung ist der teuerste Einzelposten des Befehls: je Ziel eine Projektion auf das
	// Navmesh. Wer sie schon gemacht hat, reicht bTargetsAlreadyValidated durch - sonst lief sie im
	// Serverpfad ZWEIMAL ueber alle Ziele (einmal hier, einmal beim Aufrufer).
	const TArray<FVector> UsedTargets = bTargetsAlreadyValidated
		? NewTargetLocations
		: AdjustBatchTargetsForNav(Einheiten, NewTargetLocations);

	if ((Units.Num() != NewTargetLocations.Num() || Units.Num() != DesiredSpeeds.Num() || Units.Num() != AcceptanceRadii.Num()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BatchMove] Array size mismatch. Units:%d Locs:%d Speeds:%d Radii:%d (processing min count)"), Units.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num(), AcceptanceRadii.Num());
	}

	const int32 Count = FMath::Min(Einheiten.Num(), FMath::Min3(NewTargetLocations.Num(), DesiredSpeeds.Num(), AcceptanceRadii.Num()));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AUnitBase* Unit = Einheiten[Index];
		FVector UseLocation = UsedTargets.IsValidIndex(Index) ? UsedTargets[Index] : NewTargetLocations[Index];
		const float DesiredSpeed = DesiredSpeeds[Index];
		const float AcceptanceRadius = AcceptanceRadii[Index];

		if (!Unit)
		{
			continue;
		}
		
		if (!Unit->IsInitialized)
		{
			continue;
		}
		if (!Unit->CanMove)
		{
			continue;
		}
		if (Unit->UnitState == UnitData::Dead)
		{
			continue;
		}

		if (Unit->CurrentSnapshot.AbilityClass)
		{
			UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
			const bool bCancelable = !(AbilityCDO && !AbilityCDO->AbilityCanBeCanceled);
			if (!bCancelable)
			{
				continue;
			}
			CancelCurrentAbility(Unit);
		}

		if (bResetHoldPosition)
		{
			Unit->bHoldPosition = false;
		}

		if (bResetFollowTarget)
		{
			Unit->ApplyFollowTarget(nullptr);
			if (!Unit->MassActorBindingComponent->CanMoveWhileAttacking) Unit->RemoveFocusEntityTarget();
			else Unit->RemoveFriendlyFocusEntityTarget();
		}

		// Worker move-command authority: a player move order must win. Clear the worker's job +
		// AutoMining so the server-side WorkerSyncProcessor (auto-mine trigger, auto-mine recovery
		// rescan, actor-state->tag mirror, and the periodic UpdateMoveTarget) cannot re-route a unit
		// the player just commanded. Mirrors the single-unit CorrectSetUnitMoveTarget reset (which the
		// batch path was missing), plus AutoMining=false (the recovery block keys off Unit->AutoMining
		// directly, so nulling ResourcePlace alone is not enough). Re-mining is a deliberate later
		// action (right-clicking a resource node re-assigns ResourcePlace + AutoMining).
		if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(Unit))
		{
			Worker->AutoMining = false;
			if (IsValid(Worker->BuildArea))
			{
				Worker->BuildArea->StartedBuilding = false;
				Worker->BuildArea->PlannedBuilding = false;
				Worker->BuildArea->RemoveWorkerFromArray(Worker);
				Worker->BuildArea = nullptr;
			}
			if (IsValid(Worker->ResourcePlace))
			{
				Worker->ResourcePlace->RemoveWorkerFromArray(Worker);
				Worker->ResourcePlace = nullptr;
			}
		}

		// Mass entity handle
		FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
		
		if (!EntityManager.IsEntityActive(MassEntityHandle))
		{
			continue;
		}

		FMassCombatStatsFragment* CombatStatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MassEntityHandle);

		// Skip if Mass has Dead tag
		if (DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateDeadTag::StaticStruct()))
		{
			continue;
		}

		// Unless queuing with Shift, clear any existing path waypoints for Mass units before issuing a fresh command
		FMassUnitPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FMassUnitPathFragment>(MassEntityHandle);
		if (!this->IsShiftPressed && Unit->bIsMassUnit)
		{
			if (PathFrag)
			{
				PathFrag->Waypoints.Reset();
				PathFrag->CurrentIndex = 0;
				PathFrag->bIgnoreEnemiesDuringPath = false;
				PathFrag->bAttackMoveDuringPath = false;
			}
		}

		if (this->IsShiftPressed && Unit->bIsMassUnit && PathFrag)
		{
			if (PathFrag->Waypoints.Num() < 10)
			{
				PathFrag->Waypoints.Add(UseLocation);
				PathFrag->bAttackMoveDuringPath = AttackT;
				PathFrag->bAttackToggled = AttackT;
				PathFrag->bIgnoreEnemiesDuringPath = !AttackT;

				if (PathFrag->Waypoints.Num() > 1)
				{
					// Already moving on a path, we just appended. The processor will handle the switch once the current WP is reached.
					if (AttackT || (CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking)) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
					else EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);

					continue; // Skip setting MoveTarget now; UpdateUnitArrayMovement will pick it up
				}
			}
			else
			{
				continue;
			}
		}

		FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
		FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
		if (!MoveTargetFragmentPtr || !AiStatePtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] Missing fragments. MoveTarget:%s AI:%s"), *GetNameSafe(Unit), MoveTargetFragmentPtr ? TEXT("OK") : TEXT("NULL"), AiStatePtr ? TEXT("OK") : TEXT("NULL"));
			continue;
		}
		
		AiStatePtr->StoredLocation = UseLocation;

		bool bIsAttackingOrPausing = DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateAttackTag::StaticStruct()) || DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStatePauseTag::StaticStruct());
		bool bIsMovingWhileAttacking = CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking && bIsAttackingOrPausing;

		if (!bIsMovingWhileAttacking)
		{
			AiStatePtr->PlaceholderSignal = UnitSignals::Run;
		}
		
		// Final check: if UseLocation is still invalid, do a last-resort snap
		if (NavSys)
		{
			FNavLocation NavLoc;
			const bool bOnNav = NavSys->ProjectPointToNavigation(UseLocation, NavLoc, NavMeshProjectionExtent);
			bool bDirty = false;
			if (bOnNav)
			{
				const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
				if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
				{
					const uint32 PolyAreaID = Recast->GetPolyAreaID(NavLoc.NodeRef);
					const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
					bDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
				}
			}
			
			if (!bOnNav || bDirty)
			{
				// Radial search for nearest clean spot
				static const float Radii[] = {150.f, 300.f, 600.f};
				static const int32 Slices = 8;
				bool bFound = false;
				for (float R : Radii)
				{
					for (int32 s = 0; s < Slices; ++s)
					{
						const float Angle = (2 * PI) * (float(s) / float(Slices));
						const FVector Candidate = UseLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * R;
						FNavLocation CandNav;
						if (NavSys->ProjectPointToNavigation(Candidate, CandNav, FVector(1000.f, 1000.f, 1000.f)))
						{
							if (const ARecastNavMesh* RM = Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(FNavAgentProperties())))
							{
								const uint32 AID = RM->GetPolyAreaID(CandNav.NodeRef);
								const UClass* AC = RM->GetAreaClass(AID);
								if (!(AC && AC->IsChildOf(UNavArea_Obstacle::StaticClass())))
								{
									UseLocation = CandNav.Location;
									bFound = true;
									break;
								}
							}
						}
					}
					if (bFound) break;
				}
				if (!bFound && bOnNav) UseLocation = NavLoc.Location; // use dirty projected if nothing else
			}
		}

		UpdateMoveTarget(*MoveTargetFragmentPtr, UseLocation, DesiredSpeed, World);

		
		MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;

		// Tags manipulation
		if (!bIsMovingWhileAttacking) EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);

		if (AttackT || (CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking))
		{
			if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized)
			{
				EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
			}
		}
		else
		{
			EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
		}

		EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
		
		if (!bIsMovingWhileAttacking)
		{
			EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
			EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
		}
		
		EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateCastingTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateBuildTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateRepairTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToRepairTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);

	}
}

void ACustomControllerBase::Server_Batch_CorrectSetUnitMoveTargets_Implementation(
	UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	const TArray<float>& AcceptanceRadii,
	bool AttackT,
	bool bResetHoldPosition,
	bool bResetFollowTarget,
	bool bOriginatorPredictsLocally)
{
	// Die Navmesh-Pruefung EINMAL hier, und dasselbe Ergebnis geht sowohl in die autoritative
	// Anwendung als auch an die Clients - Server und Clients steuern damit denselben Punkt an.
	// Frueher lief sie zweimal ueber alle Ziele: einmal hier und gleich nochmal im Ausfuehrer.
	const TArray<FVector> ValidatedTargets = AdjustBatchTargetsForNav(Units, NewTargetLocations);

	ExecuteBatchMove(WorldContextObject, Units, ValidatedTargets, DesiredSpeeds, AcceptanceRadii,
		AttackT, bResetHoldPosition, bResetFollowTarget, /*bTargetsAlreadyValidated=*/true);

	NotifyClientsOfBatchMove(Units, ValidatedTargets, DesiredSpeeds, AcceptanceRadii,
		AttackT, bResetHoldPosition, bResetFollowTarget, bOriginatorPredictsLocally);
}

void ACustomControllerBase::RTSPerfTestFixCamera()
{
	// Kamera auf einen festen Standpunkt ueber der Gruppe zwingen.
	//
	// WOFUER: die Kamera entscheidet, wie viele Einheiten projiziert, gezeichnet und in den
	// Auswahlindikatoren verarbeitet werden - also genau die Posten, die hier gemessen werden.
	// Eine von Hand bewegte oder gezoomte Kamera macht zwei Laeufe unvergleichbar; genau daran
	// sind die Messungen vom 17.09.2026 gescheitert (Bildzeit unveraendert, obwohl die
	// Abschnitte zusammen 3,7 ms verloren hatten - die Streuung lag bei p5 8,8 bis p95 23,6 ms).
	ACameraBase* Kamera = Cast<ACameraBase>(GetPawn());
	if (!Kamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfTest] Keine CameraBase - Kamera bleibt wie sie ist."));
		return;
	}

	FVector Center = FVector::ZeroVector;
	int32 Counter = 0;
	for (AUnitBase* U : SelectedUnits)
	{
		if (IsValid(U)) { Center += U->GetActorLocation(); ++Counter; }
	}
	if (Counter == 0)
	{
		return;
	}
	Center /= Counter;

	Kamera->SetActorLocation(Center + FVector(0.f, 0.f, RTSPerfTestCameraHeight));
	Kamera->CameraDistanceToCharacter = RTSPerfTestCameraDistance;
	if (Kamera->SpringArm)
	{
		Kamera->SpringArm->TargetArmLength = RTSPerfTestCameraDistance;
		Kamera->SpringArm->SetRelativeRotation(Kamera->SpringArmRotator);
	}
}

void ACustomControllerBase::RTSPerfSpawn(int32 Count, int32 Id, float Spread)
{
	ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(UGameplayStatics::GetGameMode(this));
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfTest] Kein ARTSGameModeBase - kann nicht spawnen."));
		return;
	}

	// Schwerpunkt der vorhandenen eigenen Einheiten. GetMassActorLocation, nicht GetActorLocation:
	// unter der Drosselung hinkt die Aktorlage bis zu 0,5 s nach.
	FVector Center = FVector::ZeroVector;
	int32 ExistingUnits = 0;
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		AUnitBase* U = *It;
		if (IsValid(U) && U->TeamId == SelectableTeamId && U->GetUnitState() != UnitData::Dead)
		{
			Center += U->GetMassActorLocation();
			++ExistingUnits;
		}
	}
	if (ExistingUnits == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfTest] Keine eigenen Einheiten als Bezugspunkt."));
		return;
	}
	Center /= ExistingUnits;

	// WELCHE ZEILE? NICHT RATEN - NACHSEHEN.
	//
	// Der erste Versuch lief mit Id 1 und meldete "0 von 90 erzeugt": diese Id gibt es in den
	// Spawntabellen dieser Karte nicht. Ohne die Erfolgsmeldung waere der Lauf als "600 Einheiten"
	// ausgewertet worden, obwohl es 510 blieben. Deshalb sucht die Funktion sich die Row jetzt
	// selbst, wenn Id negativ ist - und schreibt in jedem Fall auf, welche Ids es ueberhaupt gibt.
	FUnitSpawnParameter SelectedRow;
	bool bRowFound = false;
	FString AvailableIds;

	for (UDataTable* Table : GameMode->UnitSpawnParameters)
	{
		if (!Table) continue;
		for (const FName& RowName : Table->GetRowNames())
		{
			const FUnitSpawnParameter* Row = Table->FindRow<FUnitSpawnParameter>(RowName, TEXT(""));
			if (!Row || !Row->UnitBaseClass) continue;

			AvailableIds += FString::Printf(TEXT("%d "), Row->Id);
			const bool bMatches = (Id < 0) ? true : (Row->Id == Id);
			if (bMatches && !bRowFound)
			{
				SelectedRow = *Row;
				bRowFound = true;
			}
		}
	}

	if (!bRowFound)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[PerfTest] RTSPerfSpawn: keine brauchbare Row fuer Id %d. ExistingUnits Ids: %s"),
			Id, *AvailableIds);
		return;
	}

	int32 SpawnedCount = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// Goldener Angle: verteilt gleichmaessig, ohne Ringe oder Speichen zu bilden. Ein
		// Zufallsversatz waere hier schlechter - die Messung soll wiederholbar sein.
		const float Angle = 2.39996323f * i;
		const float Radius = Spread * FMath::Sqrt(float(i + 1) / float(FMath::Max(Count, 1)));
		const FVector Location = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);

		// SpawnSingleUnit statt SpawnSingleUnitFromDataTable: letztere sucht die Row jedes Mal
		// neu und haengt das Ergebnis zusaetzlich an den Sammelpunkt des naechsten Gebaeudes -
		// beides ist hier unerwuenscht.
		if (GameMode->SpawnSingleUnit(SelectedRow, Location, nullptr, SelectableTeamId, nullptr))
		{
			++SpawnedCount;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[PerfTest] RTSPerfSpawn: %d von %d erzeugt (Zeilen-Id %d), vorher %d eigene Einheiten. ExistingUnits Ids: %s"),
		SpawnedCount, Count, SelectedRow.Id, ExistingUnits, *AvailableIds);
}

void ACustomControllerBase::RTSSelectAllOwn()
{
	if (!HUDBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfTest] Kein HUD - Auswahl nicht moeglich."));
		return;
	}

	HUDBase->DeselectAllUnits();

	int32 SelectedCount = 0;
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		AUnitBase* U = *It;
		if (!IsValid(U) || U->GetUnitState() == UnitData::Dead)
		{
			continue;
		}
		if (U->TeamId != SelectableTeamId || IsForeignCameraUnit(U))
		{
			continue;
		}
		HUDBase->SelectedUnits.AddUnique(U);
		HUDBase->SelectedUnitsSet.Add(U);
		U->SetSelected();
		++SelectedCount;
	}
	SelectedUnits = HUDBase->SelectedUnits;

	UE_LOG(LogTemp, Warning, TEXT("[PerfTest] %d Einheiten von Team %d gewaehlt."),
		SelectedCount, SelectableTeamId);
}

static TAutoConsoleVariable<int32> CVarRTS_PerfTestGroups(
	TEXT("RTS.PerfTest.Gruppen"),
	1,
	TEXT("1 = alle Einheiten marschieren gemeinsam zum selben Ziel (Vorgabe). N > 1 = die Auswahl ")
	TEXT("wird in N Gruppen geteilt, N-1 davon bekommen JE EIN EIGENES Ziel sternfoermig um die ")
	TEXT("Mitte, die letzte bleibt stehen.")
	TEXT("")
	TEXT("WOFUER: die Pruefung der Chunk-Tag-Abfrage (RTS.ChunkTags.Verify) meldete 0 Deviations ")
	TEXT("bei 200.000 Vergleichen - aber nur ZWEI Tag-Kombinationen. Das ist kein Wunder, wenn alle ")
	TEXT("Einheiten dasselbe tun: sie haben dann zwangslaeufig dieselben Tags, und ein Fehler in der ")
	TEXT("Annahme 'ein Chunk = ein Archetyp' koennte sich gar nicht zeigen. Mit mehreren Gruppen ")
	TEXT("laufen Marschierende (MassStateRun) und Stehende (Idle/Patrol) GLEICHZEITIG, und die ")
	TEXT("Erkennungs- und Verfolgungstags streuen zusaetzlich. Erst dann hat die Pruefung Aussagekraft."),
	ECVF_Default);

void ACustomControllerBase::RTSPerfTest(float StandSeconds, float MarchSeconds, float Distance)
{
	RTSPerfTestDistance = Distance;
	RTSPerfTestStandSeconds = StandSeconds;
	RTSPerfTestMoveSeconds = MarchSeconds;
	RTSPerfTestStart = FPlatformTime::Seconds();
	RTSPerfTestPhase = 0;
	RTSPerfTestOrderCount = 0;

	RTSSelectAllOwn();
	RTSPerfTestFixCamera();

	CSV_EVENT_GLOBAL(TEXT("Phase_Standing"));
	UE_LOG(LogTemp, Warning,
		TEXT("[PerfTest] Start | %.0f s stehen, dann %.0f s marschieren (%.0f uu, Nachbefehl alle %.0f s) | %d Einheiten"),
		StandSeconds, MarchSeconds, Distance, RTSPerfTestOrderInterval, SelectedUnits.Num());

	GetWorldTimerManager().SetTimer(RTSPerfTestTimer, this,
		&ACustomControllerBase::RTSPerfTestTick, 1.0f, true);
}

void ACustomControllerBase::RTSPerfTestTick()
{
	const double Elapsed = FPlatformTime::Seconds() - RTSPerfTestStart;

	// ---- Phase 0: stehen -------------------------------------------------------------------
	if (Elapsed < RTSPerfTestStandSeconds)
	{
		return;
	}

	// ---- Phase 2: wieder stehen ------------------------------------------------------------
	if (Elapsed > RTSPerfTestStandSeconds + RTSPerfTestMoveSeconds)
	{
		if (RTSPerfTestPhase != 2)
		{
			RTSPerfTestPhase = 2;
			CSV_EVENT_GLOBAL(TEXT("Phase_StandingAgain"));
			UE_LOG(LogTemp, Warning, TEXT("[PerfTest] Marschphase beendet nach %d Befehlen."),
				RTSPerfTestOrderCount);
		}
		// Nach weiteren 15 s ist der Messfall fertig.
		if (Elapsed > RTSPerfTestStandSeconds + RTSPerfTestMoveSeconds + 15.0)
		{
			CSV_EVENT_GLOBAL(TEXT("Phase_Done"));
			UE_LOG(LogTemp, Warning, TEXT("[PerfTest] FERTIG."));
			GetWorldTimerManager().ClearTimer(RTSPerfTestTimer);
		}
		return;
	}

	// ---- Phase 1: marschieren --------------------------------------------------------------
	if (RTSPerfTestPhase != 1)
	{
		RTSPerfTestPhase = 1;
		CSV_EVENT_GLOBAL(TEXT("Phase_Moving"));
	}

	const double SincePhaseStart = Elapsed - RTSPerfTestStandSeconds;
	const int32 Due = FMath::FloorToInt(SincePhaseStart / RTSPerfTestOrderInterval) + 1;
	if (RTSPerfTestOrderCount >= Due)
	{
		return;   // Der naechste Befehl ist noch nicht dran.
	}

	// Die Auswahl JEDES MAL neu setzen - siehe den Kommentar an RTSPerfTestTick.
	RTSSelectAllOwn();
	if (SelectedUnits.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfTest] Keine eigenen Einheiten mehr."));
		return;
	}

	FVector Center = FVector::ZeroVector;
	int32 Counter = 0;
	for (AUnitBase* U : SelectedUnits)
	{
		if (IsValid(U)) { Center += U->GetActorLocation(); ++Counter; }
	}
	if (Counter == 0) { return; }
	Center /= Counter;

	// Abwechselnd hin und zurueck, damit die Gruppe in Bewegung bleibt statt anzukommen.
	const float Sign = (RTSPerfTestOrderCount % 2 == 0) ? 1.f : -1.f;
	const FVector Target = Center + FVector(RTSPerfTestDistance * Sign,
	                                     RTSPerfTestDistance * Sign * 0.6f, 0.f);

	++RTSPerfTestOrderCount;
	RTSPerfTestFixCamera();

	const int32 GroupCount = FMath::Max(1, CVarRTS_PerfTestGroups.GetValueOnGameThread());
	if (GroupCount <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfTest] Befehl %d an %d Einheiten nach %s (t=%.0f s)"),
			RTSPerfTestOrderCount, SelectedUnits.Num(), *Target.ToCompactString(), Elapsed);
		RightClickPressedMassMinimap(Target);
		return;
	}

	// Streumodus - siehe RTS.PerfTest.GroupCount. Die Auswahl wird nur voruebergehend beschnitten und
	// am Ende vollstaendig wiederhergestellt; RTSPerfTestTick setzt sie ohnehin jedes Mal neu.
	TArray<AUnitBase*> AllSelectedUnits = SelectedUnits;
	const int32 PerGroup = FMath::DivideAndRoundUp(AllSelectedUnits.Num(), GroupCount);

	UE_LOG(LogTemp, Warning,
		TEXT("[PerfTest] Befehl %d STREUEND an %d Einheiten in %d GroupCount (%d marschieren, %d bleiben stehen) (t=%.0f s)"),
		RTSPerfTestOrderCount, AllSelectedUnits.Num(), GroupCount,
		FMath::Min(PerGroup * (GroupCount - 1), AllSelectedUnits.Num()),
		FMath::Max(0, AllSelectedUnits.Num() - PerGroup * (GroupCount - 1)), Elapsed);

	// Die LETZTE Gruppe bekommt bewusst keinen Befehl - sie liefert die stehenden Einheiten,
	// ohne die neben Run- keine Idle-Tags im selben Bild vorkaemen.
	for (int32 g = 0; g < GroupCount - 1; ++g)
	{
		const int32 From = g * PerGroup;
		const int32 To = FMath::Min(From + PerGroup, AllSelectedUnits.Num());
		if (From >= To)
		{
			break;
		}

		SelectedUnits.Reset();
		for (int32 i = From; i < To; ++i)
		{
			if (IsValid(AllSelectedUnits[i]))
			{
				SelectedUnits.Add(AllSelectedUnits[i]);
			}
		}
		if (SelectedUnits.Num() == 0)
		{
			continue;
		}

		const float Angle = (2.f * PI * g) / float(GroupCount - 1);
		const FVector GroupTarget = Center + FVector(
			FMath::Cos(Angle) * RTSPerfTestDistance * Sign,
			FMath::Sin(Angle) * RTSPerfTestDistance * Sign, 0.f);

		RightClickPressedMassMinimap(GroupTarget);
	}

	SelectedUnits = MoveTemp(AllSelectedUnits);
}

void ACustomControllerBase::NotifyClientsOfBatchMove(
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& UsedTargets,
	const TArray<float>& DesiredSpeeds,
	const TArray<float>& AcceptanceRadii,
	bool AttackT,
	bool bResetHoldPosition,
	bool bResetFollowTarget,
	bool bOriginatorPredictsLocally)
{
	UWorld* PCWorld = GetWorld();
	if (!PCWorld || Units.Num() == 0)
	{
		return;
	}

	// Eine RPC fuer den ganzen Befehl. ClientPredictMaxPerRPC = 0 heisst unbegrenzt; frueher stand
	// hier eine feste Teilung bei 200, die einen Befehl ohne Not auf mehrere Pakete verteilte.
	const int32 MaxPerRPC = (ClientPredictMaxPerRPC > 0) ? ClientPredictMaxPerRPC : Units.Num();

	for (int32 i = 0; i < Units.Num(); i += MaxPerRPC)
	{
		const int32 CurrentBatchNum = FMath::Min(MaxPerRPC, Units.Num() - i);

		// Replizierte UnitIndices statt Aktorzeiger: Objektverweise in RPCs koennen auf dem
		// Empfaenger null werden, wenn ihre NetGUID dort nicht aufgeloest ist (der "steckende
		// Einheit im Kampf"-Fehler). Die Clients loesen die Einheiten lokal ueber den Bindungscache auf.
		TArray<int32> BatchIndices;
		TArray<FVector> BatchLocations;
		TArray<float> BatchSpeeds;
		TArray<float> BatchRadii;

		BatchIndices.Reserve(CurrentBatchNum);
		BatchLocations.Reserve(CurrentBatchNum);
		BatchSpeeds.Reserve(CurrentBatchNum);
		BatchRadii.Reserve(CurrentBatchNum);

		for (int32 j = 0; j < CurrentBatchNum; ++j)
		{
			const int32 GlobalIdx = i + j;
			const AUnitBase* U = Units[GlobalIdx];
			// INDEX_NONE haelt die Reihen buendig; der Client ueberspringt diese Eintraege.
			BatchIndices.Add(U ? U->UnitIndex : INDEX_NONE);
			BatchLocations.Add(UsedTargets.IsValidIndex(GlobalIdx) ? UsedTargets[GlobalIdx] : FVector::ZeroVector);
			BatchSpeeds.Add(DesiredSpeeds.IsValidIndex(GlobalIdx) ? DesiredSpeeds[GlobalIdx] : 0.f);
			BatchRadii.Add(AcceptanceRadii.IsValidIndex(GlobalIdx) ? AcceptanceRadii[GlobalIdx] : 0.f);
		}

		for (FConstPlayerControllerIterator It = PCWorld->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
			{
				// Den Umweg ueber den Absender sparen, wenn der schon lokal vorhergesagt hat -
				// sonst wird bei ihm doppelt angewendet.
				if (bOriginatorPredictsLocally && PC == this)
				{
					continue;
				}
				PC->Client_Predict_Batch_CorrectSetUnitMoveTargets(nullptr, BatchIndices, BatchLocations, BatchSpeeds, BatchRadii, AttackT, bResetHoldPosition, bResetFollowTarget);
			}
		}
	}
}

void ACustomControllerBase::Batch_KickUnits(const TArray<AUnitBase*>& Units)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem) return;

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

	for (AUnitBase* Unit : Units)
	{
		if (!Unit) continue;

		// Ein laufender Cast darf vom Kick NICHT angefasst werden.
		//
		// Der Kick entfernt weiter unten bedingungslos FMassStateCastingTag (und setzt fuer bewegliche
		// Einheiten zusaetzlich Run + ein neues MoveTarget). Er ist dafuer gedacht, frisch replizierte
		// oder eingefrorene Einheiten anzustossen - eine Einheit mitten im Cast ist weder das eine noch
		// das andere. Gemessen am 16.08.2026: Casts auf BP_BuildingBase_Singularian_DataCenter_C_1
		// endeten reihenweise nach 0,86-1,75 s mit cancelled=1 und Zustand bereits 0/6, ohne dass je ein
		// EndCast kam - der Cast erreichte seine CastTime nie und es entstand keine Einheit.
		//
		// Verschaerfend: in UServerReplicationKickProcessor ist in der InitialKickQuery die Zeile
		// RemoveTag<FMassStateNeedsInitialKickTag> auskommentiert, der Tag bleibt also stehen und die
		// betroffene Einheit wird bei ExecutionInterval 0,1 s dauerhaft weitergekickt.
		if (Unit->GetUnitState() == UnitData::Casting)
		{
			continue;
		}

		// On Client, we might be called before IsInitialized is true, but we still want to unfreeze.
		// However, we MUST have a valid mass entity.
		FMassEntityHandle EntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
		if (!EntityManager.IsEntityActive(EntityHandle)) continue;

		FVector CurrentLocation = Unit->GetMassActorLocation();
		FVector ProjectLocation = CurrentLocation;

		// Ensure vertical alignment and movement state for mobile units
		if (Unit->CanMove)
		{
			if (NavSys)
			{
				FNavLocation NavLoc;
				// Using a larger Z-extent (500.f) to ensure units spawned above the grid are correctly snapped down.
				if (NavSys->ProjectPointToNavigation(CurrentLocation, NavLoc, FVector(200.f, 200.f, 500.f)))
				{
					ProjectLocation = NavLoc.Location;
				}
			}

			if (FMassMoveTargetFragment* MoveTarget = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(EntityHandle))
			{
				// Refined to work like Batch_CorrectSetUnitMoveTargets:
				// Use UpdateMoveTarget instead of StopMovement to trigger "Move" action
				UpdateMoveTarget(*MoveTarget, ProjectLocation, Unit->Attributes->GetRunSpeed(), World);
				MoveTarget->DistanceToGoal = 0.f;
				MoveTarget->SlackRadius = 50.f;
			}
			
			// Re-add Run tag as it's done in Batch_CorrectSetUnitMoveTargets
			EntityManager.Defer().AddTag<FMassStateRunTag>(EntityHandle);
		}

		// Tags Manipulation: Synchronize state between Server and Client
		EntityManager.Defer().AddTag<FMassStateDetectTag>(EntityHandle);
		
		EntityManager.Defer().RemoveTag<FMassStateIdleTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateFrozenTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateNeedsInitialKickTag>(EntityHandle);

		// Comprehensive tag removal to match Batch_CorrectSetUnitMoveTargets
		EntityManager.Defer().RemoveTag<FMassStateChaseTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateAttackTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePauseTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateCastingTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateBuildTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateRepairTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToRepairTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(EntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(EntityHandle);

		if (Unit->MassActorBindingComponent && !Unit->MassActorBindingComponent->StopSeparation && !Cast<AConstructionUnit>(Unit))
		{
			EntityManager.Defer().RemoveTag<FMassStateStopSeparationTag>(EntityHandle);
		}
	}
    
	// Crucial: Flush commands so tags are applied immediately for the next simulation tick
	EntityManager.FlushCommands();
}

void ACustomControllerBase::ApplyMovePredictionToUnit(
	FMassEntityManager& EntityManager,
	UWorld* World,
	AUnitBase* Unit,
	const FVector& NewTargetLocation,
	float DesiredSpeed,
	float AcceptanceRadius,
	bool AttackT,
	bool bResetHoldPosition,
	bool bResetFollowTarget)
{
	if (!Unit || !World)
	{
		return;
	}
	if (!Unit->IsInitialized)
	{
		return;
	}
	if (!Unit->CanMove)
	{
		return;
	}
	if (Unit->UnitState == UnitData::Dead)
	{
		return;
	}

	if (bResetHoldPosition)
	{
		Unit->bHoldPosition = false;
	}

	if (bResetFollowTarget)
	{
		Unit->ApplyFollowTarget(nullptr);
		if (!Unit->MassActorBindingComponent->CanMoveWhileAttacking) Unit->RemoveFocusEntityTarget();
		else Unit->RemoveFriendlyFocusEntityTarget();
	}

	// Worker move-command prediction: clear job + AutoMining on client to match server
	if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(Unit))
	{
		Worker->AutoMining = false;
		if (IsValid(Worker->BuildArea))
		{
			Worker->BuildArea->StartedBuilding = false;
			Worker->BuildArea->PlannedBuilding = false;
			Worker->BuildArea->RemoveWorkerFromArray(Worker);
			Worker->BuildArea = nullptr;
		}
		if (IsValid(Worker->ResourcePlace))
		{
			Worker->ResourcePlace->RemoveWorkerFromArray(Worker);
			Worker->ResourcePlace = nullptr;
		}
	}

	FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();

	if (!EntityManager.IsEntityValid(MassEntityHandle))
	{
		return;
	}

	FMassCombatStatsFragment* CombatStatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MassEntityHandle);

	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	if (!AiStatePtr)
	{
		return;
	}

	// Crucial: reset switching state so the move command is processed immediately by client processors.
	// SwitchingStateClient must also be cleared: several client state processors early-out
	// (`if (SwitchingStateClient) continue;`) while it is latched true (e.g. from a worker arrival
	// transition), which would make them skip the freshly commanded unit for a tick.
	AiStatePtr->SwitchingState = false;
	AiStatePtr->SwitchingStateClient = false;
	AiStatePtr->StateTimer = 0.f;

	AiStatePtr->StoredLocation = NewTargetLocation;

	bool bIsAttackingOrPausing = DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateAttackTag::StaticStruct()) || DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStatePauseTag::StaticStruct());
	bool bIsMovingWhileAttacking = CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking && bIsAttackingOrPausing;


	if (!bIsMovingWhileAttacking)
	{
		AiStatePtr->PlaceholderSignal = UnitSignals::Run;
	}

	// Add Run tag so client processors include this entity immediately
	if (!bIsMovingWhileAttacking)
	{
		Unit->SetUnitState(UnitData::Run);
		EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	}
	// Tagging and prediction
	if (FMassClientPredictionFragment* PredFrag = EntityManager.GetFragmentDataPtr<FMassClientPredictionFragment>(MassEntityHandle))
	{
		PredFrag->Location = NewTargetLocation;
		PredFrag->PredDesiredSpeed = DesiredSpeed;
		PredFrag->PredAcceptanceRadius = AcceptanceRadius;
		PredFrag->bHasData = true;
		PredFrag->PredSource = 6; // [PredDiag]
		// Stamp the command time so ApplyReplicatedTagBits can let this predicted Run beat the
		// stale replicated worker bits for a bounded grace window (see bSuppressWorkerStomp).
		PredFrag->CommandPredictTime = World->GetTimeSeconds();
	}

	// Update path fragment for client-side visualization/logic if Shift is held
	if (this->IsShiftPressed && Unit->bIsMassUnit)
	{
		if (FMassUnitPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FMassUnitPathFragment>(MassEntityHandle))
		{
			if (PathFrag->Waypoints.Num() < 10)
			{
				PathFrag->Waypoints.Add(NewTargetLocation);
				PathFrag->bAttackMoveDuringPath = AttackT;
				PathFrag->bAttackToggled = AttackT;
				PathFrag->bIgnoreEnemiesDuringPath = !AttackT;
			}
		}
	}
	else if (!this->IsShiftPressed && Unit->bIsMassUnit)
	{
		if (FMassUnitPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FMassUnitPathFragment>(MassEntityHandle))
		{
			PathFrag->Waypoints.Reset();
			PathFrag->CurrentIndex = 0;
			PathFrag->bIgnoreEnemiesDuringPath = false;
			PathFrag->bAttackMoveDuringPath = false;
		}
	}
	// Ensure client won't skip movement this tick
	AiStatePtr->CanMove = true;
	if (bResetHoldPosition)
	{
		AiStatePtr->HoldPosition = false;
	}

	// Reset local path state to avoid stale path following from previous order
	if (FUnitNavigationPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FUnitNavigationPathFragment>(MassEntityHandle))
	{
		PathFrag->ResetPath();
		PathFrag->bIsPathfindingInProgress = false;
	}

	if (AttackT || (CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking))
	{
		if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized)
		{
			EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
		}
	}
	else
	{
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
	}

	// Strip other mutually exclusive state tags
	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);

	if (!bIsMovingWhileAttacking)
	{
		EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
	}

	EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateCastingTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateRepairTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToRepairTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);
}

void ACustomControllerBase::Client_Predict_Batch_CorrectSetUnitMoveTargets_Implementation(
	UObject* WorldContextObject,
	const TArray<int32>& UnitIndices,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	const TArray<float>& AcceptanceRadii,
	bool AttackT,
	bool bResetHoldPosition,
	bool bResetFollowTarget)
{
	//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Received batch prediction request: Units=%d"), Units.Num());
	// Run prediction only on non-authority (clients). Avoid double-applying on listen servers.

	if (HasAuthority())
	{
		//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Early return: HasAuthority()==true. Skipping client prediction."));
		return;
	}

	UWorld* World = nullptr;
	if (WorldContextObject)
	{
		World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	}
	if (!World)
	{
		World = GetWorld();
	}
	if (!World)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Early return: World is null (WorldContextObject=%s)."), *GetNameSafe(WorldContextObject));
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Early return: MassEntitySubsystem is null for world %s."), *GetNameSafe(World));
		return;
	}
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	const int32 Count = FMath::Min3(UnitIndices.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num());

	// Resolve units locally from their replicated UnitIndex via the shared binding cache.
	// Refresh once up front (throttled); if a commanded unit is missing (recent spawn) we force a
	// single rescan below and retry, so freshly-spawned units aren't silently dropped.
	URTSWorldCacheSubsystem* CacheSub = World->GetSubsystem<URTSWorldCacheSubsystem>();
	if (CacheSub)
	{
		CacheSub->RebuildBindingCacheIfNeeded();
	}
	bool bForcedCacheRebuild = false;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (NavSys && NewTargetLocations.Num() > 0)
	{
		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(NewTargetLocations[0], NavLoc, NavMeshProjectionExtent))
		{
			//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Early return: Target location %s is not on NavMesh."), *NewTargetLocations[0].ToString());
			return;
		}
	}

	//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Begin batch: Units=%d Targets=%d Speeds=%d Count=%d World=%s"), Units.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num(), Count, *GetNameSafe(World));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 UnitIndex = UnitIndices[Index];
		if (UnitIndex == INDEX_NONE)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[BatchMove][Client][%d] UnitIndex == INDEX_NONE. Skipping."), Index);
			continue;
		}

		// UnitIndex -> binding component -> owning actor. Resolved locally, so it survives even when
		// an actor-pointer RPC arg would have arrived null (unmapped NetGUID under relevance churn).
		UMassActorBindingComponent* Bind = CacheSub ? CacheSub->FindBindingByUnitIndex(UnitIndex) : nullptr;
		if (!Bind && CacheSub && !bForcedCacheRebuild)
		{
			// Commanded unit not cached yet (recent spawn): force one rescan this batch and retry.
			CacheSub->RebuildBindingCacheIfNeeded(0.f);
			bForcedCacheRebuild = true;
			Bind = CacheSub->FindBindingByUnitIndex(UnitIndex);
		}
		AUnitBase* Unit = Bind ? Cast<AUnitBase>(Bind->GetOwner()) : nullptr;
		if (!Unit)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[BatchMove][Client][%d] No unit for UnitIndex=%d. Skipping."), Index, UnitIndex);
			continue;
		}

		// Apply the shared prediction logic for this resolved unit. Skips (not initialized / can't move /
		// dead / invalid entity) are handled inside the helper. Flush happens once after the loop.
		ApplyMovePredictionToUnit(EntityManager, World, Unit, NewTargetLocations[Index], DesiredSpeeds[Index], AcceptanceRadii[Index], AttackT, bResetHoldPosition, bResetFollowTarget);
	}
	// Ensure deferred commands (tags added/removed) are applied immediately so prediction is visible to processors
	EntityManager.FlushCommands();
	//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Flushed deferred Mass commands for batch (%d units)"), Count);
	
}

void ACustomControllerBase::CorrectSetUnitMoveTargetForAbility_Implementation(UObject* WorldContextObject, AUnitBase* Unit, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius, bool AttackT)
{
	if (!Unit) return;
	
	if (!Unit->IsInitialized) return;
	
	if (!Unit->CanMove) return;
	
	// Do not accept move orders for dead units (ability path)
	if (Unit->UnitState == UnitData::Dead) return;

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: WorldContextObject is invalid or could not provide World."));
        return;
    }
	
    UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
    if (!MassSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: MassEntitySubsystem not found. Is Mass enabled?"));
        return;
    }

    FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (NavSys)
	{
		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(NewTargetLocation, NavLoc, NavMeshProjectionExtent) || IsLocationInDirtyArea(NavLoc.Location))
		{
			UE_LOG(LogTemp, Warning, TEXT("[AbilityMove] Early return: Target location %s is not on NavMesh or is in a dirty area."), *NewTargetLocation.ToString());
			return;
		}
	}

	if (!Unit->MassActorBindingComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("CorrectSetUnitMoveTargetForAbility: Unit %s has no MassActorBindingComponent."), *GetNameSafe(Unit));
		return;
	}

	FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent->GetMassEntityHandle();

	if (!EntityManager.IsEntityActive(MassEntityHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is not active."), *MassEntityHandle.DebugGetDescription());
		return;
	}
   	FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
   	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	FMassCombatStatsFragment* CombatStatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MassEntityHandle);
	
   	if (!MoveTargetFragmentPtr || !AiStatePtr)
   	{
   		UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *MassEntityHandle.DebugGetDescription());
   		return;
   	}
	
   	// DIAGNOSE (19.08.): Wer setzt ein Ziel weit ausserhalb der Karte?
   	//
   	// Gemessen: in einer von vier Partien stellen Team-2-Einheiten ~2800 Pfadanfragen auf Punkte bei
   	// X -16 000 bis -17 300 - das NavMeshBoundsVolume endet bei X=-12 455. Der Zielpunkt steckt in
   	// StateFrag.StoredLocation, und das Feld wird an ueber einem Dutzend Stellen geschrieben; welche
   	// davon es ist, war durch Lesen nicht zu entscheiden (ein Versuch traf per Teilstring sogar das
   	// falsche Feld). Der Callstack sagt es direkt - dieselbe Methode, die den Bauplatz-Deckel fand.
   	//
   	// Nur die ersten drei Faelle je Controller, sonst laeuft das Log zu.
   	if (UWorld* Welt = GetWorld())
   	{
   		if (UNavigationSystemV1* DiagNavSys = UNavigationSystemV1::GetCurrent(Welt))
   		{
   			FNavLocation Projiziert;
   			if (!DiagNavSys->ProjectPointToNavigation(NewTargetLocation, Projiziert, FVector(500.f, 500.f, 1000.f))
   				&& ZielAusserhalbZaehler < 3)
   			{
   				++ZielAusserhalbZaehler;
   				UE_LOG(LogTemp, Warning,
   					TEXT("[ZielAusserhalb] SetUnitMoveTarget auf (%.0f, %.0f, %.0f) - nicht auf dem Navigationsnetz (Fall %d)"),
   					NewTargetLocation.X, NewTargetLocation.Y, NewTargetLocation.Z, ZielAusserhalbZaehler);
   				FDebug::DumpStackTraceToLog(TEXT("[ZielAusserhalb] Aufrufer:"), ELogVerbosity::Warning);
   			}
   		}
   	}

   	AiStatePtr->StoredLocation = NewTargetLocation;
	
	bool bIsAttackingOrPausing = DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateAttackTag::StaticStruct()) || DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStatePauseTag::StaticStruct());
	bool bIsMovingWhileAttacking = CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking && bIsAttackingOrPausing;

	if (!bIsMovingWhileAttacking)
	{
		AiStatePtr->PlaceholderSignal = UnitSignals::Run;
	}
	
   	UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);
	MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;
	
   	if (!bIsMovingWhileAttacking) EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	
   	if (AttackT || (CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking))
   	{
   		if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
   	}else
   	{
   		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
   	}
	
   	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);

	if (!bIsMovingWhileAttacking)
	{
		EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
	}
	
   	//EntityManager.Defer().RemoveTag<FMassStateDeadTag>(MassEntityHandle); 
   	EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateCastingTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(MassEntityHandle);
	
   	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);

	// Inform every client to predict locally for this single unit (ability path)
	if (UWorld* PCWorld = GetWorld())
	{
		TArray<int32> IndicesArr;
		TArray<FVector> LocationsArr;
		TArray<float> SpeedsArr;
		TArray<float> RadiiArr;
		IndicesArr.Add(Unit ? Unit->UnitIndex : INDEX_NONE);
		LocationsArr.Add(NewTargetLocation);
		SpeedsArr.Add(DesiredSpeed);
		RadiiArr.Add(AcceptanceRadius);
		for (FConstPlayerControllerIterator It = PCWorld->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
			{
				PC->Client_Predict_Batch_CorrectSetUnitMoveTargets(nullptr, IndicesArr, LocationsArr, SpeedsArr, RadiiArr, AttackT, true, true);
			}
		}
	}
	
}

void ACustomControllerBase::LoadUnitsMass_Implementation(const TArray<AUnitBase*>& UnitsToLoad, AUnitBase* Transporter)
{
		if (Transporter && Transporter->IsATransporter) // Transporter->IsATransporter
		{
			// Set up start and end points for the line trace (downward direction)
			FVector Start = Transporter->GetMassActorLocation();
			
			FVector End = Start - FVector(0.f, 0.f, 10000.f); // Trace far enough downwards

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			// Ignore the transporter itself
			QueryParams.AddIgnoredActor(Transporter);

			// Perform the line trace on a suitable collision channel, e.g., ECC_Visibility or a custom one
			bool DidHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
			
			// Prepare batch arrays for mass units
			TArray<AUnitBase*> BatchUnits;
			TArray<FVector>    BatchLocations;
			TArray<float>      BatchSpeeds;
			
   for (int32 i = 0; i < UnitsToLoad.Num(); i++)
			{
				if (UnitsToLoad[i] && UnitsToLoad[i]->UnitState != UnitData::Dead && UnitsToLoad[i]->CanBeTransported)
				{
					// Bind this unit to the clicked transporter so it won't load into others en route
			
					UnitsToLoad[i]->RemoveFocusEntityTarget();
					// Calculate the distance between the selected unit and the transport unit in X/Y space only.

					FVector UnitToLoadLocation = UnitsToLoad[i]->GetMassActorLocation();

					float Distance = FVector::Dist2D(UnitToLoadLocation, Start);

					// If the unit is within 250 units, load it instantly.
					if (Distance <= Transporter->InstantLoadRange)
					{
						Transporter->LoadUnit(UnitsToLoad[i]);
					}
					else
					{
						// Otherwise, set it as ready for transport so it can move towards the transporter.
						UnitsToLoad[i]->SetRdyForTransport(true);
					}
					// Perform the line trace on a suitable collision channel, e.g., ECC_Visibility or a custom one
					if (DidHit)
					{
						// Use the hit location's Z coordinate and keep X and Y from the transporter
						FVector NewRunLocation = Start;

						
						NewRunLocation.Z = HitResult.Location.Z+50.f;
						UnitsToLoad[i]->RunLocation = NewRunLocation;
					}
					else
					{
						// Fallback: if no hit, subtract a default fly height
						UnitsToLoad[i]->RunLocation = Start;
					}
					

					bool UnitIsValid = true;
					
					if (!UnitsToLoad[i]->IsInitialized) UnitIsValid = false;
					if (!UnitsToLoad[i]->CanMove) UnitIsValid = false;
				
					if (UnitsToLoad[i]->CurrentSnapshot.AbilityClass)
					{
					
						UGameplayAbilityBase* AbilityCDO = UnitsToLoad[i]->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
						
						if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) UnitIsValid = false;
						else CancelCurrentAbility(UnitsToLoad[i]);
					}

					if (UnitsToLoad[i]->bIsMassUnit && UnitIsValid)
					{
						float Speed = UnitsToLoad[i]->Attributes->GetRunSpeed();
						// Accumulate for batched RPC instead of sending one RPC per unit
						BatchUnits.Add(UnitsToLoad[i]);
						BatchLocations.Add(UnitsToLoad[i]->RunLocation);
						BatchSpeeds.Add(Speed);
						SetUnitState_Replication(UnitsToLoad[i], 1);
					}
					else
					{
						RightClickRunUEPF(UnitsToLoad[i], UnitsToLoad[i]->RunLocation, true);
					}
				}
			}

			// Send a single batched RPC for all valid mass units gathered above
			if (BatchUnits.Num() > 0)
			{
				TArray<float> BatchRadii;
				for (AUnitBase* Unit : BatchUnits)
				{
					BatchRadii.Add(Unit->MovementAcceptanceRadius);
				}
				Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocations, BatchSpeeds, BatchRadii, false, true);
			}

			if (Transporter->GetUnitState() != UnitData::Casting)
				SetUnitState_Replication(Transporter,0);

		}else
		{
			for (int32 i = 0; i < UnitsToLoad.Num(); i++)
			{
				if (UnitsToLoad[i] && UnitsToLoad[i]->UnitState != UnitData::Dead && UnitsToLoad[i]->CanBeTransported)
				{
					UnitsToLoad[i]->SetRdyForTransport(false);
				}
			}
		}
	
}


void ACustomControllerBase::Server_SetUnitsFollowTarget_Implementation(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT)
{
	// Authority-only: schedule retries if Mass or units are not ready yet
	if (!HasAuthority())
	{
		return;
	}

	for (AUnitBase* Unit : Units)
	{
		if (Unit && Unit->IsWorker && FollowTarget)
		{
			if (FollowTarget->bIsBuilding)
			{
				ABuildingBase* Building = static_cast<ABuildingBase*>(FollowTarget);
				// Only adopt it as the drop-off if it actually accepts this worker's load.
				if (Building->IsBase && Unit->CanDeliverToBase(Building))
				{
					Unit->Base = Building;
				}
			}
		}
	}

	if (IsFollowCommandReady(Units))
	{
		ExecuteFollowCommand(Units, FollowTarget, AttackT);
	}
	else
	{
		ScheduleFollowRetry(Units, FollowTarget, AttackT, 8, 0.5f);
	}
}

bool ACustomControllerBase::IsFollowCommandReady(const TArray<AUnitBase*>& Units)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return false;
	}
	// Ensure units have Mass ready
	bool bAnyUnit = false;
	for (AUnitBase* Unit : Units)
	{
		if (!Unit) { continue; }
		bAnyUnit = true;
		if (!Unit->MassActorBindingComponent)
		{
			return false;
		}
		if (Unit->MassActorBindingComponent->bNeedsMassUnitSetup)
		{
			return false; // still setting up
		}
	}
	// If there are no valid units, consider ready (no-op)
	return true;
}

void ACustomControllerBase::ScheduleFollowRetry(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT, int32 MaxAttempts, float DelaySeconds)
{
	if (!HasAuthority()) return;
	// Save pending parameters as weak pointers
	PendingFollowUnits.Reset();
	PendingFollowUnits.Reserve(Units.Num());
	for (AUnitBase* Unit : Units)
	{
		PendingFollowUnits.Add(Unit);
	}
	PendingFollowTarget = FollowTarget;
	PendingFollowAttackT = AttackT;
	FollowRetryRemaining = MaxAttempts;

	// Clear any existing timer and set a new one
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FollowRetryTimerHandle);
		World->GetTimerManager().SetTimer(FollowRetryTimerHandle, this, &ACustomControllerBase::Retry_Server_SetUnitsFollowTarget, DelaySeconds, false);
	}
}

void ACustomControllerBase::Retry_Server_SetUnitsFollowTarget()
{
	if (!HasAuthority()) return;
	if (FollowRetryRemaining <= 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FollowRetryTimerHandle);
		}
		return;
	}
	FollowRetryRemaining--;

	// Rebuild strong array from weaks
	TArray<AUnitBase*> StrongUnits;
	StrongUnits.Reserve(PendingFollowUnits.Num());
	for (const TWeakObjectPtr<AUnitBase>& WeakUnit : PendingFollowUnits)
	{
		if (AUnitBase* U = WeakUnit.Get())
		{
			StrongUnits.Add(U);
		}
	}
	AUnitBase* StrongTarget = PendingFollowTarget.Get();

	for (AUnitBase* Unit : StrongUnits)
	{
		if (Unit && Unit->IsWorker && StrongTarget)
		{
			if (StrongTarget->bIsBuilding)
			{
				ABuildingBase* Building = static_cast<ABuildingBase*>(StrongTarget);
				// Only adopt it as the drop-off if it actually accepts this worker's load.
				if (Building->IsBase && Unit->CanDeliverToBase(Building))
				{
					Unit->Base = Building;
				}
			}
		}
	}

	if (IsFollowCommandReady(StrongUnits))
	{
		ExecuteFollowCommand(StrongUnits, StrongTarget, PendingFollowAttackT);
		// Clear timer and pending state
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FollowRetryTimerHandle);
		}
		PendingFollowUnits.Reset();
		PendingFollowTarget.Reset();
		PendingFollowAttackT = false;
		FollowRetryRemaining = 0;
	}
	else
	{
		// Schedule next attempt if any left
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FollowRetryTimerHandle, this, &ACustomControllerBase::Retry_Server_SetUnitsFollowTarget, 0.5f, false);
		}
	}
}

void ACustomControllerBase::ExecuteFollowCommand(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget, bool AttackT)
{
	UWorld* World = GetWorld();

	// Apply follow target immediately
	for (AUnitBase* Unit : Units)
	{
		if (!Unit) continue;
		
		if (FollowTarget) Unit->RemoveFocusEntityTarget();
		
		if (Unit != FollowTarget)
		{
			Unit->ApplyFollowTarget(FollowTarget);
		}
		else
		{
			Unit->ApplyFollowTarget(nullptr);
		}

		if (Unit->IsWorker && FollowTarget)
		{
			if (FollowTarget->bIsBuilding)
			{
				ABuildingBase* Building = static_cast<ABuildingBase*>(FollowTarget);
				// Only adopt it as the drop-off if it actually accepts this worker's load.
				if (Building->IsBase && Unit->CanDeliverToBase(Building))
				{
					Unit->Base = Building;
				}
			}
		}
	}

	ApplyTransportTags(Units, FollowTarget);

	if (FollowTarget)
	{
		FVector FollowLocation = FollowTarget->GetMassActorLocation();

		if (World)
		{
			FVector TraceStart = FollowLocation + FVector(0.f, 0.f, 5000.f);
			FVector TraceEnd = FollowLocation - FVector(0.f, 0.f, 5000.f);
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(FollowTarget);
			for (AUnitBase* Unit : Units)
			{
				if (Unit) QueryParams.AddIgnoredActor(Unit);
			}

			if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
			{
				FollowLocation = HitResult.Location;
			}

			// Ensure follow point is on navmesh and not a dirty area (e.g., building obstacle)
			if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
			{
				FNavLocation CenterNav;
				bool bOnNav = NavSys->ProjectPointToNavigation(FollowLocation, CenterNav, FVector(600.f, 600.f, 5000.f));
				bool bDirty = false;
				if (bOnNav)
				{
					const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
					if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
					{
						const uint32 PolyAreaID = Recast->GetPolyAreaID(CenterNav.NodeRef);
						const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
						bDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
					}
				}

				if (!bOnNav || bDirty)
				{
					// Radial search for nearest non-dirty projected point
					static const float Radii[] = {150.f, 300.f, 600.f, 900.f};
					static const int32 Slices = 12;
					bool bFound = false;
					for (float R : Radii)
					{
						for (int32 s = 0; s < Slices; ++s)
						{
							const float Angle = (2 * PI) * (float(s) / float(Slices));
							const FVector Candidate = FollowLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * R;
							FNavLocation CandNav;
							if (NavSys->ProjectPointToNavigation(Candidate, CandNav, FVector(600.f, 600.f, 5000.f)))
							{
								bool bCandDirty = false;
								if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(FNavAgentProperties())))
								{
									const uint32 AreaID = Recast->GetPolyAreaID(CandNav.NodeRef);
									const UClass* AreaClass = Recast->GetAreaClass(AreaID);
									bCandDirty = AreaClass && AreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
								}
								if (!bCandDirty)
								{
									FollowLocation = CandNav.Location;
									bFound = true;
									break;
								}
							}
						}
						if (bFound) break;
					}
				}
			}
		}

		TArray<AUnitBase*> ValidUnits;
		TArray<FVector> NewTargetLocations;
		TArray<float> DesiredSpeeds;
		ValidUnits.Reserve(Units.Num() + 1);
		NewTargetLocations.Reserve(Units.Num() + 1);
		DesiredSpeeds.Reserve(Units.Num() + 1);

		// Derive authoritative FollowOffset from Proxy bounds > Capsule > Bbox
		float FollowOffset = 300.f;
		if (FollowTarget)
		{
			bool bFoundBounds = false;
			if (FollowTarget->NavObstacleProxy)
			{
				const FBox ProxyBox = FollowTarget->NavObstacleProxy->GetComponentsBoundingBox(true);
				if (ProxyBox.IsValid)
				{
					const FVector ProxyExt = ProxyBox.GetExtent();
					FollowOffset = FMath::Max(ProxyExt.X, ProxyExt.Y) + 50.f; // Buffer
					bFoundBounds = true;
				}
			}
			
			if (!bFoundBounds)
			{
				if (UCapsuleComponent* Cap = FollowTarget->FindComponentByClass<UCapsuleComponent>())
				{
					FollowOffset = Cap->GetScaledCapsuleRadius() * 2.f + 10.f;
				}
				else
				{
					const FBox B = FollowTarget->GetComponentsBoundingBox(true);
					if (B.IsValid)
					{
						const FVector Ext = B.GetExtent();
						FollowOffset = FMath::Max(Ext.X, Ext.Y) * 2.f + 10.f;
					}
				}
			}
		}

		// Compute group center of followers to determine approach direction
		FVector BldCenter = FollowTarget ? FollowTarget->GetMassActorLocation() : FollowLocation;
		FVector GroupCenter = FVector::ZeroVector;
		int32 GroupCount = 0;
		for (AUnitBase* U : Units)
		{
			if (!U || U == FollowTarget) continue;
			GroupCenter += U->GetActorLocation();
			++GroupCount;
		}
		if (GroupCount > 0)
		{
			GroupCenter /= float(GroupCount);
		}
		else
		{
			GroupCenter = FollowLocation;
		}

		FVector DirBG = (GroupCenter - BldCenter);
		DirBG.Z = 0.f;
		DirBG = DirBG.GetSafeNormal();
		if (DirBG.IsNearlyZero())
		{
			DirBG = FVector(1.f, 0.f, 0.f);
		}
		const float GroundZ = FollowLocation.Z;
		FollowLocation = BldCenter + DirBG * FollowOffset;
		FollowLocation.Z = GroundZ;

		for (AUnitBase* Unit : Units)
		{
			if (!Unit || Unit == FollowTarget) continue;
			// FIX: nur aus dem Batch-Move ausschliessen, wenn der Worker tatsaechlich reparieren WIRD.
			// Muss dieselbe vollstaendige Bedingung wie der GoToRepair-Trigger in
			// AMassUnitBase::ApplyFollowTargetForUnit (Health < MaxHealth) verwenden. Sonst wurde der Worker
			// auch bei VOLLER HP uebersprungen (kein MoveTarget) waehrend der State auf Run/GoToBase ging ->
			// er blieb stehen statt zu folgen/spaeter eingeladen zu werden.
			const bool bWantsRepair = (Unit->IsWorker && Unit->CanRepair && FollowTarget && FollowTarget->CanBeRepaired
				&& FollowTarget->Attributes && FollowTarget->Attributes->GetHealth() < FollowTarget->Attributes->GetMaxHealth());
			if (bWantsRepair)
			{
				continue;
			}
			ValidUnits.Add(Unit);
			NewTargetLocations.Add(FollowLocation);
			float Speed = 300.f;
			if (Unit->Attributes)
			{
				Speed = Unit->Attributes->GetRunSpeed();
			}
			DesiredSpeeds.Add(Speed);
		}

		if (ValidUnits.Num() > 0)
		{
			TArray<float> BatchRadii;
			for (AUnitBase* Unit : ValidUnits)
			{
				BatchRadii.Add(Unit->MovementAcceptanceRadius);
			}
   Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), ValidUnits, NewTargetLocations, DesiredSpeeds, BatchRadii, AttackT, true, false);
		}
	}
}

void ACustomControllerBase::ApplyTransportTags(const TArray<AUnitBase*>& Units, AUnitBase* FollowTarget)
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* MassSubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = MassSubsystem ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	if (!EntityManager) return;

	ATransportUnit* Transporter = Cast<ATransportUnit>(FollowTarget);
	bool bTargetIsTransporter = Transporter && Transporter->IsATransporter;
	bool bAnyUnitTagged = false;

	for (AUnitBase* Unit : Units)
	{
		if (!Unit || !Unit->MassActorBindingComponent) continue;

	
		const FMassEntityHandle UnitHandle = Unit->MassActorBindingComponent->GetMassEntityHandle();
			/*if (EntityManager->IsEntityValid(UnitHandle))
			{
				// Clear any existing transport tag as we are issuing a new command
				EntityManager->RemoveTagFromEntity(UnitHandle, FMassTransportProcessorActiveTag::StaticStruct());
				//EntityManager->Defer().RemoveTag<FMassTransportProcessorActiveTag>(UnitHandle);
			}*/

		bool bShouldApplyToUnit = false;
		if (bTargetIsTransporter && Unit->CanBeTransported)
		{
			// Check space and transport IDs
			if ((Transporter->CurrentUnitsLoaded + Unit->UnitSpaceNeeded) <= Transporter->MaxTransportUnits)
			{
				if (Transporter->TransportId == 0 || Unit->TransportId == 0 || Transporter->TransportId == Unit->TransportId)
				{
					bShouldApplyToUnit = true;
				}
			}
		}

		// Repair-specific logic: only allow loading if target is full health OR unit is already in repair state.
		// FIX: nur unterdruecken, wenn das Ziel auch WIRKLICH reparierbar ist (CanBeRepaired). Ohne diese
		// Bedingung wurde der Transport-Tag bei einem NICHT reparierbaren, aber beschaedigten Gebaeude
		// faelschlich entfernt -> der Worker folgte, wurde aber nie eingeladen. Der bIsAlreadyRepairing-Zweig
		// bleibt unveraendert -> Zwei-Klick-Flow (erst reparieren, beim Klick waehrend des Reparierens einladen)
		// funktioniert weiter.
		if (Unit->CanRepair && FollowTarget && FollowTarget->CanBeRepaired)
		{
			const bool bFollowTargetHasMaxHealth = FollowTarget->Attributes && FollowTarget->Attributes->GetHealth() >= FollowTarget->Attributes->GetMaxHealth();
			const bool bIsAlreadyRepairing = DoesEntityHaveTag(*EntityManager, UnitHandle, FMassStateRepairTag::StaticStruct());

			if (!bFollowTargetHasMaxHealth && !bIsAlreadyRepairing)
			{
				bShouldApplyToUnit = false;
			}
		}

		if (bShouldApplyToUnit)
		{
			if (EntityManager->IsEntityValid(UnitHandle))
			{
				// FIX: Use Defer().AddTag<T> instead of synchronous AddTagToEntity
				EntityManager->Defer().AddTag<FMassTransportProcessorActiveTag>(UnitHandle);
				bAnyUnitTagged = true;
			}
		}
	}

	if (bAnyUnitTagged && bTargetIsTransporter)
	{
		const FMassEntityHandle TransporterHandle = Transporter->MassActorBindingComponent->GetMassEntityHandle();
		if (EntityManager->IsEntityValid(TransporterHandle))
		{
			EntityManager->Defer().AddTag<FMassTransportProcessorActiveTag>(TransporterHandle);
			if (FMassTransportFragment* TransportFrag = EntityManager->GetFragmentDataPtr<FMassTransportFragment>(TransporterHandle))
			{
				TransportFrag->DeactivationTimer = 0.f;
			}
		}
	}
}

AUnitBase* ACustomControllerBase::GetUnitFromHitResult(const FHitResult& Hit) const
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor) return nullptr;

	// 1. Direct check (for units / actors)
	if (AUnitBase* Unit = Cast<AUnitBase>(HitActor)) return Unit;

	// 2. ISM mapping via VisualManager (for mass units)
	if (UInstancedStaticMeshComponent* HitISM = Cast<UInstancedStaticMeshComponent>(Hit.Component.Get()))
	{
		if (UUnitVisualManager* VisualManager = GetWorld()->GetSubsystem<UUnitVisualManager>())
		{
			AMassUnitBase* MUB = VisualManager->GetUnitFromInstance(HitISM, Hit.Item);
			return Cast<AUnitBase>(MUB);
		}
	}

	// Build sites: hitting a WorkArea resolves to its construction unit, so attack/focus/follow act on it.
	if (AWorkArea* WorkArea = Cast<AWorkArea>(HitActor))
	{
		return WorkArea->ConstructionUnit;
	}

	return nullptr;
}

bool ACustomControllerBase::GetSelectableHitUnderCursor(FHitResult& OutHit) const
{
	// Die gewohnte Pawn-Spur zuerst, damit OutHit auch im Misserfolgsfall unveraendert der
	// bisherige Treffer bleibt (in der Regel der Bodenpunkt) und bodenzielende Faehigkeiten
	// sich nicht anders verhalten als vorher.
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, OutHit);

	// ZUERST die Einheit, die UMassUnitHoverProcessor ohnehin schon markiert hat.
	//
	// Er prueft den Mausstrahl mit 10 Hz geometrisch gegen Kapsel bzw. Box jeder Mass-Einheit -
	// ohne jede Kollision, und deshalb auch dann, wenn die sichtbare ISM eines Gebaeudes gar keine
	// hat. Genau das war der Fall, an dem Extension_D haengenblieb: die Kapsel deckt von 407x475 uu
	// Mesh nur 250 uu Durchmesser ab, und die ISM war fuer jede Spur unsichtbar.
	//
	// Der Umweg ueber zusaetzliche Kollision waere in einem RTS mit hunderten Einheiten teuer
	// gewesen - diese Abfrage kostet einen Zeigervergleich.
	if (AUnitBase* Markierte = HoveredUnit.Get())
	{
		if (IsValid(Markierte))
		{
			FHitResult HoverTreffer;
			HoverTreffer.HitObjectHandle = FActorInstanceHandle(Markierte);
			HoverTreffer.Location = Markierte->GetActorLocation();
			HoverTreffer.ImpactPoint = HoverTreffer.Location;
			HoverTreffer.bBlockingHit = true;
			// Den Bodenpunkt der Pawn-Spur behalten, falls er gesetzt war: Faehigkeiten, die auf
			// eine Stelle zielen, lesen TraceStart/TraceEnd und sollen dieselbe Stelle bekommen.
			HoverTreffer.TraceStart = OutHit.TraceStart;
			HoverTreffer.TraceEnd = OutHit.TraceEnd;

			OutHit = HoverTreffer;
			return true;
		}
	}

	if (GetUnitFromHitResult(OutHit))
	{
		return true;
	}

	FVector Start, Richtung;
	if (!DeprojectMousePositionToWorld(Start, Richtung) || !GetWorld())
	{
		return false;
	}
	const FVector Ende = Start + Richtung * 100000.f;

	// Drei Kanaele, weil im Projekt auf dreien etwas Anklickbares liegt:
	//   ECC_Pawn         - Einheiten (AUnitBase-Kapsel auf ECR_Block)
	//   ECC_WorldDynamic - Gebaeudekapseln (ECC_Pawn steht dort bewusst auf Ignore, damit
	//                      Einheiten hindurchlaufen) und die gepoolten Gebaeude-ISMs
	//   ECC_Visibility   - Bauplaetze (AWorkArea blockt ausschliesslich diesen Kanal)
	static const ECollisionChannel Kanaele[] =
		{ ECC_Pawn, ECC_WorldDynamic, ECC_Visibility };

	for (const ECollisionChannel Kanal : Kanaele)
	{
		FCollisionQueryParams Params(TEXT("SelectableHitUnderCursor"), /*bTraceComplex*/ false);

		// Was keine Einheit ergibt, wird ignoriert und die Spur laeuft weiter, statt den Klick zu
		// verbrauchen. Ohne das schluckt der erste beliebige Blocker die Auswahl - gemeldet fuer
		// BP_StoryTriggerActor_Survive_AH, dessen Box ueber einem Gebaeude steht: die Box loest auf
		// keine Einheit auf, lag aber vor dem Gebaeude, und damit war das Gebaeude unerreichbar.
		// Die Grenze von acht Versuchen deckelt den Aufwand; ein einzelner Klick durchdringt damit
		// bis zu sieben nicht auswaehlbare Aktoren.
		for (int32 Versuch = 0; Versuch < 8; ++Versuch)
		{
			FHitResult Treffer;
			if (!GetWorld()->LineTraceSingleByChannel(Treffer, Start, Ende, Kanal, Params))
			{
				break; // nichts mehr auf diesem Kanal
			}
			if (GetUnitFromHitResult(Treffer))
			{
				OutHit = Treffer;
				return true;
			}
			AActor* Blocker = Treffer.GetActor();
			if (!Blocker)
			{
				break;
			}
			Params.AddIgnoredActor(Blocker);
		}
	}

	// Nichts Anklickbares - OutHit bleibt die Pawn-Spur.
	return false;
}

bool ACustomControllerBase::TryHandleFollowOnRightClick(const FHitResult& HitPawn)
{
	// Denselben Rueckfall benutzen wie die Auswahl per Linksklick (siehe
	// GetSelectableHitUnderCursor): zuerst die Einheit, die UMassUnitHoverProcessor ohnehin schon
	// markiert hat.
	//
	// WOFUER: der Prozessor prueft den Mausstrahl mit 10 Hz GEOMETRISCH gegen Kapsel bzw. Box jeder
	// Mass-Einheit - ganz ohne Kollision. Wer hier nur die Pawn-Spur auswertet, braucht auf jeder
	// Einheit eine Kollisionsform, nur damit ein Rechtsklick sie trifft. Mit dem Rueckfall koennen
	// die Einheiten-Parents auf NoCollision stehen; Kollision braucht dann nur noch, wer sie
	// wirklich benutzt - etwa eine Heldeneinheit, die einen MapSwitchActor ausloesen soll.
	//
	// Der Bodenpunkt der urspruenglichen Spur bleibt erhalten: bodenzielende Zweige weiter unten
	// lesen Location, und die soll bei einem Treffer auf die Einheit zeigen, sonst auf den Boden.
	FHitResult ResolvedHit = HitPawn;
	if (AUnitBase* HoveredMarked = HoveredUnit.Get())
	{
		if (IsValid(HoveredMarked))
		{
			ResolvedHit.HitObjectHandle = FActorInstanceHandle(HoveredMarked);
			ResolvedHit.Location = HoveredMarked->GetActorLocation();
			ResolvedHit.ImpactPoint = ResolvedHit.Location;
			ResolvedHit.bBlockingHit = true;
		}
	}


	// If we clicked on a unit while having a selection, assign follow or attack and early return
	if (SelectedUnits.Num() > 0 && ResolvedHit.bBlockingHit)
	{
		if (!Cast<AConstructionUnit>(ResolvedHit.GetActor()))
		{
			if (AUnitBase* HitUnit = GetUnitFromHitResult(ResolvedHit))
			{
				const bool bFriendly = (HitUnit->TeamId == SelectableTeamId);
				if (bFriendly)
				{
					Server_SetUnitsFollowTarget(SelectedUnits, HitUnit);

						// #2 Local instant follow prediction (client): set FollowUnit now so SyncAITarget writes
						// FriendlyTargetEntity THIS frame instead of waiting ~0.5s for FollowUnit to replicate, and
						// drop any lingering move prediction so the unit doesn't keep heading to its old target during
						// the gap (the "server follows, client goes elsewhere" symptom). Replicated FollowUnit +
						// SyncAITarget stay the authoritative confirmation (Fix #1, already present).
						if (!HasAuthority())
						{
							if (UMassEntitySubsystem* MassSub = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr)
							{
								FMassEntityManager& EM = MassSub->GetMutableEntityManager();
								const FMassEntityHandle TgtH = HitUnit->MassActorBindingComponent ? HitUnit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
								for (AUnitBase* U : SelectedUnits)
								{
									if (!U || U == HitUnit || !U->MassActorBindingComponent) continue;
									U->FollowUnit = HitUnit;
									const FMassEntityHandle H = U->MassActorBindingComponent->GetMassEntityHandle();
									if (!EM.IsEntityValid(H)) continue;
									if (FMassAITargetFragment* AIT = EM.GetFragmentDataPtr<FMassAITargetFragment>(H))
									{
										if (EM.IsEntityActive(TgtH))
										{
											AIT->FriendlyTargetEntity = TgtH;
											AIT->LastKnownFriendlyLocation = HitUnit->GetMassActorLocation();
										}
									}
									if (FMassClientPredictionFragment* Pred = EM.GetFragmentDataPtr<FMassClientPredictionFragment>(H))
									{
										//Pred->bHasData = false; // drop stale move prediction so follow takes over
									}
								}
							}
						}
					return true;
				}
				else
				{
					// If it's an enemy unit, issue an attack command (chase/focus logic)
					TArray<FVector> Locations;
					for (int32 i = 0; i < SelectedUnits.Num(); ++i)
					{
						Locations.Add(ResolvedHit.Location);
					}
					LeftClickAttackMass(SelectedUnits, Locations, false, HitUnit);

					// Play attack sound if available
					if (AttackSound)
					{
						UGameplayStatics::PlaySound2D(this, AttackSound, GetSoundMultiplier());
					}

					return true;
				}
			}
		}
	}
	
	if (SelectedUnits.Num() > 0)
	{
		Server_SetUnitsFollowTarget(SelectedUnits, nullptr);
	}
	
	return false;
}

bool ACustomControllerBase::TryCancelActiveAbilities()
{
	bool bAnyAbilityCanceled = false;
	for (AUnitBase* Unit : SelectedUnits)
	{
		if (Unit && Unit->IsAnyAbilityActive() && Unit->CurrentSnapshot.AbilityClass)
		{
			UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
			if (AbilityCDO && AbilityCDO->AbilityCanBeCanceled)
			{
				ABuildingBase* BuildingBase = Cast<ABuildingBase>(Unit);
				// Cancel if CanMove is true, or if CanMove is false, or if it's a building without a waypoint.
				if (!BuildingBase || (BuildingBase && !BuildingBase->HasWaypoint))
				{
					// Local correction on client
					if (!HasAuthority() && Unit->MassActorBindingComponent)
					{
						FMassEntityHandle Entity = Unit->MassActorBindingComponent->GetMassEntityHandle();
						if (UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
						{
							ClearMassStateTagsLocally(Entity, MassSubsystem->GetMutableEntityManager());
						}
					}

					CancelCurrentAbility(Unit);
					bAnyAbilityCanceled = true;
				}
			}
		}
	}
	return bAnyAbilityCanceled;
}

void ACustomControllerBase::RightClickPressedMass()
{
	Batch_RemoveRotateToMouseTag();

	if (TryCancelActiveAbilities())
	{
		return;
	}
	
	if (SwapAttackMove && AttackToggled)
	{
		// Swapped layout: attack-move lives on the RIGHT button. Arm the line drag here, before
		// HandleAttackMovePressed clears AttackToggled, so the release knows this was attack-move.
		if (CanStartFormationLineDrag())
		{
			FHitResult DragHit;
			GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, DragHit);
			if (DragHit.bBlockingHit)
			{
				BeginFormationLineDrag(DragHit.Location, /*bAttackMove=*/true, /*bFromRightMouse=*/true);
			}
		}

		HandleAttackMovePressed();
		AttackToggled = false;
		return;
	}
	AttackToggled = false;

	FHitResult HitPawn;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitPawn);

	// Zielt der Klick auf einen Bauplatz, hat der Folgen-Zweig hier nichts zu suchen.
	//
	// Die beiden Spuren laufen auf verschiedenen Kanaelen: Folgen auf ECC_Pawn, der Bauplatz auf
	// ECC_Visibility. Die ConstructionUnit blockt bewusst NUR Visibility (Einheiten sollen durch sie
	// hindurchlaufen koennen), also geht die Pawn-Spur durch sie hindurch und trifft den Arbeiter,
	// der dahinter schon baut. Der ist verbuendet -> Folgen-Befehl -> return, und CheckClickOnWorkArea
	// wurde nie erreicht. Die Ausnahme fuer die ConstructionUnit weiter unten greift nicht, weil sie
	// den Pawn-Treffer prueft und der eben nicht mehr die ConstructionUnit ist.
	bool bZieltAufBauplatz = false;
	{
		FHitResult SichtTreffer;
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, SichtTreffer);
		AActor* SichtAktor = SichtTreffer.GetActor();

		const AWorkArea* Getroffen = Cast<AWorkArea>(SichtAktor);
		if (!Getroffen)
		{
			if (const AConstructionUnit* CU = Cast<AConstructionUnit>(SichtAktor))
			{
				Getroffen = CU->WorkArea;
			}
		}
		bZieltAufBauplatz = (Getroffen != nullptr) && !Getroffen->IsNoBuildZone;
	}

	if (!bZieltAufBauplatz && TryHandleFollowOnRightClick(HitPawn))
	{
		return;
	}
	
	FHitResult Hit;
	if (!SelectedUnits.Num() || !SelectedUnits[0] || !SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
		if (!CheckClickOnWorkArea(Hit))
		{
			// Arm the move line drag. The press-time order below still runs, so a plain click is
			// unchanged; a real drag simply re-targets the same units on release.
			if (Hit.bBlockingHit && CanStartFormationLineDrag())
			{
				BeginFormationLineDrag(Hit.Location, /*bAttackMove=*/false, /*bFromRightMouse=*/true);
			}

			RunUnitsAndSetWaypointsMass(Hit);
		}
	}

	if (SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		DestroyDraggedArea(SelectedUnits[0]);
	}
}

void ACustomControllerBase::RightClickPressedMassMinimap(const FVector& GroundLocation)
{
	// Ignore pawn hits and follow logic for minimap
	AttackToggled = false;

	if (!SelectedUnits.Num())
	{
		return;
	}

	if (TryCancelActiveAbilities())
	{
		return;
	}

	// If we are dragging a work area, destroy it as in normal right-click behavior
	if (SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		DestroyDraggedArea(SelectedUnits[0]);
		return;
	}

	// Synthesize a hit result using minimap ground location and reuse existing logic
	FHitResult SynthHit;
	SynthHit.Location = GroundLocation;
	RunUnitsAndSetWaypointsMass(SynthHit);
}

void ACustomControllerBase::LeftClickPressedMassMinimapAttack(const FVector& GroundLocation)
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());

	// Mimic LeftClickPressedMass attack branch using GroundLocation from minimap
	if (!AttackToggled)
	{
		// Enforce attack mode for this call as per requirement
		AttackToggled = true;
	}

	int32 NumUnits = SelectedUnits.Num();
	if (NumUnits == 0)
	{
		AttackToggled = false;
		return;
	}

	// Consistency: Sort units by radius so that formation validation and assignment match Move logic
	TArray<AUnitBase*> UnitsToProcess = SelectedUnits;
	UnitsToProcess.Sort([](const AUnitBase& A, const AUnitBase& B) {
		float RA = 50.0f;
		if (A.GetCapsuleComponent()) RA = A.GetCapsuleComponent()->GetScaledCapsuleRadius();
		float RB = 50.0f;
		if (B.GetCapsuleComponent()) RB = B.GetCapsuleComponent()->GetScaledCapsuleRadius();
		return RA > RB;
	});

	FVector AdjustedLocation = GroundLocation;
	TArray<FVector> Offsets;
	float UsedSpacing;
	ValidateAndAdjustGridLocation(UnitsToProcess, AdjustedLocation, Offsets, UsedSpacing);

	AWaypoint* BWaypoint = nullptr;
	bool PlayWaypointSound = false;
	bool PlayAttackSound   = false;

	TArray<AUnitBase*> MassUnits;
	TArray<FVector>    MassLocations;
	TArray<AUnitBase*> BuildingUnits;
	TArray<FVector>    BuildingLocs;
	for (int32 i = 0; i < NumUnits; ++i)
	{
		AUnitBase* U = UnitsToProcess[i];
		if (U == nullptr || U == CameraUnitWithTag) continue;

		// OPTIMIZATION: Cast once per iteration using the flag
		ABuildingBase* B = U->bIsBuilding ? static_cast<ABuildingBase*>(U) : nullptr;
		// Buildings and construction sites (rally-waypoint owners) use the exact click location.
		FVector RunLocation = (U->bIsBuilding || U->bIsConstructionUnit) ? GroundLocation : AdjustedLocation + Offsets[i];

		bool bNavMod;
		RunLocation = TraceRunLocation(RunLocation, bNavMod);
		if (bNavMod) continue; //  || IsLocationInDirtyArea(RunLocation)

		bool bSuccess = false;
		SetBuildingWaypoint(RunLocation, U, BWaypoint, PlayWaypointSound, bSuccess);
		if (bSuccess)
		{
			// waypoint placed
			BuildingUnits.Add(U);
			BuildingLocs.Add(RunLocation);
		}
		else
		{
			DrawCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
			if (U->bIsMassUnit)
			{
				if (IsShiftPressed)
				{
					// Queue into path fragment; start movement only if not already running
					if (UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
					{
						FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
						const FMassEntityHandle EHandle = U->MassActorBindingComponent ? U->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
						if (EntityManager.IsEntityValid(EHandle))
						{
							if (FMassUnitPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FMassUnitPathFragment>(EHandle))
							{
								if (PathFrag->Waypoints.Num() < 10)
								{
									PathFrag->Waypoints.Add(RunLocation);
								}
							}
						}
					}
					if (U->GetUnitState() != UnitData::Run)
					{
						MassUnits.Add(U);
						MassLocations.Add(RunLocation);
					}
				}
				else
				{
					MassUnits.Add(U);
					MassLocations.Add(RunLocation);
				}
			}
			else
			{
				LeftClickAttack(U, RunLocation);
			}

			PlayAttackSound = true;
		}
	}

	if (BuildingUnits.Num() > 0)
	{
		Server_Batch_SetBuildingWaypoints(BuildingLocs, BuildingUnits);
	}

	if (MassUnits.Num() > 0)
	{
		LeftClickAttackMass(MassUnits, MassLocations, true, nullptr);
	}

	// Reset toggle after issuing attack move, as in normal LeftClickPressedMass
	AttackToggled = false;

	if (WaypointSound && PlayWaypointSound)
	{
		UGameplayStatics::PlaySound2D(this, WaypointSound, GetSoundMultiplier());
	}
	if (AttackSound && PlayAttackSound)
	{
		UGameplayStatics::PlaySound2D(this, AttackSound, GetSoundMultiplier());
	}
}

// Helper to get a unit's world location (actor or ISM)
FVector ACustomControllerBase::GetUnitWorldLocation(const AUnitBase* Unit) const
{
    if (!Unit) return FVector::ZeroVector;

	return Unit->GetMassActorLocation();
}

namespace
{
    // Formation-local space for the directional shapes: +X points the way the group is heading,
    // +Y is to its right. A zero Forward means "leave it world-axis aligned".
    FVector OrientFormationOffset(const FVector& Local, const FVector& Forward)
    {
        if (Forward.IsNearlyZero())
        {
            return Local;
        }
        const FRotator YawOnly(0.f, Forward.Rotation().Yaw, 0.f);
        return YawOnly.RotateVector(Local);
    }

    // Concentric rings (bHalf = false) or concentric 180-degree arcs (bHalf = true), filled from
    // the centre outwards. Radii must be sorted descending: each ring is sized from the largest
    // unit still unplaced, which is the conservative choice against overlap.
    TArray<FVector> BuildRingFormationOffsets(const TArray<float>& Radii, float Spacing, bool bHalf, TArray<float>& OutCapacities)
    {
        const int32 NumUnits = Radii.Num();
        TArray<FVector> Local;
        Local.Reserve(NumUnits);
        OutCapacities.Reset();
        OutCapacities.Reserve(NumUnits);

        const float Sweep = bHalf ? PI : 2.f * PI;

        int32 Index = 0;
        float PrevRingRadius = 0.f;
        float PrevUnitRadius = 0.f;
        bool bFirstRing = true;

        while (Index < NumUnits)
        {
            const float UnitRadius = FMath::Max(Radii[Index], 1.f);

            // Ring N sits far enough out that its units clear ring N-1's units.
            const float RingRadius = bFirstRing ? 0.f : (PrevRingRadius + PrevUnitRadius + UnitRadius + Spacing);
            const float ArcStep = 2.f * UnitRadius + Spacing; // arc length one unit occupies

            int32 Capacity;
            if (RingRadius <= KINDA_SMALL_NUMBER)
            {
                Capacity = 1; // the centre point holds exactly one unit
            }
            else
            {
                // A closed ring wraps around, an open arc does not: K units on an arc only need
                // K-1 gaps, so it fits one more than a full ring of the same length.
                Capacity = FMath::FloorToInt((Sweep * RingRadius) / ArcStep) + (bHalf ? 1 : 0);
            }
            Capacity = FMath::Max(Capacity, 1);

            // A partly filled outermost ring stays spread over the whole sweep. We cannot pull it
            // inwards - RingRadius is the no-overlap constraint from the ring below it.
            const int32 Count = FMath::Min(Capacity, NumUnits - Index);

            for (int32 Slot = 0; Slot < Count; ++Slot)
            {
                float Angle;
                if (Count == 1)
                {
                    Angle = 0.f;
                }
                else if (bHalf)
                {
                    // Inclusive sweep, so the two arc ends land on the flat side of the half disc.
                    Angle = -Sweep * 0.5f + Sweep * (static_cast<float>(Slot) / static_cast<float>(Count - 1));
                }
                else
                {
                    // Exclusive, otherwise the first and last slot of a full ring would coincide.
                    Angle = Sweep * (static_cast<float>(Slot) / static_cast<float>(Count));
                }

                Local.Add(FVector(RingRadius * FMath::Cos(Angle), RingRadius * FMath::Sin(Angle), 0.f));
                // Every slot on this ring was spaced for UnitRadius. Radii arrives sorted
                // descending, so UnitRadius >= the radius of any unit at index >= Index, which
                // guarantees the identity assignment is always feasible for the solver.
                OutCapacities.Add(UnitRadius);
            }

            PrevRingRadius = RingRadius;
            PrevUnitRadius = UnitRadius;
            bFirstRing = false;
            Index += Count; // Count is always >= 1, so this terminates
        }

        return Local;
    }

    // Wedge with rows of 1, 2, 3, ... units, apex at local +X.
    TArray<FVector> BuildWedgeFormationOffsets(const TArray<float>& Radii, float Spacing, TArray<float>& OutCapacities)
    {
        const int32 NumUnits = Radii.Num();
        TArray<FVector> Local;
        Local.Reserve(NumUnits);
        OutCapacities.Reset();
        OutCapacities.Reserve(NumUnits);

        int32 Index = 0;
        int32 Row = 0;
        float Depth = 0.f; // distance behind the apex
        float PrevRowRadius = 0.f;

        while (Index < NumUnits)
        {
            const int32 Count = FMath::Min(Row + 1, NumUnits - Index);

            float RowRadius = 0.f;
            for (int32 Slot = 0; Slot < Count; ++Slot)
            {
                RowRadius = FMath::Max(RowRadius, Radii[Index + Slot]);
            }
            RowRadius = FMath::Max(RowRadius, 1.f);

            if (Row > 0)
            {
                Depth += PrevRowRadius + RowRadius + Spacing;
            }

            const float LateralStep = 2.f * RowRadius + Spacing;
            const float FirstLateral = -LateralStep * 0.5f * static_cast<float>(Count - 1);

            for (int32 Slot = 0; Slot < Count; ++Slot)
            {
                Local.Add(FVector(-Depth, FirstLateral + LateralStep * static_cast<float>(Slot), 0.f));
                OutCapacities.Add(RowRadius);
            }

            PrevRowRadius = RowRadius;
            Index += Count; // Count is always >= 1, so this terminates
            ++Row;
        }

        return Local;
    }

    // Shifts the layout so its centroid sits on the click point, matching how the grid path
    // subtracts its TrueCenter.
    void CenterFormationOffsets(TArray<FVector>& Local)
    {
        if (Local.Num() == 0)
        {
            return;
        }

        FVector Centroid = FVector::ZeroVector;
        for (const FVector& Offset : Local)
        {
            Centroid += Offset;
        }
        Centroid /= static_cast<float>(Local.Num());

        for (FVector& Offset : Local)
        {
            Offset -= Centroid;
        }
    }
}

FVector ACustomControllerBase::ComputeApproachDirection(const TArray<AUnitBase*>& Units, const FVector& TargetCenter) const
{
    FVector Centroid = FVector::ZeroVector;
    int32 Count = 0;
    for (const AUnitBase* Unit : Units)
    {
        if (!Unit) continue;
        Centroid += GetUnitWorldLocation(Unit);
        ++Count;
    }

    if (Count == 0)
    {
        return FVector::ForwardVector;
    }
    Centroid /= static_cast<float>(Count);

    FVector Direction = TargetCenter - Centroid;
    Direction.Z = 0.f;

    // Clicking on top of the group gives no usable heading. The offsets are stateless, so there is
    // no previous orientation to keep - fall back to world +X.
    return Direction.IsNearlyZero(1.f) ? FVector::ForwardVector : Direction.GetSafeNormal();
}

TArray<FVector> ACustomControllerBase::ComputeSlotOffsets(const TArray<AUnitBase*>& Units, float Spacing) const
{
    return ComputeSlotOffsetsDirectional(Units, Spacing, FVector::ZeroVector);
}

TArray<FVector> ACustomControllerBase::ComputeSlotOffsetsDirectional(const TArray<AUnitBase*>& Units, float Spacing, const FVector& Forward, TArray<float>* OutSlotCapacities) const
{
    if (OutSlotCapacities)
    {
        OutSlotCapacities->Reset();
    }

    int32 NumUnits = Units.Num();
    if (NumUnits == 0) return TArray<FVector>();

	float ActualSpacing = (Spacing < 0.f) ? GridSpacing : Spacing;

    // 1. Collect radii for all units
    TArray<float> Radii;
    Radii.Reserve(NumUnits);
    for (const AUnitBase* Unit : Units)
    {
        float R = 50.0f; // Default fallback radius
        if (Unit && Unit->GetCapsuleComponent())
        {
            R = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius()*GridCapsuleMultiplier;
        }
        Radii.Add(R);
    }

    // 1b. Ring- and wedge-based shapes do not use the row/column grid at all.
    if (!IsGridBasedFormationShape(GridFormationShape))
    {
        TArray<FVector> Local;
        TArray<float> Capacities;
        switch (GridFormationShape)
        {
        case EGridShape::Circle:
            Local = BuildRingFormationOffsets(Radii, ActualSpacing, /*bHalf=*/false, Capacities);
            break;
        case EGridShape::HalfCircle:
            Local = BuildRingFormationOffsets(Radii, ActualSpacing, /*bHalf=*/true, Capacities);
            break;
        case EGridShape::Triangle:
        default:
            Local = BuildWedgeFormationOffsets(Radii, ActualSpacing, Capacities);
            break;
        }

        CenterFormationOffsets(Local);
        for (FVector& Offset : Local)
        {
            Offset = OrientFormationOffset(Offset, Forward);
        }

        if (OutSlotCapacities)
        {
            // Radii here carry GridCapsuleMultiplier, but BuildCostMatrix compares against raw
            // capsule radii. Divide it back out so the two are on the same scale.
            const float InvMultiplier = 1.f / FMath::Max(GridCapsuleMultiplier, KINDA_SMALL_NUMBER);
            OutSlotCapacities->Reserve(Capacities.Num());
            for (float Capacity : Capacities)
            {
                OutSlotCapacities->Add(Capacity * InvMultiplier);
            }
        }
        return Local;
    }

    // 2. Determine grid dimensions
    int32 GridSize = ComputeGridSize(NumUnits);
    int32 NumRows = FMath::CeilToInt((float)NumUnits / (float)GridSize);

    // 3. Compute max radius per row and column to ensure no overlaps in a non-uniform grid
    TArray<float> MaxR_Col; MaxR_Col.Init(0.0f, GridSize);
    TArray<float> MaxR_Row; MaxR_Row.Init(0.0f, NumRows);

    for (int32 i = 0; i < NumUnits; ++i)
    {
        int32 Row = i / GridSize;
        int32 Col = i % GridSize;
        float R = Radii[i];
        MaxR_Col[Col] = FMath::Max(MaxR_Col[Col], R);
        MaxR_Row[Row] = FMath::Max(MaxR_Row[Row], R);
    }

    // 4. Calculate X and Y positions for each column and row
    TArray<float> XPositions; XPositions.Init(0.0f, GridSize);
    TArray<float> YPositions; YPositions.Init(0.0f, NumRows);

    // Start with first column/row at 0. Next positions are previous + radii + spacing
    for (int32 c = 1; c < GridSize; ++c)
    {
        XPositions[c] = XPositions[c - 1] + MaxR_Col[c - 1] + MaxR_Col[c] + ActualSpacing;
    }

    for (int32 r = 1; r < NumRows; ++r)
    {
        YPositions[r] = YPositions[r - 1] + MaxR_Row[r - 1] + MaxR_Row[r] + ActualSpacing;
    }

    // 5. Center the grid including unit widths
    float LeftEdge = XPositions[0] - MaxR_Col[0];
    float RightEdge = XPositions[GridSize - 1] + MaxR_Col[GridSize - 1];
    float TopEdge = YPositions[0] - MaxR_Row[0];
    float BottomEdge = YPositions[NumRows - 1] + MaxR_Row[NumRows - 1];
    // 6. Generate offsets
    // Staggered shifts every odd row sideways by half a column pitch. The columns are
    // non-uniform (they are sized from the widest unit in each), so use the average pitch.
    const float StaggerShift = (GridFormationShape == EGridShape::Staggered && GridSize > 1)
        ? ((XPositions[GridSize - 1] - XPositions[0]) / static_cast<float>(GridSize - 1)) * 0.5f
        : 0.f;

    // Fold half the shift into the centre. Only odd rows move right, so without this the whole
    // group's centroid would sit right of the point the player clicked.
    FVector TrueCenter((LeftEdge + RightEdge) * 0.5f + StaggerShift * 0.5f, (TopEdge + BottomEdge) * 0.5f, 0.0f);

    TArray<FVector> Offsets;
    Offsets.Reserve(NumUnits);
    for (int32 i = 0; i < NumUnits; ++i)
    {
        int32 Row = i / GridSize;
        int32 Col = i % GridSize;
        const float RowShift = (Row % 2 == 1) ? StaggerShift : 0.f;
        Offsets.Add(FVector(XPositions[Col] + RowShift, YPositions[Row], 0.f) - TrueCenter);
    }

    return Offsets;
}

TArray<TArray<float>> ACustomControllerBase::BuildCostMatrix(
    const TArray<AUnitBase*>& Units,
    const TArray<FVector>& SlotOffsets,
    const FVector& TargetCenter,
    const TArray<float>& SlotCapacities) const
{
    int32 N = Units.Num();
    if (N == 0) return TArray<TArray<float>>();

    // 1. Collect radii and identify slot capacities
    TArray<float> Radii;
    Radii.Reserve(N);
    for (const AUnitBase* Unit : Units)
    {
        float R = 50.0f;
        if (Unit && Unit->GetCapsuleComponent())
            R = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius();
        Radii.Add(R);
    }

    int32 GridSize = ComputeGridSize(N);
    int32 NumRows = FMath::CeilToInt((float)N / (float)GridSize);

    TArray<float> MaxR_Col; MaxR_Col.Init(0.0f, GridSize);
    TArray<float> MaxR_Row; MaxR_Row.Init(0.0f, NumRows);

    for (int32 i = 0; i < N; ++i)
    {
        int32 Row = i / GridSize;
        int32 Col = i % GridSize;
        float R = Radii[i];
        MaxR_Col[Col] = FMath::Max(MaxR_Col[Col], R);
        MaxR_Row[Row] = FMath::Max(MaxR_Row[Row], R);
    }

    // 2. Build the matrix with penalties
    // A slot's capacity is the unit radius it was spaced for. The ring and wedge layouts report
    // that directly (SlotCapacities); the grid layout does not, so for it we re-derive the
    // capacity from the slot's row/column, which is only meaningful because grid slots really do
    // sit on a regular row/column lattice.
    const bool bHaveExplicitCapacities = (SlotCapacities.Num() == N);

    TArray<TArray<float>> Cost;
    Cost.SetNum(N);
    for (int32 i = 0; i < N; ++i)
    {
        float UnitR = Radii[i];
        FVector UnitLoc = GetUnitWorldLocation(Units[i]);

        Cost[i].SetNum(N);
        for (int32 j = 0; j < N; ++j)
        {
            FVector SlotWorld = TargetCenter + (SlotOffsets.IsValidIndex(j) ? SlotOffsets[j] : FVector::ZeroVector);
            float DistSq = FVector::DistSquared(UnitLoc, SlotWorld);

            float Capacity;
            if (bHaveExplicitCapacities)
            {
                Capacity = SlotCapacities[j];
            }
            else
            {
                int32 Row = j / GridSize;
                int32 Col = j % GridSize;
                Capacity = FMath::Min(MaxR_Col[Col], MaxR_Row[Row]);
            }

            // If unit is too large for the slot's allocated space, add a massive penalty.
            // We use a small epsilon for float comparison.
            if (UnitR > Capacity + 0.1f)
            {
                Cost[i][j] = DistSq + 1e10f;
            }
            else
            {
                Cost[i][j] = DistSq;
            }
        }
    }
    return Cost;
}

TArray<int32> ACustomControllerBase::SolveHungarian(const TArray<TArray<float>>& Matrix) const
{
    int n = Matrix.Num();
    TArray<float> u; u.Init(0.f, n+1);
    TArray<float> v; v.Init(0.f, n+1);
    TArray<int32> p; p.Init(0, n+1);
    TArray<int32> way; way.Init(0, n+1);

    for (int i = 1; i <= n; ++i)
    {
        p[0] = i;
        int j0 = 0;
        TArray<float> minv; minv.Init(FLT_MAX, n+1);
        TArray<bool> used; used.Init(false, n+1);
        do
        {
            used[j0] = true;
            int i0 = p[j0];
            float delta = FLT_MAX;
            int j1 = 0;
            for (int j = 1; j <= n; ++j)
            {
                if (!used[j])
                {
                    float cur = Matrix[i0-1][j-1] - u[i0] - v[j];
                    if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                    if (minv[j] < delta) { delta = minv[j]; j1 = j; }
                }
            }
            for (int j = 0; j <= n; ++j)
            {
                if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
                else { minv[j] -= delta; }
            }
            j0 = j1;
        } while (p[j0] != 0);

        do
        {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0);
    }

    TArray<int32> Assignment;
    Assignment.SetNum(n);
    for (int j = 1; j <= n; ++j)
    {
        if (p[j] > 0) Assignment[p[j] - 1] = j - 1;
    }
    return Assignment;
}

void ACustomControllerBase::ForceFormationRecalculation()
{
    bForceFormationRecalculation = true;
}

bool ACustomControllerBase::ShouldRecalculateFormation() const
{
    if (bForceFormationRecalculation) return true;

    // The directional shapes bake the approach direction into their offsets. Caching them across
    // orders was safe while every shape was world-axis aligned, but now a second order in another
    // direction would reuse the FIRST order's orientation - a wedge sent north then south would
    // arrive pointing backwards.
    if (IsDirectionalFormationShape(GridFormationShape)) return true;

    if (SelectedUnits.Num() != LastFormationUnits.Num()) return true;
    TSet<TWeakObjectPtr<AUnitBase>> LastSet(LastFormationUnits);
    for (AUnitBase* U : SelectedUnits)
        if (!LastSet.Contains(U)) return true;
    return false;
}

TArray<int32> ACustomControllerBase::SolveAssignmentGreedy(const TArray<TArray<float>>& Matrix) const
{
    const int32 n = Matrix.Num();
    TArray<int32> Assignment;
    Assignment.Init(0, n);
    if (n == 0)
    {
        return Assignment;
    }

    const int32 m = Matrix[0].Num();
    TArray<bool> SlotTaken;
    SlotTaken.Init(false, m);

    for (int32 i = 0; i < n; ++i)
    {
        int32 BestSlot = INDEX_NONE;
        float BestCost = FLT_MAX;
        for (int32 j = 0; j < m; ++j)
        {
            if (!SlotTaken[j] && Matrix[i][j] < BestCost)
            {
                BestCost = Matrix[i][j];
                BestSlot = j;
            }
        }

        // Mehr Einheiten als Plaetze: der Rest bekommt reihum einen gueltigen Index, damit der
        // Aufrufer (Offsets[Assign[i]]) nicht ins Leere greift.
        Assignment[i] = (BestSlot != INDEX_NONE) ? BestSlot : FMath::Min(i, m - 1);
        if (BestSlot != INDEX_NONE)
        {
            SlotTaken[BestSlot] = true;
        }
    }

    return Assignment;
}

void ACustomControllerBase::RecalculateFormation(const FVector& TargetCenter, float Spacing)
{
    int32 N = SelectedUnits.Num();
    if (N == 0) return;
    UnitFormationOffsets.Empty();
    LastFormationUnits.Empty();

    // 1. Sort a local copy of units by radius to ensure size-matched formation assignment
    TArray<AUnitBase*> SortedUnits = SelectedUnits;
    SortedUnits.Sort([](const AUnitBase& A, const AUnitBase& B) {
        float RA = 50.0f;
        if (A.GetCapsuleComponent()) RA = A.GetCapsuleComponent()->GetScaledCapsuleRadius();
        float RB = 50.0f;
        if (B.GetCapsuleComponent()) RB = B.GetCapsuleComponent()->GetScaledCapsuleRadius();
        return RA > RB;
    });

    // 2. Compute non-uniform offsets tailored for these units. The directional shapes are
    // oriented along the way the group is about to travel, so a wedge points at the destination
    // and a half circle bulges towards it instead of always facing world +X.
    TArray<float> SlotCapacities;
    auto Offsets = ComputeSlotOffsetsDirectional(SortedUnits, Spacing, ComputeApproachDirection(SortedUnits, TargetCenter), &SlotCapacities);

    // 3. Match units to slots. By including radius info in BuildCostMatrix, we ensure big units get big slots.
    auto Cost = BuildCostMatrix(SortedUnits, Offsets, TargetCenter, SlotCapacities);

    // Siehe FormationHungarianMaxUnits: die optimale Loesung ist kubisch und war bei grossen
    // Gruppen der gesamte Klick-Ruckler.
    const bool bUseGreedy = (FormationHungarianMaxUnits > 0 && N > FormationHungarianMaxUnits);
    auto Assign = bUseGreedy ? SolveAssignmentGreedy(Cost) : SolveHungarian(Cost);

    for (int32 i = 0; i < N; ++i)
    {
        UnitFormationOffsets.Add(SortedUnits[i], Offsets[Assign[i]]);
        LastFormationUnits.Add(SortedUnits[i]);
    }
    bForceFormationRecalculation = false;
}

bool ACustomControllerBase::ValidateAndAdjustGridLocation(const TArray<AUnitBase*>& Units, FVector& InOutLocation, TArray<FVector>& OutOffsets, float& OutSpacing)
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    OutSpacing = GridSpacing;

    // Orientation for the directional shapes. It is derived from the selection's centroid, so it
    // does not depend on unit order and stays the same for the sorted copy built further down.
    // Grid shapes ignore it entirely.
    const FVector FormationForward = ComputeApproachDirection(Units, InOutLocation);

    if (!NavSys || Units.Num() == 0)
    {
        OutOffsets = ComputeSlotOffsetsDirectional(Units, OutSpacing, FormationForward);
        return true;
    }

    // 0. Ensure InOutLocation is not in a dirty area before we start
    FNavLocation CenterNav;
    if (NavSys->ProjectPointToNavigation(InOutLocation, CenterNav, FVector(1000.f, 1000.f, 1000.f)))
    {
        bool bCenterDirty = false;
        const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
        if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
        {
            const uint32 PolyAreaID = Recast->GetPolyAreaID(CenterNav.NodeRef);
            const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
            bCenterDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
        }

        if (bCenterDirty)
        {
            // Radial search for nearest non-dirty center
            static const float Radii[] = {200.f, 400.f, 800.f, 1600.f};
            static const int32 Slices = 12;
            bool bFoundCenter = false;
            for (float R : Radii)
            {
                for (int32 s = 0; s < Slices; ++s)
                {
                    const float Angle = (2 * PI) * (float(s) / float(Slices));
                    const FVector Candidate = InOutLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * R;
                    FNavLocation CandNav;
                    if (NavSys->ProjectPointToNavigation(Candidate, CandNav, FVector(1000.f, 1000.f, 1000.f)))
                    {
                        if (const ARecastNavMesh* RM = Cast<ARecastNavMesh>(NavData))
                        {
                            const uint32 AID = RM->GetPolyAreaID(CandNav.NodeRef);
                            const UClass* AC = RM->GetAreaClass(AID);
                            if (!(AC && AC->IsChildOf(UNavArea_Obstacle::StaticClass())))
                            {
                                InOutLocation = CandNav.Location;
                                bFoundCenter = true;
                                break;
                            }
                        }
                    }
                }
                if (bFoundCenter) break;
            }
        }
        else
        {
            InOutLocation = CenterNav.Location;
        }
    }

    // 1. Consistency: Sort by radius to match RecalculateFormation using a stable sort
    TArray<AUnitBase*> LocalUnits = Units;
    LocalUnits.Sort([](const AUnitBase& A, const AUnitBase& B) {
        float RA = 50.0f;
        if (A.GetCapsuleComponent()) RA = A.GetCapsuleComponent()->GetScaledCapsuleRadius();
        float RB = 50.0f;
        if (B.GetCapsuleComponent()) RB = B.GetCapsuleComponent()->GetScaledCapsuleRadius();
        if (FMath::IsNearlyEqual(RA, RB)) return A.GetName() > B.GetName();
        return RA > RB;
    });

    const float SpacingSteps[] = { 1.0f, 0.7f, 0.4f, 0.1f };
    bool bFinalSuccess = false;

    for (float StepMult : SpacingSteps)
    {
        OutSpacing = GridSpacing * StepMult;
        
        // Try up to 5 times to shift the grid to a valid location at this spacing
        for (int32 Try = 0; Try < 5; ++Try)
        {
            OutOffsets = ComputeSlotOffsetsDirectional(LocalUnits, OutSpacing, FormationForward);
            bool bAllValid = true;
            FVector FirstFailedPointShift = FVector::ZeroVector;

            for (const FVector& Off : OutOffsets)
            {
                FVector TargetP = InOutLocation + Off;
                FNavLocation NavLoc;
                bool bOnNavMesh = NavSys->ProjectPointToNavigation(TargetP, NavLoc, NavMeshProjectionExtent);

            				bool bInDirtyArea = false;
            				if (bOnNavMesh)
            				{
            					const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
            					if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
            					{
            						const uint32 PolyAreaID = Recast->GetPolyAreaID(NavLoc.NodeRef);
            						const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
            						bInDirtyArea = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
            					}
            				}

                if (!bOnNavMesh || bInDirtyArea)
                {
                    bAllValid = false;
                    // Find nearest nav location for THIS point to calculate a shift for the whole grid
                    if (NavSys->ProjectPointToNavigation(TargetP, NavLoc, FVector(1000.f, 1000.f, 1000.f)))
                    {
                        FirstFailedPointShift = NavLoc.Location - TargetP;
                    }
                    break; 
                }
            }

            if (bAllValid)
            {
                bFinalSuccess = true;
                break;
            }

            // If not all valid, shift InOutLocation based on the first point that failed
            if (!FirstFailedPointShift.IsNearlyZero())
            {
                InOutLocation += FirstFailedPointShift;
            }
            else
            {
                // Fallback: just project center again with wide extent
                if (NavSys->ProjectPointToNavigation(InOutLocation, CenterNav, FVector(1000.f, 1000.f, 1000.f)))
                {
                    InOutLocation = CenterNav.Location;
                }
            }
        }
        
        if (bFinalSuccess) break;
    }

    if (!bFinalSuccess)
    {
        // Final attempt: individually adjust each point that is still invalid
        OutOffsets = ComputeSlotOffsetsDirectional(LocalUnits, OutSpacing, FormationForward);
        for (int32 i = 0; i < OutOffsets.Num(); ++i)
        {
            FVector TargetP = InOutLocation + OutOffsets[i];
            FNavLocation NavLoc;
            if (NavSys->ProjectPointToNavigation(TargetP, NavLoc, FVector(1000.f, 1000.f, 1000.f)))
            {
                bool bDirty = false;
                const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties());
                if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
                {
                    const uint32 PolyAreaID = Recast->GetPolyAreaID(NavLoc.NodeRef);
                    const UClass* PolyAreaClass = Recast->GetAreaClass(PolyAreaID);
                    bDirty = PolyAreaClass && PolyAreaClass->IsChildOf(UNavArea_Obstacle::StaticClass());
                }

                if (bDirty)
                {
                    // Snap to nearest non-dirty
                    static const float Radii[] = {150.f, 300.f, 600.f, 900.f};
                    static const int32 Slices = 12;
                    bool bFoundSafe = false;
                    for (float R : Radii)
                    {
                        for (int32 s = 0; s < Slices; ++s)
                        {
                            const float Angle = (2 * PI) * (float(s) / float(Slices));
                            const FVector Candidate = TargetP + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * R;
                            FNavLocation CandNav;
                            if (NavSys->ProjectPointToNavigation(Candidate, CandNav, FVector(1000.f, 1000.f, 1000.f)))
                            {
                                if (const ARecastNavMesh* RM = Cast<ARecastNavMesh>(NavData))
                                {
                                    const uint32 AID = RM->GetPolyAreaID(CandNav.NodeRef);
                                    const UClass* AC = RM->GetAreaClass(AID);
                                    if (!(AC && AC->IsChildOf(UNavArea_Obstacle::StaticClass())))
                                    {
                                        OutOffsets[i] = CandNav.Location - InOutLocation;
                                        bFoundSafe = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (bFoundSafe) break;
                    }
                    if (!bFoundSafe)
                    {
                        OutOffsets[i] = NavLoc.Location - InOutLocation; // Fallback to dirty projected point if no safe one found
                    }
                }
                else
                {
                    OutOffsets[i] = NavLoc.Location - InOutLocation;
                }
            }
            else
            {
                // If individual projection fails, fallback to the already-validated formation center
                OutOffsets[i] = FVector::ZeroVector; 
            }
        }
    }

    return true; 
}


void ACustomControllerBase::SetHoldPositionOnSelectedUnits()
{
	// Zuschauer duerfen anwaehlen, aber nicht befehlen (siehe IsSpectatorController).
	if (IsSpectatorController()) return;

	for (AUnitBase* U : SelectedUnits)
	{
		if (!U) continue;

		SetHoldPositionOnUnit(U);
		SetHoldPositionOnUnit_Implementation(U);
	}
}

void ACustomControllerBase::SetHoldPositionOnUnit_Implementation(AUnitBase* Unit)
{
	// Zuschauer duerfen anwaehlen, aber nicht befehlen (siehe IsSpectatorController).
	if (IsSpectatorController()) return;

	Unit->bHoldPosition = true;
}

void ACustomControllerBase::Server_ClearWaypointForManualOrder_Implementation(
	const TArray<AUnitBase*>& Units)
{
	// Auf dem Listen-Server fuehrt die Engine den Aufruf direkt aus, dort aendert sich nichts.
	ClearWaypointForManualOrderInternal(Units);
}

void ACustomControllerBase::ClearWaypointForManualOrder(const TArray<AUnitBase*>& Units)
{
	// Waechter fuer die Aufrufer, die nur auf dem Server etwas bewirken sollen.
	if (!HasAuthority())
	{
		return;
	}
	ClearWaypointForManualOrderInternal(Units);
}

void ACustomControllerBase::ClearWaypointForManualOrderInternal(const TArray<AUnitBase*>& Units)
{
	// BEWUSST ohne Berechtigungspruefung: der befehlende Client sagt die Bewegung lokal voraus
	// (siehe ApplyMovePredictionToUnit in RunUnitsAndSetWaypointsMass) und haelt dabei seine
	// EIGENE Kopie von NextWaypoint und FMassPatrolFragment. Wird die nicht mitgeloescht, schickt
	// der IdleStateProcessor des Clients die Einheit lokal wieder nach Hause - auch wenn der
	// Server laengst sauber ist. Deshalb laeuft diese Funktion auf BEIDEN Seiten.

	UMassEntitySubsystem* MassSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassEntityManager* EntityManager = MassSubsystem ? &MassSubsystem->GetMutableEntityManager() : nullptr;

	for (AUnitBase* Unit : Units)
	{
		if (!IsValid(Unit))
		{
			continue;
		}

		// Gebaeude und Baustellen sind ausgenommen: deren NextWaypoint ist der SAMMELPUNKT, kein
		// Patrouillenziel. Ohne diese Ausnahme loeschte der Marschbefehl den Sammelpunkt des
		// gerade ausgewaehlten Gebaeudes - und weil danach ein neuer angelegt wurde, entstand bei
		// JEDEM Klick ein zusaetzlicher Wegpunkt. Im Log gut zu sehen: erster Aufruf "Vorhanden=None"
		// (frisch geloescht) legt an, der zweite versetzt korrekt, der naechste Klick loescht wieder.
		if (Unit->bIsBuilding || Cast<AConstructionUnit>(Unit))
		{
			continue;
		}

		// Der Actor haelt den Wegpunkt, das Fragment die daraus abgeleitete Position. BEIDE muessen
		// weg - das Fragment wird nur beim Binden aus dem Actor befuellt, ein spaeteres Nullen des
		// Actors allein bliebe also wirkungslos.
		Unit->NextWaypoint = nullptr;

		if (!EntityManager || !Unit->MassActorBindingComponent)
		{
			continue;
		}

		const FMassEntityHandle Handle = Unit->MassActorBindingComponent->GetMassEntityHandle();
		if (!EntityManager->IsEntityValid(Handle))
		{
			continue;
		}

		if (FMassPatrolFragment* PatrolFrag = EntityManager->GetFragmentDataPtr<FMassPatrolFragment>(Handle))
		{
			PatrolFrag->TargetWaypointLocation = FVector::ZeroVector;
			PatrolFrag->CurrentWaypointIndex = INDEX_NONE;
		}
	}
}

void ACustomControllerBase::RunUnitsAndSetWaypointsMass(FHitResult Hit)
{
	
    // 1. Setup
    if (SelectedUnits.Num() == 0) return;


	// Vom Wegpunkt loesen, BEVOR der Befehl ergeht - sonst holt der naechste Idle-Takt die
	// Einheit ueber StoredLocation wieder nach Hause.
	//
	// BEIDE Seiten, und das ist kein Versehen:
	//
	// Der RPC raeumt den Server auf - ohne ihn behielt der Server NextWaypoint und
	// TargetWaypointLocation, weil diese Funktion auf dem befehlenden CLIENT laeuft und
	// ClearWaypointForManualOrder bei !HasAuthority() aussteigt.
	//
	// Der lokale Aufruf raeumt den Client auf. Der sagt die Bewegung gleich unten selbst voraus
	// (ApplyMovePredictionToUnit) und haelt dafuer eine eigene Kopie der Fragmente. Bleibt die
	// stehen, schickt sein eigener IdleStateProcessor die Einheit trotzdem wieder nach Hause -
	// genau das war nach dem ersten Anlauf noch zu sehen.
	//
	// Beim Listen-Server fuehrt die Engine den RPC direkt aus und HasAuthority() ist wahr, der
	// zweite Aufruf entfaellt dort also.
	Server_ClearWaypointForManualOrder(SelectedUnits);
	if (!HasAuthority())
	{
		ClearWaypointForManualOrderInternal(SelectedUnits);
	}

	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem) return;
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	FVector AdjustedLocation = Hit.Location;
	TArray<FVector> DummyOffsets;
	float UsedSpacing;
	
	ValidateAndAdjustGridLocation(SelectedUnits, AdjustedLocation, DummyOffsets, UsedSpacing);

    AWaypoint* BWaypoint = nullptr;
    bool PlayWaypoint = false, PlayRun = false;

    // 2. Formation check
    // We recalculate if selection changed OR if the target location was adjusted significantly
    if (ShouldRecalculateFormation() || !AdjustedLocation.Equals(Hit.Location, 1.0f))
    {
        RecalculateFormation(AdjustedLocation, UsedSpacing);
    }

    // 3. Assign final positions
    TMap<AUnitBase*, FVector> Finals;
    for (AUnitBase* U : SelectedUnits)
    {
    	if (!U) continue;
    	
		bool UnitIsValid = true;
    	
    	if (!U->IsInitialized) UnitIsValid = false;
    	
    	if (U->CurrentSnapshot.AbilityClass)
    	{

    		UGameplayAbilityBase* AbilityCDO = U->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		
    		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) UnitIsValid = false;
			else
			{
				ABuildingBase* BuildingBase = U->bIsBuilding ? static_cast<ABuildingBase*>(U) : nullptr;
				if (!BuildingBase || (!BuildingBase->HasWaypoint && BuildingBase->CancelsAbilityOnRightClick))
					CancelCurrentAbility(U);
					
			}
    	}

    	if (UnitIsValid)
    	{
    		FVector FinalLoc;
    		if (U->bIsBuilding || U->bIsConstructionUnit)
    		{
    			// Buildings and construction sites (rally-waypoint owners) use the exact click.
    			FinalLoc = Hit.Location;
    		}
    		else
    		{
    			FVector Off = UnitFormationOffsets.FindRef(U);
    			FinalLoc = AdjustedLocation + Off;
    		}
    		Finals.Add(U, FinalLoc);
    	}
    }

    // 4. Issue moves & sounds
    TArray<AUnitBase*> BatchUnits;
    TArray<FVector>    BatchLocs;
    TArray<float>      BatchSpeeds;
    TArray<AUnitBase*> BuildingUnits;
    TArray<FVector>    BuildingLocs;

    for (auto& P : Finals)
    {
        auto* U = P.Key;
        FVector Loc = P.Value;
        if (!U || U == CameraUnitWithTag || U->UnitState == UnitData::Dead) continue;
    	
        bool bNavMod;
        Loc = TraceRunLocation(Loc, bNavMod);
        if (bNavMod) continue; 

    	// Not every selected unit has a live Mass entity: buildings, construction units and anything whose
    	// entity was destroyed between selection and this click hand back a default FMassEntityHandle.
    	// GetFragmentDataPtr on such a handle walks into a null archetype and trips the
    	// "Assertion failed: CurrentArchetype" in MassEntityManager - reproduced by a human player simply
    	// right-clicking a move order. Ask whether the entity is usable BEFORE touching it.
    	FMassEntityHandle MassEntityHandle = U->MassActorBindingComponent ? U->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
    	const bool bEntityUsable = RTSUnitUtils::IsEntityUsable(EntityManager, MassEntityHandle);
    	FMassCombatStatsFragment* CombatStatsPtr = bEntityUsable
    		? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MassEntityHandle)
    		: nullptr;
    	bool bIsAttackingOrPausing = bEntityUsable
    		&& (DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateAttackTag::StaticStruct())
    		 || DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStatePauseTag::StaticStruct()));
    	bool bIsMovingWhileAttacking = CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking && bIsAttackingOrPausing;

        if (!bIsMovingWhileAttacking) U->RemoveFocusEntityTarget();

        U->SetRdyForTransport(false);

        float Speed = U->Attributes->GetRunSpeed();

        bool bSuccess = false;
        SetBuildingWaypoint(Loc, U, BWaypoint, PlayWaypoint, bSuccess);
        if (bSuccess)
        {
            PlayWaypoint = true;
            BuildingUnits.Add(U);
            BuildingLocs.Add(Loc);
        }
        else if (IsShiftPressed)
        {
            // Der Kreis wird ERST NACH der Zielanpassung gezeichnet - siehe unten bei
            // AdjustBatchTargetsForNav. Im Shift-Zweig laeuft die Einheit ueber
            // RightClickRunShift zum unveraenderten Loc, dieser Kreis ist also sofort richtig.
            if (!U->IsInitialized || !U->CanMove) continue;
            DrawCircleAtLocation(GetWorld(), Loc, FColor::Green);
            if (U->bIsMassUnit)
            {
                if (U->GetUnitState() != UnitData::Run)
                {
                    BatchUnits.Add(U);
                    BatchLocs.Add(Loc);
                    BatchSpeeds.Add(Speed);
                }
                RightClickRunShift(U, Loc);
                if (!bIsMovingWhileAttacking)
                {
                    SetUnitState_Replication(U, 1);
                }
            }
            PlayRun = true;
        }
        else
        {
            if (!U->IsInitialized || !U->CanMove) continue;
            if (!U->bIsMassUnit)
            {
                // Nicht-Mass-Einheiten laufen direkt zum Klickpunkt, ihr Kreis stimmt sofort.
                DrawCircleAtLocation(GetWorld(), Loc, FColor::Green);
            }
            if (U->bIsMassUnit)
            {
                BatchUnits.Add(U);
                BatchLocs.Add(Loc);
                BatchSpeeds.Add(Speed);
                if (!bIsMovingWhileAttacking)
                {
                    SetUnitState_Replication(U, 1);
                }
            }

            PlayRun = true;
        }
    }

    if (BuildingUnits.Num() > 0)
    {
       Server_Batch_SetBuildingWaypoints(BuildingLocs, BuildingUnits);
    }

    if (BatchUnits.Num() > 0)
    {
    	// Single source of truth: snap any off-nav / dirty-area formation slots to nearest valid points
    	// HERE, on the commanding client, so the SAME validated targets drive (a) the local prediction
    	// below and (b) the server (which re-validates to a no-op and forwards these to all clients).
    	// Previously the server adjusted independently while the client predicted to the raw off-nav point
    	// -> client units appeared stuck while the server moved them. RecalculateFormation does not
    	// nav-validate its per-slot offsets, so this step is what guarantees reachable predicted targets.
    	BatchLocs = AdjustBatchTargetsForNav(BatchUnits, BatchLocs);

    	// DIE KREISE GEHOEREN DORTHIN, WO DIE EINHEITEN WIRKLICH HINLAUFEN.
    	//
    	// Frueher wurden sie oben in der Schleife am Klickpunkt gezeichnet - also BEVOR
    	// AdjustBatchTargetsForNav die Ziele auf das Navigationsnetz und in die Formation legt.
    	// Bei vielen Einheiten verschiebt diese Anpassung die Ziele deutlich, und die Kreise
    	// standen sichtbar woanders als die Einheiten hinliefen.
    	for (const FVector& Ziel : BatchLocs)
    	{
    		DrawCircleAtLocation(GetWorld(), Ziel, FColor::Green);
    	}

    	TArray<float> BatchRadii;
    	for (AUnitBase* Unit : BatchUnits)
    	{
    		BatchRadii.Add(Unit->MovementAcceptanceRadius);
    	}
		// bOriginatorPredictsLocally = true: this client predicts locally just below, so the server
		// won't echo a redundant Client_Predict back to us.
		Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocs, BatchSpeeds, BatchRadii, false, true, true, true);

		// Local immediate prediction on the commanding client: its own selected units react this frame
		// instead of waiting for the server round-trip Client_Predict RPC (which can drop units under
		// relevance churn). The server still applies authority and re-affirms to every client afterwards
		// (idempotent re-stamp). Listen-server hosts already move via the authoritative Batch_ path, so
		// only remote clients predict locally here; refs are the locally-selected units (always valid).
		if (!HasAuthority())
		{
			for (int32 i = 0; i < BatchUnits.Num(); ++i)
			{
				ApplyMovePredictionToUnit(EntityManager, GetWorld(), BatchUnits[i], BatchLocs[i], BatchSpeeds[i], BatchRadii[i], false, true, true);
			}
			EntityManager.FlushCommands();
		}
    }

    if (WaypointSound && PlayWaypoint)
    {
        UGameplayStatics::PlaySound2D(this, WaypointSound, GetSoundMultiplier());
    }

    const bool bCanPlayRunSound = RunSound && PlayRun && (GetWorld()->GetTimeSeconds() - LastRunSoundTime >= RunSoundDelayTime);
    if (bCanPlayRunSound)
    {
        UGameplayStatics::PlaySound2D(this, RunSound, GetSoundMultiplier());
        LastRunSoundTime = GetWorld()->GetTimeSeconds();
    }
	

}


void ACustomControllerBase::LeftClickPressedMass()
{
    LeftClickIsPressed = true;
    int32 SavedAbilityIndex = AbilityArrayIndex;
    AbilityArrayIndex = 0;

    if (!CameraBase || CameraBase->TabToggled) return;

    if (SwapAttackMove) AttackToggled = false;
	
    // --- ALT: cancel / destroy area ---
	if (AltIsPressed)
    {
        DestroyWorkArea();
        for (AUnitBase* U : SelectedUnits)
        {
            CancelAbilitiesIfNoBuilding(U);
        }
    }
    else if (AttackToggled && !SwapAttackMove)
    {
        // Default layout: attack-move lives on the LEFT button. Arm the line drag before
        // HandleAttackMovePressed clears AttackToggled. This branch never reaches the box-select
        // code, so bSelectFriendly stays false and the selection is stable for the whole drag.
        if (CanStartFormationLineDrag())
        {
            FHitResult DragHit;
            GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, DragHit);
            if (DragHit.bBlockingHit)
            {
                BeginFormationLineDrag(DragHit.Location, /*bAttackMove=*/true, /*bFromRightMouse=*/false);
            }
        }

        HandleAttackMovePressed();
    }
    else
    {
        FHitResult HitPawn;
        // Kaskade ueber Pawn -> WorldDynamic -> Visibility, siehe GetSelectableHitUnderCursor.
        // Der frueher hier stehende Rueckfall pruefte `!HitPawn.bBlockingHit` und lief deshalb nie:
        // die Pawn-Spur geht durch Gebaeude und Bauplaetze hindurch und trifft das Landscape, und
        // das IST ein blockierender Treffer. Gebaeude und WorkAreas waren dadurch per Einzelklick
        // nicht selektierbar - ueber die Rechteckauswahl im HUD dagegen schon, weil die keine Spur
        // benutzt. Genau dieses Muster war das Symptom.
        GetSelectableHitUnderCursor(HitPawn);

        // Check if any unit is currently aiming an ability, dragging a workarea, or if we have an indicator active
        bool bAnyUnitIsAimingOrDragging = bUsedKeyboardAbilityBeforeClick;
        if (!bAnyUnitIsAimingOrDragging)
        {
            if (CurrentDraggedAbilityIndicator)
            {
                bAnyUnitIsAimingOrDragging = true;
            }
            else
            {
                for (AUnitBase* U : SelectedUnits)
                {
                    if (U && (U->CurrentSnapshot.AbilityClass || U->CurrentDraggedWorkArea))
                    {
                        bAnyUnitIsAimingOrDragging = true;
                        break;
                    }
                }
            }
        }

        if (bAnyUnitIsAimingOrDragging)
        {
            // Indicator Cleanup (Client-side)
            for (AUnitBase* U : SelectedUnits)
            {
                if (U && U->CurrentSnapshot.AbilityClass)
                {
                    UGameplayAbilityBase* AbilityCDO = U->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
                    if (AbilityCDO && AbilityCDO->AbilityIndicatorClass)
                    {
                        HandleAbilityIndicatorEnd(U);
                    }
                }
            }

            bUsedKeyboardAbilityBeforeClick = false; // consume the flag
            // Send client work area transform (if any) to ensure server has the same placement
            bool bHasClientWorkAreaTransform = false;
            FTransform ClientWorkAreaTransform;
            if (SelectedUnits.Num() > 0 && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
            {
                bHasClientWorkAreaTransform = true;
                ClientWorkAreaTransform = SelectedUnits[0]->CurrentDraggedWorkArea->GetActorTransform();
            }
            Server_HandleAbilityUnderCursor(SelectedUnits, HitPawn, WorkAreaIsSnapped, DropWorkAreaFailedSound, bHasClientWorkAreaTransform, ClientWorkAreaTransform, SavedAbilityIndex);
        }
        else
        {
            // Skip server and just continue with selection locally
            Client_ContinueSelectionAfterAbility(HitPawn);
        }
        return;
    }
	
}

void ACustomControllerBase::Server_HandleAbilityUnderCursor_Implementation(const TArray<AUnitBase*>& Units, const FHitResult& HitPawn, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound, bool bHasClientWorkAreaTransform, FTransform ClientWorkAreaTransform, int32 InAbilityIndex)
{
    if (Units.Num() == 0) return;

    // Ensure server has the same transform for the dragged work area as the client
    if (bHasClientWorkAreaTransform && Units[0] && Units[0]->CurrentDraggedWorkArea)
    {
        Units[0]->CurrentDraggedWorkArea->SetActorTransform(ClientWorkAreaTransform);
    }
    // Try to drop any active work area for the first unit using the new parameterized variant
    if (Units[0] && Units[0]->CurrentDraggedWorkArea)
    {
        if (!Units[0]->CurrentDraggedWorkArea->InstantDrop) DropWorkAreaForUnit(Units[0], bWorkAreaIsSnapped, InDropWorkAreaFailedSound);
    }

    bool AbilityFired = false;
    bool AbilityUnSynced = false;
    bool bFromCooldown = false;
    bool bAnyAbilityWantsToKeepSelection = false;

    if (this->IsShiftPressed)
    {
        bAnyAbilityWantsToKeepSelection = true;
    }

    for (AUnitBase* U : Units)
    {
        if (U && U->CurrentSnapshot.AbilityClass)
        {

            // Indicator Cleanup (Server-side)
            UGameplayAbilityBase* AbilityCDO = U->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
            if (AbilityCDO)
            {
                if (AbilityCDO->AbilityIndicatorClass)
                {
                    HandleAbilityIndicatorEnd(U);
                }

                if (AbilityCDO->bKeepSelectionAfterAbility)
                {
                    bAnyAbilityWantsToKeepSelection = true;
                }
            }

            FireAbilityMouseHit(U, HitPawn);
            AbilityFired = true;
        }
        else if (U)
        {
            AbilityUnSynced = true;
            TArray<TSubclassOf<UGameplayAbilityBase>> AbilityArray = GetAbilityArrayForUnit(U);
            if (AbilityArray.IsValidIndex(InAbilityIndex))
            {
                if (UGameplayAbilityBase* AbilityCDO = AbilityArray[InAbilityIndex]->GetDefaultObject<UGameplayAbilityBase>())
                {
                    if (AbilityCDO->bKeepSelectionAfterAbility)
                    {
                        bAnyAbilityWantsToKeepSelection = true;
                        if (AGASUnit* GASUnit = Cast<AGASUnit>(U))
                        {
                            if (GASUnit->IsAbilityOnCooldownByClass(AbilityArray[InAbilityIndex]))
                            {
                                bFromCooldown = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if ((AbilityFired || AbilityUnSynced) && bAnyAbilityWantsToKeepSelection)
    {
        // Reset the flag for the client, but don't perform selection follow-up
        Client_ContinueSelectionAfterAbility(FHitResult(), false, true);
        return;
    }

    // Ask owning client to continue with selection handling
    Client_ContinueSelectionAfterAbility(HitPawn, bFromCooldown, false);
}

void ACustomControllerBase::Client_ContinueSelectionAfterAbility_Implementation(const FHitResult& HitPawn, bool bFromCooldown, bool bResetFlagOnly)
{
    if (bFromCooldown) {
        // Ability attempt failed due to cooldown; skip deselection now and set flag for next click
        bDeselectOnNextClick = true;
        return; 
    }

    bool bWasDeselectFlagActive = bDeselectOnNextClick;
    bDeselectOnNextClick = false;

    if (bResetFlagOnly)
    {
        return;
    }

    // Prevent any selection changes or deselection if an ability button is currently held down
    if (!HeldAbilityInputs.IsEmpty())
    {
        return;
    }

    if (bWasDeselectFlagActive) {
        HUDBase->DeselectAllUnits();
    } 

    // if we hit a pawn, try to select it (client-side UI and input state)
    if (HitPawn.bBlockingHit && HUDBase)
    {
        AActor* HitActor = HitPawn.GetActor();
        if (HitActor && !HitActor->IsA(ALandscape::StaticClass()))
            ClickedActor = HitActor;
        else
            ClickedActor = nullptr;

        AUnitBase* HitUnit = GetUnitFromHitResult(HitPawn);
        
        // --- NEW: Select ConstructionUnit if we clicked on a WorkArea ---
        if (!HitUnit && HitActor)
        {
            if (AWorkArea* WorkArea = Cast<AWorkArea>(HitActor))
            {
                if (WorkArea->ConstructionUnit)
                {
                    HitUnit = WorkArea->ConstructionUnit;
                }
            }
        }
        // ----------------------------------------------------------------

        ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(HitActor);

        if (HitUnit && HitUnit->CanBeSelected && (HitUnit->TeamId == SelectableTeamId || SelectableTeamId == 0) && !SUnit)
        {
            if (IsCtrlPressed)
            {
                FGameplayTag Tag = HitUnit->UnitTags.First();
                SelectUnitsWithTag(Tag, SelectableTeamId);
            }
            else
            {
                HUDBase->DeselectAllUnits();
                HUDBase->SetUnitSelected(HitUnit, bIsAi);
                DragUnitBase(HitUnit);

                if (CameraBase)
                {
                    if (CameraBase->AutoLockOnSelect)
                    {
                        LockCameraToUnit = true;
                    }
                }
            }
        }
        else
        {
            HUDBase->InitialPoint = HUDBase->GetMousePos2D();
            HUDBase->bSelectFriendly = true;
        }
    }
}

void ACustomControllerBase::LeftClickAttackMass_Implementation(const TArray<AUnitBase*>& Units, const TArray<FVector>& Locations, bool AttackT, AActor* CursorHitActor)
{
	const int32 Count = FMath::Min(Units.Num(), Locations.Num());
	
	if (Count <= 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[LeftClickAttackMass] Nothing to process (Count<=0)"));
		return;
	}

	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem) return;
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	// If we clicked an enemy unit, set chase on all provided units
	AUnitBase* TargetUnitBase = CursorHitActor ? Cast<AUnitBase>(CursorHitActor) : nullptr;
	if (TargetUnitBase)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			AUnitBase* Unit = Units[i];
			if (!Unit)
			{
				continue;
			}
			
			if (Unit->UnitState == UnitData::Dead)
			{
				continue;
			}

			// Validate whether this unit can attack the target based on capabilities and target traits
			if (!Unit->CanAttack)
			{
				continue;
			}
			// Invisible targets require detection capability
			if (TargetUnitBase->bIsInvisible)
			{
				continue;
			}

			// Unless queuing with Shift, clear any existing path waypoints before chasing
			if (!IsShiftPressed && Unit->bIsMassUnit)
			{
				Unit->ClearPathWaypoints();
			}

			// Respect ground/flying attack restrictions
			if (Unit->CanOnlyAttackGround && TargetUnitBase->IsFlying)
			{
				continue;
			}
			if (Unit->CanOnlyAttackFlying && !TargetUnitBase->IsFlying)
			{
				continue;
			}

			Unit->UnitToChase = TargetUnitBase;
			bool bSuccess = Unit->FocusEntityTarget(TargetUnitBase);
			Unit->SetRdyForTransport(false);
			// Unit->TransportId = 0;

			FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
			FMassCombatStatsFragment* CombatStatsPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MassEntityHandle);
			bool bIsAttackingOrPausing = DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateAttackTag::StaticStruct()) || DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStatePauseTag::StaticStruct());
			bool bIsMovingWhileAttacking = CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking && bIsAttackingOrPausing;

			if (!bIsMovingWhileAttacking)
			{
				SetUnitState_Replication(Unit, 3);
				Unit->SwitchEntityTagByState(UnitData::Chase, Unit->UnitStatePlaceholder);
			}
		}
		return;
	}

	if (UseUnrealEnginePathFinding)
	{
		// Delegate to UE pathfinding batched move
		LeftClickAMoveUEPFMass(Units, Locations, AttackT);
		for (int32 i = 0; i < Count; ++i)
		{
			if (Units[i])
			{
				Units[i]->RemoveFocusEntityTarget();
				UE_LOG(LogTemp, VeryVerbose, TEXT("[LeftClickAttackMass] Cleared focus for unit[%d] %s (UE PF)"), i, *Units[i]->GetName());
			}
		}
	}
	else
	{
		// Fallback: custom pathfinding per unit
		for (int32 i = 0; i < Count; ++i)
		{
			AUnitBase* Unit = Units[i];
			if (!Unit || Unit->UnitState == UnitData::Dead) continue;
			LeftClickAMove(Unit, Locations[i]);
			Unit->RemoveFocusEntityTarget();
			UE_LOG(LogTemp, VeryVerbose, TEXT("[LeftClickAttackMass] Issued custom move and cleared focus for unit[%d] %s"), i, *Unit->GetName());
		}
	}
}

void ACustomControllerBase::LeftClickAMoveUEPFMass_Implementation(const TArray<AUnitBase*>& Units, const TArray<FVector>& Locations, bool AttackT)
{
	const int32 Count = FMath::Min(Units.Num(), Locations.Num());
	if (Count <= 0) return;

	TArray<AUnitBase*> BatchUnits;
	TArray<FVector> BatchLocations;
	TArray<float> BatchSpeeds;

	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem) return;
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	for (int32 i = 0; i < Count; ++i)
	{
		AUnitBase* Unit = Units[i];
		if (!Unit) continue;
		if (!Unit->IsInitialized) continue;
		if (!Unit->CanMove) continue;

		if (Unit->CurrentSnapshot.AbilityClass)
		{
			UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
			if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) continue;
			else CancelCurrentAbility(Unit);
		}

		FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
		// #130: ein nicht gesetztes oder bereits abgemeldetes Handle laeuft in ein Null-Archetype
		// und loest "Assertion failed: CurrentArchetype" aus. Reproduziert durch die KI, die diesen
		// Pfad ueber LeftClickAttackMass benutzt. Gleiche Absicherung wie an der Stelle aus #98.
		const bool bEntityValid = MassEntityHandle.IsSet() && EntityManager.IsEntityValid(MassEntityHandle);
		FMassCombatStatsFragment* CombatStatsPtr = bEntityValid
			? EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(MassEntityHandle)
			: nullptr;
		bool bIsAttackingOrPausing = bEntityValid && (DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateAttackTag::StaticStruct()) || DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStatePauseTag::StaticStruct()));
		bool bIsMovingWhileAttacking = CombatStatsPtr && CombatStatsPtr->bCanMoveWhileAttacking && bIsAttackingOrPausing;

		// RunSpeed, NICHT BaseRunSpeed - siehe unten.
		//
		// Es gibt zwei Bewegungspfade: die Mass-Prozessoren lesen FMassCombatStatsFragment::RunSpeed
		// (gespeist aus Attributes->GetRunSpeed()), die Spielerbefehle lasen bis zum 11.09.2026
		// BaseRunSpeed. Ein Haste-Punkt aus dem Attributbaum beschleunigte damit die KI-Bewegung,
		// aber nicht das, was der Spieler anklickt. BaseRunSpeed ist jetzt reiner
		// Wiederherstellungspunkt fuer ResetTalents/ResetLevel.
		float Speed = Unit->Attributes->GetRunSpeed();
		if (!bIsMovingWhileAttacking)
		{
			SetUnitState_Replication(Unit, 1);
		}
		Unit->SetRdyForTransport(false);

		if (Unit->bIsMassUnit)
		{
			BatchUnits.Add(Unit);
			BatchLocations.Add(Locations[i]);
			BatchSpeeds.Add(Speed);
		}
		else if (UseUnrealEnginePathFinding)
		{
			RightClickRunUEPF(Unit, Locations[i], true);
		}
	}

	if (BatchUnits.Num() > 0)
	{
		TArray<float> BatchRadii;
		for (AUnitBase* Unit : BatchUnits)
		{
			BatchRadii.Add(Unit->MovementAcceptanceRadius);
		}
  Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocations, BatchSpeeds, BatchRadii, AttackT, true);
	}
}

// ==============================================================================================
// Formation drag line
// ==============================================================================================

namespace
{
	/** Perpendicular distance from P to the segment AB, in the XY plane. */
	float PointSegmentDistance2D(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector2D PP(P.X, P.Y);
		const FVector2D AA(A.X, A.Y);
		const FVector2D BB(B.X, B.Y);
		const FVector2D AB = BB - AA;
		const float LenSq = AB.SizeSquared();
		if (LenSq <= KINDA_SMALL_NUMBER)
		{
			return FVector2D::Distance(PP, AA);
		}
		const float T = FMath::Clamp(FVector2D::DotProduct(PP - AA, AB) / LenSq, 0.f, 1.f);
		return FVector2D::Distance(PP, AA + AB * T);
	}

	/**
	 * Douglas-Peucker. Iterative with an explicit stack: the recursive form can nest once per input
	 * point, and the sample buffer can hold hundreds.
	 */
	void SimplifyPath2D(const TArray<FVector>& In, float Tolerance, TArray<FVector>& Out)
	{
		Out.Reset();
		const int32 Num = In.Num();
		if (Num == 0) return;
		if (Num <= 2 || Tolerance <= KINDA_SMALL_NUMBER)
		{
			Out = In;
			return;
		}

		TArray<bool> Keep;
		Keep.Init(false, Num);
		Keep[0] = true;
		Keep[Num - 1] = true;

		TArray<TPair<int32, int32>> Stack;
		Stack.Push(TPair<int32, int32>(0, Num - 1));

		while (Stack.Num() > 0)
		{
			const TPair<int32, int32> Range = Stack.Pop();
			const int32 First = Range.Key;
			const int32 Last = Range.Value;
			if (Last <= First + 1) continue;

			float MaxDist = 0.f;
			int32 MaxIndex = INDEX_NONE;
			for (int32 i = First + 1; i < Last; ++i)
			{
				const float D = PointSegmentDistance2D(In[i], In[First], In[Last]);
				if (D > MaxDist)
				{
					MaxDist = D;
					MaxIndex = i;
				}
			}

			if (MaxIndex != INDEX_NONE && MaxDist > Tolerance)
			{
				Keep[MaxIndex] = true;
				Stack.Push(TPair<int32, int32>(First, MaxIndex));
				Stack.Push(TPair<int32, int32>(MaxIndex, Last));
			}
		}

		Out.Reserve(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			if (Keep[i]) Out.Add(In[i]);
		}
	}

	/** Total 2D length of a polyline. */
	float PathLength2D(const TArray<FVector>& Path)
	{
		float Total = 0.f;
		for (int32 i = 1; i < Path.Num(); ++i)
		{
			Total += FVector::Dist2D(Path[i - 1], Path[i]);
		}
		return Total;
	}
}

TArray<FVector> ACustomControllerBase::DistributeAlongPath(const TArray<FVector>& Path, int32 NumPoints)
{
	TArray<FVector> Points;
	if (NumPoints <= 0 || Path.Num() == 0)
	{
		return Points;
	}
	if (Path.Num() == 1)
	{
		Points.Init(Path[0], NumPoints);
		return Points;
	}

	// Cumulative arc length, so spacing is even along the CURVE rather than along the chord.
	TArray<float> Cumulative;
	Cumulative.Reserve(Path.Num());
	Cumulative.Add(0.f);
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		Cumulative.Add(Cumulative[i - 1] + FVector::Dist2D(Path[i - 1], Path[i]));
	}
	const float Total = Cumulative.Last();

	Points.Reserve(NumPoints);
	if (Total <= KINDA_SMALL_NUMBER)
	{
		Points.Init(Path[0], NumPoints);
		return Points;
	}

	int32 Segment = 0;
	for (int32 i = 0; i < NumPoints; ++i)
	{
		// Endpoints inclusive: the first and last unit stand on the ends of the drawn stroke.
		const float Target = (NumPoints == 1) ? (Total * 0.5f)
		                                      : (Total * static_cast<float>(i) / static_cast<float>(NumPoints - 1));

		while (Segment < Cumulative.Num() - 2 && Cumulative[Segment + 1] < Target)
		{
			++Segment;
		}

		const float SegLen = Cumulative[Segment + 1] - Cumulative[Segment];
		const float Alpha = (SegLen <= KINDA_SMALL_NUMBER) ? 0.f
		                                                   : FMath::Clamp((Target - Cumulative[Segment]) / SegLen, 0.f, 1.f);
		Points.Add(FMath::Lerp(Path[Segment], Path[Segment + 1], Alpha));
	}

	return Points;
}

void ACustomControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController() || !bFormationLineDragActive)
	{
		return;
	}

	// A menu or the loading screen taking over mid-gesture must not leave a stuck line.
	if (!CameraBase || CameraBase->BlockControls || CameraBase->TabToggled)
	{
		CancelFormationLineDrag();
		return;
	}

	// A box-select started with the other button rewrites SelectedUnits and deselects the group
	// the line was drawn for. Drop the gesture rather than issue it to whatever got boxed.
	if (HUDBase && HUDBase->bSelectFriendly)
	{
		CancelFormationLineDrag();
		return;
	}

	// Bewusst NICHT TraceMouseToGround: der Strahl gegen die Landschaft trifft auf erhoehtem Boden
	// frueher, wodurch der Punkt zur Kamera wandert und die gezeichnete Linie sich mit der
	// Gelaendehoehe verschiebt. Die Ebene auf Starthoehe haelt die Linie genau unter dem Zeiger.
	FVector MouseGround;
	if (TraceMouseToHorizontalPlane(FormationLinePlaneZ, MouseGround))
	{
		UpdateFormationLineDrag(MouseGround);
	}

	// UpdateFormationLineDrag cancels the gesture if the selection collapsed.
	if (!bFormationLineDragActive)
	{
		return;
	}

	if (HUDBase)
	{
		TArray<AUnitBase*> PreviewUnits;
		TArray<FVector> PreviewPath;
		TArray<FVector> PreviewSlots;
		if (IsFormationLineDragValid() && BuildFormationLineOrder(PreviewUnits, PreviewPath, PreviewSlots))
		{
			// Exactly what FinishFormationLineDrag will issue - same helper, same inputs.
			HUDBase->UpdateFormationPath(PreviewPath, PreviewSlots);
		}
		else
		{
			// Below the threshold the gesture is still just a click - show nothing yet.
			HUDBase->ClearFormationLine();
		}
	}

	// The right mouse button has no release event in this project: InputTag.RightClick_Released is
	// declared but never registered in AddAllTags, so a BindActionByTag for it would silently never
	// fire. Polling here is what makes the right-drag possible without authoring a new InputAction,
	// an IMC_Controls row, a ControlAsset row and a native-tag registration.
	//
	// This tests the button's LEVEL, not a falling edge. Input is dispatched in PlayerTick, before
	// AActor::Tick, so a press+release inside one frame (an ordinary fast click, or any long frame)
	// would never be observed as "down" and an edge could therefore never fire - the drag would
	// latch forever with a dashed line trailing the cursor. On the level test that case simply ends
	// the gesture on the next tick with Start == End, which IsFormationLineDragValid rejects, so it
	// degrades cleanly to a plain click.
	//
	// The left button is polled too. Its release does call LeftClickReleasedMass, but that path
	// early-returns while BlockControls is set, which would otherwise leave the drag latched.
	const bool bOwningButtonDown = IsInputKeyDown(bFormationLineDragFromRightMouse ? EKeys::RightMouseButton
	                                                                              : EKeys::LeftMouseButton);
	if (bOwningButtonDown)
	{
		return;
	}

	// Alt-Tab / clicking another window makes UGameViewportClient::LostFocus call FlushPressedKeys,
	// which releases every held key. That looks identical to a real release, so without this check
	// switching away mid-drag would COMMIT an order the player never confirmed. Focus is only
	// consulted here, at the moment of release, so a normal drag is unaffected.
	bool bViewportFocused = true;
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			if (FViewport* Viewport = ViewportClient->Viewport)
			{
				bViewportFocused = Viewport->HasFocus();
			}
		}
	}

	if (!bViewportFocused)
	{
		CancelFormationLineDrag();
		return;
	}

	FinishFormationLineDrag(bFormationLineDragFromRightMouse);
}

bool ACustomControllerBase::IsUnitEligibleForFormationLine(const AUnitBase* Unit) const
{
	if (!Unit || Unit == CameraUnitWithTag) return false;
	if (Unit->UnitState == UnitData::Dead) return false;
	if (!Unit->IsInitialized || !Unit->CanMove) return false;
	// Buildings and construction sites have no business being strung out on a line.
	if (Unit->bIsBuilding || Unit->bIsConstructionUnit) return false;
	if (!Unit->bIsMassUnit) return false;
	return true;
}

bool ACustomControllerBase::IsFormationLineDragValid() const
{
	if (!bFormationLineDragActive || FormationLineDragUnits.Num() < 2)
	{
		return false;
	}
	if (FormationLinePath.Num() < 2)
	{
		return false;
	}

	// Arc length, not start-to-end distance: a tight curve or a hook can cover plenty of ground
	// while its endpoints sit close together, and that is still a deliberate gesture.
	return PathLength2D(FormationLinePath) >= FMath::Max(FormationLineDragThreshold, 1.f);
}

bool ACustomControllerBase::CanStartFormationLineDrag() const
{
	// A line through a single unit is just a move.
	if (SelectedUnits.Num() < 2) return false;

	// Shift queues waypoints and Alt cancels/destroys - both own the click already.
	if (IsShiftPressed || AltIsPressed) return false;

	// Tab overlay swallows the whole left-click path (LeftClickPressedMass early-outs on it).
	if (!CameraBase || CameraBase->TabToggled) return false;

	// Another mouse-driven mode is mid-gesture.
	if (CurrentDraggedAbilityIndicator || CurrentDraggedUnitBase) return false;
	if (SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea) return false;

	// Box-select rebuilds SelectedUnits every frame while it is running, so the count we would
	// distribute along the line is not stable.
	if (HUDBase && HUDBase->bSelectFriendly) return false;

	// Need at least two units that can actually be sent to a slot. A selection of buildings would
	// otherwise arm a drag, draw a full preview and then command nobody.
	int32 EligibleCount = 0;
	for (const AUnitBase* Unit : SelectedUnits)
	{
		if (IsUnitEligibleForFormationLine(Unit) && ++EligibleCount >= 2)
		{
			return true;
		}
	}

	return false;
}

void ACustomControllerBase::BeginFormationLineDrag(const FVector& StartWorld, bool bAttackMove, bool bFromRightMouse)
{
	if (StartWorld.ContainsNaN())
	{
		return;
	}

	// Pressing the other button mid-gesture must not hijack a live drag: that would silently move
	// its origin, flip move/attack-move and hand ownership to a button whose release the first
	// button's release then cannot match.
	if (bFormationLineDragActive)
	{
		return;
	}

	// Snapshot the group now. SelectedUnits can be rewritten before the release (box-select,
	// units dying, HUD re-selection) and the order must still go to the units the line was drawn
	// for. Weak pointers so a unit destroyed mid-drag simply drops out.
	FormationLineDragUnits.Reset();
	FormationLineDragUnits.Reserve(SelectedUnits.Num());
	for (AUnitBase* Unit : SelectedUnits)
	{
		if (Unit)
		{
			FormationLineDragUnits.Add(Unit);
		}
	}

	bFormationLineDragActive = true;
	FormationLinePlaneZ = StartWorld.Z;
	bFormationLineDragIsAttackMove = bAttackMove;
	bFormationLineDragFromRightMouse = bFromRightMouse;
	FormationLineStartWorld = StartWorld;
	// Until the mouse actually moves the path holds one point, so IsFormationLineDragValid stays
	// false and nothing is drawn or issued.
	FormationLineEndWorld = StartWorld;
	FormationLinePath.Reset();
	FormationLinePath.Add(StartWorld);
}

void ACustomControllerBase::UpdateFormationLineDrag(const FVector& CurrentWorld)
{
	if (!bFormationLineDragActive || CurrentWorld.ContainsNaN())
	{
		return;
	}

	FormationLineEndWorld = CurrentWorld;

	if (FormationLinePath.Num() == 0)
	{
		FormationLinePath.Add(CurrentWorld);
		return;
	}

	// Record a new sample only once the cursor has actually travelled, so a still hand does not
	// pack the buffer with duplicates.
	if (FVector::Dist2D(FormationLinePath.Last(), CurrentWorld) < FMath::Max(FormationPathSampleDistance, 1.f))
	{
		return;
	}

	const int32 MaxSamples = FMath::Clamp(FormationPathMaxSamples, 8, 2048);
	if (FormationLinePath.Num() >= MaxSamples)
	{
		// Drop every other interior sample instead of refusing to grow: the stroke keeps its full
		// extent at half the resolution, rather than freezing partway through the drag.
		TArray<FVector> Decimated;
		Decimated.Reserve(FormationLinePath.Num() / 2 + 2);
		Decimated.Add(FormationLinePath[0]);
		for (int32 i = 1; i < FormationLinePath.Num() - 1; i += 2)
		{
			Decimated.Add(FormationLinePath[i]);
		}
		Decimated.Add(FormationLinePath.Last());
		FormationLinePath = MoveTemp(Decimated);
	}

	FormationLinePath.Add(CurrentWorld);
}

void ACustomControllerBase::CancelFormationLineDrag()
{
	bFormationLineDragActive = false;
	bFormationLineDragIsAttackMove = false;
	bFormationLineDragFromRightMouse = false;
	FormationLineDragUnits.Reset();
	FormationLinePath.Reset();

	if (HUDBase)
	{
		HUDBase->ClearFormationLine();
	}
}

void ACustomControllerBase::GetEffectiveFormationPath(int32 NumUnits, float MaxUnitRadius, TArray<FVector>& OutPath) const
{
	// Flatten out the hand wobble so a roughly straight drag collapses to a clean two-point line,
	// while a deliberate curve keeps its shape.
	SimplifyPath2D(FormationLinePath, FMath::Max(FormationPathSimplifyTolerance, 0.f), OutPath);

	for (FVector& P : OutPath)
	{
		P.Z = 0.f;
	}

	if (OutPath.Num() < 2 || !bFormationLineEnforceMinSpacing || NumUnits < 2)
	{
		return;
	}

	const float BaseLength = PathLength2D(OutPath);
	if (BaseLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Two neighbours need (r_a + r_b + GridSpacing) between them. Size from the widest unit so no
	// pair can end up too tight regardless of who lands where.
	const float MaxRadius = (MaxUnitRadius > 0.f) ? MaxUnitRadius : 50.f;
	const float MinStep = 2.f * MaxRadius + GridSpacing;

	// Clamped, otherwise a large selection on a short drag would fling the outer units off the map.
	// Past the clamp units simply stand closer together and Mass avoidance copes.
	const float MaxLength = FMath::Max(FormationLineMaxLength, BaseLength);

	const TArray<FVector> BasePath = OutPath;
	float Required = FMath::Min(MinStep * static_cast<float>(NumUnits - 1), MaxLength);

	// Slots are spaced evenly along the ARC, but two neighbours overlap based on their straight-line
	// distance, and on a curve the chord is shorter than the arc. So requiring arc == MinStep*(N-1)
	// is not enough: measure the real chord gap and stretch until it clears, or until the clamp.
	// Converges in one or two rounds for any reasonable stroke.
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		OutPath = BasePath;

		const float Extra = FMath::Max(Required - BaseLength, 0.f) * 0.5f;
		if (Extra > KINDA_SMALL_NUMBER)
		{
			// Grow at BOTH ends along their terminal tangents. On a straight path this is exactly
			// "widen about the midpoint"; on a curve it extends the curve instead of distorting it.
			const FVector FrontDir = (BasePath[0] - BasePath[1]).GetSafeNormal2D();
			const FVector BackDir = (BasePath.Last() - BasePath[BasePath.Num() - 2]).GetSafeNormal2D();
			if (!FrontDir.IsNearlyZero())
			{
				OutPath.Insert(BasePath[0] + FrontDir * Extra, 0);
			}
			if (!BackDir.IsNearlyZero())
			{
				OutPath.Add(BasePath.Last() + BackDir * Extra);
			}
		}

		if (Required >= MaxLength - KINDA_SMALL_NUMBER)
		{
			return; // at the clamp; accept whatever spacing we get
		}

		const TArray<FVector> Slots = DistributeAlongPath(OutPath, NumUnits);
		if (Slots.Num() < 2)
		{
			return;
		}

		float MinChord = TNumericLimits<float>::Max();
		for (int32 i = 1; i < Slots.Num(); ++i)
		{
			MinChord = FMath::Min(MinChord, FVector::Dist2D(Slots[i - 1], Slots[i]));
		}

		if (MinChord >= MinStep - 1.f || MinChord <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		Required = FMath::Min(Required * (MinStep / MinChord), MaxLength);
	}
}

bool ACustomControllerBase::BuildFormationLineOrder(TArray<AUnitBase*>& OutUnits, TArray<FVector>& OutPath, TArray<FVector>& OutSlots) const
{
	OutUnits.Reset();
	OutPath.Reset();
	OutSlots.Reset();

	// Only units that can really be sent to a slot get one - otherwise buildings and dead units
	// silently consume positions and leave gaps in a line advertised as evenly spaced.
	float MaxRadius = 0.f;
	for (const TWeakObjectPtr<AUnitBase>& WeakUnit : FormationLineDragUnits)
	{
		AUnitBase* Unit = WeakUnit.Get();
		if (!IsUnitEligibleForFormationLine(Unit)) continue;
		OutUnits.Add(Unit);
		if (Unit->GetCapsuleComponent())
		{
			MaxRadius = FMath::Max(MaxRadius, Unit->GetCapsuleComponent()->GetScaledCapsuleRadius() * GridCapsuleMultiplier);
		}
	}

	if (OutUnits.Num() < 2)
	{
		return false;
	}

	GetEffectiveFormationPath(OutUnits.Num(), MaxRadius, OutPath);
	if (OutPath.Num() < 2)
	{
		return false;
	}

	// Order units along the path and hand out slots in the same order, so nobody crosses anybody
	// else's route - which is the whole point of drawing a line by hand.
	const FVector PathStart = OutPath[0];
	const FVector PathDir = (OutPath.Last() - PathStart).GetSafeNormal2D();
	OutUnits.Sort([this, PathDir, PathStart](const AUnitBase& A, const AUnitBase& B)
	{
		const float PA = FVector::DotProduct(GetUnitWorldLocation(&A) - PathStart, PathDir);
		const float PB = FVector::DotProduct(GetUnitWorldLocation(&B) - PathStart, PathDir);
		if (FMath::IsNearlyEqual(PA, PB))
		{
			// Ties would otherwise depend on selection order, which is not stable across frames.
			return A.GetName() < B.GetName();
		}
		return PA < PB;
	});

	OutSlots = DistributeAlongPath(OutPath, OutUnits.Num());

	// Die Linie liegt auf einer waagerechten Ebene; fuer den Marschbefehl braucht jeder Platz aber
	// eine sinnvolle Hoehe. Nur die Z-Komponente kommt vom Navigationsnetz, X und Y bleiben exakt
	// so, wie gezeichnet - sonst waere die Gelaendeabhaengigkeit hier wieder drin.
	if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		for (FVector& Platz : OutSlots)
		{
			FNavLocation Projiziert;
			if (NavSys->ProjectPointToNavigation(Platz, Projiziert, FVector(150.f, 150.f, 5000.f)))
			{
				Platz.Z = Projiziert.Location.Z;
			}
		}
	}

	return OutSlots.Num() == OutUnits.Num();
}

TArray<FVector> ACustomControllerBase::ComputeFormationLinePoints(const TArray<AUnitBase*>& Units) const
{
	const int32 NumUnits = Units.Num();
	if (NumUnits == 0)
	{
		return TArray<FVector>();
	}

	float MaxRadius = 0.f;
	for (const AUnitBase* Unit : Units)
	{
		if (Unit && Unit->GetCapsuleComponent())
		{
			MaxRadius = FMath::Max(MaxRadius, Unit->GetCapsuleComponent()->GetScaledCapsuleRadius() * GridCapsuleMultiplier);
		}
	}

	TArray<FVector> Path;
	GetEffectiveFormationPath(NumUnits, MaxRadius, Path);
	if (Path.Num() < 2)
	{
		return TArray<FVector>();
	}

	return DistributeAlongPath(Path, NumUnits);
}

bool ACustomControllerBase::FinishFormationLineDrag(bool bFromRightMouse)
{
	if (!bFormationLineDragActive)
	{
		return false;
	}

	// Releasing the other button must not end this drag. It also must not cancel it: with
	// SwapAttackMove on, both gestures live on the right button while the left button is still
	// free to box-select, and a left release fires LeftClickReleasedMass every time.
	if (bFormationLineDragFromRightMouse != bFromRightMouse)
	{
		return false;
	}

	const bool bWasValid = IsFormationLineDragValid();
	const bool bAttackMove = bFormationLineDragIsAttackMove;

	if (!bWasValid)
	{
		CancelFormationLineDrag();
		return false;
	}

	// Exactly the same computation the HUD preview used this frame, so the units land on the
	// markers the player was looking at.
	TArray<AUnitBase*> OrderedUnits;
	TArray<FVector> EffectivePath;
	TArray<FVector> LinePoints;
	if (!BuildFormationLineOrder(OrderedUnits, EffectivePath, LinePoints))
	{
		CancelFormationLineDrag();
		return false;
	}

	// Ground-snap and drop anything that lands on a nav modifier, exactly like the normal paths do.
	TArray<AUnitBase*> TargetUnits;
	TArray<FVector> TargetLocs;
	TArray<float> TargetSpeeds;
	TargetUnits.Reserve(OrderedUnits.Num());
	TargetLocs.Reserve(OrderedUnits.Num());
	TargetSpeeds.Reserve(OrderedUnits.Num());

	for (int32 i = 0; i < OrderedUnits.Num(); ++i)
	{
		AUnitBase* Unit = OrderedUnits[i];
		// Re-check: a unit can die between the filter above and here is not possible in one frame,
		// but the weak pointer resolve above is the only thing guaranteeing non-null.
		if (!Unit) continue;

		bool bNavMod = false;
		const FVector Loc = TraceRunLocation(LinePoints[i], bNavMod);
		if (bNavMod) continue;

		TargetUnits.Add(Unit);
		TargetLocs.Add(Loc);
		TargetSpeeds.Add(Unit->Attributes ? Unit->Attributes->GetRunSpeed() : 300.f);
	}

	bFormationLineDragActive = false;
	bFormationLineDragIsAttackMove = false;
	bFormationLineDragFromRightMouse = false;
	FormationLineDragUnits.Reset();
	FormationLinePath.Reset();
	if (HUDBase)
	{
		HUDBase->ClearFormationLine();
	}

	if (TargetUnits.Num() == 0)
	{
		return false;
	}

	if (bAttackMove)
	{
		// Attack-move keeps its own server entry point (it also arms the units to engage en route).
		LeftClickAttackMass(TargetUnits, TargetLocs, true, nullptr);
		return true;
	}

	// Pre-validate on the commanding client so prediction and the server agree.
	//
	// Deliberately NOT AdjustBatchTargetsForNav: that helper is a whole-FORMATION re-solver. One
	// off-navmesh target makes it discard every point and rebuild the current GridFormationShape
	// around the centre, which would silently collapse the line into a blob whenever an endpoint
	// clipped a cliff or a building footprint. Project each point on its own instead.
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		for (FVector& Loc : TargetLocs)
		{
			FNavLocation NavLoc;
			if (NavSys->ProjectPointToNavigation(Loc, NavLoc, NavMeshProjectionExtent))
			{
				Loc = NavLoc.Location;
			}
		}
	}

	// Destination feedback for the order that actually executes. The press-time order already drew
	// indicators at the click point; without these the player would see circles where the units are
	// not going and none where they are.
	for (const FVector& Loc : TargetLocs)
	{
		DrawCircleAtLocation(GetWorld(), Loc, FColor::Green);
	}

	TArray<float> TargetRadii;
	TargetRadii.Reserve(TargetUnits.Num());
	for (AUnitBase* Unit : TargetUnits)
	{
		TargetRadii.Add(Unit->MovementAcceptanceRadius);
		SetUnitState_Replication(Unit, 1);
	}

	Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), TargetUnits, TargetLocs, TargetSpeeds, TargetRadii, false, true, true, true);

	if (!HasAuthority())
	{
		if (UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
		{
			FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
			for (int32 i = 0; i < TargetUnits.Num(); ++i)
			{
				ApplyMovePredictionToUnit(EntityManager, GetWorld(), TargetUnits[i], TargetLocs[i], TargetSpeeds[i], TargetRadii[i], false, true, true);
			}
			EntityManager.FlushCommands();
		}
	}

	if (RunSound && (GetWorld()->GetTimeSeconds() - LastRunSoundTime >= RunSoundDelayTime))
	{
		UGameplayStatics::PlaySound2D(this, RunSound, GetSoundMultiplier());
		LastRunSoundTime = GetWorld()->GetTimeSeconds();
	}

	return true;
}

void ACustomControllerBase::LeftClickReleasedMass()
{
	// Run before the bookkeeping below: FinishFormationLineDrag reads SelectedUnits, and the
	// line below overwrites it from the HUD.
	FinishFormationLineDrag(/*bFromRightMouse=*/false);

	LeftClickIsPressed = false;
	HUDBase->bSelectFriendly = false;
	SelectedUnits = HUDBase->SelectedUnits;

	DropUnitBase();
	int BestIndex = GetHighestPriorityWidgetIndex();
	CurrentUnitWidgetIndex = BestIndex;
	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);
	if (ExtendedCameraBase)
	{
		if(ExtendedCameraBase->TabToggled)
		{
			SetWidgets(BestIndex);
		}
	}
	
}

void ACustomControllerBase::UpdateFogMaskWithCircles(const TArray<FMassEntityHandle>& Entities)
{
    UWorld* World = GetWorld();
    if (!ensure(World)) return;

    // Prepare three parallel arrays
    TArray<FVector_NetQuantize> Positions;
    TArray<float>               WorldRadii;
    TArray<uint8>               UnitTeamIds;

    if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
    {
        FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

        for (const FMassEntityHandle& E : Entities)
        {
            if (!EM.IsEntityValid(E))
                continue;

            const FTransformFragment*               TF       = EM.GetFragmentDataPtr<FTransformFragment>(E);
            const FMassCombatStatsFragment*         StateFrag= EM.GetFragmentDataPtr<FMassCombatStatsFragment>(E);
            const FMassAIStateFragment*             AI       = EM.GetFragmentDataPtr<FMassAIStateFragment>(E);
            const FMassAgentCharacteristicsFragment* CharFrag = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(E);

            if (!TF || !StateFrag || !CharFrag)
                continue;

            // skip dead/despawned (if AI state is present)
            if (AI && StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime)
                continue;

                // 1) world‐loc
                Positions.Add(FVector_NetQuantize(TF->GetTransform().GetLocation()));
                // 2) world‐space sight radius
                WorldRadii.Add(StateFrag->SightRadius);
                // 3) unit’s team
                UnitTeamIds.Add(StateFrag->TeamId);
        }
    }
	
    for (TActorIterator<AFogActor> It(World); It; ++It)
    {
        // Wer seine Maske von aussen bekommt, will von den lebenden Einheiten nichts wissen.
        // Ohne diese Zeile schreiben zwei Quellen abwechselnd in dieselbe Maske - siehe
        // AFogActor::bExternalFogSource.
        if (It->bExternalFogSource)
        {
            continue;
        }

        It->UpdateFogMaskWithCircles_Local(Positions, WorldRadii, UnitTeamIds);
    }
}

void ACustomControllerBase::UpdateMinimap(const TArray<FMassEntityHandle>& Entities)
{
	if (bStopMinimapSearch) return;

	UWorld* World = GetWorld();
	if (!ensure(World)) return;

	// --- Suche/Cache Logik ---
	if (CachedMinimapActor)
	{
		if (CachedMinimapActor->TeamId != SelectableTeamId)
		{
			CachedMinimapActor = nullptr;
		}
	}

	if (!CachedMinimapActor)
	{
		for (TActorIterator<AMinimapActor> It(World); It; ++It)
		{
			if (It->TeamId == SelectableTeamId)
			{
				CachedMinimapActor = *It;
				break;
			}
		}

		if (!CachedMinimapActor)
		{
			if (World->GetTimeSeconds() > MinimapSearchEndTime)
			{
				bStopMinimapSearch = true;
			}
			return;
		}
	}

	// Bereite die Arrays vor.
	TArray<AActor*>             ActorRefs;
	TArray<FVector_NetQuantize> Positions;
	TArray<float>               UnitRadii;
	TArray<float>               FogRadii;
	TArray<uint8>               UnitTeamIds;

	if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

		for (const FMassEntityHandle& E : Entities)
		{
			if (!EM.IsEntityValid(E)) continue;

			// Hole alle notwendigen Fragmente
			FMassActorFragment* ActorFrag = EM.GetFragmentDataPtr<FMassActorFragment>(E);
			const FTransformFragment* TF = EM.GetFragmentDataPtr<FTransformFragment>(E);
			const FMassCombatStatsFragment* StateFrag = EM.GetFragmentDataPtr<FMassCombatStatsFragment>(E);
			
			// Wir brauchen den Aktor und die Basisdaten
			if (!ActorFrag || !TF || !StateFrag) continue;

			AActor* Actor = ActorFrag->GetMutable();
			if (!Actor) continue;

			float MinimapRadius = 150.f;
			bool bIsValidForMinimap = false;

			if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
			{
				const FMassAIStateFragment* AI = EM.GetFragmentDataPtr<FMassAIStateFragment>(E);
				const FMassAgentCharacteristicsFragment* CharFrag = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(E);
				
				if (AI && CharFrag)
				{
					if (StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime) continue;
				}

				if (Unit->GetCapsuleComponent())
				{
					MinimapRadius = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius() * 3.f;
				}
				bIsValidForMinimap = true;
			}
			else if (AEffectArea* Area = Cast<AEffectArea>(Actor))
			{
				MinimapRadius = Area->BaseRadius;
				
				// Falls vorhanden, nutze den aktuellen Radius aus dem ImpactFragment
				if (const FEffectAreaImpactFragment* ImpactFrag = EM.GetFragmentDataPtr<FEffectAreaImpactFragment>(E))
				{
					MinimapRadius = ImpactFrag->CurrentRadius;
				}
				bIsValidForMinimap = true;
			}

			if (bIsValidForMinimap)
			{
				ActorRefs.Add(Actor);
				Positions.Add(FVector_NetQuantize(TF->GetTransform().GetLocation()));
				UnitRadii.Add(MinimapRadius);
				FogRadii.Add(StateFrag->SightRadius);
				UnitTeamIds.Add(StateFrag->TeamId);
			}
		}
	}

	// Nutze den Cache
	if (CachedMinimapActor)
	{
		CachedMinimapActor->UpdateMinimap_Local(ActorRefs, Positions, UnitRadii, FogRadii, UnitTeamIds);
	}
}



void ACustomControllerBase::Multi_SetupPlayerMiniMap_Implementation()
{
	
	if (!CameraBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupPlayerUI: Could not get AExtendedCameraBase pawn."));
		return;
	}

	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);
	if (!ExtendedCameraBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupPlayerUI: Could not get AExtendedCameraBase pawn."));
		return;
	}
	
	if (ExtendedCameraBase->Minimap)
	{
		ExtendedCameraBase->Minimap->InitializeForTeam(SelectableTeamId);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupPlayerUI: MinimapWidgetInstance was not valid on the camera pawn. Has it been created?"));
	}
}


void ACustomControllerBase::Client_ReceiveCooldown_Implementation(int32 AbilityIndex, float RemainingTime)
{

	AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase);
	
	if (ExtendedCameraBase && ExtendedCameraBase->UnitSelectorWidget)
	{
		ExtendedCameraBase->UnitSelectorWidget->SetWidgetCooldown(AbilityIndex, RemainingTime);
	}
}

void ACustomControllerBase::Client_ApplyOwnerAbilityKeyToggle_Implementation(AUnitBase* Unit, const FString& Key, bool bEnable)
{
	if (!IsLocalController())
	{
		return;
	}
	UAbilitySystemComponent* ASC = Unit ? Unit->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}
	UGameplayAbilityBase::ApplyOwnerAbilityKeyToggle_Local(ASC, Key, bEnable);

	// Refresh the unit selector UI immediately
	if (AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase))
	{
		if (ExtendedCameraBase->UnitSelectorWidget)
		{
			ExtendedCameraBase->UnitSelectorWidget->UpdateSelectedUnits();
		}
	}
}

void ACustomControllerBase::Client_ApplyTeamAbilityKeyToggle_Implementation(int32 TeamId, const FString& Key, bool bEnable)
{
	if (!IsLocalController())
	{
		return;
	}

	UGameplayAbilityBase::ApplyTeamAbilityKeyToggle_Local(TeamId, Key, bEnable);

	// Refresh the unit selector UI immediately
	if (AExtendedCameraBase* ExtendedCameraBase = Cast<AExtendedCameraBase>(CameraBase))
	{
		if (ExtendedCameraBase->UnitSelectorWidget)
		{
			ExtendedCameraBase->UnitSelectorWidget->UpdateSelectedUnits();
		}
	}
}

void ACustomControllerBase::Server_RequestCooldown_Implementation(AUnitBase* Unit, int32 AbilityIndex, UGameplayAbilityBase* Ability)
{
	// Ensure we only execute on the server
	if (!HasAuthority() || !Unit || !Ability)
	{
		Client_ReceiveCooldown(AbilityIndex, 0.f);
		return;
	}

	UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
	if (!ASC)
	{
		Client_ReceiveCooldown(AbilityIndex, 0.f);
		return;
	}

	float RTime = 0.f;
	float Duration = 0.f;

	// Wir suchen die Spec für diese Ability
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Ability->GetClass());
	if (Spec)
	{
		// Wir nutzen die instanziierte Ability, da diese Zugriff auf die aktuelle WeaponComponent hat
		TArray<UGameplayAbility*> Instances = Spec->GetAbilityInstances();
		if (Instances.Num() > 0)
		{
			Instances[0]->GetCooldownTimeRemainingAndDuration(Spec->Handle, ASC->AbilityActorInfo.Get(), RTime, Duration);
		}
		else
		{
			// Fallback auf das Default-Objekt, falls keine Instanz aktiv ist
			Ability->GetCooldownTimeRemainingAndDuration(Spec->Handle, ASC->AbilityActorInfo.Get(), RTime, Duration);
		}
	}

	Client_ReceiveCooldown(AbilityIndex, RTime);
}

void ACustomControllerBase::RequestSetTeam(int32 NewTeamId)
{
	// On the client, we call the server RPC.
	// We don't need to check Role here, as calling a Server RPC from the server
	// will just execute the function locally.
	Server_SetPendingTeam(NewTeamId);
}


void ACustomControllerBase::Server_SetPendingTeam_Implementation(int32 TeamId)
{
	// This code now runs ONLY ON THE SERVER.
	UPlayerTeamSubsystem* TeamSubsystem = GetGameInstance()->GetSubsystem<UPlayerTeamSubsystem>();
	if (TeamSubsystem)
	{
		// Because this is running on the server, it's updating the
		// server's authoritative version of the subsystem.
		TeamSubsystem->SetTeamForPlayer(this, TeamId);
	}

	// Reset Minimap Cache for new team
	CachedMinimapActor = nullptr;
}

void ACustomControllerBase::HandleAttackMovePressed()
{
    // 1) get world hit under cursor for ground and pawn
    	
    FHitResult HitPawn;
    // Dieselbe Kaskade wie bei der Auswahl: ohne sie liess sich kein Gebaeude und kein Bauplatz
    // als Ziel eines Angriffsbefehls anklicken (Pawn-Spur laeuft durch beide hindurch).
    GetSelectableHitUnderCursor(HitPawn);
    AUnitBase* TargetUnit = GetUnitFromHitResult(HitPawn);
    AActor* CursorHitActor = TargetUnit ? static_cast<AActor*>(TargetUnit) : (HitPawn.bBlockingHit ? HitPawn.GetActor() : nullptr);

    FHitResult Hit;
    GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

    int32 NumUnits = SelectedUnits.Num();
    if (NumUnits == 0)
    {
        AttackToggled = false;
        return;
    }

	// Wie beim Marschbefehl: ein Angriffsbefehl loest die Einheit von ihrem Wegpunkt.
	ClearWaypointForManualOrder(SelectedUnits);

    // Consistency: Sort units by radius so that formation validation and assignment match Move logic
    TArray<AUnitBase*> UnitsToProcess = SelectedUnits;
    UnitsToProcess.Sort([](const AUnitBase& A, const AUnitBase& B) {
        float RA = 50.0f;
        if (A.GetCapsuleComponent()) RA = A.GetCapsuleComponent()->GetScaledCapsuleRadius();
        float RB = 50.0f;
        if (B.GetCapsuleComponent()) RB = B.GetCapsuleComponent()->GetScaledCapsuleRadius();
        return RA > RB;
    });

    FVector AdjustedLocation = Hit.Location;
    TArray<FVector> Offsets;
    float UsedSpacing;
        
    // If we are not clicking directly on a pawn, validate and adjust the grid on the NavMesh
    if (!HitPawn.bBlockingHit)
    {
        ValidateAndAdjustGridLocation(UnitsToProcess, AdjustedLocation, Offsets, UsedSpacing);
    }
    else
    {
        UsedSpacing = GridSpacing;
        Offsets = ComputeSlotOffsetsDirectional(UnitsToProcess, UsedSpacing, ComputeApproachDirection(UnitsToProcess, AdjustedLocation));
    }

    AWaypoint* BWaypoint = nullptr;
    bool PlayWaypointSound = false;
    bool PlayAttackSound   = false;

    // 3) issue each unit (collect arrays for mass units)
    TArray<AUnitBase*> MassUnits;
    TArray<FVector>    MassLocations;
    TArray<AUnitBase*> BuildingUnits;
    TArray<FVector>    BuildingLocs;
    for (int32 i = 0; i < NumUnits; ++i)
    {
        AUnitBase* U = UnitsToProcess[i];
        if (U == nullptr || U == CameraUnitWithTag) continue;
        
        // OPTIMIZATION: Cast once per iteration using the flag
        ABuildingBase* B = U->bIsBuilding ? static_cast<ABuildingBase*>(U) : nullptr;

        // Buildings and construction sites (rally-waypoint owners) use the exact click location.
        FVector RunLocation = (U->bIsBuilding || U->bIsConstructionUnit) ? (FVector)Hit.Location : AdjustedLocation + Offsets[i];

        bool bNavMod;
        RunLocation = TraceRunLocation(RunLocation, bNavMod);
        if (bNavMod) continue; // || IsLocationInDirtyArea(RunLocation)
        
        bool bSuccess = false;

        // FIX: Check if building can attack and we have a valid target unit
        bool bIsBuildingFocus = (B && B->CanAttack && TargetUnit);

        if (!bIsBuildingFocus)
        {
            SetBuildingWaypoint(RunLocation, U, BWaypoint, PlayWaypointSound, bSuccess);
        }

        if (bSuccess)
        {
            // waypoint placed
            BuildingUnits.Add(U);
            BuildingLocs.Add(RunLocation);
        }
        else
        {
            DrawCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
            if (U->bIsMassUnit)
            {
                MassUnits.Add(U);
                MassLocations.Add(RunLocation);
            }
            else
            {
                // TargetUnit stammt aus GetSelectableHitUnderCursor weiter oben und traegt damit
                // den Hover-Rueckfall - die Umsetzung muss nicht mehr selbst spuren.
                LeftClickAttack(U, RunLocation, TargetUnit);
            }

            PlayAttackSound = true;
        }

        // still fire any dragged ability on each unit
        FireAbilityMouseHit(U, Hit);
    }

    if (BuildingUnits.Num() > 0)
    {
        Server_Batch_SetBuildingWaypoints(BuildingLocs, BuildingUnits);
    }

    if (MassUnits.Num() > 0)
    {
        LeftClickAttackMass(MassUnits, MassLocations, AttackToggled, CursorHitActor);
    }

    AttackToggled = false;

    // 4) play sounds
    if (WaypointSound && PlayWaypointSound)
    {
        UGameplayStatics::PlaySound2D(this, WaypointSound, GetSoundMultiplier());
    }
    if (AttackSound && PlayAttackSound)
    {
        UGameplayStatics::PlaySound2D(this, AttackSound, GetSoundMultiplier());
    }
}

// === Client mirror helpers ===

void ACustomControllerBase::ShowFriendlyHealthbars()
{
	if (UWorld* World = GetWorld())
	{
		LastHealthBarPingTime = World->GetTimeSeconds();
	}
}


void ACustomControllerBase::Client_InitializeMainHUD_Implementation()
{
	if (!IsLocalController()) return;

	if (MainHUDs.Num() > 0)
	{
		TSubclassOf<UUserWidget> HUDClass = nullptr;
		if (MainHUDs.IsValidIndex(SelectableTeamId) && MainHUDs[SelectableTeamId])
		{
			HUDClass = MainHUDs[SelectableTeamId];
		}
		else if (MainHUDs[0])
		{
			HUDClass = MainHUDs[0];
		}

		if (HUDClass)
		{
			MainHUDInstance = CreateWidget<UUserWidget>(this, HUDClass);
			if (MainHUDInstance)
			{
				MainHUDInstance->AddToViewport();
			}
		}
	}

	MainHUDRetryCount = 0;
	GetWorldTimerManager().SetTimer(MainHUDRetryTimerHandle, this, &ACustomControllerBase::Retry_InitializeMainHUD, 0.5f, true);
}

void ACustomControllerBase::Retry_InitializeMainHUD()
{
	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(GetPawn());
	if (Camera)
	{
		bool bAllSuccess = true;
		bAllSuccess &= Camera->InitUnitSelectorWidgetController(this);
		bAllSuccess &= Camera->InitTaggedSelectorWidgetController(this);
		bAllSuccess &= Camera->InitAbiltiyChooserWidgetController(this);
		bAllSuccess &= Camera->InitializeWinConditionDisplay();
		bAllSuccess &= Camera->SetupResourceWidget(this);

		if (bAllSuccess)
		{
			Camera->TabMode = 1;
			Camera->UpdateTabModeUI();
			GetWorldTimerManager().ClearTimer(MainHUDRetryTimerHandle);
			return;
		}
	}

	MainHUDRetryCount++;
	if (MainHUDRetryCount >= 5)
	{
		if (Camera)
		{
			Camera->TabMode = 1;
			Camera->UpdateTabModeUI();
		}
		GetWorldTimerManager().ClearTimer(MainHUDRetryTimerHandle);
	}
}

void ACustomControllerBase::Batch_RemoveRotateToMouseTag()
{
	if (SelectedUnits.Num() > 0)
	{
		BatchSetRotateToMouseTagLocally(SelectedUnits, false);
		Server_BatchSetRotateToMouseTag(SelectedUnits, false);
	}
}



void ACustomControllerBase::Server_SpendAbilityPointsForTier_Implementation(
	FGameplayTag TierTag, EGASAbilityInputID AbilityID, int32 AbilityIndex)
{
	if (!TierTag.IsValid())
	{
		return;
	}

	// Keine Punktvorgabe mehr (02.09.2026): der Chooser ist eine Vorlage je Tierklasse, die
	// automatisch angewandt wird. Frueher wurden punktlose Einheiten uebersprungen - dann galt
	// die Wahl fuer einen Teil der Klasse und fuer den Rest nicht, was von aussen wie ein
	// Fehler aussah.
	int32 Vergeben = 0, Unveraendert = 0;
	for (TActorIterator<AAbilityUnit> It(GetWorld()); It; ++It)
	{
		AAbilityUnit* Unit = *It;
		if (!IsValid(Unit) || Unit->TeamId != SelectableTeamId) continue;
		if (!Unit->UnitTags.HasTag(TierTag)) continue;

		if (Unit->ApplyAbilityFromTemplate(AbilityID, AbilityIndex))
		{
			++Vergeben;
		}
		else
		{
			++Unveraendert;
		}
	}

	// Die Wahl merken, damit spaeter gebaute Einheiten sie automatisch bekommen.
	// Ohne das musste der Spieler nach jeder neuen Einheit erneut klicken.
	if (UWorld* Welt = GetWorld())
	{
		if (UAbilityTemplateSubsystem* Vorlagen = Welt->GetSubsystem<UAbilityTemplateSubsystem>())
		{
			Vorlagen->Merken(SelectableTeamId, TierTag, AbilityIndex, AbilityID);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Tiervergabe] Team %d Tag=%s Slot=%d: %d Einheiten gesetzt, %d schon so."),
		SelectableTeamId, *TierTag.ToString(), AbilityIndex, Vergeben, Unveraendert);
}

bool ACustomControllerBase::TierHasUnitsWithAbilityPoints(FGameplayTag TierTag) const
{
	if (!TierTag.IsValid() || !GetWorld())
	{
		return false;
	}
	for (TActorIterator<AAbilityUnit> It(GetWorld()); It; ++It)
	{
		AAbilityUnit* Unit = *It;
		if (!IsValid(Unit) || Unit->TeamId != SelectableTeamId) continue;
		if (!Unit->UnitTags.HasTag(TierTag)) continue;
		if (Unit->LevelData.AbilityPoints > 0)
		{
			return true;
		}
	}
	return false;
}
