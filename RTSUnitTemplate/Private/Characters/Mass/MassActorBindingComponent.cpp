// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Mass/MassActorBindingComponent.h"


// --- REQUIRED MASS INCLUDES ---
#include "MassEntityManager.h"
#include "MassArchetypeTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Characters/Mass/UnitMassTag.h" // Include your custom tag definition (Adjust path)
#include "MassEntityUtils.h"

#include "MassRepresentationSubsystem.h"  
#include "MassRepresentationTypes.h"
// -----------------------------
#include "MassEntitySubsystem.h"
#include "MassRepresentationFragments.h"
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "GameFramework/Actor.h"


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
	
	if(!MassEntitySubsystemCache)
	{
		if(World)
		{
			MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
		}
	}

	if (!MassEntityHandle.IsValid() && MassEntitySubsystemCache) // Only create if not already set/created
	{
		MassEntityHandle = CreateAndLinkOwnerToMassEntity();
	}


	if (!World)
	{
		return; // World might not be valid yet
	}
	
	MyOwner = GetOwner();

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

    // 1. Define the Archetype (Hardcoded here, could be made configurable)
	TArray<const UScriptStruct*> FragmentsAndTags = {
		FTransformFragment::StaticStruct(),
		FMassVelocityFragment::StaticStruct(),
		FMassMoveTargetFragment::StaticStruct(),
		FUnitMassTag::StaticStruct(),

    	// --- ADD REQUIRED FRAGMENTS FOR ACTOR SYNC & REPRESENTATION ---
		FMassActorFragment::StaticStruct(),             // ** CRUCIAL for linking **
		FMassRepresentationFragment::StaticStruct(),    // Needed by representation system
		FMassRepresentationLODFragment::StaticStruct(), // Needed by representation system

    	FAgentRadiusFragment::StaticStruct(),
		FMassForceFragment::StaticStruct(),
		FMassAvoidanceColliderFragment::StaticStruct(),
	};

	// 1. Create the Archetype Creation Parameters
	FMassArchetypeCreationParams CreationParams;
	CreationParams.ChunkMemorySize=0;
	CreationParams.DebugName=FName("UMassActorBindingComponent");
	
	// Get the NON-CONST EntityManager (This is correct now)
	FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();


	FMassArchetypeHandle ArchetypeHandle = EntityManager.CreateArchetype(FragmentsAndTags, CreationParams);
	
	if (!ArchetypeHandle.IsValid()) {
		UE_LOG(LogTemp, Error, TEXT("UMassActorBindingComponent: Failed to create archetype for %s!"), *Owner->GetName());
		return FMassEntityHandle();
	}
	
    FMassEntityHandle NewEntityHandle = EntityManager.CreateEntity(ArchetypeHandle);

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

    // 4. Store the handle within this component
    MassEntityHandle = NewEntityHandle;

    UE_LOG(LogTemp, Log, TEXT("UMassActorBindingComponent: Created Mass Entity %d and linked to Actor %s"), MassEntityHandle.Index, *Owner->GetName());

    return MassEntityHandle;
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
		FUnitStatsFragment::StaticStruct(),      // Add stats
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
	FUnitStatsFragment& StatsFrag = EntityManager.GetFragmentDataChecked<FUnitStatsFragment>(NewEntityHandle);
	StatsFrag.CurrentHealth = StatsFrag.MaxHealth; // Set current health to max health initially

	FUnitStateFragment& StateFrag = EntityManager.GetFragmentDataChecked<FUnitStateFragment>(NewEntityHandle);
	StateFrag.CurrentState = EUnitState::Idle;     // Set initial state to Idle
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