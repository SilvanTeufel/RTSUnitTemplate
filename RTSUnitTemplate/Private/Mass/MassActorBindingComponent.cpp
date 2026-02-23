// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/MassActorBindingComponent.h"


// --- REQUIRED MASS INCLUDES ---
#include "MassSpawnerSubsystem.h"
#include "MassEntityManager.h"
#include "MassArchetypeTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h" // Include your custom tag definition (Adjust path)
#include "MassEntityUtils.h"

#include "Characters/Unit/TransportUnit.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "MassRepresentationSubsystem.h"  
#include "MassRepresentationTypes.h"
// -----------------------------
#include "MassEntitySubsystem.h"
#include "MassRepresentationFragments.h"
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "GameFramework/Actor.h"
#include "Mass/UnitNavigationFragments.h"
#include "MassCommands.h"  
#include "MassEntityTemplateRegistry.h" 
#include "Steering/MassSteeringFragments.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Actors/Waypoint.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Actors/EffectArea.h"
#include "GameModes/RTSGameModeBase.h"
#include "Mass/Replication/UnitReplicationCacheSubsystem.h"
#include "MassReplicationFragments.h"
#include "MassReplicationSubsystem.h"
#include "Iris/ReplicationSystem/ReplicationFragment.h"
#include "MassReplicationTypes.h" 
#include "Mass/Signals/MassUnitSpawnerSubsystem.h"
#include "Mass/Signals/MySignals.h"
#include "Mass/Traits/UnitReplicationFragments.h"
// Replication pipeline helpers
#include "Mass/Replication/ReplicationSettings.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "EngineUtils.h"
#include "Mass/Replication/MassUnitReplicatorBase.h"
#include "Mass/Replication/ReplicationBootstrap.h"
#include "GameStates/ResourceGameState.h"

// CVAR for startup freeze
static TAutoConsoleVariable<int32> CVarRTS_StartupFreeze_Enable(
	TEXT("net.RTS.StartupFreeze.Enable"),
	1,
	TEXT("When 1, units are frozen with FMassStateStopMovementTag until registration and loading phase are complete."),
	ECVF_Default);

static void ApplyInitialStartupFreeze(AActor* Owner, FMassEntityManager& EM, FMassEntityHandle Entity)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || CVarRTS_StartupFreeze_Enable.GetValueOnGameThread() == 0)
	{
		return;
	}

	bool bShouldFreeze = false;
	AResourceGameState* GS = World->GetGameState<AResourceGameState>();
	
	if (GS)
	{
		// If match already started/released, do not freeze new units.
		if (GS->bStartupFreezeReleased)
		{
			return;
		}

		// If MatchStartTime is set, freeze until reached.
		if (GS->MatchStartTime > 0.f && World->GetTimeSeconds() < GS->MatchStartTime)
		{
			bShouldFreeze = true;
		}
	}
	else
	{
		// No GameState yet? Definitely freeze as we are very early in the startup.
		bShouldFreeze = true;
	}

	if (!bShouldFreeze)
	{
		// Registration not complete? 
		// In Standalone or Singleplayer, we don't wait for registration once time is reached (or if no time set).
		const bool bIsMultiplayer = (World->GetNetMode() == NM_DedicatedServer || World->GetNumPlayerControllers() > 1);
		if (bIsMultiplayer)
		{
			if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World))
			{
				if (!Reg->AreAllUnitsRegistered())
				{
					bShouldFreeze = true;
				}
			}
		}
	}

	if (bShouldFreeze)
	{
		EM.AddTagToEntity(Entity, FMassStateFrozenTag::StaticStruct());
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				MoveComp->SetMovementMode(MOVE_None);
			}
		}
	}
}


UMassActorBindingComponent::UMassActorBindingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickInterval(0.5);
}

// Called when the game starts
void UMassActorBindingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMassActorBindingComponent::SetupMassOnActor()
{
	switch (BindingType)
	{
	case EMassBindingType::Unit:
		SetupMassOnUnit();
		break;
	case EMassBindingType::Building:
		SetupMassOnBuilding();
		break;
	case EMassBindingType::EffectArea:
		SetupMassOnEffectArea();
		break;
	}
}

void UMassActorBindingComponent::SetupMassOnUnit()
{
	BindingType = EMassBindingType::Unit;
	UWorld* World = GetWorld();
	MyOwner = GetOwner();
	AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);
	
	// Do not set up Mass or attempt registration for dead units
	if (UnitBase && UnitBase->UnitState == UnitData::Dead)
	{
		return;
	}
	
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("World not FOUND!"));
		return;
	}

	// Only on Server!
	/*
	if (World->GetNetMode() == NM_Client)
	{
		return; // We are a client, do not create a Mass entity. Wait for replication.
	}
	*/
	
	if(!MassEntitySubsystemCache)
	{
		if(World)
		{
			MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
			if (!MassEntitySubsystemCache)
			{
				// This is the log that will likely fire for placed actors on the first attempt.
				UE_LOG(LogTemp, Error, TEXT("MassActorBindingComponent on %s: UMassEntitySubsystem not ready yet. Will retry on Tick."), *MyOwner->GetName());
			}
		}
	}

	if (!MassEntityHandle.IsValid() && MassEntitySubsystemCache) // Only create if not already set/created
	{
		bNeedsMassUnitSetup = true;
	}


	if (!World)
	{
		return; // World might not be valid yet
	}


	if (UMassUnitSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassUnitSpawnerSubsystem>())
	{
		SpawnerSubsystem->RegisterUnitForMassCreation(UnitBase);
	}
}


void UMassActorBindingComponent::ConfigureNewEntity(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}
	
	MassEntityHandle = Entity;
	InitTransform(EntityManager, Entity);
	InitMovementFragments(EntityManager, Entity);
	InitAIFragments(EntityManager, Entity);
	InitRepresentation(EntityManager, Entity);
	
	bNeedsMassUnitSetup = false;
	AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);
	UnitBase->bIsMassUnit = true;
}

