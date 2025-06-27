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
#include "Mass/Signals/MassUnitSpawnerSubsystem.h"
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

	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("World not FOUND!"));
		return;
	}
	
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
		//UE_LOG(LogTemp, Log, TEXT("MassActorBindingComponent on %s: Flagged for Mass unit setup."), *MyOwner->GetName());
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

	// This function contains all the logic that used to be in CreateAndLinkOwnerToMassEntity
	// AFTER the entity was created.
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
	
	if (MassEntityHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Already created for %s"), *GetOwner()->GetName());
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
    	FMassChargeTimerFragment::StaticStruct(),

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
		UE_LOG(LogTemp, Warning, TEXT("Already created for %s"), *GetOwner()->GetName());
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
		if (!BuildArchetypeAndSharedValues(Archetype, SharedValues)) // BuildArchetypeAndSharedValuesForBuilding
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
		}
	}
	
	return NewMassEntityHandle;
	
}


bool UMassActorBindingComponent::BuildArchetypeAndSharedValuesForBuilding(FMassArchetypeHandle& OutArchetype,
                                                               FMassArchetypeSharedFragmentValues& OutSharedValues)
{

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

    FMassArchetypeCreationParams Params;
	Params.ChunkMemorySize=0;
	Params.DebugName=FName("UMassActorBindingComponent");

    OutArchetype = EntityManager.CreateArchetype(FragmentsAndTags, Params);
    if (!OutArchetype.IsValid()) return false;
	
	FMassArchetypeSharedFragmentValues SharedValues;

	
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
        	CharFrag->FlyHeight = UnitOwner->FlyHeight;
            CharFrag->bCanOnlyAttackFlying = UnitOwner->CanOnlyAttackFlying;
            CharFrag->bCanOnlyAttackGround = UnitOwner->CanOnlyAttackGround;
            CharFrag->bIsInvisible = UnitOwner->IsInvisible;
        	CharFrag->bCanBeInvisible = UnitOwner->bCanBeInvisible;
            CharFrag->bCanDetectInvisible = UnitOwner->CanDetectInvisible;
        	CharFrag->DespawnTime = DespawnTime;
			CharFrag->RotatesToMovement = RotatesToMovement;
        	CharFrag->RotatesToEnemy = RotatesToEnemy;
        	CharFrag->RotationSpeed = RotationSpeed;
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

void UMassActorBindingComponent::CleanupMassEntity()
{

	AActor* Owner = GetOwner(); 
	
	if (MassEntityHandle.IsValid())
	{
		
		// Ensure our cached subsystem pointer is still valid
		if (MassEntitySubsystemCache)
		{
			FMassEntityManager& EntityManager = MassEntitySubsystemCache->GetMutableEntityManager();

			if (EntityManager.IsEntityValid(MassEntityHandle))
			{
				// 1. Queue the destruction command as normal.
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