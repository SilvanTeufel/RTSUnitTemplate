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

void UMassActorBindingComponent::SetupMassOnUnit()
{
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
			InitTransform(EM, NewMassEntityHandle);
			InitMovementFragments(EM, NewMassEntityHandle);
			InitAIFragments(EM, NewMassEntityHandle);
			InitRepresentation(EM, NewMassEntityHandle);
			bNeedsMassUnitSetup = false;
			AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);
			UnitBase->bIsMassUnit = true;
			UnitBase->UpdatePredictionFragment(UnitBase->GetMassActorLocation(), 0);
			UnitBase->SyncTranslation();
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
							const uint32 NewID = Reg->GetNextNetID();
							NetFrag->NetID = FMassNetworkID(NewID);
							const FName OwnerName = MyOwner ? MyOwner->GetFName() : NAME_None;
							const int32 UnitIndex = (Cast<AUnitBase>(MyOwner)) ? Cast<AUnitBase>(MyOwner)->UnitIndex : INDEX_NONE;
							FUnitRegistryItem* Existing = nullptr;
							if (UnitIndex != INDEX_NONE)
							{
								Existing = Reg->Registry.FindByUnitIndex(UnitIndex);
							}
							if (!Existing)
							{
								Existing = Reg->Registry.FindByOwner(OwnerName);
							}
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

	AUnitBase* UnitBase = Cast<AUnitBase>(Owner);
	if (!UnitBase) return false;
	
    UWorld* World = Owner->GetWorld();
    if (!World) return false;

    FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();

    TArray<const UScriptStruct*> FragmentsAndTags = {
        // Fragments & tags as before, plus a one-shot init tag

    	// --- ADD THESE FRAGMENTS FOR REPLICATION ---
    	FMassNetworkIDFragment::StaticStruct(),
    	FMassReplicatedAgentFragment::StaticStruct(),
    	FMassReplicationViewerInfoFragment::StaticStruct(),
    	FMassReplicationLODFragment::StaticStruct(),
    	FMassReplicationGridCellLocationFragment::StaticStruct(),
    	FMassInReplicationGridTag::StaticStruct(),
    	FUnitReplicatedTransformFragment::StaticStruct(),
    	// --- END OF ADDITIONS ---
    	
    	// Core Movement & State
    	FTransformFragment::StaticStruct(),
		FMassVelocityFragment::StaticStruct(),          // Needed by Avoidance & Movement
		FMassForceFragment::StaticStruct(),             // Needed by Movement Processor
		FMassMoveTargetFragment::StaticStruct(),        // Input for your UnitMovementProcessor
		FAgentRadiusFragment::StaticStruct(),           // Often used by Avoidance/Movement
    	FMassClientPredictionFragment::StaticStruct(), 
    	//FMassMovementParameters::StaticStruct(), 
		// Steering & Avoidance
		FMassSteeringFragment::StaticStruct(),          // ** REQUIRED: Output of UnitMovementProcessor, Input for Steer/Avoid/Move **
		FMassAvoidanceColliderFragment::StaticStruct(), // ** REQUIRED: Avoidance shape **
    	
		FMassGhostLocationFragment::StaticStruct(),
		FMassNavigationEdgesFragment::StaticStruct(),
		//FMassStandingAvoidanceParameters::StaticStruct(),
		//FMassMovingAvoidanceParameters::StaticStruct(),
		//FMassMovementParameters::StaticStruct(),
		// Your Custom Logic
		FUnitMassTag::StaticStruct(),                   // Your custom tag
		FMassPatrolFragment::StaticStruct(), 
		FUnitNavigationPathFragment::StaticStruct(),    // ** REQUIRED: Used by your UnitMovementProcessor for path state **
    	
		FMassAIStateFragment::StaticStruct(),
    	FMassSightFragment::StaticStruct(),
		FMassAITargetFragment::StaticStruct(), 
		FMassCombatStatsFragment::StaticStruct(), 
		FMassAgentCharacteristicsFragment::StaticStruct(),
    	FMassChargeTimerFragment::StaticStruct(),

		FMassWorkerStatsFragment::StaticStruct(),
		// Actor Representation & Sync
		FMassActorFragment::StaticStruct(),             // ** REQUIRED: Links Mass entity to Actor **
		FMassRepresentationFragment::StaticStruct(),    // Needed by representation system
		FMassRepresentationLODFragment::StaticStruct(),  // Needed by representation system
    	
        //FNeedsActorBindingInitTag::StaticStruct(), // one-shot init tag
    };
	
	if(UnitBase->AddEffectTargetFragement)
		FragmentsAndTags.Add(FMassGameplayEffectTargetFragment::StaticStruct());
	
	
	if(UnitBase->AddGameplayEffectFragement)
		FragmentsAndTags.Add(FMassGameplayEffectFragment::StaticStruct());
	
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
	MovementParamsInstance.MaxAcceleration = 4000.0f; // Set desired value
	MovementParamsInstance.DefaultDesiredSpeed = MyUnit->Attributes->GetRunSpeed(); //400.0f; // Example: Default speed slightly less than max
	MovementParamsInstance.DefaultDesiredSpeedVariance = 0.00f; // Example: +/- 5% variance is 0.05
	MovementParamsInstance.HeightSmoothingTime = 0.0f; // 0.2f 
	// Ensure values are validated if needed (or use MovementParamsInstance.GetValidated())
	// FMassMovementParameters ValidatedParams = MovementParamsInstance.GetValidated();

	// Get the const shared fragment handle for this specific set of parameter values
	FConstSharedStruct MovementParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovementParamsInstance); // Use instance directly
	
	// Package the shared fragments
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(MovementParamSharedFragment);
	// Add other shared fragments here if needed (e.g., RepresentationParams) using the same pattern

	// 2. Steering Parameters (Using default values initially)
	FMassMovingSteeringParameters MovingSteeringParamsInstance;
	// You can modify defaults here if needed: MovingSteeringParamsInstance.ReactionTime = 0.2f;
	MovingSteeringParamsInstance.ReactionTime = 0.05f; // Faster reaction (Default 0.3) // 0.05f;
	MovingSteeringParamsInstance.LookAheadTime = 0.25f; // Look less far ahead (Default 1.0) - might make turns sharper but potentially start sooner

	FConstSharedStruct MovingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovingSteeringParamsInstance);
	SharedValues.Add(MovingSteeringParamSharedFragment);

	FMassStandingSteeringParameters StandingSteeringParamsInstance;
	// You can modify defaults here if needed
	FConstSharedStruct StandingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(StandingSteeringParamsInstance);
	SharedValues.Add(StandingSteeringParamSharedFragment);
	
	// 3. Avoidance Parameters (Now explicitly initialized)
	FMassMovingAvoidanceParameters MovingAvoidanceParamsInstance;
	// Core detection radius
	MovingAvoidanceParamsInstance.ObstacleDetectionDistance    = 400.f;  // How far agents see each other
	// Separation tuning
	MovingAvoidanceParamsInstance.ObstacleSeparationDistance   = AvoidanceDistance;  //75.f // How close they can get before repelling
	MovingAvoidanceParamsInstance.EnvironmentSeparationDistance= AvoidanceDistance;   // Wall‐avoidance distance
	// Predictive avoidance tuning
	MovingAvoidanceParamsInstance.PredictiveAvoidanceTime      = 2.5f;  // How far ahead in seconds
	MovingAvoidanceParamsInstance.PredictiveAvoidanceDistance  = AvoidanceDistance;   // Look-ahead distance in cm
	// (you can also tweak stiffness if desired)
	// --- INCREASE THESE FOR STRONGER PUSHING ---
	MovingAvoidanceParamsInstance.ObstacleSeparationDistance   = AvoidanceDistance + 50.f; // Or a fixed larger value like 100.f or 125.f
	MovingAvoidanceParamsInstance.ObstacleSeparationStiffness  = ObstacleSeparationStiffness; // Significantly increase this for a stronger push
	
	//MovingAvoidanceParamsInstance.ObstacleSeparationStiffness  = 250.f;
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

	// Inject replication shared fragment so Mass replication knows our replicator and bubble type
	// NOTE: Temporarily disabled to avoid engine assertion until valid client bubbles exist reliably in UE5.7 P2P.
	#if 0
	// NOTE: Only inject on server/authority worlds. On clients, skip to avoid triggering
	// UMassReplicationProcessor paths that expect valid client handles/bubbles.
	if (UWorld* NetWorld = GetWorld())
	{
		if (NetWorld->GetNetMode() != NM_Client)
		{
			FMassReplicationSharedFragment RepShared;
			// Share a single replicator instance per world so entities can batch into the same chunk
			{
				static TMap<const UWorld*, TWeakObjectPtr<UMassUnitReplicatorBase>> GReplicatorPerWorld;
				UWorld* W = GetWorld();
				UMassUnitReplicatorBase* SharedReplicator = nullptr;
				if (W)
				{
					if (TWeakObjectPtr<UMassUnitReplicatorBase>* Found = GReplicatorPerWorld.Find(W))
					{
						SharedReplicator = Found->Get();
					}
					if (!SharedReplicator)
					{
						SharedReplicator = NewObject<UMassUnitReplicatorBase>((UObject*)GetTransientPackage(), UMassUnitReplicatorBase::StaticClass());
						GReplicatorPerWorld.Add(W, SharedReplicator);
					}
				}
				RepShared.CachedReplicator = SharedReplicator;
			}

			// Resolve BubbleInfo class handle as early as possible, but only if there is at least one valid client handle
			if (UWorld* WorldPtr = GetWorld())
			{
				FMassBubbleInfoClassHandle Handle; // leave invalid by default to opt-out safely
				if (UMassReplicationSubsystem* RepSub = WorldPtr->GetSubsystem<UMassReplicationSubsystem>())
				{
					const TArray<FMassClientHandle>& Handles = RepSub->GetClientReplicationHandles();
					bool bHasValidClient = false;
					for (const FMassClientHandle& H : Handles)
					{
						if (RepSub->IsValidClientHandle(H)) { bHasValidClient = true; break; }
					}
					if (bHasValidClient)
					{
						// First, try our bootstrap-registered handle
						Handle = RTSReplicationBootstrap::GetUnitBubbleHandle(WorldPtr);
						// If still invalid on server, ask the replication subsystem for the handle
						if (!Handle.IsValid() && WorldPtr->GetNetMode() != NM_Client)
						{
							const TSubclassOf<AMassClientBubbleInfoBase> BubbleCls = AUnitClientBubbleInfo::StaticClass();
							Handle = RepSub->GetBubbleInfoClassHandle(BubbleCls);
							if (!RepSub->IsBubbleClassHandleValid(Handle))
							{
								// As a last resort (early in world start), register it here on the server
								Handle = RepSub->RegisterBubbleInfoClass(BubbleCls);
							}
						}
					}
				}
				RepShared.BubbleInfoClassHandle = Handle; // remains invalid when no valid clients -> engine will skip safely
			}

			FSharedStruct SharedRep = EntityManager.GetOrCreateSharedFragment(RepShared);
			// Only add the replication shared fragment when we have at least one valid client and a valid bubble handle
			if (RepShared.BubbleInfoClassHandle.IsValid())
			{
				SharedValues.Add(SharedRep);
			}
		}
	}
	#endif

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
		MT.SlackRadius = Unit->StopRunTolerance;
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
			InitTransform(EM, NewMassEntityHandle);

			if (AUnitBase* Unit = Cast<AUnitBase>(MyOwner))
				if (Unit->CanMove)
					InitMovementFragments(EM, NewMassEntityHandle);
			
			InitAIFragments(EM, NewMassEntityHandle);
			InitRepresentation(EM, NewMassEntityHandle);
			bNeedsMassBuildingSetup = false;
			AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);
			UnitBase->bIsMassUnit = true;

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
								const uint32 NewID = Reg->GetNextNetID();
								NetFrag->NetID = FMassNetworkID(NewID);
								const FName OwnerName = MyOwner ? MyOwner->GetFName() : NAME_None;
								const int32 UnitIdxVal = UnitBaseLocal ? UnitBaseLocal->UnitIndex : INDEX_NONE;
								FUnitRegistryItem* Existing = nullptr;
								if (UnitIdxVal != INDEX_NONE)
								{
									Existing = Reg->Registry.FindByUnitIndex(UnitIdxVal);
								}
								if (!Existing)
								{
									Existing = Reg->Registry.FindByOwner(OwnerName);
								}
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

	/*
    AActor* Owner = GetOwner();
    if (!Owner) return false;

	AUnitBase* UnitBase = Cast<AUnitBase>(Owner);
	if (!UnitBase) return false;

    UWorld* World = Owner->GetWorld();
    if (!World) return false;

	if(UnitBase->CanMove) 
	{
		return BuildArchetypeAndSharedValues(OutArchetype, OutSharedValues);
	}

	
    FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();

    TArray<const UScriptStruct*> FragmentsAndTags = {
        // Fragments & tags as before, plus a one-shot init tag

    	// --- ADD THESE FRAGMENTS FOR REPLICATION ---
    	FMassNetworkIDFragment::StaticStruct(),
		FMassReplicatedAgentFragment::StaticStruct(),
		FMassReplicationViewerInfoFragment::StaticStruct(),
		FMassReplicationLODFragment::StaticStruct(),
		FMassReplicationGridCellLocationFragment::StaticStruct(),
		FMassInReplicationGridTag::StaticStruct(),
		FUnitReplicatedTransformFragment::StaticStruct(),
		// --- END OF ADDITIONS ---
    	
    	// Core Movement & State
    	FTransformFragment::StaticStruct(),
		//FMassVelocityFragment::StaticStruct(),          // Needed by Avoidance & Movement
		//FMassForceFragment::StaticStruct(),             // Needed by Movement Processor
		//FMassMoveTargetFragment::StaticStruct(),        // Input for your UnitMovementProcessor
		FAgentRadiusFragment::StaticStruct(),           // Often used by Avoidance/Movement
		//FMassMovementParameters::StaticStruct(), 
		// Steering & Avoidance
		//FMassSteeringFragment::StaticStruct(),          // ** REQUIRED: Output of UnitMovementProcessor, Input for Steer/Avoid/Move **
		FMassAvoidanceColliderFragment::StaticStruct(), // ** REQUIRED: Avoidance shape **

		FMassGhostLocationFragment::StaticStruct(),
		//FMassNavigationEdgesFragment::StaticStruct(),
		//FMassStandingAvoidanceParameters::StaticStruct(),
		//FMassMovingAvoidanceParameters::StaticStruct(),
		//FMassMovementParameters::StaticStruct(),
		// Your Custom Logic
		FUnitMassTag::StaticStruct(),                   // Your custom tag
		FMassPatrolFragment::StaticStruct(), 
		//FUnitNavigationPathFragment::StaticStruct(),    // ** REQUIRED: Used by your UnitMovementProcessor for path state **
    
    	
		FMassAIStateFragment::StaticStruct(),
    	FMassSightFragment::StaticStruct(),
		FMassAITargetFragment::StaticStruct(), 
		FMassCombatStatsFragment::StaticStruct(), 
		FMassAgentCharacteristicsFragment::StaticStruct(), 

		//FMassWorkerStatsFragment::StaticStruct(),
		// Actor Representation & Sync
		FMassActorFragment::StaticStruct(),             // ** REQUIRED: Links Mass entity to Actor **
		FMassRepresentationFragment::StaticStruct(),    // Needed by representation system
		FMassRepresentationLODFragment::StaticStruct(),  // Needed by representation system
    };


	if(UnitBase->AddEffectTargetFragement)
		FragmentsAndTags.Add(FMassGameplayEffectTargetFragment::StaticStruct());
	
	
	if(UnitBase->AddGameplayEffectFragement)
		FragmentsAndTags.Add(FMassGameplayEffectFragment::StaticStruct());
	
    FMassArchetypeCreationParams Params;
	Params.ChunkMemorySize=0;
	Params.DebugName=FName("UMassActorBindingComponent");

    OutArchetype = EntityManager.CreateArchetype(FragmentsAndTags, Params);
    if (!OutArchetype.IsValid()) return false;
	
	FMassArchetypeSharedFragmentValues SharedValues;

	// Inject replication shared fragment so Mass replication knows our replicator and bubble type
	{
		FMassReplicationSharedFragment RepShared;
    	// Share a single replicator instance per world so entities can batch into the same chunk
	    {
			static TMap<const UWorld*, TWeakObjectPtr<UMassUnitReplicatorBase>> GReplicatorPerWorld;
			UWorld* W = GetWorld();
			UMassUnitReplicatorBase* SharedReplicator = nullptr;
			if (W)
			{
				if (TWeakObjectPtr<UMassUnitReplicatorBase>* Found = GReplicatorPerWorld.Find(W))
				{
					SharedReplicator = Found->Get();
				}
				if (!SharedReplicator)
				{
					SharedReplicator = NewObject<UMassUnitReplicatorBase>((UObject*)GetTransientPackage(), UMassUnitReplicatorBase::StaticClass());
					GReplicatorPerWorld.Add(W, SharedReplicator);
				}
			}
			RepShared.CachedReplicator = SharedReplicator;
	    }

    	// Resolve BubbleInfo class handle as early as possible
    	if (UWorld* WorldPtr = GetWorld())
    	{
    		// First, try our bootstrap-registered handle
    		FMassBubbleInfoClassHandle Handle = RTSReplicationBootstrap::GetUnitBubbleHandle(WorldPtr);
    		// If still invalid on server, ask the replication subsystem for the handle (avoid late registration on clients)
    		if (!Handle.IsValid() && WorldPtr->GetNetMode() != NM_Client)
    		{
    			if (UMassReplicationSubsystem* RepSub = WorldPtr->GetSubsystem<UMassReplicationSubsystem>())
    			{
    				const TSubclassOf<AMassClientBubbleInfoBase> BubbleCls = AUnitClientBubbleInfo::StaticClass();
    				Handle = RepSub->GetBubbleInfoClassHandle(BubbleCls);
    				if (!RepSub->IsBubbleClassHandleValid(Handle))
    				{
    					// As a last resort (early in world start), register it here on the server
    					Handle = RepSub->RegisterBubbleInfoClass(BubbleCls);
    				}
    			}
    		}
    		RepShared.BubbleInfoClassHandle = Handle;
    	}
	}
	
    SharedValues.Sort();

	OutSharedValues = SharedValues;

    return true;
	*/
}