FMassEntityHandle UMassActorBindingComponent::CreateAndLinkOwnerToMassEntity()
{
	UWorld* World = GetWorld();
	
	// Do not create or register Mass for dead units
	AActor* Owner = GetOwner();
	if (AUnitBase* UnitBaseLocal = Cast<AUnitBase>(Owner))
	{
		if (UnitBaseLocal->UnitState == UnitData::Dead)
		{
			return {};
		}
	}
	
	if (MassEntityHandle.IsValid())
	{
		//UE_LOG(LogTemp, Warning, TEXT("Already created for %s"), *GetOwner()->GetName());
		return MassEntityHandle;
	}
	
	MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
	
	if (!MassEntitySubsystemCache)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEntitySubsystem not found"));
		return {};
	}

	FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
	FMassEntityHandle NewMassEntityHandle;
	
	//if (!EM.IsProcessing())
	{
		
		FMassArchetypeHandle Archetype;
		FMassArchetypeSharedFragmentValues SharedValues;
		if (!BuildArchetypeAndSharedValues(Archetype, SharedValues))
		{
			return {};
		}
		
		NewMassEntityHandle = EM.CreateEntity(Archetype, SharedValues);
		if (NewMassEntityHandle.IsValid())
		{
			// Perform synchronous initializations
			MassEntityHandle = NewMassEntityHandle;
			ApplyInitialStartupFreeze(MyOwner, EM, NewMassEntityHandle);
			InitTransform(EM, NewMassEntityHandle);
			InitMovementFragments(EM, NewMassEntityHandle);
			InitAIFragments(EM, NewMassEntityHandle);
			InitRepresentation(EM, NewMassEntityHandle);

			if (StopSeparation || Cast<AConstructionUnit>(MyOwner))
			{
				EM.Defer().AddTag<FMassStateStopSeparationTag>(NewMassEntityHandle);
				
				if (Cast<AConstructionUnit>(MyOwner))
				{
					EM.Defer().AddTag<FMassDisableAvoidanceTag>(NewMassEntityHandle);
					EM.Defer().AddTag<FMassStateDisableObstacleTag>(NewMassEntityHandle);
				}
			}
			
			bNeedsMassUnitSetup = false;
			AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);
			UnitBase->bIsMassUnit = true;
			UnitBase->UpdatePredictionFragment(UnitBase->GetMassActorLocation(), 0);
			UnitBase->SyncTranslation();
			
			// Client: Clear stale cache for any NetID this actor might have had previously 
			// or might be about to receive. Better yet, the ClientReplicationProcessor 
			// handles the actual NetID assignment from registry.
			
			// Server: assign NetID and update authoritative registry so clients can reconcile
			if (UWorld* WorldPtr = GetWorld())
			{
				if (WorldPtr->GetNetMode() != NM_Client)
				{
					if (FMassNetworkIDFragment* NetFrag = EM.GetFragmentDataPtr<FMassNetworkIDFragment>(NewMassEntityHandle))
					{
							// Skip registration if the owning unit is dead
							AUnitBase* UnitBaseLocal2 = Cast<AUnitBase>(MyOwner);
							if (UnitBaseLocal2 && UnitBaseLocal2->UnitState == UnitData::Dead)
							{
								// Do not assign NetID or add to registry for dead units
							}
							else if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*WorldPtr))
							{
								// Ensure the unit has a valid unique UnitIndex before entering the registry.
								int32 UnitIndex = UnitBaseLocal2 ? UnitBaseLocal2->UnitIndex : INDEX_NONE;
								if (UnitBaseLocal2 && UnitIndex <= 0)
								{
									if (ARTSGameModeBase* GM = WorldPtr->GetAuthGameMode<ARTSGameModeBase>())
									{
										GM->AddUnitIndexAndAssignToAllUnitsArrayWithIndex(UnitBaseLocal2, INDEX_NONE, FUnitSpawnParameter());
										UnitIndex = UnitBaseLocal2->UnitIndex;
									}
								}
								if (UnitIndex <= 0)
								{
									// Cannot safely register without a stable UnitIndex.
									// (Should not happen in normal flow; runtime-spawn paths must assign UnitIndex.)
									return NewMassEntityHandle;
								}

								const uint32 NewID = Reg->GetNextNetID();
								NetFrag->NetID = FMassNetworkID(NewID);
								const FName OwnerName = MyOwner ? MyOwner->GetFName() : NAME_None;
								FUnitRegistryItem* Existing = Reg->Registry.FindByUnitIndex(UnitIndex);
							
								if (Existing)
								{
									Existing->OwnerName = OwnerName;
									Existing->UnitIndex = UnitIndex;
								Existing->NetID = NetFrag->NetID;
								Reg->Registry.MarkItemDirty(*Existing);
							}
							else
							{
								const int32 NewIdx2 = Reg->Registry.Items.AddDefaulted();
								Reg->Registry.Items[NewIdx2].OwnerName = OwnerName;
								Reg->Registry.Items[NewIdx2].UnitIndex = UnitIndex;
								Reg->Registry.Items[NewIdx2].NetID = NetFrag->NetID;
								Reg->Registry.MarkItemDirty(Reg->Registry.Items[NewIdx2]);
							}
								Reg->Registry.MarkArrayDirty();
								Reg->ForceNetUpdate();
							}
					}
				}
			}
		}
    }
	
	return NewMassEntityHandle;
}


