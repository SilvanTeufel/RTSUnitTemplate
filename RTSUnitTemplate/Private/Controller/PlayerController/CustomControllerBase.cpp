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
#include "NavAreas/NavArea_Null.h"
#include "NavMesh/RecastNavMesh.h"
#include "System/PlayerTeamSubsystem.h"
#include "TimerManager.h"
#include "Templates/Function.h"


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

	// And we can create the FogManager
	for (int32 i = 0; i < NewSelection.Num(); i++)
	{
		if (NewSelection[i] && NewSelection[i]->TeamId == SelectableTeamId)
		{
			NewSelection[i]->IsMyTeam = true;

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
	// Only execute for the local controller
	if (!IsLocalController())
	{
		return;
	}

	ARLAgent* Camera = Cast<ARLAgent>(CameraBase);
	if (Camera)
		Camera->AgentInitialization();
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
	
	AiStatePtr->StoredLocation = NewTargetLocation;
	AiStatePtr->PlaceholderSignal = UnitSignals::Run;

	UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);
	
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
}

void ACustomControllerBase::Batch_CorrectSetUnitMoveTargets(UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	float AcceptanceRadius,
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

	if (Units.Num() != NewTargetLocations.Num() || Units.Num() != DesiredSpeeds.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BatchMove] Array size mismatch. Units:%d Locs:%d Speeds:%d (processing min count)"), Units.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num());
	}

	const int32 Count = FMath::Min3(Units.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AUnitBase* Unit = Units[Index];
		const FVector& NewTargetLocation = NewTargetLocations[Index];
		const float DesiredSpeed = DesiredSpeeds[Index];

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
		
		AiStatePtr->StoredLocation = NewTargetLocation;
		AiStatePtr->PlaceholderSignal = UnitSignals::Run;
		
		UpdateMoveTarget(*MoveTargetFragmentPtr, NewTargetLocation, DesiredSpeed, World);

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
		EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);
		
	}
}

void ACustomControllerBase::Server_Batch_CorrectSetUnitMoveTargets_Implementation(
	UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	float AcceptanceRadius,
	bool AttackT)
{
	// Diagnostics: server received batch move
	UE_LOG(LogTemp, Warning, TEXT("[Server][BatchMove] Server_Batch_CorrectSetUnitMoveTargets: Units=%d"), Units.Num());
	// Apply authoritative changes on the server
	Batch_CorrectSetUnitMoveTargets(WorldContextObject, Units, NewTargetLocations, DesiredSpeeds, AcceptanceRadius, AttackT);

	// Inform every client to predict locally (adds Run tag and updates MoveTarget on the client)
	if (UWorld* PCWorld = GetWorld())
	{
		for (FConstPlayerControllerIterator It = PCWorld->GetPlayerControllerIterator(); It; ++It)
		{
			if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
			{
				PC->Client_Predict_Batch_CorrectSetUnitMoveTargets(nullptr, Units, NewTargetLocations, DesiredSpeeds, AcceptanceRadius, AttackT);
			}
		}
	}
}

