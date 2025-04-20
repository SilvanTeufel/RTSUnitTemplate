// Fill out your copyright notice in the Description page of Project Settings.


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
#include "MassEntityTemplateRegistry.h" 
#include "Steering/MassSteeringFragments.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "MassEntitySubsystem.h"
#include "Actors/Waypoint.h"
#include "Characters/Unit/UnitBase.h"


UMassActorBindingComponent::UMassActorBindingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickInterval(0.5);
}

// Called when the game starts
void UMassActorBindingComponent::BeginPlay()
{
	Super::BeginPlay();
	// Cache subsystem here too if needed for Tick
	UWorld* World = GetWorld();
	MyOwner = GetOwner();
	AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);
	UE_LOG(LogTemp, Log, TEXT("BeginPlay MassUnit TeamId %d"), UnitBase->TeamId);
	if(!MassEntitySubsystemCache)
	{
		if(World)
		{
			MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
		}
	}

	if (!MassEntityHandle.IsValid() && MassEntitySubsystemCache) // Only create if not already set/created
	{
		
		UE_LOG(LogTemp, Log, TEXT("Got MassEntitySubsystemCache Trying to Spawn MassUnit %d"), UnitBase->TeamId);
		MassEntityHandle = CreateAndLinkOwnerToMassEntity();
	}


	if (!World)
	{
		return; // World might not be valid yet
	}


	if(MassEntitySubsystemCache )
	{
		UE_LOG(LogTemp, Log, TEXT("Got MassEntitySubsystemCache Trying to Spawn MassUnit"));
		FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();
		
		SpawnMassUnitIsm(
		EntityManager,
		UnitMassMesh,
		MyOwner->GetActorLocation() + FVector(0, 0, 200.f) ,
		World);
	}
}