bool UMassActorBindingComponent::BuildArchetypeAndSharedValues(FMassArchetypeHandle& OutArchetype,
                                                               FMassArchetypeSharedFragmentValues& OutSharedValues)
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;

	if (BindingType == EMassBindingType::EffectArea)
	{
		FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();
		TArray<const UScriptStruct*> FragmentsAndTags = {
			FTransformFragment::StaticStruct(),
			FMassActorFragment::StaticStruct(),
			FMassCombatStatsFragment::StaticStruct(),
			FMassVisibilityFragment::StaticStruct(),
			FEffectAreaTag::StaticStruct(),
			FMassSightFragment::StaticStruct(),
			FMassAgentCharacteristicsFragment::StaticStruct(),
			FMassIsEffectAreaTag::StaticStruct()
		};

		FMassArchetypeCreationParams Params;
		Params.ChunkMemorySize = 0;
		Params.DebugName = FName("UMassActorBindingComponent_EffectArea");
		OutArchetype = EntityManager.CreateArchetype(FragmentsAndTags, Params);
		OutSharedValues = FMassArchetypeSharedFragmentValues();
		return OutArchetype.IsValid();
	}

	AUnitBase* UnitBase = Cast<AUnitBase>(Owner);
	if (!UnitBase) return false;
	
    UWorld* World = Owner->GetWorld();
    if (!World) return false;

    FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();

    TArray<const UScriptStruct*> FragmentsAndTags = {

    	FMassNetworkIDFragment::StaticStruct(),
    	FMassReplicatedAgentFragment::StaticStruct(),
    	FMassReplicationViewerInfoFragment::StaticStruct(),
    	FMassReplicationLODFragment::StaticStruct(),
    	FMassReplicationGridCellLocationFragment::StaticStruct(),
    	FMassInReplicationGridTag::StaticStruct(),
    	FUnitReplicatedTransformFragment::StaticStruct(),

    	FTransformFragment::StaticStruct(),
		FMassVelocityFragment::StaticStruct(),          // Needed by Avoidance & Movement
		FMassForceFragment::StaticStruct(),             // Needed by Movement Processor
		FMassMoveTargetFragment::StaticStruct(),        // Input for your UnitMovementProcessor
		FAgentRadiusFragment::StaticStruct(),           // Often used by Avoidance/Movement
    	FMassClientPredictionFragment::StaticStruct(), 

		FMassSteeringFragment::StaticStruct(),          // ** REQUIRED: Output of UnitMovementProcessor, Input for Steer/Avoid/Move **
		FMassAvoidanceColliderFragment::StaticStruct(), // ** REQUIRED: Avoidance shape **
    	
		FMassGhostLocationFragment::StaticStruct(),
		FMassNavigationEdgesFragment::StaticStruct(),
		FUnitMassTag::StaticStruct(),                   // Your custom tag
		FMassPatrolFragment::StaticStruct(), 
		FUnitNavigationPathFragment::StaticStruct(),    // ** REQUIRED: Used by your UnitMovementProcessor for path state **
    	
		FMassAIStateFragment::StaticStruct(),
    	FMassSightFragment::StaticStruct(),
		FMassAITargetFragment::StaticStruct(), 
		FMassCombatStatsFragment::StaticStruct(), 
		FMassAgentCharacteristicsFragment::StaticStruct(),
    	FMassChargeTimerFragment::StaticStruct(),
		FMassVisibilityFragment::StaticStruct(),

		FMassWorkerStatsFragment::StaticStruct(),

		FMassActorFragment::StaticStruct(),             // ** REQUIRED: Links Mass entity to Actor **
		FMassRepresentationFragment::StaticStruct(),    // Needed by representation system
		FMassRepresentationLODFragment::StaticStruct(),  // Needed by representation system
    };
	
	if(UnitBase->AddEffectTargetFragement)
		FragmentsAndTags.Add(FMassGameplayEffectTargetFragment::StaticStruct());
	
	
	if(UnitBase->AddGameplayEffectFragement)
		FragmentsAndTags.Add(FMassGameplayEffectFragment::StaticStruct());

	if (ATransportUnit* TransportUnit = Cast<ATransportUnit>(Owner))
	{
		if (TransportUnit->IsATransporter)
		{
			FragmentsAndTags.Add(FMassTransportFragment::StaticStruct());
			FragmentsAndTags.Add(FMassTransportTag::StaticStruct());
		}
	}

	if (StopSeparation || Cast<AConstructionUnit>(Owner))
	{
		FragmentsAndTags.Add(FMassStateStopSeparationTag::StaticStruct());
		if (Cast<AConstructionUnit>(Owner))
		{
			FragmentsAndTags.Add(FMassDisableAvoidanceTag::StaticStruct());
			FragmentsAndTags.Add(FMassStateDisableObstacleTag::StaticStruct());
		}
	}
	
    FMassArchetypeCreationParams Params;
	Params.ChunkMemorySize=0;
	Params.DebugName=FName("UMassActorBindingComponent");

    OutArchetype = EntityManager.CreateArchetype(FragmentsAndTags, Params);
    if (!OutArchetype.IsValid()) return false;

	AUnitBase* MyUnit = Cast<AUnitBase>(MyOwner);
