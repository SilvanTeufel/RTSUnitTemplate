// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerController/CustomControllerBase.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Characters/Camera/RLAgent.h"
#include "MassEntitySubsystem.h"     // Needed for FMassEntityManager, UMassEntitySubsystem
#include "MassNavigationFragments.h" // Needed for the engine's FMassMoveTargetFragment
#include "MassMovementFragments.h"  // Needed for EMassMovementAction, FMassVelocityFragment
#include "MassEntityManager.h"  // For FMassEntityManager::FlushCommands
#include "MassExecutor.h"          // Provides Defer() method context typically
#include "MassCommandBuffer.h"      // Needed for FMassDeferredSetCommand, AddFragmentInstance, PushCommand
#include "Mass/UnitNavigationFragments.h"  // For FUnitNavigationPathFragment reset on client prediction
#include "MassReplicationFragments.h" // For FMassNetworkIDFragment
#include "Mass/Replication/RTSWorldCacheSubsystem.h" // For MarkSkipMoveForNetID
#include "NavModifierVolume.h"
#include "Actors/FogActor.h"
#include "Actors/SelectionCircleActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Mass/Signals/MySignals.h"
#include "Mass/UnitMassTag.h"
#include "Actors/MinimapActor.h" 
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
#include "Characters\Unit\UnitBase.h"
#include "Characters/Unit/SpeakingUnit.h"
#include "GameplayTagContainer.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "GAS/AttributeSetBase.h"
#include "Blueprint/UserWidget.h"


void ACustomControllerBase::BeginPlay()
{
	Super::BeginPlay();
}

void ACustomControllerBase::Multi_SetMyTeamUnits_Implementation(const TArray<AActor*>& AllUnits)
{
	if (!IsLocalController()) return;
	
	// TSGameMode->AllUnits is onyl available on Server why we start running on server
	// We Mutlicast to CLient and send him AllUnits as parameter
	TArray<AUnitBase*> NewSelection;
	for (int32 i = 0; i < AllUnits.Num(); i++)
	{
		AUnitBase* Unit = Cast<AUnitBase>(AllUnits[i]);
		// Every Controller can Check his own TeamId
		if (Unit && Unit->GetUnitState() != UnitData::Dead && Unit->TeamId == SelectableTeamId)
		{
			NewSelection.Add(Unit);
		}
	}

	AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(GetPawn());
	if (Camera)
	{
		Camera->SetupResourceWidget(this);
	}
	
}

void ACustomControllerBase::Multi_SetCamLocation_Implementation(FVector NewLocation)
{
	//if (!IsLocalController()) return;
	UE_LOG(LogTemp, Log, TEXT("Multi_HideWidgetWhenNoControl_Implementation - TeamId is now: %d"), SelectableTeamId);
	
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

	UE_LOG(LogTemp, Log, TEXT("Multi_HideEnemyWaypoints_Implementation - TeamId is now: %d"), SelectableTeamId);

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
	UE_LOG(LogTemp, Log, TEXT("[AgentInit] Enter Controller=%s IsLocal=%s HasAuthority=%s Role=%d"), *GetNameSafe(this), IsLocalController() ? TEXT("true") : TEXT("false"), HasAuthority() ? TEXT("true") : TEXT("false"), (int32)GetLocalRole());
	// Only execute for the local controller (Client RPC). Server-spawned AI controllers without owning client will skip here.
	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AgentInit] Skipping AgentInitialization because controller is not local (likely AI without owning client)."));
		return;
	}

	ARLAgent* Camera = Cast<ARLAgent>(CameraBase);
	if (Camera)
	{
		UE_LOG(LogTemp, Log, TEXT("[AgentInit] Calling AgentInitialization on Pawn=%s (Controller TeamId=%d)"), *GetNameSafe(Camera), SelectableTeamId);
		Camera->AgentInitialization();
	}
	else
	{
  UE_LOG(LogTemp, Warning, TEXT("[AgentInit] CameraBase is not ARLAgent. Name=%s Class=%s"), *GetNameSafe(CameraBase), *GetNameSafe(CameraBase ? CameraBase->GetClass() : nullptr));
	}
}