void ACustomControllerBase::Client_Predict_Batch_CorrectSetUnitMoveTargets_Implementation(
	UObject* WorldContextObject,
	const TArray<AUnitBase*>& Units,
	const TArray<FVector>& NewTargetLocations,
	const TArray<float>& DesiredSpeeds,
	float AcceptanceRadius,
	bool AttackT)
{
	UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Received batch prediction request: Units=%d"), Units.Num());
	// Run prediction only on non-authority (clients). Avoid double-applying on listen servers.
	if (HasAuthority())
	{
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
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	const int32 Count = FMath::Min3(Units.Num(), NewTargetLocations.Num(), DesiredSpeeds.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AUnitBase* Unit = Units[Index];
		if (!Unit)
		{
			continue;
		}

		const FVector& NewTargetLocation = NewTargetLocations[Index];
		const float DesiredSpeed = DesiredSpeeds[Index];

		FMassEntityHandle MassEntityHandle = Unit->MassActorBindingComponent ? Unit->MassActorBindingComponent->GetMassEntityHandle() : FMassEntityHandle();
		if (!EntityManager.IsEntityValid(MassEntityHandle))
		{
			continue;
		}

		FMassAIStateFragment* AiStatePtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(MassEntityHandle);
		if (!AiStatePtr)
		{
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
			PredFrag->PredAcceptanceRadius = AcceptanceRadius;
			PredFrag->bHasData = true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Tagging %s: +Run +PredFrag(bHasData=1), Dest=%s, Speed=%.1f, Radius=%.1f, AttackT=%d"),
			*GetNameSafe(Unit), *NewTargetLocation.ToString(), DesiredSpeed, AcceptanceRadius, AttackT ? 1 : 0);
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
		EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(MassEntityHandle);
		EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(MassEntityHandle);
	}
	// Ensure deferred commands (tags added/removed) are applied immediately so prediction is visible to processors
	EntityManager.FlushCommands();
	UE_LOG(LogTemp, Warning, TEXT("[Client][Prediction] Flushed deferred Mass commands for batch (%d units)"), Count);
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
					
					if (!SelectedUnits[i]->IsInitialized) UnitIsValid = false;
					if (!SelectedUnits[i]->CanMove) UnitIsValid = false;
				
					if (SelectedUnits[i]->CurrentSnapshot.AbilityClass)
					{
					
						UGameplayAbilityBase* AbilityCDO = SelectedUnits[i]->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
						
						if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled) UnitIsValid = false;
						else CancelCurrentAbility(SelectedUnits[i]);
					}

					if (SelectedUnits[i]->bIsMassUnit && UnitIsValid)
					{
						float Speed = SelectedUnits[i]->Attributes->GetBaseRunSpeed();
						// Accumulate for batched RPC instead of sending one RPC per unit
						BatchUnits.Add(SelectedUnits[i]);
						BatchLocations.Add(UnitsToLoad[i]->RunLocation);
						BatchSpeeds.Add(Speed);
						SetUnitState_Replication(SelectedUnits[i], 1);
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
    Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocations, BatchSpeeds, 40.f, false);
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

bool ACustomControllerBase::CheckClickOnTransportUnitMass(FHitResult Hit_Pawn)
{
	if (!Hit_Pawn.bBlockingHit) return false;


		AActor* HitActor = Hit_Pawn.GetActor();
		
		AUnitBase* UnitBase = Cast<AUnitBase>(HitActor);
	
		if (!UnitBase || !UnitBase->CanBeSelected) return false;
	
		if (UnitBase && UnitBase->IsATransporter){
			LoadUnitsMass(SelectedUnits, UnitBase);
			
			UnitBase->RemoveFocusEntityTarget();
			TArray<AUnitBase*> NewSelection;

			NewSelection.Emplace(UnitBase);
			
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[this, NewSelection]()
				{
					Client_UpdateHUDSelection(NewSelection, SelectableTeamId);
				},
				0.25f,  // Delay time in seconds (change as needed)
				false  // Do not loop
			);
			return true;
		}
	return false;
}

void ACustomControllerBase::RightClickPressedMass()
{
	
	AttackToggled = false;
	FHitResult Hit;
	GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, Hit);
	
	if (!CheckClickOnTransportUnitMass(Hit))
	{
		if(!SelectedUnits.Num() || !SelectedUnits[0]->CurrentDraggedWorkArea)
		{
			GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
			if(!CheckClickOnWorkArea(Hit))
			{
					RunUnitsAndSetWaypointsMass(Hit);
			}
		}
	}


	if (SelectedUnits.Num() && SelectedUnits[0] && SelectedUnits[0]->CurrentDraggedWorkArea)
	{
		DestroyDraggedArea(SelectedUnits[0]);
	}
	
}

// Helper to get a unit's world location (actor or ISM)
FVector ACustomControllerBase::GetUnitWorldLocation(const AUnitBase* Unit) const
{
    if (!Unit) return FVector::ZeroVector;

	return Unit->GetMassActorLocation();
}

TArray<FVector> ACustomControllerBase::ComputeSlotOffsets(int32 NumUnits) const
{
    int32 GridSize = ComputeGridSize(NumUnits);
    float Half = (GridSize - 1) * GridSpacing / 2.0f;
    FVector Center(Half, Half, 0.f);
    TArray<FVector> Offsets;
    Offsets.Reserve(NumUnits);
    for (int32 i = 0; i < NumUnits; ++i)
    {
        int32 Row = i / GridSize;
        int32 Col = i % GridSize;
        Offsets.Add(FVector(Col * GridSpacing, Row * GridSpacing, 0.f) - Center);
    }
    return Offsets;
}