// --- Define and Package Shared Fragment Values --
	if (!MyUnit) return false;
	
	FMassMovementParameters MovementParamsInstance;
	MovementParamsInstance.MaxSpeed = 10000.0f; //MyUnit->Attributes->GetRunSpeed()*20; //500.0f;     // Set desired value
	MovementParamsInstance.MaxAcceleration = MaxAcceleration; // Set desired value
	MovementParamsInstance.DefaultDesiredSpeed = MyUnit->Attributes->GetRunSpeed(); //400.0f; // Example: Default speed slightly less than max
	MovementParamsInstance.DefaultDesiredSpeedVariance = 0.00f; // Example: +/- 5% variance is 0.05
	MovementParamsInstance.HeightSmoothingTime = 0.0f; // 0.2f 

	FConstSharedStruct MovementParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovementParamsInstance); // Use instance directly
	
	// Package the shared fragments
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(MovementParamSharedFragment);

	FMassMovingSteeringParameters MovingSteeringParamsInstance;
	
	MovingSteeringParamsInstance.ReactionTime = 0.05f; // Faster reaction (Default 0.3) // 0.05f;
	MovingSteeringParamsInstance.LookAheadTime = 0.25f; // Look less far ahead (Default 1.0) - might make turns sharper but potentially start sooner

	FConstSharedStruct MovingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovingSteeringParamsInstance);
	SharedValues.Add(MovingSteeringParamSharedFragment);

	FMassStandingSteeringParameters StandingSteeringParamsInstance;

	FConstSharedStruct StandingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(StandingSteeringParamsInstance);
	SharedValues.Add(StandingSteeringParamSharedFragment);
	

	FMassMovingAvoidanceParameters MovingAvoidanceParamsInstance;

	MovingAvoidanceParamsInstance.ObstacleDetectionDistance    = 400.f;  // How far agents see each other

	MovingAvoidanceParamsInstance.ObstacleSeparationDistance   = AvoidanceDistance;  //75.f // How close they can get before repelling
	MovingAvoidanceParamsInstance.EnvironmentSeparationDistance= AvoidanceDistance;   // Wall‐avoidance distance

	MovingAvoidanceParamsInstance.PredictiveAvoidanceTime      = 2.5f;  // How far ahead in seconds
	MovingAvoidanceParamsInstance.PredictiveAvoidanceDistance  = AvoidanceDistance;   // Look-ahead distance in cm
	
	MovingAvoidanceParamsInstance.ObstacleSeparationDistance   = AvoidanceDistance + 50.f; // Or a fixed larger value like 100.f or 125.f
	MovingAvoidanceParamsInstance.ObstacleSeparationStiffness  = ObstacleSeparationStiffness; // Significantly increase this for a stronger push
	
	MovingAvoidanceParamsInstance.EnvironmentSeparationStiffness = 500.f;
	MovingAvoidanceParamsInstance.ObstaclePredictiveAvoidanceStiffness = 700.f;
	MovingAvoidanceParamsInstance.EnvironmentPredictiveAvoidanceStiffness = 200.f;

	
	// Validate and create the shared fragment
	FConstSharedStruct MovingAvoidanceParamSharedFragment =
		EntityManager.GetOrCreateConstSharedFragment(MovingAvoidanceParamsInstance.GetValidated());
	SharedValues.Add(MovingAvoidanceParamSharedFragment);

	// Standing avoidance (if you also use standing avoidance)
	FMassStandingAvoidanceParameters StandingAvoidanceParamsInstance;
	StandingAvoidanceParamsInstance.GhostObstacleDetectionDistance = 300.f;
	StandingAvoidanceParamsInstance.GhostSeparationDistance       = 20.f;
	StandingAvoidanceParamsInstance.GhostSeparationStiffness      = 200.f;
	// … any other Ghost* fields you want to override …
	FConstSharedStruct StandingAvoidanceParamSharedFragment =
	EntityManager.GetOrCreateConstSharedFragment(StandingAvoidanceParamsInstance.GetValidated());
	SharedValues.Add(StandingAvoidanceParamSharedFragment);
	SharedValues.Sort();
	
	OutSharedValues = SharedValues;

	return true;
}


void UMassActorBindingComponent::InitTransform(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle)
{
    FTransformFragment& Frag = EntityManager.GetFragmentDataChecked<FTransformFragment>(Handle);
    Frag.SetTransform(GetOwner()->GetActorTransform());
}

void UMassActorBindingComponent::InitMovementFragments(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle)
{
    // Velocity
    FMassVelocityFragment& Vel = EntityManager.GetFragmentDataChecked<FMassVelocityFragment>(Handle);
    Vel.Value = GetOwner()->GetVelocity();

	AUnitBase* Unit = Cast<AUnitBase>(MyOwner);
    // MoveTarget
	
    FMassMoveTargetFragment& MT = EntityManager.GetFragmentDataChecked<FMassMoveTargetFragment>(Handle);
    MT.Center = GetOwner()->GetActorLocation();

	if (Unit)
	{
		MT.SlackRadius = Unit->MovementAcceptanceRadius;
	}
    MT.DistanceToGoal = 0.f;
    MT.DesiredSpeed.Set(0.f);
    MT.IntentAtGoal = EMassMovementAction::Stand;
    MT.Forward = GetOwner()->GetActorForwardVector();
}

void UMassActorBindingComponent::InitAIFragments(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle)
{
    // Stats, characteristics, patrol, etc.
    InitStats(EntityManager, Handle, GetOwner());
}

void UMassActorBindingComponent::InitRepresentation(FMassEntityManager& EntityManager, const FMassEntityHandle& Handle)
{
    FMassActorFragment& ActorFrag = EntityManager.GetFragmentDataChecked<FMassActorFragment>(Handle);
    ActorFrag.SetAndUpdateHandleMap(Handle, GetOwner(), false);

    FMassRepresentationLODFragment& LODFrag = EntityManager.GetFragmentDataChecked<FMassRepresentationLODFragment>(Handle);
    LODFrag.LOD = EMassLOD::High;
    LODFrag.PrevLOD = EMassLOD::Max;
}

void UMassActorBindingComponent::InitStats(FMassEntityManager& EntityManager,
                                         const FMassEntityHandle& Handle,
                                         AActor* OwnerActor)
{
	InitializeMassEntityStatsFromOwner(EntityManager, Handle, OwnerActor); // <<< CALL THE NEW FUNCTION
}


void UMassActorBindingComponent::SetupMassOnBuilding()
{
	BindingType = EMassBindingType::Building;
	UWorld* World = GetWorld();
	MyOwner = GetOwner();
	AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);

	if (!World)
	{
		return; // World might not be valid yet
	}
	
	// Clients never create Mass entities directly; they will request a safe link later
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}
	
	// Cache subsystem if needed
	if(!MassEntitySubsystemCache)
	{
		MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
	}

	// Try to create immediately on the server to avoid early access with an unset handle
	if (!MassEntityHandle.IsValid() && MassEntitySubsystemCache)
	{
		FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
		const bool bSafeNow = (EM.IsProcessing() == false) && (World->GetTimeSeconds() > 0.01f);
		if (bSafeNow)
		{
			// Synchronous creation for buildings
			FMassEntityHandle NewHandle = CreateAndLinkBuildingToMassEntity();
			if (NewHandle.IsValid())
			{
				MassEntityHandle = NewHandle;
				bNeedsMassBuildingSetup = false;
				return; // Done
			}
		}
		// If we couldn't create now (e.g., during Mass processing), defer via spawner
		bNeedsMassBuildingSetup = true;
	}
	
	if (UMassUnitSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassUnitSpawnerSubsystem>())
	{
		SpawnerSubsystem->RegisterUnitForMassCreation(UnitBase);
	}
}



