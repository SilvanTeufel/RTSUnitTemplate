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
#include "GameFramework/CharacterMovementComponent.h"
#include "Mass/Signals/MySignals.h"


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
	//SetupMassOnUnit();

	/*
	if(MassEntitySubsystemCache )
	{
		UE_LOG(LogTemp, Log, TEXT("Got MassEntitySubsystemCache Trying to Spawn MassUnit"));
		FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();
		
		SpawnMassUnitIsm(
		EntityManager,
		UnitMassMesh,
		MyOwner->GetActorLocation() + FVector(0, 0, 200.f) ,
		World);
	}*/
}

void UMassActorBindingComponent::SetupMassOnUnit()
{
	UWorld* World = GetWorld();
	MyOwner = GetOwner();
	AUnitBase* UnitBase = Cast<AUnitBase>(MyOwner);

	if(!MassEntitySubsystemCache)
	{
		if(World)
		{
			MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
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
}



FMassEntityHandle UMassActorBindingComponent::CreateAndLinkOwnerToMassEntity()
{
	
	if (MassEntityHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Already created for %s"), *GetOwner()->GetName());
		return MassEntityHandle;
	}
	if (!MassEntitySubsystemCache)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEntitySubsystem not found"));
		return {};
	}

	FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
	FMassEntityHandle NewMassEntityHandle;
	
	if (!EM.IsProcessing())
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
			InitTransform(EM, NewMassEntityHandle);
			InitMovementFragments(EM, NewMassEntityHandle);
			InitAIFragments(EM, NewMassEntityHandle);
			InitRepresentation(EM, NewMassEntityHandle);
			bNeedsMassUnitSetup = false;
		}
    }
	
	return NewMassEntityHandle;
}

bool UMassActorBindingComponent::BuildArchetypeAndSharedValues(FMassArchetypeHandle& OutArchetype,
                                                               FMassArchetypeSharedFragmentValues& OutSharedValues)
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;

    UWorld* World = Owner->GetWorld();
    if (!World) return false;

    FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();

    TArray<const UScriptStruct*> FragmentsAndTags = {
        // Fragments & tags as before, plus a one-shot init tag
    	// Core Movement & State
    	FTransformFragment::StaticStruct(),
		FMassVelocityFragment::StaticStruct(),          // Needed by Avoidance & Movement
		FMassForceFragment::StaticStruct(),             // Needed by Movement Processor
		FMassMoveTargetFragment::StaticStruct(),        // Input for your UnitMovementProcessor
		FAgentRadiusFragment::StaticStruct(),           // Often used by Avoidance/Movement
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

		FMassWorkerStatsFragment::StaticStruct(),
		// Actor Representation & Sync
		FMassActorFragment::StaticStruct(),             // ** REQUIRED: Links Mass entity to Actor **
		FMassRepresentationFragment::StaticStruct(),    // Needed by representation system
		FMassRepresentationLODFragment::StaticStruct(),  // Needed by representation system
    	
        //FNeedsActorBindingInitTag::StaticStruct(), // one-shot init tag
    };

    FMassArchetypeCreationParams Params;
	Params.ChunkMemorySize=0;
	Params.DebugName=FName("UMassActorBindingComponent");

    OutArchetype = EntityManager.CreateArchetype(FragmentsAndTags, Params);
    if (!OutArchetype.IsValid()) return false;

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
	MovingAvoidanceParamsInstance.ObstacleSeparationStiffness  = 250.f;
	MovingAvoidanceParamsInstance.EnvironmentSeparationStiffness = 500.f;
	MovingAvoidanceParamsInstance.ObstaclePredictiveAvoidanceStiffness = 700.f;
	MovingAvoidanceParamsInstance.EnvironmentPredictiveAvoidanceStiffness = 200.f;

	// Validate and create the shared fragment
	FConstSharedStruct MovingAvoidanceParamSharedFragment =
		EntityManager.GetOrCreateConstSharedFragment(MovingAvoidanceParamsInstance.GetValidated());
	SharedValues.AddConstSharedFragment(MovingAvoidanceParamSharedFragment);

	// Standing avoidance (if you also use standing avoidance)
	FMassStandingAvoidanceParameters StandingAvoidanceParamsInstance;
	StandingAvoidanceParamsInstance.GhostObstacleDetectionDistance = 300.f;
	StandingAvoidanceParamsInstance.GhostSeparationDistance       = 20.f;
	StandingAvoidanceParamsInstance.GhostSeparationStiffness      = 200.f;
	// … any other Ghost* fields you want to override …
	FConstSharedStruct StandingAvoidanceParamSharedFragment =
	EntityManager.GetOrCreateConstSharedFragment(StandingAvoidanceParamsInstance.GetValidated());
	SharedValues.AddConstSharedFragment(StandingAvoidanceParamSharedFragment);
	// ***** --- ADD THIS LINE --- *****
	// ***** --- END ADDED LINE --- *****

	
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

    // MoveTarget
    FMassMoveTargetFragment& MT = EntityManager.GetFragmentDataChecked<FMassMoveTargetFragment>(Handle);
    MT.Center = GetOwner()->GetActorLocation();
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
	if(!MassEntitySubsystemCache)
	{
		if(World)
		{
			MassEntitySubsystemCache = World->GetSubsystem<UMassEntitySubsystem>();
		}
	}

	if (!MassEntityHandle.IsValid() && MassEntitySubsystemCache) // Only create if not already set/created
	{
		bNeedsMassBuildingSetup = true;
	}


	if (!World)
	{
		return; // World might not be valid yet
	}
}