void UMassActorBindingComponent::InitializeMassEntityStatsFromOwner(FMassEntityManager& EntityManager,
	FMassEntityHandle EntityHandle, AActor* OwnerActor)
{
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
		/*
    	if (UnitOwner->GetCharacterMovement())
    	{
    		// Make sure the CharacterMovementComponent still ticks even if unpossessed
    		UnitOwner->GetCharacterMovement()->SetComponentTickEnabled(true);
    		UnitOwner->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
    	} */
    }
    else
    {
        //UE_LOG(LogTemp, Warning, TEXT("InitializeMassEntityStatsFromOwner: Owner %s is not of expected type 'AUnitBase'. Using default stats."), *OwnerActor->GetName());
    }

    if (UnitOwner && !UnitAttributes)
    {
         //UE_LOG(LogTemp, Warning, TEXT("InitializeMassEntityStatsFromOwner: Owner %s (%s) is missing expected 'UUnitAttributesComponent'. Using default stats."), *OwnerActor->GetName(), *UnitOwner->GetName());
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
            //PatrolFrag->bPatrolRandomAroundWaypoint = UnitOwner->NextWaypoint->PatrolCloseToWaypoint;
            //PatrolFrag->RandomPatrolRadius = UnitOwner->NextWaypoint->PatrolCloseMaxInterval;
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
       RadiusFrag->Radius = UnitOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    }

    if(FMassAvoidanceColliderFragment* AvoidanceFrag = EntityManager.GetFragmentDataPtr<FMassAvoidanceColliderFragment>(EntityHandle))
    {
       // Make sure collider type matches expectations (Circle assumed here)
       *AvoidanceFrag = FMassAvoidanceColliderFragment(FMassCircleCollider(UnitOwner->GetCapsuleComponent()->GetScaledCapsuleRadius()));
    }

	if(FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(EntityHandle))
	{
		TargetFrag->FollowRadius = FollowRadius;
		TargetFrag->FollowOffset = FollowOffset;
	}
	
}

void UMassActorBindingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	/*
	if (!MassEntityHandle.IsSet() || !MassEntityHandle.IsValid() && bNeedsMassUnitSetup)
	{
		MassEntityHandle = CreateAndLinkOwnerToMassEntity();
	}

	if (!MassEntityHandle.IsSet() || !MassEntityHandle.IsValid() && bNeedsMassBuildingSetup)
	{
		MassEntityHandle = CreateAndLinkBuildingToMassEntity();
	}
	*/
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
	// Prevent client-authoritative unlink to avoid desync; server drives destruction
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}
	// On server, fall back to proper cleanup
	CleanupMassEntity();
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