FMassEntityHandle UMassActorBindingComponent::CreateAndLinkBuildingToMassEntity()
{
	//return CreateAndLinkOwnerToMassEntity();


	UWorld* World = GetWorld();
	
	if (MassEntityHandle.IsValid())
	{
		//UE_LOG(LogTemp, Warning, TEXT("Already created for %s"), *GetOwner()->GetName());
		return MassEntityHandle;
	}

	MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
	
	if (!MassEntitySubsystemCache)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEntitySubsystem not found"));
		return {};
	}

	FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
	FMassEntityHandle NewMassEntityHandle;
	
	//if (!EM.IsProcessing())
	{
		FMassArchetypeHandle Archetype;
		FMassArchetypeSharedFragmentValues SharedValues;
		if (!BuildArchetypeAndSharedValuesForBuilding(Archetype, SharedValues))
		{
			return {};
		}

		NewMassEntityHandle = EM.CreateEntity(Archetype, SharedValues);

		if (NewMassEntityHandle.IsValid())
		{
			
			MassEntityHandle = NewMassEntityHandle;
			ApplyInitialStartupFreeze(MyOwner, EM, NewMassEntityHandle);
			InitTransform(EM, NewMassEntityHandle);

			if (AUnitBase* Unit = Cast<AUnitBase>(MyOwner))
				if (Unit->CanMove)
					InitMovementFragments(EM, NewMassEntityHandle);
			
			InitAIFragments(EM, NewMassEntityHandle);
			InitRepresentation(EM, NewMassEntityHandle);

			if (StopSeparation || Cast<AConstructionUnit>(MyOwner))
			{
				EM.Defer().AddTag<FMassStateStopSeparationTag>(NewMassEntityHandle);

				if (Cast<AConstructionUnit>(MyOwner))
				{
					EM.Defer().AddTag<FMassDisableAvoidanceTag>(NewMassEntityHandle);
					EM.Defer().AddTag<FMassStateDisableObstacleTag>(NewMassEntityHandle);
				}
			}
			
			bNeedsMassBuildingSetup = false;
			if (AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner))
			{
				UnitBase->bIsMassUnit = true;
			}
			// Server: assign NetID and update authoritative registry for buildings as well
			if (UWorld* WorldPtr = GetWorld())
			{
				if (WorldPtr->GetNetMode() != NM_Client)
				{
					if (FMassNetworkIDFragment* NetFrag = EM.GetFragmentDataPtr<FMassNetworkIDFragment>(NewMassEntityHandle))
					{
						AUnitBase* UnitBaseLocal = Cast<AUnitBase>(MyOwner);
						if (UnitBaseLocal && UnitBaseLocal->UnitState != UnitData::Dead)
						{
							if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*WorldPtr))
							{
								// Ensure stable UnitIndex before entering registry
								int32 UnitIdxVal = UnitBaseLocal->UnitIndex;
								if (UnitIdxVal <= 0)
								{
									if (ARTSGameModeBase* GM = WorldPtr->GetAuthGameMode<ARTSGameModeBase>())
									{
										GM->AddUnitIndexAndAssignToAllUnitsArrayWithIndex(UnitBaseLocal, INDEX_NONE, FUnitSpawnParameter());
										UnitIdxVal = UnitBaseLocal->UnitIndex;
									}
								}
								if (UnitIdxVal <= 0)
								{
									return NewMassEntityHandle;
								}

								const uint32 NewID = Reg->GetNextNetID();
								NetFrag->NetID = FMassNetworkID(NewID);
								const FName OwnerName = MyOwner ? MyOwner->GetFName() : NAME_None;
								FUnitRegistryItem* Existing = nullptr;
								Existing = Reg->Registry.FindByUnitIndex(UnitIdxVal);
								
								if (Existing)
								{
									Existing->OwnerName = OwnerName;
									Existing->UnitIndex = UnitIdxVal;
									Existing->NetID = NetFrag->NetID;
									Reg->Registry.MarkItemDirty(*Existing);
								}
								else
								{
									const int32 NewIdx2 = Reg->Registry.Items.AddDefaulted();
									Reg->Registry.Items[NewIdx2].OwnerName = OwnerName;
									Reg->Registry.Items[NewIdx2].UnitIndex = UnitIdxVal;
									Reg->Registry.Items[NewIdx2].NetID = NetFrag->NetID;
									Reg->Registry.MarkItemDirty(Reg->Registry.Items[NewIdx2]);
								}
								Reg->Registry.MarkArrayDirty();
								Reg->ForceNetUpdate();
							}
						}
					}
				}
			}
		}
	}
	
	return NewMassEntityHandle;
	
}


bool UMassActorBindingComponent::BuildArchetypeAndSharedValuesForBuilding(FMassArchetypeHandle& OutArchetype,
                                                               FMassArchetypeSharedFragmentValues& OutSharedValues)
{
	return BuildArchetypeAndSharedValues(OutArchetype, OutSharedValues);
}