FMassEntityHandle UMassActorBindingComponent::CreateAndLinkBuildingToMassEntity()
{
	if (MassEntityHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Already created for %s"), *GetOwner()->GetName());
		return MassEntityHandle;
	}
	if (!MassEntitySubsystemCache)
	{
		UE_LOG(LogTemp, Error, TEXT("MassEntitySubsystem not found"));
		return {};
	}

	FMassEntityManager& EM = MassEntitySubsystemCache->GetMutableEntityManager();
	FMassEntityHandle NewMassEntityHandle;
	
	if (!EM.IsProcessing())
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
			InitTransform(EM, NewMassEntityHandle);
			InitAIFragments(EM, NewMassEntityHandle);
			InitRepresentation(EM, NewMassEntityHandle);
			bNeedsMassBuildingSetup = false;
			
		}
	}
	
	return NewMassEntityHandle;
}


bool UMassActorBindingComponent::BuildArchetypeAndSharedValuesForBuilding(FMassArchetypeHandle& OutArchetype,
                                                               FMassArchetypeSharedFragmentValues& OutSharedValues)
{
    AActor* Owner = GetOwner();
    if (!Owner) return false;

    UWorld* World = Owner->GetWorld();
    if (!World) return false;

    FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();

    TArray<const UScriptStruct*> FragmentsAndTags = {
        // Fragments & tags as before, plus a one-shot init tag
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
    	
        //FNeedsActorBindingInitTag::StaticStruct(), // one-shot init tag
    };

    FMassArchetypeCreationParams Params;
	Params.ChunkMemorySize=0;
	Params.DebugName=FName("UMassActorBindingComponent");

    OutArchetype = EntityManager.CreateArchetype(FragmentsAndTags, Params);
    if (!OutArchetype.IsValid()) return false;


	//FMassMovementParameters MovementParamsInstance;
	//MovementParamsInstance.MaxSpeed = 500.0f;     // Set desired value
	//MovementParamsInstance.MaxAcceleration = 4000.0f; // Set desired value
	//MovementParamsInstance.DefaultDesiredSpeed = 400.0f; // Example: Default speed slightly less than max
	//MovementParamsInstance.DefaultDesiredSpeedVariance = 0.00f; // Example: +/- 5% variance is 0.05
	//MovementParamsInstance.HeightSmoothingTime = 0.0f; // 0.2f 
	
	//FConstSharedStruct MovementParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovementParamsInstance); // Use instance directly

	// Package the shared fragments
	FMassArchetypeSharedFragmentValues SharedValues;
	//SharedValues.AddConstSharedFragment(MovementParamSharedFragment);
	// Add other shared fragments here if needed (e.g., RepresentationParams) using the same pattern

	// 2. Steering Parameters (Using default values initially)
	//FMassMovingSteeringParameters MovingSteeringParamsInstance;
	// You can modify defaults here if needed: MovingSteeringParamsInstance.ReactionTime = 0.2f;
	//MovingSteeringParamsInstance.ReactionTime = 0.0f; // Faster reaction (Default 0.3) // 0.05f;
	//MovingSteeringParamsInstance.LookAheadTime = 0.25f; // Look less far ahead (Default 1.0) - might make turns sharper but potentially start sooner

	//FConstSharedStruct MovingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(MovingSteeringParamsInstance);
	//SharedValues.AddConstSharedFragment(MovingSteeringParamSharedFragment);

/*
	FMassStandingSteeringParameters StandingSteeringParamsInstance;
	// You can modify defaults here if needed
	FConstSharedStruct StandingSteeringParamSharedFragment = EntityManager.GetOrCreateConstSharedFragment(StandingSteeringParamsInstance);
	SharedValues.AddConstSharedFragment(StandingSteeringParamSharedFragment);

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
	MovingAvoidanceParamsInstance.ObstacleSeparationStiffness  = 250.f;
	MovingAvoidanceParamsInstance.EnvironmentSeparationStiffness = 500.f;
	MovingAvoidanceParamsInstance.ObstaclePredictiveAvoidanceStiffness = 700.f;
	MovingAvoidanceParamsInstance.EnvironmentPredictiveAvoidanceStiffness = 200.f;
	
	// Validate and create the shared fragment
	FConstSharedStruct MovingAvoidanceParamSharedFragment =
		EntityManager.GetOrCreateConstSharedFragment(MovingAvoidanceParamsInstance.GetValidated());
	SharedValues.AddConstSharedFragment(MovingAvoidanceParamSharedFragment);
	*/
	// Standing avoidance (if you also use standing avoidance)
	//FMassStandingAvoidanceParameters StandingAvoidanceParamsInstance;
	//StandingAvoidanceParamsInstance.GhostObstacleDetectionDistance = 300.f;
	//StandingAvoidanceParamsInstance.GhostSeparationDistance       = 20.f;
	//StandingAvoidanceParamsInstance.GhostSeparationStiffness      = 200.f;
	// … any other Ghost* fields you want to override …
	/*
	FConstSharedStruct StandingAvoidanceParamSharedFragment =
	EntityManager.GetOrCreateConstSharedFragment(StandingAvoidanceParamsInstance.GetValidated());
	SharedValues.AddConstSharedFragment(StandingAvoidanceParamSharedFragment);
	// ***** --- ADD THIS LINE --- *****
	// ***** --- END ADDED LINE --- *****
	*/
	
    SharedValues.Sort();

	OutSharedValues = SharedValues;

    return true;
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
            CombatStatsFrag->PauseDuration = UnitOwner->PauseDuration;// We need to add this to Attributes i guess;
        	CombatStatsFrag->AttackDuration = UnitOwner->AttackDuration;
        	CombatStatsFrag->bCanAttack = UnitOwner->CanAttack; // Assuming CanAttack() on Attributes
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
            CharFrag->bCanOnlyAttackFlying = UnitOwner->CanOnlyAttackFlying;
            CharFrag->bCanOnlyAttackGround = UnitOwner->CanOnlyAttackFlying;
            CharFrag->bIsInvisible = UnitOwner->IsInvisible;
        	CharFrag->bCanBeInvisible = UnitOwner->bCanBeInvisible;
            CharFrag->bCanDetectInvisible = UnitOwner->CanDetectInvisible;
        	CharFrag->DespawnTime = DespawnTime;
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
       RadiusFrag->Radius = CalculatedAgentRadius;
    }

    if(FMassAvoidanceColliderFragment* AvoidanceFrag = EntityManager.GetFragmentDataPtr<FMassAvoidanceColliderFragment>(EntityHandle))
    {
       // Make sure collider type matches expectations (Circle assumed here)
       *AvoidanceFrag = FMassAvoidanceColliderFragment(FMassCircleCollider(CalculatedAgentRadius));
    }
	
}

void UMassActorBindingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
		
	if (!MassEntityHandle.IsSet() || !MassEntityHandle.IsValid() && bNeedsMassUnitSetup)
	{
		MassEntityHandle = CreateAndLinkOwnerToMassEntity();
	}

	if (!MassEntityHandle.IsSet() || !MassEntityHandle.IsValid() && bNeedsMassBuildingSetup)
	{
		MassEntityHandle = CreateAndLinkBuildingToMassEntity();
	}
	
}


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

void UMassActorBindingComponent::CleanupMassEntity()
{
	AActor* Owner = GetOwner(); // Get owner for logging clarity

	// Check if the entity handle we stored is valid
	if (MassEntityHandle.IsValid())
	{
		// Also check if the Mass Entity Subsystem we cached is still accessible.
		if (MassEntitySubsystemCache)
		{
			FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();

			// Double check the entity is still considered valid by the EntityManager itself
			if (EntityManager.IsEntityValid(MassEntityHandle))
			{
				// --- REMOVED SECTION: Attempting to modify FMassActorFragment directly ---
				// It appears there's no standard public API to explicitly clear the Actor reference
				// within the fragment before destruction. We will rely on DestroyEntity
				// to handle the necessary cleanup within the Mass framework.
				// --- END REMOVED SECTION ---

				// Destroy the Mass entity. The Mass framework should handle unlinking
				// the Actor reference stored in FMassActorFragment and associated subsystem maps.
				EntityManager.Defer().DestroyEntity(MassEntityHandle);
	
			}
		}
		
		// Reset the handle in this component regardless of success/failure of subsystem access.
		MassEntityHandle.Reset();
	}
	else
	{
		 // Optional Log: UE_LOG(LogTemp, Log, TEXT("UMassActorBindingComponent::CleanupMassEntity: No valid MassEntityHandle to clean up for Actor %s."), *GetNameSafe(Owner));
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