FMassEntityHandle UMassActorBindingComponent::CreateAndLinkOwnerToMassEntity()
{
    // Prevent creating multiple entities for the same component
	/*
	if (!EntityConfig) // Check if the EntityConfig is assigned
	{
		UE_LOG(LogTemp, Error, TEXT("UMassActorBindingComponent: EntityConfig is not set on Actor %s!"), *GetOwner()->GetName());
		return FMassEntityHandle();
	}*/

    if (MassEntityHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("UMassActorBindingComponent: Entity already created for Actor %s."), *GetOwner()->GetName());
        return MassEntityHandle;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogTemp, Error, TEXT("UMassActorBindingComponent: Cannot create entity, Owner is null."));
        return FMassEntityHandle();
    }

    UWorld* World = Owner->GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("UMassActorBindingComponent: Cannot create entity for %s, World is null."), *Owner->GetName());
        return FMassEntityHandle();
    }

	
    if (!MassEntitySubsystemCache)
    {
        UE_LOG(LogTemp, Error, TEXT("UMassActorBindingComponent: Cannot create entity for %s, MassEntitySubsystem not found."), *Owner->GetName());
        return FMassEntityHandle();
    }

	TArray<const UScriptStruct*> FragmentsAndTags = {
    	// Core Movement & State
    	FTransformFragment::StaticStruct(),
		FMassVelocityFragment::StaticStruct(),          // Needed by Avoidance & Movement
		FMassForceFragment::StaticStruct(),             // Needed by Movement Processor
		FMassMoveTargetFragment::StaticStruct(),        // Input for your UnitMovementProcessor
		FAgentRadiusFragment::StaticStruct(),           // Often used by Avoidance/Movement

    	FMassMovementParameters::StaticStruct(), 
		// Steering & Avoidance
		FMassSteeringFragment::StaticStruct(),          // ** REQUIRED: Output of UnitMovementProcessor, Input for Steer/Avoid/Move **
		FMassAvoidanceColliderFragment::StaticStruct(), // ** REQUIRED: Avoidance shape **

		// Your Custom Logic
		FUnitMassTag::StaticStruct(),                   // Your custom tag
		FUnitNavigationPathFragment::StaticStruct(),    // ** REQUIRED: Used by your UnitMovementProcessor for path state **

		// Actor Representation & Sync
		FMassActorFragment::StaticStruct(),             // ** REQUIRED: Links Mass entity to Actor **
		FMassRepresentationFragment::StaticStruct(),    // Needed by representation system
		FMassRepresentationLODFragment::StaticStruct()  // Needed by representation system
	};

	// 1. Create the Archetype Creation Parameters

	
	// Get the NON-CONST EntityManager (This is correct now)
	FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();


	// --- Get Archetype DIRECTLY from UMassEntityConfigAsset ---

	// 1. Get the Entity Template directly from the Config Asset
	//const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World); // <-- Use the method on the Config Asset itself!

	// 2. Get the Archetype Handle from the Template
	//FMassArchetypeHandle ArchetypeHandle = EntityTemplate.GetArchetype();

	
	// --- End Archetype Retrieval ---
	FMassArchetypeCreationParams CreationParams;
	CreationParams.ChunkMemorySize=0;
	CreationParams.DebugName=FName("UMassActorBindingComponent");
	FMassArchetypeHandle ArchetypeHandle = EntityManager.CreateArchetype(FragmentsAndTags, CreationParams);
	
	if (!ArchetypeHandle.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("UMassActorBindingComponent: Failed to create archetype for %s!"), *Owner->GetName());
		return FMassEntityHandle();
	}

	// --- Define and Package Shared Fragment Values ---
	FMassMovementParameters MovementParamsInstance;
	MovementParamsInstance.MaxSpeed = 500.0f;     // Set desired value
	MovementParamsInstance.MaxAcceleration = 4000.0f; // Set desired value
	MovementParamsInstance.DefaultDesiredSpeed = 400.0f; // Example: Default speed slightly less than max
	MovementParamsInstance.DefaultDesiredSpeedVariance = 0.00f; // Example: +/- 5% variance is 0.05
	MovementParamsInstance.HeightSmoothingTime = 0.0f; // 0.2f 
	// Ensure values are validated if needed (or use MovementParamsInstance.GetValidated())
	// FMassMovementParameters ValidatedParams = MovementParamsInstance.GetValidated();

	// Get the const shared fragment handle for this specific set of parameter values
	FConstSharedStruct MovementParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovementParamsInstance); // Use instance directly

	// Package the shared fragments
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.AddConstSharedFragment(MovementParamSharedFragment);
	// Add other shared fragments here if needed (e.g., RepresentationParams) using the same pattern

	// 2. Steering Parameters (Using default values initially)
	FMassMovingSteeringParameters MovingSteeringParamsInstance;
	// You can modify defaults here if needed: MovingSteeringParamsInstance.ReactionTime = 0.2f;
	MovingSteeringParamsInstance.ReactionTime = 0.0f; // Faster reaction (Default 0.3) // 0.05f;
	MovingSteeringParamsInstance.LookAheadTime = 0.25f; // Look less far ahead (Default 1.0) - might make turns sharper but potentially start sooner

	FConstSharedStruct MovingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovingSteeringParamsInstance);
	SharedValues.AddConstSharedFragment(MovingSteeringParamSharedFragment);

	FMassStandingSteeringParameters StandingSteeringParamsInstance;
	// You can modify defaults here if needed
	FConstSharedStruct StandingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(StandingSteeringParamsInstance);
	SharedValues.AddConstSharedFragment(StandingSteeringParamSharedFragment);

	// 3. Avoidance Parameters (Using default values initially)
	FMassMovingAvoidanceParameters MovingAvoidanceParamsInstance;
	// You can modify defaults here if needed: MovingAvoidanceParamsInstance.PredictiveAvoidanceTime = 1.5f;
	FConstSharedStruct MovingAvoidanceParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovingAvoidanceParamsInstance.GetValidated()); // Use GetValidated if it exists
	SharedValues.AddConstSharedFragment(MovingAvoidanceParamSharedFragment);

	FMassStandingAvoidanceParameters StandingAvoidanceParamsInstance;
	// You can modify defaults here if needed
	FConstSharedStruct StandingAvoidanceParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(StandingAvoidanceParamsInstance.GetValidated()); // Use GetValidated if it exists
	SharedValues.AddConstSharedFragment(StandingAvoidanceParamSharedFragment);

	// ***** --- ADD THIS LINE --- *****
	SharedValues.Sort(); // Sort the internal lists before passing to CreateEntity
	// ***** --- END ADDED LINE --- *****

    FMassEntityHandle NewEntityHandle = EntityManager.CreateEntity(ArchetypeHandle, SharedValues);

    if (!NewEntityHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("UMassActorBindingComponent: Failed to create Mass Entity for %s!"), *Owner->GetName());
        return FMassEntityHandle();
    }
	
	// 3. Initialize Entity Fragments using the Owner Actor's state

	// 3a. Initialize Transform Fragment
	FTransformFragment& TransformFrag = EntityManager.GetFragmentDataChecked<FTransformFragment>(NewEntityHandle);
	TransformFrag.SetTransform(Owner->GetActorTransform()); // Use owner's current transform

	// 3b. Initialize Velocity Fragment
	FMassVelocityFragment& VelocityFrag = EntityManager.GetFragmentDataChecked<FMassVelocityFragment>(NewEntityHandle);
	VelocityFrag.Value = Owner->GetVelocity(); // Initialize with the owner Actor's current velocity

	// 3c. (Optional but Recommended) Initialize Move Target Fragment to a default state
	// Systems often expect this fragment to exist and have sensible defaults.
	FMassMoveTargetFragment& MoveTargetFrag = EntityManager.GetFragmentDataChecked<FMassMoveTargetFragment>(NewEntityHandle);
	MoveTargetFrag.Center = Owner->GetActorLocation(); // Set initial target location to current actor location
	MoveTargetFrag.DistanceToGoal = 0.f;              // No distance to cover initially
	MoveTargetFrag.DesiredSpeed.Set(0.f);             // No desired speed initially
	MoveTargetFrag.IntentAtGoal = EMassMovementAction::Stand; // Default action is to stand
	MoveTargetFrag.Forward = Owner->GetActorForwardVector(); // Align forward direction initially
	// --- INITIALIZE NEWLY ADDED FRAGMENTS ---

	//FMassMovementParameters& MoveParameters = EntityManager.GetFragmentDataChecked<FMassMovementParameters>(NewEntityHandle);

	


	// 3d. Initialize Actor Fragment ** THIS ESTABLISHES THE LINK **
	FMassActorFragment& ActorFrag = EntityManager.GetFragmentDataChecked<FMassActorFragment>(NewEntityHandle);
	ActorFrag.SetAndUpdateHandleMap(NewEntityHandle, Owner, false);
	// 3e. Initialize Representation Fragments (Set sensible defaults)
	FMassRepresentationFragment& RepFrag = EntityManager.GetFragmentDataChecked<FMassRepresentationFragment>(NewEntityHandle);
	// If the Actor already exists in the level and is managed externally (not by Mass spawning pools)


	FMassRepresentationLODFragment& RepLODFrag = EntityManager.GetFragmentDataChecked<FMassRepresentationLODFragment>(NewEntityHandle);
	RepLODFrag.LOD = EMassLOD::High; // Start visible, representation processor will adjust
	RepLODFrag.PrevLOD = EMassLOD::Max; // Indicate it wasn't visible before


	if(FAgentRadiusFragment* RadiusFrag = EntityManager.GetFragmentDataPtr<FAgentRadiusFragment>(NewEntityHandle))
	{
		RadiusFrag->Radius = 35.f; // Set a sensible radius
		UE_LOG(LogTemp, Log, TEXT("Entity %s: Set AgentRadius = %.2f"), *NewEntityHandle.DebugGetDescription(), RadiusFrag->Radius);
	}
	
	if(FMassAvoidanceColliderFragment* AvoidanceFrag = EntityManager.GetFragmentDataPtr<FMassAvoidanceColliderFragment>(NewEntityHandle))
	{
		*AvoidanceFrag = FMassAvoidanceColliderFragment(FMassCircleCollider(35.f)); // Set shape AND radius
		UE_LOG(LogTemp, Log, TEXT("Entity %s: Set Avoidance Radius = %.2f"), *NewEntityHandle.DebugGetDescription(), AvoidanceFrag->GetCircleCollider().Radius);
	}

	// --- Call Helper Function to Initialize Stats ---
	InitializeMassEntityStatsFromOwner(EntityManager, NewEntityHandle, Owner); // <<< CALL THE NEW FUNCTION

    // 4. Store the handle within this component
    MassEntityHandle = NewEntityHandle;

    UE_LOG(LogTemp, Log, TEXT("UMassActorBindingComponent: Created Mass Entity %d and linked to Actor %s"), MassEntityHandle.Index, *Owner->GetName());

    return MassEntityHandle;
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
        UnitAttributes = UnitOwner->Attributes; // <<< REPLACE UUnitAttributesComponent
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeMassEntityStatsFromOwner: Owner %s is not of expected type 'AUnitBase'. Using default stats."), *OwnerActor->GetName());
    }

    if (UnitOwner && !UnitAttributes)
    {
         UE_LOG(LogTemp, Warning, TEXT("InitializeMassEntityStatsFromOwner: Owner %s (%s) is missing expected 'UUnitAttributesComponent'. Using default stats."), *OwnerActor->GetName(), *UnitOwner->GetName());
    }
    // --- End: Get Owner References ---


    float CalculatedAgentRadius = 35.f; // Default radius, will be updated from stats if possible

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
            CombatStatsFrag->Armor = UnitAttributes->GetArmor();
            CombatStatsFrag->MagicResistance = UnitAttributes->GetMagicResistance();
            CombatStatsFrag->MaxShield = UnitAttributes->GetMaxShield();
            CombatStatsFrag->Shield = CombatStatsFrag->MaxShield; // Start full
            //CombatStatsFrag->AgentRadius = UnitAttributes->GetAgentRadius(); // Get radius from stats
            //CombatStatsFrag->AcceptanceRadius = UnitAttributes->GetAcceptanceRadius();
            CombatStatsFrag->SightRadius = SightRadius;// We need to add this to Attributes i guess;
            CombatStatsFrag->LoseSightRadius = LoseSightRadius;// We need to add this to Attributes i guess;
            CombatStatsFrag->AttackPauseDuration = AttackPauseDuration;// We need to add this to Attributes i guess;
            CombatStatsFrag->bCanAttack = UnitOwner->CanAttack; // Assuming CanAttack() on Attributes
            CombatStatsFrag->bUseProjectile = UnitOwner->UseProjectile; // Assuming UsesProjectile() on Attributes

            //CalculatedAgentRadius = CombatStatsFrag->AgentRadius; // Use the value from stats
        }
        else // Use default values
        {
            *CombatStatsFrag = FMassCombatStatsFragment(); // Initialize with struct defaults
            CombatStatsFrag->TeamId = 0; // Sensible default team
            //CalculatedAgentRadius = CombatStatsFrag->AgentRadius; // Use default struct radius
             UE_LOG(LogTemp, Log, TEXT("Entity %s: Using default Combat Stats."), *EntityHandle.DebugGetDescription());
        }
    }

    // 2. Agent Characteristics Fragment
    if (FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(EntityHandle))
    {
        if (UnitOwner) // Use data from Actor if available
        {
            // <<< REPLACE Properties/Getters with your actual variable names/functions >>>
            CharFrag->bIsFlying = UnitOwner->IsFlying; // Assuming direct property access
            CharFrag->bCanAttackFlying = UnitOwner->CanOnlyAttackFlying;
            CharFrag->bCanAttackGround = UnitOwner->CanOnlyAttackFlying;
            CharFrag->bIsInvisible = UnitOwner->IsInvisible;
            CharFrag->bCanDetectInvisible = UnitOwner->CanDetectInvisible;
        }
        else // Use default values
        {
             *CharFrag = FMassAgentCharacteristicsFragment(); // Initialize with struct defaults
             UE_LOG(LogTemp, Log, TEXT("Entity %s: Using default Agent Characteristics."), *EntityHandle.DebugGetDescription());
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
        }
         else // Use default values
         {
             *PatrolFrag = FMassPatrolFragment(); // Initialize with struct defaults
             UE_LOG(LogTemp, Log, TEXT("Entity %s: Using default Patrol Config."), *EntityHandle.DebugGetDescription());
         }
         // Ensure transient data starts clean regardless of source
         PatrolFrag->CurrentWaypointIndex = INDEX_NONE;
         PatrolFrag->TargetWaypointLocation = UnitOwner->NextWaypoint->GetActorLocation();
    }

    // --- Initialize Radius/Avoidance Fragments USING the calculated AgentRadius ---
    // (Moved here as it depends on CombatStats)
    if(FAgentRadiusFragment* RadiusFrag = EntityManager.GetFragmentDataPtr<FAgentRadiusFragment>(EntityHandle))
    {
       RadiusFrag->Radius = CalculatedAgentRadius;
       UE_LOG(LogTemp, Log, TEXT("Entity %s: Set AgentRadius = %.2f"), *EntityHandle.DebugGetDescription(), RadiusFrag->Radius);
    }

    if(FMassAvoidanceColliderFragment* AvoidanceFrag = EntityManager.GetFragmentDataPtr<FMassAvoidanceColliderFragment>(EntityHandle))
    {
       // Make sure collider type matches expectations (Circle assumed here)
       *AvoidanceFrag = FMassAvoidanceColliderFragment(FMassCircleCollider(CalculatedAgentRadius));
       UE_LOG(LogTemp, Log, TEXT("Entity %s: Set Avoidance Radius = %.2f"), *EntityHandle.DebugGetDescription(), AvoidanceFrag->GetCircleCollider().Radius);
    }
}

void UMassActorBindingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MassEntityHandle.IsSet() || !MassEntityHandle.IsValid())
	{
		return;
	}
/*
	if (MassEntitySubsystemCache)
	{
		const FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetEntityManager();
		if (const FMassRepresentationFragment* RepFrag = EntityManager.GetFragmentDataPtr<FMassRepresentationFragment>(MassEntityHandle))
		{
			if (RepFrag->StaticMeshDescHandle.IsValid())
			{
				UE_LOG(LogTemp, Log, TEXT("TickComponent: Valid StaticMeshDescHandle, Index: %d"), RepFrag->StaticMeshDescHandle.ToIndex());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("TickComponent: Invalid StaticMeshDescHandle in Representation Fragment."));
			}
		}
	}*/
}
/*
void UMassActorBindingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction); // <-- Call the base class tick function
	
	if (!MassEntityHandle.IsSet() || !MassEntityHandle.IsValid()) // Combine checks for clarity
	{
		return;
	}
	//UE_LOG(LogTemp, Log, TEXT("TickComponent!!!!"));
	
	if (MassEntitySubsystemCache) // No need to check IsValid() again here, already done above
	{
		// Get the EntityManager from the subsystem
		const FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetEntityManager();

		// Retrieve the transform fragment data for this entity using the EntityManager

		if (const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(MassEntityHandle))
		{

			if (MyOwner)
			{
				const FVector CurrentLocation = MyOwner->GetActorLocation();
				const FVector NewLocation = TransformFragment->GetTransform().GetLocation();
				const float LocationDifferenceSqr = FVector::DistSquared(CurrentLocation, NewLocation);

				// Only update if moved more than, say, 1 unit squared (adjust threshold)
				if (LocationDifferenceSqr > 1.0f)
				{
					UE_LOG(LogTemp, Log, TEXT("NewLocation!!!! %s"), *NewLocation.ToString());
					MyOwner->SetActorLocation(NewLocation);
					
					// Optionally update rotation only if needed/changed significantly too
					// Optionally update rotation/scale as needed:
					// Owner->SetActorRotation(TransformFragment->GetTransform().GetRotation());
					// Owner->SetActorScale3D(TransformFragment->GetTransform().GetScale3D());

					// Or set the whole transform at once (potentially more efficient)
					// Owner->SetActorTransform(TransformFragment->GetTransform());
				}
			}
		}
	}
	
}*/