void UMassActorBindingComponent::InitializeMassEntityStatsFromOwner(FMassEntityManager& EntityManager,
	FMassEntityHandle EntityHandle, AActor* OwnerActor)
{
	if (BindingType == EMassBindingType::EffectArea)
	{
		AEffectArea* EffectArea = Cast<AEffectArea>(OwnerActor);
		if (EffectArea)
		{
			if (FMassCombatStatsFragment* CombatStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(EntityHandle))
			{
				CombatStatsFrag->TeamId = EffectArea->TeamId;
				CombatStatsFrag->SightRadius = SightRadius;
				CombatStatsFrag->LoseSightRadius = LoseSightRadius;
			}
			if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
			{
				CharFrag->bCanBeInvisible = EffectArea->bCanBeInvisible;
				CharFrag->bIsInvisible = EffectArea->bIsInvisible;
				CharFrag->bCanDetectInvisible = false;
			}
			if (FMassVisibilityFragment* VisibilityFrag = EntityManager.GetFragmentDataPtr<FMassVisibilityFragment>(EntityHandle))
			{
				VisibilityFrag->bIsMyTeam = true;
				VisibilityFrag->bIsOnViewport = true;
			}
			return;
		}
	}

	 // --- Get Owner References (Replace Placeholders) ---
    // We assume OwnerActor is valid as it's checked before calling this function
    AUnitBase* UnitOwner = Cast<AUnitBase>(OwnerActor); // <<< REPLACE AUnitBase
    UAttributeSetBase* UnitAttributes = nullptr; // <<< REPLACE UUnitAttributesComponent

    if (UnitOwner)
    {
    	UnitOwner->AbilitySystemComponent->InitAbilityActorInfo(UnitOwner, UnitOwner);
    	UnitOwner->InitializeAttributes();
    	UnitOwner->GiveAbilities();
    	UnitOwner->SetupAbilitySystemDelegates();
    	UnitOwner->GetAbilitiesArrays();
    	UnitOwner->AutoAbility();
        UnitAttributes = UnitOwner->Attributes; // <<< REPLACE UUnitAttributesComponent

    }
    // --- End: Get Owner References ---
	

    // --- INITIALIZE NEW FRAGMENTS ---

    // 1. Combat Stats Fragment
    if (FMassCombatStatsFragment* CombatStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(EntityHandle))
    {
        if (UnitOwner && UnitAttributes) // Use data from Actor/Attributes if available
        {
            // <<< REPLACE Getters with your actual function names >>>
            CombatStatsFrag->MaxHealth = UnitAttributes->GetMaxHealth();
            CombatStatsFrag->Health = CombatStatsFrag->MaxHealth; // Start full
            CombatStatsFrag->AttackRange = UnitAttributes->GetRange();
            CombatStatsFrag->AttackDamage = UnitAttributes->GetAttackDamage();
            //CombatStatsFrag->AttackSpeed = UnitAttributes->GetAttackSpeed();
            CombatStatsFrag->RunSpeed = UnitAttributes->GetRunSpeed();
            CombatStatsFrag->TeamId = UnitOwner->TeamId; // Assuming GetTeamId() is on AUnitBase
        	CombatStatsFrag->SquadId = UnitOwner->SquadId; // Assuming GetSquadId() is on AUnitBase
        	CombatStatsFrag->Armor = UnitAttributes->GetArmor();
            CombatStatsFrag->MagicResistance = UnitAttributes->GetMagicResistance();
            CombatStatsFrag->MaxShield = UnitAttributes->GetMaxShield();
            CombatStatsFrag->Shield = CombatStatsFrag->MaxShield; // Start full
            //CombatStatsFrag->AgentRadius = UnitAttributes->GetAgentRadius(); // Get radius from stats
            //CombatStatsFrag->AcceptanceRadius = UnitAttributes->GetAcceptanceRadius();
            CombatStatsFrag->SightRadius = SightRadius;// We need to add this to Attributes i guess;
            CombatStatsFrag->LoseSightRadius = LoseSightRadius;// We need to add this to Attributes i guess;
            CombatStatsFrag->PauseDuration = UnitOwner->PauseDuration;// We need to add this to Attributes i guess;
        	CombatStatsFrag->AttackDuration = UnitOwner->AttackDuration;
            CombatStatsFrag->bUseProjectile = UnitOwner->UseProjectile; // Assuming UsesProjectile() on Attributes
        	CombatStatsFrag->IsAttackedDuration = IsAttackedDuration;
        	CombatStatsFrag->CastTime = UnitOwner->CastTime;
        	CombatStatsFrag->IsInitialized = UnitOwner->IsInitialized;
        	CombatStatsFrag->MinRange = MinRange;
            //CalculatedAgentRadius = CombatStatsFrag->AgentRadius; // Use the value from stats
        }
        else // Use default values
        {
            *CombatStatsFrag = FMassCombatStatsFragment(); // Initialize with struct defaults
            CombatStatsFrag->TeamId = 0; // Sensible default team
        }
    }

    // 2. Agent Characteristics Fragment
    if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
    {
        if (UnitOwner) // Use data from Actor if available
        {
            // <<< REPLACE Properties/Getters with your actual variable names/functions >>>
            CharFrag->bIsFlying = UnitOwner->IsFlying; // Assuming direct property access
        	CharFrag->FlyHeight = UnitOwner->FlyHeight;
            CharFrag->bCanOnlyAttackFlying = UnitOwner->CanOnlyAttackFlying;
            CharFrag->bCanOnlyAttackGround = UnitOwner->CanOnlyAttackGround;
            CharFrag->bIsInvisible = UnitOwner->bIsInvisible;
        	CharFrag->bCanBeInvisible = UnitOwner->bCanBeInvisible;
            CharFrag->bCanDetectInvisible = UnitOwner->CanDetectInvisible;
        	CharFrag->CanManipulateNavMesh = CanManipulateNavMesh;
        	CharFrag->DespawnTime = DespawnTime;
			CharFrag->RotatesToMovement = RotatesToMovement;
        	CharFrag->RotatesToEnemy = RotatesToEnemy;
        	CharFrag->RotationSpeed = RotationSpeed;
        	CharFrag->PositionedTransform = UnitOwner->GetActorTransform();
        	CharFrag->CapsuleHeight = UnitOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        	CharFrag->CapsuleRadius = UnitOwner->GetCapsuleComponent()->GetScaledCapsuleRadius()+AdditionalCapsuleRadius;
        	CharFrag->VerticalDeathRotationMultiplier = VerticalDeathRotationMultiplier;
        	CharFrag->GroundAlignment = GroundAlignment;

            // Only for buildings! (units handle this in the movement processor)
            if (!UnitOwner->CanMove)
            {
                // Perform LineTrace to find the ground height at startup
                FHitResult Hit;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(UnitOwner);
                FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);

                FVector ActorLoc = UnitOwner->GetActorLocation();
                FVector TraceStart = ActorLoc + FVector(0.f, 0.f, 500.f);
                FVector TraceEnd = ActorLoc - FVector(0.f, 0.f, 2500.f);

                if (UnitOwner->GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
                {
                    CharFrag->LastGroundLocation = Hit.ImpactPoint.Z;
                }
                else
                {
                    CharFrag->LastGroundLocation = ActorLoc.Z; // Fallback to current Z
                }
            }
        }
        else // Use default values
        {
             *CharFrag = FMassAgentCharacteristicsFragment(); // Initialize with struct defaults
        }
    }
	if (FMassAIStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(EntityHandle))
	{
		if (UnitOwner) // Use data from Actor if available
		{
			StateFragment->StateTimer = UnitOwner->UnitControlTimer;
			TEnumAsByte<UnitData::EState> State = UnitOwner->GetUnitState();
			
			 // map every state to its FName signal
        switch (State)
        {
        case UnitData::Idle:
        	{
        		StateFragment->PlaceholderSignal = UnitSignals::Idle;
        		UnitOwner->UnitStatePlaceholder = UnitData::Idle;
        	}
        	break;
        case UnitData::Run:
        	{
        		StateFragment->PlaceholderSignal = UnitSignals::Run;
        		UnitOwner->UnitStatePlaceholder = UnitData::Run;
        	}
        	break;
        case UnitData::PatrolRandom:
        	{
        		StateFragment->PlaceholderSignal = UnitSignals::PatrolRandom;
        		UnitOwner->UnitStatePlaceholder = UnitData::PatrolRandom;
        	}
        	break;
        case UnitData::PatrolIdle:
        	{
        		StateFragment->PlaceholderSignal = UnitSignals::PatrolIdle;
        		UnitOwner->UnitStatePlaceholder = UnitData::PatrolIdle;
        	}
        	break;
        case UnitData::GoToBase:
        	{
        		StateFragment->PlaceholderSignal = UnitSignals::GoToBase;
        		UnitOwner->UnitStatePlaceholder = UnitData::GoToBase;
        	}
        	break;
        default:
        	{
        		StateFragment->PlaceholderSignal = UnitSignals::Idle;
        		UnitOwner->UnitStatePlaceholder = UnitData::Idle;
			}
            break;
        }
		}
	}
	
    // 3. Patrol Fragment
    if (FMassPatrolFragment* PatrolFrag = EntityManager.GetFragmentDataPtr<FMassPatrolFragment>(EntityHandle))
    {
        if (UnitOwner && UnitOwner->NextWaypoint) // Use config from Actor if available
        {
            // <<< REPLACE Properties with your actual variable names >>>
            PatrolFrag->bLoopPatrol = UnitOwner->NextWaypoint->PatrolCloseToWaypoint; // Assuming direct property access
            PatrolFrag->RandomPatrolMinIdleTime = UnitOwner->NextWaypoint->PatrolCloseMinInterval;
            PatrolFrag->RandomPatrolMaxIdleTime = UnitOwner->NextWaypoint->PatrolCloseMaxInterval;
        	PatrolFrag->TargetWaypointLocation = UnitOwner->NextWaypoint->GetActorLocation();
        	PatrolFrag->RandomPatrolRadius = (UnitOwner->NextWaypoint->PatrolCloseOffset.X+UnitOwner->NextWaypoint->PatrolCloseOffset.Y)/2.f;
        	PatrolFrag->IdleChance = UnitOwner->NextWaypoint->PatrolCloseIdlePercentage;
        	PatrolFrag->bSetUnitsBackToPatrol = false;
        	PatrolFrag->SetUnitsBackToPatrolTime = 3.f;
      
        }
         else // Use default values
         {
             *PatrolFrag = FMassPatrolFragment(); // Initialize with struct defaults
         	PatrolFrag->CurrentWaypointIndex = INDEX_NONE;
         	PatrolFrag->TargetWaypointLocation = FVector::ZeroVector;
         }
    }

    // --- Initialize Radius/Avoidance Fragments USING the calculated AgentRadius ---
    // (Moved here as it depends on CombatStats)
    if(FAgentRadiusFragment* RadiusFrag = EntityManager.GetFragmentDataPtr<FAgentRadiusFragment>(EntityHandle))
    {
       RadiusFrag->Radius = UnitOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius() + AdditionalCapsuleRadius;
    }

    if(FMassAvoidanceColliderFragment* AvoidanceFrag = EntityManager.GetFragmentDataPtr<FMassAvoidanceColliderFragment>(EntityHandle))
    {
       // Make sure collider type matches expectations (Circle assumed here)
       *AvoidanceFrag = FMassAvoidanceColliderFragment(FMassCircleCollider(UnitOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() + AdditionalCapsuleRadius));
    }

	if (FMassVisibilityFragment* VisibilityFrag = EntityManager.GetFragmentDataPtr<FMassVisibilityFragment>(EntityHandle))
	{
		if (APerformanceUnit* PerfUnit = Cast<APerformanceUnit>(UnitOwner))
		{
			VisibilityFrag->bIsMyTeam = PerfUnit->IsMyTeam;
			VisibilityFrag->bIsOnViewport = PerfUnit->IsOnViewport;
			VisibilityFrag->bIsVisibleEnemy = PerfUnit->IsVisibleEnemy;
			VisibilityFrag->VisibilityOffset = PerfUnit->VisibilityOffset;
			VisibilityFrag->bLastIsMyTeam = PerfUnit->IsMyTeam;
			VisibilityFrag->bLastIsOnViewport = PerfUnit->IsOnViewport;
			VisibilityFrag->bLastIsVisibleEnemy = PerfUnit->IsVisibleEnemy;
		}
	}

	if(FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(EntityHandle))
	{
		TargetFrag->FollowRadius = FollowRadius;
		TargetFrag->FollowOffset = FollowOffset;
	}

	if (FMassTransportFragment* TransportFrag = EntityManager.GetFragmentDataPtr<FMassTransportFragment>(EntityHandle))
	{
		if (ATransportUnit* TransportUnit = Cast<ATransportUnit>(OwnerActor))
		{
			TransportFrag->InstantLoadRange = TransportUnit->InstantLoadRange;
			TransportFrag->TransportId = TransportUnit->TransportId;
		}
		else
		{
			TransportFrag->InstantLoadRange = InstantLoadRange;
			TransportFrag->TransportId = TransportId;
		}
	}
}