void ACustomControllerBase::CorrectSetUnitMoveTarget_Implementation(UObject* WorldContextObject, AUnitBase* Unit, const FVector& NewTargetLocation, float DesiredSpeed, float AcceptanceRadius, bool AttackT)
{
	if (!Unit) { UE_LOG(LogTemp, Warning, TEXT("[MoveRPC] Rejected: Unit is null")); return; }
		
	if (!Unit->IsInitialized) { UE_LOG(LogTemp, Warning, TEXT("[MoveRPC] Rejected: Unit %s not initialized"), *GetNameSafe(Unit)); return; }
		
	if (!Unit->CanMove) { UE_LOG(LogTemp, Warning, TEXT("[MoveRPC] Rejected: Unit %s CanMove=false"), *GetNameSafe(Unit)); return; }
		
	// Do not accept move orders for dead units
	if (Unit->UnitState == UnitData::Dead) { UE_LOG(LogTemp, Warning, TEXT("[MoveRPC] Rejected: Unit %s is Dead"), *GetNameSafe(Unit)); return; }
		
	if (Unit->CurrentSnapshot.AbilityClass)
	{

		UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
		
		if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) return;

		CancelCurrentAbility(Unit);
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

	FMassEntityHandle MassEntityHandle =  Unit->MassActorBindingComponent->GetMassEntityHandle();
	
    if (!EntityManager.IsEntityValid(MassEntityHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is invalid."), *MassEntityHandle.DebugGetDescription());
        return;
    }

    // Do not process move if entity is marked Dead in Mass
    if (DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateDeadTag::StaticStruct()))
    {
        return;
    }

    // --- Access the PER-ENTITY fragment ---
    FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	
    if (!MoveTargetFragmentPtr || !AiStatePtr)
    {
        UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *MassEntityHandle.DebugGetDescription());
        return;
    }
	
	AiStatePtr->StoredLocation = FinalTargetLocation;
	AiStatePtr->PlaceholderSignal = UnitSignals::Run;
	
	UpdateMoveTarget(*MoveTargetFragmentPtr, FinalTargetLocation, DesiredSpeed, World);
	MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;
	
	EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	
	if (AttackT)
	{
		if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
	}else
	{
		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
	}

	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
	EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
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
		TArray<AUnitBase*> UnitsArr;
		TArray<FVector> LocationsArr;
		TArray<float> SpeedsArr;
		TArray<float> RadiiArr;
		UnitsArr.Add(Unit);
		LocationsArr.Add(FinalTargetLocation);
		SpeedsArr.Add(DesiredSpeed);
		RadiiArr.Add(AcceptanceRadius);
		for (FConstPlayerControllerIterator It = PCWorld->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
			{
				PC->Client_Predict_Batch_CorrectSetUnitMoveTargets(nullptr, UnitsArr, LocationsArr, SpeedsArr, RadiiArr, AttackT);
			}
		}
	}
}

void ACustomControllerBase::Batch_CorrectSetUnitMoveTargets(UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	const TArray<float>& AcceptanceRadii,
	bool AttackT)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[BatchMove] Invalid WorldContextObject (World == nullptr)."));
		return;
	}
	
	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[BatchMove] MassEntitySubsystem not found. Is Mass enabled?"));
		return;
	}
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	bool bNeedsFormationAdjust = false;
	FVector FormationCenter = (NewTargetLocations.Num() > 0) ? NewTargetLocations[0] : FVector::ZeroVector;
	
	if (NavSys && NewTargetLocations.Num() > 0)
	{
		// Check ALL target locations to see if we need formation adjustment
		for (const FVector& Loc : NewTargetLocations)
		{
			FNavLocation NavLoc;
			const bool bOnNav = NavSys->ProjectPointToNavigation(Loc, NavLoc, NavMeshProjectionExtent);
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
				bNeedsFormationAdjust = true;
				break;
			}
		}
	}

	// If needed, adjust formation center & build new per-unit targets using ValidateAndAdjustGridLocation
	TArray<FVector> AdjustedTargets;
	if (bNeedsFormationAdjust)
	{
		TArray<FVector> OutOffsets;
		float UsedSpacing = 0.f;
		ValidateAndAdjustGridLocation(Units, FormationCenter, OutOffsets, UsedSpacing);
		
		// ValidateAndAdjustGridLocation sorts its LocalUnits. 
		// We need to match the offsets back to the input Units order using a stable sort.
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

		AdjustedTargets.SetNum(Units.Num());
		for (int32 i = 0; i < Units.Num(); ++i)
		{
			AdjustedTargets[i] = FormationCenter + TempOffsets.FindRef(Units[i]);
		}
	}

	if (Units.Num() != NewTargetLocations.Num() || Units.Num() != DesiredSpeeds.Num() || Units.Num() != AcceptanceRadii.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BatchMove] Array size mismatch. Units:%d Locs:%d Speeds:%d Radii:%d (processing min count)"), Units.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num(), AcceptanceRadii.Num());
	}

	const int32 Count = FMath::Min(Units.Num(), FMath::Min3(NewTargetLocations.Num(), DesiredSpeeds.Num(), AcceptanceRadii.Num()));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AUnitBase* Unit = Units[Index];
		const FVector& OriginalTargetLocation = NewTargetLocations[Index];
		FVector UseLocation = (bNeedsFormationAdjust && AdjustedTargets.IsValidIndex(Index)) ? AdjustedTargets[Index] : OriginalTargetLocation;
		const float DesiredSpeed = DesiredSpeeds[Index];
		const float AcceptanceRadius = AcceptanceRadii[Index];

		if (!Unit)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%d] Unit is null. Skipping."), Index);
			continue;
		}
		
		if (!Unit->IsInitialized)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] Not initialized. Skipping."), *GetNameSafe(Unit));
			continue;
		}
		if (!Unit->CanMove)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] CanMove == false. Skipping."), *GetNameSafe(Unit));
			continue;
		}
		if (Unit->UnitState == UnitData::Dead)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] UnitState == Dead. Skipping."), *GetNameSafe(Unit));
			continue;
		}

		if (Unit->CurrentSnapshot.AbilityClass)
		{
			UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
			const bool bCancelable = !(AbilityCDO && !AbilityCDO->AbilityCanBeCanceled);
			if (!bCancelable)
			{
				UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] Ability cannot be canceled. Skipping."), *GetNameSafe(Unit));
				continue;
			}
			CancelCurrentAbility(Unit);
		}

		Unit->bHoldPosition = false;

		// Mass entity handle
		FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
		if (!EntityManager.IsEntityValid(MassEntityHandle))
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] Invalid MassEntityHandle. Skipping."), *GetNameSafe(Unit));
			continue;
		}

		// Skip if Mass has Dead tag
		if (DoesEntityHaveTag(EntityManager, MassEntityHandle, FMassStateDeadTag::StaticStruct()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] Mass entity has Dead tag. Skipping."), *GetNameSafe(Unit));
			continue;
		}

		FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
		FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
		if (!MoveTargetFragmentPtr || !AiStatePtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BatchMove][%s] Missing fragments. MoveTarget:%s AI:%s"), *GetNameSafe(Unit), MoveTargetFragmentPtr ? TEXT("OK") : TEXT("NULL"), AiStatePtr ? TEXT("OK") : TEXT("NULL"));
			continue;
		}
		
		AiStatePtr->StoredLocation = UseLocation;
		AiStatePtr->PlaceholderSignal = UnitSignals::Run;
		
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
		EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
		if (AttackT)
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
		EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
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
	bool AttackT)
{
	// Diagnostics: server received batch move
	//UE_LOG(LogTemp, Warning, TEXT("[Server][BatchMove] Server_Batch_CorrectSetUnitMoveTargets: Units=%d"), Units.Num());
	// Apply authoritative changes on the server
	Batch_CorrectSetUnitMoveTargets(WorldContextObject, Units, NewTargetLocations, DesiredSpeeds, AcceptanceRadii, AttackT);


	// Inform every client to predict locally (adds Run tag and updates MoveTarget on the client)
	if (UWorld* PCWorld = GetWorld())
	{
		for (FConstPlayerControllerIterator It = PCWorld->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
			{
				PC->Client_Predict_Batch_CorrectSetUnitMoveTargets(nullptr, Units, NewTargetLocations, DesiredSpeeds, AcceptanceRadii, AttackT);
			}
		}
	}
	
}