// Example helper: Create and register a static mesh description, returning a handle.
// (The actual function and signature may differ. Consult UE5.5 documentation for the correct API.)
FStaticMeshInstanceVisualizationDescHandle UMassActorBindingComponent::RegisterIsmDesc(UWorld* World, UStaticMesh* UnitStaticMesh)
{
	// 1. Construct the VisualizationDesc
	FStaticMeshInstanceVisualizationDesc VisDesc;

	// Create a mesh description for the unit static mesh.
	FMassStaticMeshInstanceVisualizationMeshDesc MeshDesc;
	MeshDesc.Mesh = UnitStaticMesh;
	MeshDesc.Mobility = EComponentMobility::Movable;
	MeshDesc.bCastShadows = true;
	// (Optionally set other properties such as MaterialOverrides or LOD significance.)

	// Add the mesh description to the Meshes array.
	VisDesc.Meshes.Add(MeshDesc);

	// (Optional) Set up a collision profile if needed:
	// VisDesc.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// 2. Access the updated subsystem.
	if (UMassRepresentationSubsystem* RepresentationSubsystem = World->GetSubsystem<UMassRepresentationSubsystem>())
	{
		FStaticMeshInstanceVisualizationDescHandle Handle = RepresentationSubsystem->FindOrAddStaticMeshDesc(VisDesc);
		UE_LOG(LogTemp, Log, TEXT("RegisterIsmDesc: Handle Valid: %s, Index: %d for Mesh %s"),
			Handle.IsValid() ? TEXT("True") : TEXT("False"),
			Handle.ToIndex(),
			*GetNameSafe(UnitStaticMesh)); // Use GetNameSafe
		return Handle;
	}

	// Return an invalid handle if the subsystem could not be retrieved.
	return FStaticMeshInstanceVisualizationDescHandle();
}