FMassEntityHandle UMassActorBindingComponent::CreateAndLinkEffectAreaToMassEntity()
{
	UWorld* World = GetWorld();
	if (!World) return {};
	
	if (MassEntityHandle.IsValid()) return MassEntityHandle;

	if (!MassEntitySubsystemCache) MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassEntitySubsystemCache) return {};

	FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
	
	FMassArchetypeHandle Archetype;
	FMassArchetypeSharedFragmentValues SharedValues;
	if (!BuildArchetypeAndSharedValues(Archetype, SharedValues)) return {};

	MassEntityHandle = EM.CreateEntity(Archetype, SharedValues);
	if (MassEntityHandle.IsValid())
	{
		InitTransform(EM, MassEntityHandle);
		InitStats(EM, MassEntityHandle, GetOwner());
		
		FMassActorFragment& ActorFrag = EM.GetFragmentDataChecked<FMassActorFragment>(MassEntityHandle);
		ActorFrag.SetAndUpdateHandleMap(MassEntityHandle, GetOwner(), false);
	}
	return MassEntityHandle;
}

void UMassActorBindingComponent::SetupMassOnEffectArea()
{
	BindingType = EMassBindingType::EffectArea;
	UWorld* World = GetWorld();
	MyOwner = GetOwner();
	if (!World) return;
	if (!MassEntitySubsystemCache) MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassEntityHandle.IsValid() && MassEntitySubsystemCache)
	{
		FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
		if (!EM.IsProcessing())
		{
			CreateAndLinkEffectAreaToMassEntity();
		}
	}
}