void ACustomControllerBase::Client_Predict_Batch_CorrectSetUnitMoveTargets_Implementation(
	UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	const TArray<float>& AcceptanceRadii,
	bool AttackT)
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

	const int32 Count = FMath::Min3(Units.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num());

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
		AUnitBase* Unit = Units[Index];
		if (!Unit)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[BatchMove][Client][%d] Unit is null. Skipping."), Index);
			continue;
		}
		
		if (!Unit->IsInitialized)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[BatchMove][Client][%s] Not initialized. Skipping."), *GetNameSafe(Unit));
			continue;
		}
		if (!Unit->CanMove)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[BatchMove][Client][%s] CanMove == false. Skipping."), *GetNameSafe(Unit));
			continue;
		}
		
		if (Unit->UnitState == UnitData::Dead)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[BatchMove][Client][%s] UnitState == Dead. Skipping."), *GetNameSafe(Unit));
			continue;
		}
		

		Unit->bHoldPosition = false;
		const FVector& NewTargetLocation = NewTargetLocations[Index];
		const float DesiredSpeed = DesiredSpeeds[Index];

		Unit->SetUnitState(UnitData::Run);
		FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
		if (!EntityManager.IsEntityValid(MassEntityHandle))
		{
			//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction][%s] MassEntityHandle invalid. Skipping."), *GetNameSafe(Unit));
			continue;
		}

		FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
		if (!AiStatePtr)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction][%s] Missing FMassAIStateFragment. Skipping."), *GetNameSafe(Unit));
			continue;
		}
		
		AiStatePtr->StoredLocation = NewTargetLocation;
		AiStatePtr->PlaceholderSignal = UnitSignals::Run;
		
		// Add Run tag so client processors include this entity immediately
		EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
		// Populate client prediction fragment with location/speed/radius so client can move without editing MoveTarget
		if (FMassClientPredictionFragment* PredFrag = EntityManager.GetFragmentDataPtr<FMassClientPredictionFragment>(MassEntityHandle))
		{
			PredFrag->Location = NewTargetLocation;
			PredFrag->PredDesiredSpeed = DesiredSpeed;
			PredFrag->PredAcceptanceRadius = AcceptanceRadii[Index];
			PredFrag->bHasData = true;
		}
		// Ensure client won't skip movement this tick
		AiStatePtr->CanMove = true;
		AiStatePtr->HoldPosition = false;
		// Reset local path state to avoid stale path following from previous order
		if (FUnitNavigationPathFragment* PathFrag = EntityManager.GetFragmentDataPtr<FUnitNavigationPathFragment>(MassEntityHandle))
		{
			PathFrag->ResetPath();
			PathFrag->bIsPathfindingInProgress = false;
		}
		//UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Tagging %s: +Run +PredFrag(bHasData=1), Dest=%s, Speed=%.1f, Radius=%.1f, AttackT=%d"),
		//	*GetNameSafe(Unit), *NewTargetLocation.ToString(), DesiredSpeed, AcceptanceRadius, AttackT ? 1 : 0);
		if (AttackT)
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
		EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
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

	FMassEntityHandle MassEntityHandle =  Unit->MassActorBindingComponent->GetMassEntityHandle();
	
    if (!EntityManager.IsEntityValid(MassEntityHandle))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetUnitMoveTarget: Provided Entity Handle %s is invalid."), *MassEntityHandle.DebugGetDescription());
        return;
    }

   	// --- Access the PER-ENTITY fragment ---
   	FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(MassEntityHandle);
   	FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
	
   	if (!MoveTargetFragmentPtr || !AiStatePtr)
   	{
   		UE_LOG(LogTemp, Error, TEXT("SetUnitMoveTarget: Entity %s does not have an FMassMoveTargetFragment."), *MassEntityHandle.DebugGetDescription());
   		return;
   	}
	
   	AiStatePtr->StoredLocation = NewTargetLocation;
   	AiStatePtr->PlaceholderSignal = UnitSignals::Run;
	
   	UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);
	MoveTargetFragmentPtr->SlackRadius = AcceptanceRadius;
	
   	EntityManager.Defer().AddTag<FMassStateRunTag>(MassEntityHandle);
	
   	if (AttackT)
   	{
   		if (AiStatePtr->CanAttack && AiStatePtr->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(MassEntityHandle);
   	}else
   	{
   		EntityManager.Defer().RemoveTag<FMassStateDetectTag>(MassEntityHandle);
   	}
	
   	EntityManager.Defer().RemoveTag<FMassStateIdleTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateChaseTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStateAttackTag>(MassEntityHandle);
   	EntityManager.Defer().RemoveTag<FMassStatePauseTag>(MassEntityHandle);
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
		TArray<AUnitBase*> UnitsArr;
		TArray<FVector> LocationsArr;
		TArray<float> SpeedsArr;
		TArray<float> RadiiArr;
		UnitsArr.Add(Unit);
		LocationsArr.Add(NewTargetLocation);
		SpeedsArr.Add(DesiredSpeed);
		RadiiArr.Add(AcceptanceRadius);
		for (FConstPlayerControllerIterator It = PCWorld->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
			{
				PC->Client_Predict_Batch_CorrectSetUnitMoveTargets(nullptr, UnitsArr, LocationsArr, SpeedsArr, RadiiArr, AttackT);
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
						float Speed = UnitsToLoad[i]->Attributes->GetBaseRunSpeed();
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
				Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocations, BatchSpeeds, BatchRadii, false);
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
			if (ABuildingBase* Building = Cast<ABuildingBase>(FollowTarget))
			{
				if (Building->IsBase)
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
			if (ABuildingBase* Building = Cast<ABuildingBase>(StrongTarget))
			{
				if (Building->IsBase)
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
		if (Unit != FollowTarget)
		{
			Unit->ApplyFollowTarget(FollowTarget);
		}

		if (Unit->IsWorker && FollowTarget)
		{
			if (ABuildingBase* Building = Cast<ABuildingBase>(FollowTarget))
			{
				if (Building->IsBase)
				{
					Unit->Base = Building;
				}
			}
		}
	}

	ApplyTransportTags(Units, FollowTarget);

	if (FollowTarget)
	{
		FVector FollowLocation = FollowTarget->GetActorLocation();

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
		FVector BldCenter = FollowTarget ? FollowTarget->GetActorLocation() : FollowLocation;
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
			const bool bWantsRepair = (Unit->IsWorker && Unit->CanRepair && FollowTarget && FollowTarget->CanBeRepaired);
			if (bWantsRepair)
			{
				continue;
			}
			ValidUnits.Add(Unit);
			NewTargetLocations.Add(FollowLocation);
			float Speed = 300.f;
			if (Unit->Attributes)
			{
				Speed = Unit->Attributes->GetBaseRunSpeed();
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
			Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), ValidUnits, NewTargetLocations, DesiredSpeeds, BatchRadii, AttackT);
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
		if (EntityManager->IsEntityValid(UnitHandle))
		{
			// Clear any existing transport tag as we are issuing a new command
			EntityManager->RemoveTagFromEntity(UnitHandle, FMassTransportProcessorActiveTag::StaticStruct());
		}

		bool bShouldApplyToUnit = false;
		if (bTargetIsTransporter && Unit->CanBeTransported)
		{
			// Check space and transport IDs
			if ((Transporter->CurrentUnitsLoaded + Unit->UnitSpaceNeeded) <= Transporter->MaxTransportUnits)
			{
				if (Unit->TransportId == 0 || Transporter->TransportId == Unit->TransportId)
				{
					bShouldApplyToUnit = true;
				}
			}
		}

		// Repair-specific logic: only allow loading if target is full health OR unit is already in repair state
		if (Unit->CanRepair && FollowTarget)
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
				EntityManager->AddTagToEntity(UnitHandle, FMassTransportProcessorActiveTag::StaticStruct());
				bAnyUnitTagged = true;
			}
		}
	}

	if (bAnyUnitTagged && bTargetIsTransporter)
	{
		const FMassEntityHandle TransporterHandle = Transporter->MassActorBindingComponent->GetMassEntityHandle();
		if (EntityManager->IsEntityValid(TransporterHandle))
		{
			EntityManager->AddTagToEntity(TransporterHandle, FMassTransportProcessorActiveTag::StaticStruct());
			if (FMassTransportFragment* TransportFrag = EntityManager->GetFragmentDataPtr<FMassTransportFragment>(TransporterHandle))
			{
				TransportFrag->DeactivationTimer = 0.f;
			}
		}
	}
}