void UMassActorBindingComponent::SpawnMassUnitIsm(
    FMassEntityManager& EntityManager,
    UStaticMesh* UnitStaticMesh,
    const FVector SpawnLocation,
    UWorld* World)
{

	UE_LOG(LogTemp, Log, TEXT("Try SpawnMassUnitIsm"));
    if (!UnitStaticMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnMassUnit: UnitStaticMesh is null!"));
        return;
    }

    // 1. Fragments for basic representation + transform
    TArray<const UScriptStruct*> FragmentsAndTags = {
        FTransformFragment::StaticStruct(),
        FMassRepresentationFragment::StaticStruct(),
        FMassRepresentationLODFragment::StaticStruct(),

    	// --- ADD YOUR CUSTOM FRAGMENTS TO THE ARCHETYPE DEFINITION ---
		FMassCombatStatsFragment::StaticStruct(),      // Add stats
    	FUnitStateFragment::StaticStruct(),      // Add state

    	// --- Add other fragments needed for RTS units potentially ---
	    FMassMoveTargetFragment::StaticStruct(), // For pathfinding/movement commands
	    FMassVelocityFragment::StaticStruct(),   // For current movement physics
	   // FTeamMemberFragment::StaticStruct(),    // Example if you have teams
	   // FSelectableTag::StaticStruct()          // Example if units can be selected
    };

    FMassArchetypeHandle ArchetypeHandle = EntityManager.CreateArchetype(FragmentsAndTags);
    if (!ArchetypeHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnMassUnitISM: Failed to create archetype!"));
        return;
    }

    // 2. Create the entity
    FMassEntityHandle NewEntityHandle = EntityManager.CreateEntity(ArchetypeHandle);
    if (!NewEntityHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnMassUnitISM: Failed to create entity!"));
        return;
    }

    // 3. Initialize Fragments
    // Transform
    FTransformFragment& TransformFrag = EntityManager.GetFragmentDataChecked<FTransformFragment>(NewEntityHandle);
    TransformFrag.GetMutableTransform().SetLocation(SpawnLocation);

	FTransform& Transform = TransformFrag.GetMutableTransform();
	Transform.SetLocation(SpawnLocation);
	Transform.SetRotation(FQuat::Identity);
	Transform.SetScale3D(FVector(1.0f, 1.0f, 1.0f)); // Ensure proper scale
	
    // Representation LOD
    FMassRepresentationLODFragment& RepLODFrag = EntityManager.GetFragmentDataChecked<FMassRepresentationLODFragment>(NewEntityHandle);
    RepLODFrag.LOD = EMassLOD::High;
    RepLODFrag.PrevLOD = EMassLOD::Max;

    // Representation
    FMassRepresentationFragment& RepFrag = EntityManager.GetFragmentDataChecked<FMassRepresentationFragment>(NewEntityHandle);

    // For purely instanced mesh usage, set these to INDEX_NONE
    RepFrag.HighResTemplateActorIndex = INDEX_NONE;
    RepFrag.LowResTemplateActorIndex  = INDEX_NONE;

    // 4. Register the StaticMesh desc and store the handle
    FStaticMeshInstanceVisualizationDescHandle MeshDescHandle = RegisterIsmDesc(World, UnitStaticMesh);
	
    if (!MeshDescHandle.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnMassUnitISM: Failed to register static mesh description!"));
        return;
    }

    // Store it in the fragment
    RepFrag.StaticMeshDescHandle = MeshDescHandle;

	
	if (RepFrag.StaticMeshDescHandle.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("SpawnMassUnitIsm: Valid StaticMeshDescHandle, Index: %d"), RepFrag.StaticMeshDescHandle.ToIndex());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnMassUnitIsm: Invalid StaticMeshDescHandle in Representation Fragment."));
	}
	
	// --- 4. INITIALIZE YOUR CUSTOM FRAGMENTS ---
	FMassCombatStatsFragment& StatsFrag = EntityManager.GetFragmentDataChecked<FMassCombatStatsFragment>(NewEntityHandle);
	StatsFrag.Health = StatsFrag.MaxHealth; // Set current health to max health initially

	FUnitStateFragment& StateFrag = EntityManager.GetFragmentDataChecked<FUnitStateFragment>(NewEntityHandle);
	StateFrag.TargetEntity.Reset();                // Ensure no initial target
	
	FMassMoveTargetFragment& MoveTargetFrag = EntityManager.GetFragmentDataChecked<FMassMoveTargetFragment>(NewEntityHandle);
	MoveTargetFrag.Center = SpawnLocation; // Target is current location initially
	MoveTargetFrag.DistanceToGoal = 0.f;
	MoveTargetFrag.DesiredSpeed.Set(0.f);
	MoveTargetFrag.IntentAtGoal = EMassMovementAction::Stand;
	MoveTargetFrag.Forward = TransformFrag.GetTransform().GetRotation().GetForwardVector();
	
	
	FMassVelocityFragment& VelocityFrag = EntityManager.GetFragmentDataChecked<FMassVelocityFragment>(NewEntityHandle);
	VelocityFrag.Value = FVector::ZeroVector;
	DrawDebugSphere(World, SpawnLocation, 25.0f, 12, FColor::Green, false, 15.0f);
    UE_LOG(LogTemp, Log, TEXT("SpawnMassUnitISM: Created Entity %d with ISM representation."), NewEntityHandle.Index);
}