void UMassActorBindingComponent::RequestClientMassLink()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Only meaningful on clients; but allow call on server to be a no-op
	if (AUnitBase* UnitBase = Cast<AUnitBase>(GetOwner()))
	{
		// Do not (re)create or register Mass for dead units
		if (UnitBase->UnitState == UnitData::Dead)
		{
			return;
		}
		const bool bCanMove = UnitBase->CanMove;
		if (bCanMove)
		{
			bNeedsMassUnitSetup = true;
			bNeedsMassBuildingSetup = false;
		}
		else
		{
			bNeedsMassBuildingSetup = true;
			bNeedsMassUnitSetup = false;
		}
		if (UMassUnitSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassUnitSpawnerSubsystem>())
		{
			SpawnerSubsystem->RegisterUnitForMassCreation(UnitBase);
		}

		// Fast path: on clients, if Mass subsystems are ready and we don't yet have an entity, create/link immediately
		// BUT never call synchronous CreateEntity during Mass processing. Guard with EntityManager.IsProcessing()==false.
		if (World->GetNetMode() == NM_Client)
		{
			if (!MassEntityHandle.IsValid())
			{
				if (!MassEntitySubsystemCache)
				{
					MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
				}
				if (MassEntitySubsystemCache)
				{
					FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
					// Avoid calling too early at time 0.0 to give systems a frame to initialize
					const float Now = World->GetTimeSeconds();
					const bool bSafeToCreateNow = (Now > 0.05f) && (EM.IsProcessing() == false);
					if (bSafeToCreateNow)
					{
						if (bCanMove)
						{
							CreateAndLinkOwnerToMassEntity();
						}
						else
						{
							CreateAndLinkBuildingToMassEntity();
						}
					}
					// If not safe, we rely on the end-of-phase CreatePendingEntities() via the spawner registration above.
				}
			}
		}
	}
}

void UMassActorBindingComponent::RequestClientMassUnlink()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	// Server-authoritative: server drives destruction via CleanupMassEntity
	if (World->GetNetMode() != NM_Client)
	{
		CleanupMassEntity();
		return;
	}

	// On clients, we allow forced unlinking (and entity destruction) if requested by reconciliation.
	// This ensures that 'ghost' entities are cleaned up even if the actor persists.
	if (MassEntityHandle.IsValid())
	{
		if (!MassEntitySubsystemCache)
		{
			MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
		}
		if (MassEntitySubsystemCache)
		{
			FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
			EM.Defer().DestroyEntity(MassEntityHandle);
		}
		MassEntityHandle.Reset();
	}
}

void UMassActorBindingComponent::CleanupMassEntity()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	// Server-authoritative: ensure this unit is removed from replication registry before entity/actor destruction
	if (World && World->GetNetMode() != NM_Client)
	{
		if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World))
		{
			bool bRemoved = false;
			if (AUnitBase* UnitBase = Cast<AUnitBase>(Owner))
			{
				if (UnitBase->UnitIndex != INDEX_NONE)
				{
					// Resolve NetID if possible to quarantine it
					if (MassEntityHandle.IsValid())
					{
						if (const FMassNetworkIDFragment* NetIDFrag = MassEntitySubsystemCache->GetEntityManager().GetFragmentDataPtr<FMassNetworkIDFragment>(MassEntityHandle))
						{
							Reg->QuarantineNetID(NetIDFrag->NetID.GetValue());
						}
					}
					
					bRemoved |= Reg->Registry.RemoveByUnitIndex(UnitBase->UnitIndex);
				}
				bRemoved |= Reg->Registry.RemoveByOwner(Owner ? Owner->GetFName() : NAME_None);
			}
			else
			{
				bRemoved |= Reg->Registry.RemoveByOwner(Owner ? Owner->GetFName() : NAME_None);
			}
			if (bRemoved)
			{
				Reg->Registry.MarkArrayDirty();
				Reg->ForceNetUpdate();
			}
		}
	}

	if (MassEntityHandle.IsValid())
	{
		// Ensure our cached subsystem pointer is still valid
		if (!MassEntitySubsystemCache && World)
		{
			MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
		}
		if (MassEntitySubsystemCache)
		{
			FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();
			if (EntityManager.IsEntityValid(MassEntityHandle))
			{
				// Before destroying, clear any client-side cached replication data for this entity's NetID
				if (const FMassNetworkIDFragment* NetIDFrag = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(MassEntityHandle))
				{
					UnitReplicationCache::Remove(NetIDFrag->NetID);
				}
				// Queue the destruction command as normal.
				EntityManager.Defer().DestroyEntity(MassEntityHandle);
			}
		}
		MassEntityHandle.Reset();
	}
}
// --- END ADDED ---


// --- MODIFIED: EndPlay implementation now calls the helper ---
/** Called when the component is being destroyed. */
void UMassActorBindingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Call our dedicated cleanup function
    CleanupMassEntity();

    // IMPORTANT: Call the base class implementation LAST.
    Super::EndPlay(EndPlayReason);
}

void UMassActorBindingComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// CleanupMassEntity();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}