bool ACustomControllerBase::TryHandleFollowOnRightClick(const FHitResult& HitPawn)
{
	// If we clicked on a unit while having a selection, assign follow or attack and early return
	if (SelectedUnits.Num() > 0 && HitPawn.bBlockingHit)
	{
		if (!Cast<AConstructionUnit>(HitPawn.GetActor()))
		{
			if (AUnitBase* HitUnit = Cast<AUnitBase>(HitPawn.GetActor()))
			{
				const bool bFriendly = (HitUnit->TeamId == SelectableTeamId);
				if (bFriendly)
				{
					Server_SetUnitsFollowTarget(SelectedUnits, HitUnit);
					return true;
				}
				else
				{
					// If it's an enemy unit, issue an attack command (chase/focus logic)
					TArray<FVector> Locations;
					for (int32 i = 0; i < SelectedUnits.Num(); ++i)
					{
						Locations.Add(HitPawn.Location);
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

void ACustomControllerBase::RightClickPressedMass()
{
	if (SwapAttackMove && AttackToggled)
	{
		HandleAttackMovePressed();
		AttackToggled = false;
		return;
	}
	AttackToggled = false;

	FHitResult HitPawn;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitPawn);
	

	
	FHitResult Hit;
	if (TryHandleFollowOnRightClick(HitPawn))
	{
		return;
	}

	if (!SelectedUnits.Num() || !SelectedUnits[0] || !SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
		if (!CheckClickOnWorkArea(Hit))
		{
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
	if (NumUnits == 0) return;

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

		FVector RunLocation = AdjustedLocation + Offsets[i];

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
				MassUnits.Add(U);
				MassLocations.Add(RunLocation);
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

TArray<FVector> ACustomControllerBase::ComputeSlotOffsets(const TArray<AUnitBase*>& Units, float Spacing) const
{
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
    FVector TrueCenter((LeftEdge + RightEdge) * 0.5f, (TopEdge + BottomEdge) * 0.5f, 0.0f);

    // 6. Generate offsets
    TArray<FVector> Offsets;
    Offsets.Reserve(NumUnits);
    for (int32 i = 0; i < NumUnits; ++i)
    {
        int32 Row = i / GridSize;
        int32 Col = i % GridSize;
        Offsets.Add(FVector(XPositions[Col], YPositions[Row], 0.f) - TrueCenter);
    }

    return Offsets;
}

TArray<TArray<float>> ACustomControllerBase::BuildCostMatrix(
    const TArray<AUnitBase*>& Units,
    const TArray<FVector>& SlotOffsets,
    const FVector& TargetCenter) const
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
    TArray<TArray<float>> Cost;
    Cost.SetNum(N);
    for (int32 i = 0; i < N; ++i)
    {
        float UnitR = Radii[i];
        FVector UnitLoc = GetUnitWorldLocation(Units[i]);

        Cost[i].SetNum(N);
        for (int32 j = 0; j < N; ++j)
        {
            int32 Row = j / GridSize;
            int32 Col = j % GridSize;
            float Capacity = FMath::Min(MaxR_Col[Col], MaxR_Row[Row]);

            FVector SlotWorld = TargetCenter + SlotOffsets[j];
            float DistSq = FVector::DistSquared(UnitLoc, SlotWorld);

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

bool ACustomControllerBase::ShouldRecalculateFormation() const
{
    if (bForceFormationRecalculation) return true;
    if (SelectedUnits.Num() != LastFormationUnits.Num()) return true;
    TSet<TWeakObjectPtr<AUnitBase>> LastSet(LastFormationUnits);
    for (AUnitBase* U : SelectedUnits)
        if (!LastSet.Contains(U)) return true;
    return false;
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

    // 2. Compute non-uniform offsets tailored for these units
    auto Offsets = ComputeSlotOffsets(SortedUnits, Spacing);
    
    // 3. Match units to slots. By including radius info in BuildCostMatrix, we ensure big units get big slots.
    auto Cost = BuildCostMatrix(SortedUnits, Offsets, TargetCenter);
    auto Assign = SolveHungarian(Cost);

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
    if (!NavSys || Units.Num() == 0) 
    {
        OutOffsets = ComputeSlotOffsets(Units, OutSpacing);
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
            OutOffsets = ComputeSlotOffsets(LocalUnits, OutSpacing);
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
        OutOffsets = ComputeSlotOffsets(LocalUnits, OutSpacing);
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
	for (AUnitBase* U : SelectedUnits)
	{
		if (!U) continue;
		if (!U->bHoldPosition)
			SetHoldPositionOnUnit(U);
	}
}

void ACustomControllerBase::SetHoldPositionOnUnit_Implementation(AUnitBase* Unit)
{
	Unit->bHoldPosition = true;
}

void ACustomControllerBase::RunUnitsAndSetWaypointsMass(FHitResult Hit)
{
    // 1. Setup
    if (SelectedUnits.Num() == 0) return;

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
				ABuildingBase* BuildingBase = Cast<ABuildingBase>(U);
				if (!BuildingBase || (!BuildingBase->HasWaypoint && BuildingBase->CancelsAbilityOnRightClick))
					CancelCurrentAbility(U);
			}
    	}

    	if (UnitIsValid)
    	{
    		FVector Off = UnitFormationOffsets.FindRef(U);
    		Finals.Add(U, AdjustedLocation + Off);
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
        if (bNavMod) continue; // || IsLocationInDirtyArea(Loc)

        U->RemoveFocusEntityTarget();
        U->SetRdyForTransport(false);

        float Speed = U->Attributes->GetBaseRunSpeed();
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
            DrawCircleAtLocation(GetWorld(), Loc, FColor::Green);
            if (!U->IsInitialized || !U->CanMove) continue;
            if (U->bIsMassUnit)
            {
                if (U->GetUnitState() != UnitData::Run)
                {
                    BatchUnits.Add(U);
                    BatchLocs.Add(Loc);
                    BatchSpeeds.Add(Speed);
                }
                RightClickRunShift(U, Loc);
                SetUnitState_Replication(U, 1);
            }
            PlayRun = true;
        }
        else
        {
            DrawCircleAtLocation(GetWorld(), Loc, FColor::Green);
            if (!U->IsInitialized || !U->CanMove) continue;
            if (U->bIsMassUnit)
            {
                BatchUnits.Add(U);
                BatchLocs.Add(Loc);
                BatchSpeeds.Add(Speed);
                SetUnitState_Replication(U, 1);
            }
            else if (UseUnrealEnginePathFinding)
            {
                RightClickRunUEPF(U, Loc, true);
            }
            else
            {
                RightClickRunDijkstraPF(U, Loc, -1);
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
    	TArray<float> BatchRadii;
    	for (AUnitBase* Unit : BatchUnits)
    	{
    		BatchRadii.Add(Unit->MovementAcceptanceRadius);
    	}
    	Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocs, BatchSpeeds, BatchRadii, false);
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
        HandleAttackMovePressed();
    }
    else
    {
        FHitResult HitPawn;
        GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitPawn);

        // Only call the server if we previously activated an ability via keyboard
        if (bUsedKeyboardAbilityBeforeClick)
        {
            bUsedKeyboardAbilityBeforeClick = false; // consume the flag
            // Send client work area transform (if any) to ensure server has the same placement
            bool bHasClientWorkAreaTransform = false;
            FTransform ClientWorkAreaTransform;
            if (SelectedUnits.Num() > 0 && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
            {
                bHasClientWorkAreaTransform = true;
                ClientWorkAreaTransform = SelectedUnits[0]->CurrentDraggedWorkArea->GetActorTransform();
            }
            Server_HandleAbilityUnderCursor(SelectedUnits, HitPawn, WorkAreaIsSnapped, DropWorkAreaFailedSound, bHasClientWorkAreaTransform, ClientWorkAreaTransform);
        }
        else
        {
            // Skip server and just continue with selection locally
            Client_ContinueSelectionAfterAbility_Implementation(HitPawn);
        }
        return;
    }
	
}

void ACustomControllerBase::Server_HandleAbilityUnderCursor_Implementation(const TArray<AUnitBase*>& Units, const FHitResult& HitPawn, bool bWorkAreaIsSnapped, USoundBase* InDropWorkAreaFailedSound, bool bHasClientWorkAreaTransform, FTransform ClientWorkAreaTransform)
{
    // Ensure server has the same transform for the dragged work area as the client
    if (bHasClientWorkAreaTransform && Units.Num() > 0 && Units[0] && Units[0]->CurrentDraggedWorkArea)
    {
        Units[0]->CurrentDraggedWorkArea->SetActorTransform(ClientWorkAreaTransform);
    }
    // Try to drop any active work area for the first unit using the new parameterized variant
    if (Units.Num() > 0 && Units[0] && Units[0]->CurrentDraggedWorkArea)
    {
        if (!Units[0]->CurrentDraggedWorkArea->InstantDrop) DropWorkAreaForUnit(Units[0], bWorkAreaIsSnapped, InDropWorkAreaFailedSound);
    }

    bool AbilityFired = false;
    bool AbilityUnSynced = false;

    for (AUnitBase* U : Units)
    {
        if (U && U->CurrentSnapshot.AbilityClass && U->CurrentDraggedAbilityIndicator)
        {
            FireAbilityMouseHit(U, HitPawn);
            AbilityFired = true;
        }
        else
        {
            AbilityUnSynced = true;
        }
    }

    if (AbilityFired && !AbilityUnSynced)
    {
        // Early exit: abilities were processed server-side for all units; no client-side selection follow-up
        return;
    }

    // Ask owning client to continue with selection handling
    Client_ContinueSelectionAfterAbility(HitPawn);
}

void ACustomControllerBase::Client_ContinueSelectionAfterAbility_Implementation(const FHitResult& HitPawn)
{
    // if we hit a pawn, try to select it (client-side UI and input state)
    if (HitPawn.bBlockingHit && HUDBase)
    {
        AActor* HitActor = HitPawn.GetActor();
        if (!HitActor->IsA(ALandscape::StaticClass()))
            ClickedActor = HitActor;
        else
            ClickedActor = nullptr;

        AUnitBase* HitUnit = Cast<AUnitBase>(HitActor);
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
                HUDBase->SetUnitSelected(HitUnit);
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
			Unit->FocusEntityTarget(TargetUnitBase);
			Unit->SetRdyForTransport(false);
			// Unit->TransportId = 0;
			SetUnitState_Replication(Unit, 3);
			Unit->SwitchEntityTagByState(UnitData::Chase, Unit->UnitStatePlaceholder);
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

		float Speed = Unit->Attributes->GetBaseRunSpeed();
		SetUnitState_Replication(Unit, 1);
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
		Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocations, BatchSpeeds, BatchRadii, AttackT);
	}
}

void ACustomControllerBase::LeftClickReleasedMass()
{
	
	LeftClickIsPressed = false;
	HUDBase->bSelectFriendly = false;
	SelectedUnits = HUDBase->SelectedUnits;

	DropUnitBase();
	int BestIndex = GetHighestPriorityWidgetIndex();
	CurrentUnitWidgetIndex = BestIndex;
	//UpdateSelectionCircles();
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
        It->UpdateFogMaskWithCircles_Local(Positions, WorldRadii, UnitTeamIds);
    }
}

// In CustomControllerBase.cpp

void ACustomControllerBase::UpdateMinimap(const TArray<FMassEntityHandle>& Entities)
{
    UWorld* World = GetWorld();
    if (!ensure(World)) return;

    // Bereite die Arrays vor. NEU: Ein Array für die Aktor-Referenzen.
    TArray<AUnitBase*>             ActorRefs;
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
          const FMassAIStateFragment* AI = EM.GetFragmentDataPtr<FMassAIStateFragment>(E);
          const FMassAgentCharacteristicsFragment* CharFrag = EM.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(E);

          // Wir brauchen den Aktor, also ist das ActorFragment jetzt zwingend erforderlich
          if (!ActorFrag || !TF || !StateFrag || !AI || !CharFrag) continue;
          if (StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime) continue;

       	AActor* UnitActor = ActorFrag->GetMutable();
       	if (!UnitActor) continue;
       	// --- NEUE LOGIK ZUR ERMITTLUNG DES DYNAMISCHEN RADIUS ---
       	float CapsuleRadius = 150.f; // Wir verwenden 150.f als Fallback-Wert.
          
       	// Wir casten zum ACharacter, da dieser standardmäßig eine Kapsel hat.
       	// Wenn Ihre AUnitBase von ACharacter erbt, ist dies der richtige Weg.
       	AUnitBase* Unit = Cast<AUnitBase>(UnitActor);
       	if (Unit && Unit->GetCapsuleComponent())
       	{
       		// GetScaledCapsuleRadius() berücksichtigt die Skalierung des Aktors.
       		CapsuleRadius = Unit->GetCapsuleComponent()->GetScaledCapsuleRadius()*3.f;
       	}
       	
       	if (!Unit) continue;
       	// --- ENDE DER NEUEN LOGIK ---
          // --- Fülle die Arrays ---
          ActorRefs.Add(Unit); // NEU: Füge den Aktor-Pointer hinzu
          Positions.Add(FVector_NetQuantize(TF->GetTransform().GetLocation()));
          UnitRadii.Add(CapsuleRadius); 
          FogRadii.Add(StateFrag->SightRadius);
          UnitTeamIds.Add(StateFrag->TeamId);
       }
    }

    // Rufe den Multicast auf allen MinimapActors auf und sende ALLE Arrays.
    for (TActorIterator<AMinimapActor> It(World); It; ++It)
    {
       It->UpdateMinimap_Local(ActorRefs, Positions, UnitRadii, FogRadii, UnitTeamIds);
    }
}


void ACustomControllerBase::UpdateSelectionCircles()
{
	UWorld* World = GetWorld();
	if (!ensure(World)) return;

	TArray<FVector_NetQuantize> Positions;
	TArray<float> WorldRadii;
    
	if (UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();

		for (AUnitBase* SelectedUnit : SelectedUnits)
		{
			if (!SelectedUnit) continue;

			FMassEntityHandle Entity = SelectedUnit->MassActorBindingComponent->GetEntityHandle();
			if (!EM.IsEntityValid(Entity)) continue;
            
			const FTransformFragment* TransformFragment = EM.GetFragmentDataPtr<FTransformFragment>(Entity);
			if (!TransformFragment) continue;

			// Use a default or calculated radius for the selection circle
			float SelectionRadius = 50.f; // Example radius, adjust as needed

			Positions.Add(FVector_NetQuantize(TransformFragment->GetTransform().GetLocation()));
			WorldRadii.Add(SelectionRadius);
		}
	}
    
	// Multicast to all SelectionCircleActors
	for (TActorIterator<ASelectionCircleActor> It(World); It; ++It)
	{
		if (It->TeamId == SelectableTeamId)
			It->Multicast_UpdateSelectionCircles(Positions, WorldRadii);
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
		UE_LOG(LogTemp, Log, TEXT("[Client_ApplyOwnerAbilityKeyToggle] Missing ASC. Unit=%s Key='%s' Enable=%s"), *GetNameSafe(Unit), *Key, bEnable ? TEXT("true") : TEXT("false"));
		return;
	}
	UGameplayAbilityBase::ApplyOwnerAbilityKeyToggle_Local(ASC, Key, bEnable);
	UE_LOG(LogTemp, Log, TEXT("[Client_ApplyOwnerAbilityKeyToggle] Applied on client. Unit=%s ASC=%s Key='%s' Enable=%s"), *GetNameSafe(Unit), *GetNameSafe(ASC), *Key, bEnable ? TEXT("true") : TEXT("false"));

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
	if (!HasAuthority())
	{
		return;
	}

	// Validate inputs
	if (!Unit || !Ability)
	{
		UE_LOG(LogTemp, Warning, TEXT("Server_RequestCooldown: Invalid Unit or Ability (Unit=%p, Ability=%p)"), Unit, Ability);
		Client_ReceiveCooldown(AbilityIndex, 0.f);
		return;
	}

	UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("Server_RequestCooldown: Unit has no AbilitySystemComponent."));
		Client_ReceiveCooldown(AbilityIndex, 0.f);
		return;
	}

	// Get cooldown GameplayEffect class from the ability
	UGameplayEffect* CooldownGEInstance = Ability->GetCooldownGameplayEffect();
	if (!CooldownGEInstance)
	{
		Client_ReceiveCooldown(AbilityIndex, 0.f);
		return;
	}

	TSubclassOf<UGameplayEffect> CooldownGEClass = CooldownGEInstance->GetClass();
	if (!CooldownGEClass)
	{
		Client_ReceiveCooldown(AbilityIndex, 0.f);
		return;
	}

	FGameplayEffectQuery CooldownQuery;
	CooldownQuery.EffectDefinition = CooldownGEClass;

	TArray<FActiveGameplayEffectHandle> ActiveCooldownHandles = ASC->GetActiveEffects(CooldownQuery);

	float RTime = 0.f;
	if (ActiveCooldownHandles.Num() > 0)
	{
		// Assuming only one cooldown effect per ability, take the first handle
		const FActiveGameplayEffectHandle ActiveHandle = ActiveCooldownHandles[0];
		const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(ActiveHandle);
		if (ActiveEffect)
		{
			UWorld* World = Unit->GetWorld();
			const float CurrentTime = World ? World->GetTimeSeconds() : 0.f;
			RTime = ActiveEffect->GetTimeRemaining(CurrentTime);
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
}

void ACustomControllerBase::HandleAttackMovePressed()
{
    // 1) get world hit under cursor for ground and pawn
    	
    FHitResult HitPawn;
    GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitPawn);
    AActor* CursorHitActor = HitPawn.bBlockingHit ? HitPawn.GetActor() : nullptr;


    	
    FHitResult Hit;
    GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);

    int32 NumUnits = SelectedUnits.Num();
    if (NumUnits == 0) return;

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
        Offsets = ComputeSlotOffsets(UnitsToProcess, UsedSpacing);
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
        
        // apply the same slot-by-index approach
        FVector RunLocation = AdjustedLocation + Offsets[i];

        bool bNavMod;
        RunLocation = TraceRunLocation(RunLocation, bNavMod);
        if (bNavMod) continue; // || IsLocationInDirtyArea(RunLocation)
        
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
                MassUnits.Add(U);
                MassLocations.Add(RunLocation);
            }
            else
            {
                LeftClickAttack(U, RunLocation);
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
	TArray<AUnitBase*> UnitsToShow;
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (Unit && Unit->TeamId == SelectableTeamId && Unit->IsOnViewport)
		{
			if (Unit->Attributes && (Unit->Attributes->GetHealth() < Unit->Attributes->GetMaxHealth() || 
				Unit->Attributes->GetShield() < Unit->Attributes->GetMaxShield()))
			{
				UnitsToShow.Add(Unit);
			}
		}
	}
	if (UnitsToShow.Num() > 0)
	{
		Server_ShowFriendlyHealthbars(UnitsToShow);
	}
}

void ACustomControllerBase::Server_ShowFriendlyHealthbars_Implementation(const TArray<AUnitBase*>& Units)
{
	for (AUnitBase* Unit : Units)
	{
		if (Unit)
		{
			Unit->OpenHealthWidget = true;
			Unit->bShowLevelOnly = false;
			GetWorld()->GetTimerManager().SetTimer(Unit->HealthWidgetTimerHandle, Unit, &ALevelUnit::HideHealthWidget, Unit->HealthWidgetDisplayDuration, false);
		}
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