TArray<TArray<float>> ACustomControllerBase::BuildCostMatrix(
    const TArray<FVector>& UnitPositions,
    const TArray<FVector>& SlotOffsets,
    const FVector& TargetCenter) const
{
    int32 N = UnitPositions.Num();
    TArray<TArray<float>> Cost;
    Cost.SetNum(N);
    for (int32 i = 0; i < N; ++i)
    {
        Cost[i].SetNum(N);
        for (int32 j = 0; j < N; ++j)
        {
            FVector SlotWorld = TargetCenter + SlotOffsets[j];
            Cost[i][j] = FVector::DistSquared(UnitPositions[i], SlotWorld);
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

void ACustomControllerBase::RecalculateFormation(const FVector& TargetCenter)
{
    int32 N = SelectedUnits.Num();
    UnitFormationOffsets.Empty();
    LastFormationUnits.Empty();

    auto Offsets = ComputeSlotOffsets(N);
    TArray<FVector> Positions;
    Positions.Reserve(N);
    for (AUnitBase* U : SelectedUnits)
        Positions.Add(GetUnitWorldLocation(U));

    auto Cost = BuildCostMatrix(Positions, Offsets, TargetCenter);
    auto Assign = SolveHungarian(Cost);

    for (int32 i = 0; i < N; ++i)
    {
        UnitFormationOffsets.Add(SelectedUnits[i], Offsets[Assign[i]]);
        LastFormationUnits.Add(SelectedUnits[i]);
    }
    bForceFormationRecalculation = false;
}

void ACustomControllerBase::SetHoldPositionOnSelectedUnits()
{
	for (AUnitBase* U : SelectedUnits)
	{
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
    AWaypoint* BWaypoint = nullptr;
    bool PlayWaypoint = false, PlayRun = false;

    // 2. Formation check
    if (ShouldRecalculateFormation())
    {
        //UE_LOG(LogTemp, Warning, TEXT("Recalculating formation..."));
        RecalculateFormation(Hit.Location);
    }

    // 3. Assign final positions
    TMap<AUnitBase*, FVector> Finals;
    for (AUnitBase* U : SelectedUnits)
    {
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
    		Finals.Add(U, Hit.Location + Off);
    	}
    }

    // 4. Issue moves & sounds
    TArray<AUnitBase*> BatchUnits;
    TArray<FVector>    BatchLocs;
    TArray<float>      BatchSpeeds;

    for (auto& P : Finals)
    {
        auto* U = P.Key;
        FVector Loc = P.Value;
        if (!U || U == CameraUnitWithTag || U->UnitState == UnitData::Dead) continue;
    
        bool bNavMod;
        Loc = TraceRunLocation(Loc, bNavMod);
        if (bNavMod) continue;

        U->RemoveFocusEntityTarget();
        float Speed = U->Attributes->GetBaseRunSpeed();
        if (SetBuildingWaypoint(Loc, U, BWaypoint, PlayWaypoint))
        {
            PlayWaypoint = true;
        }
        else if (IsShiftPressed)
        {
            DrawDebugCircleAtLocation(GetWorld(), Loc, FColor::Green);
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
            DrawDebugCircleAtLocation(GetWorld(), Loc, FColor::Green);
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

    if (BatchUnits.Num() > 0)
    {
    	Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocs, BatchSpeeds, 40.f, false);
    }

    if (WaypointSound && PlayWaypoint)
    {
        UGameplayStatics::PlaySound2D(this, WaypointSound);
    }

    const bool bCanPlayRunSound = RunSound && PlayRun && (GetWorld()->GetTimeSeconds() - LastRunSoundTime >= RunSoundDelayTime);
    if (bCanPlayRunSound)
    {
        UGameplayStatics::PlaySound2D(this, RunSound);
        LastRunSoundTime = GetWorld()->GetTimeSeconds();
    }
}


void ACustomControllerBase::LeftClickPressedMass()
{
    LeftClickIsPressed = true;
    AbilityArrayIndex = 0;

    if (!CameraBase || CameraBase->TabToggled) return;

    // --- ALT: cancel / destroy area ---
	if (AltIsPressed)
    {
        DestroyWorkArea();
        for (AUnitBase* U : SelectedUnits)
        {
            CancelAbilitiesIfNoBuilding(U);
        }
    }
    else if (AttackToggled)
    {
        // 1) get world hit under cursor for ground and pawn
        FHitResult Hit;
        GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, Hit);
        FHitResult HitPawn;
        GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitPawn);
        AActor* CursorHitActor = HitPawn.bBlockingHit ? HitPawn.GetActor() : nullptr;

        int32 NumUnits = SelectedUnits.Num();
        if (NumUnits == 0) return;

        // 2) precompute grid offsets
        TArray<FVector> Offsets = ComputeSlotOffsets(NumUnits);

        AWaypoint* BWaypoint = nullptr;
        bool PlayWaypointSound = false;
        bool PlayAttackSound   = false;

        // 3) issue each unit (collect arrays for mass units)
        TArray<AUnitBase*> MassUnits;
        TArray<FVector>    MassLocations;
        for (int32 i = 0; i < NumUnits; ++i)
        {
            AUnitBase* U = SelectedUnits[i];
            if (U == nullptr || U == CameraUnitWithTag) continue;
        
            // apply the same slot-by-index approach
            FVector RunLocation = Hit.Location + Offsets[i];

            bool bNavMod;
            RunLocation = TraceRunLocation(RunLocation, bNavMod);
            if (bNavMod) continue;
        
            if (SetBuildingWaypoint(RunLocation, U, BWaypoint, PlayWaypointSound))
            {
                // waypoint placed
            }
            else
            {
                DrawDebugCircleAtLocation(GetWorld(), RunLocation, FColor::Red);
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

        if (MassUnits.Num() > 0)
        {
            LeftClickAttackMass(MassUnits, MassLocations, AttackToggled, CursorHitActor);
        }

        AttackToggled = false;

        // 4) play sounds
        if (WaypointSound && PlayWaypointSound)
        {
            UGameplayStatics::PlaySound2D(this, WaypointSound);
        }
        if (AttackSound && PlayAttackSound)
        {
            UGameplayStatics::PlaySound2D(this, AttackSound);
        }
    }
    else
    {
        DropWorkArea();

        // handle any ability under the cursor
        FHitResult HitPawn;
        GetHitResultUnderCursor(ECollisionChannel::ECC_Pawn, false, HitPawn);

        bool AbilityFired   = false;
        bool AbilityUnSynced = false;
        for (AUnitBase* U : SelectedUnits)
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
            return;
        }

        // if we hit a pawn, try to select it
        if (HitPawn.bBlockingHit && HUDBase)
        {
            AActor* HitActor = HitPawn.GetActor();
            if (!HitActor->IsA(ALandscape::StaticClass()))
                ClickedActor = HitActor;
            else
                ClickedActor = nullptr;

            AUnitBase* HitUnit = Cast<AUnitBase>(HitActor);
            ASpeakingUnit* SUnit = Cast<ASpeakingUnit>(HitActor);

            if (HitUnit && HitUnit->CanBeSelected && (HitUnit->TeamId == SelectableTeamId || SelectableTeamId == 0) && !SUnit )
            {
            	if (IsCtrlPressed)
            	{
            		FGameplayTag Tag = HitUnit->UnitTags.First();
            		SelectUnitsWithTag(Tag, SelectableTeamId);
            	}else
            	{
            		HUDBase->DeselectAllUnits();
            		HUDBase->SetUnitSelected(HitUnit);
            		DragUnitBase(HitUnit);

            		if (CameraBase->AutoLockOnSelect)
            			LockCameraToUnit = true;	
            	}
            }
            else
            {
                HUDBase->InitialPoint = HUDBase->GetMousePos2D();
                HUDBase->bSelectFriendly = true;
            }
        }
    }
	
}

void ACustomControllerBase::Server_ReportUnitVisibility_Implementation(APerformanceUnit* Unit, bool bVisible)
{
	if (IsValid(Unit))
	{
		Unit->SetClientVisibility(bVisible);
	}
}

void ACustomControllerBase::LeftClickAttackMass_Implementation(const TArray<AUnitBase*>& Units, const TArray<FVector>& Locations, bool AttackT, AActor* CursorHitActor)
{
	const int32 Count = FMath::Min(Units.Num(), Locations.Num());
	if (Count <= 0) return;

	// If we clicked an enemy unit, set chase on all provided units
	AUnitBase* TargetUnitBase = CursorHitActor ? Cast<AUnitBase>(CursorHitActor) : nullptr;
	if (TargetUnitBase)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			AUnitBase* Unit = Units[i];
			if (!Unit || Unit->UnitState == UnitData::Dead) continue;
			Unit->UnitToChase = TargetUnitBase;
			Unit->FocusEntityTarget(TargetUnitBase);
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
			if (Units[i]) Units[i]->RemoveFocusEntityTarget();
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
  Server_Batch_CorrectSetUnitMoveTargets(GetWorld(), BatchUnits, BatchLocations, BatchSpeeds, 40.f, AttackT);
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

            if (!TF || !StateFrag || !AI || !CharFrag)
                continue;

            // skip dead/despawned
            if (StateFrag->Health <= 0.f && AI->StateTimer >= CharFrag->DespawnTime)
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

// === Client mirror helpers ===

