﻿// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationTypes.h"
#include "NavigationSystem.h" // Für GetRandomReachablePointInRadius
#include "Core/UnitData.h"
#include "UnitMassTag.generated.h"

USTRUCT()
struct FUnitMassTag : public FMassTag
{
	GENERATED_BODY()
};

// Position + movement are handled by built-in fragments like FMassMovementFragment, so no need to add here.
// --- Kern-Zustände ---
USTRUCT() struct FMassStateIdleTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateChaseTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateAttackTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStatePauseTag : public FMassTag { GENERATED_BODY() }; // Für Pause nach Angriff
USTRUCT() struct FMassStateDeadTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateRunTag : public FMassTag { GENERATED_BODY() }; // Generischer Bewegungs-Tag (für Run/Patrol)
USTRUCT() struct FMassStateDetectTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateStopMovementTag : public FMassTag { GENERATED_BODY() };

// --- Worker-Zustände ---
USTRUCT() struct FMassStateGoToBaseTag : public FMassTag { GENERATED_BODY() }; // Für Pause nach Angriff
USTRUCT() struct FMassStateGoToBuildTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateGoToResourceExtractionTag : public FMassTag { GENERATED_BODY() }; // Generischer Bewegungs-Tag (für Run/Patrol)
USTRUCT() struct FMassStateBuildTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateResourceExtractionTag : public FMassTag { GENERATED_BODY() };


// --- Patrouillen-Zustände ---
USTRUCT() struct FMassStatePatrolTag : public FMassTag { GENERATED_BODY() }; // Direkt zum WP
USTRUCT() struct FMassStatePatrolRandomTag : public FMassTag { GENERATED_BODY() }; // Zufällig um WP
USTRUCT() struct FMassStatePatrolIdleTag : public FMassTag { GENERATED_BODY() }; // Idle bei Zufalls-Patrouille

// --- Andere Zustände ---
USTRUCT() struct FMassStateEvasionTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateRootedTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateCastingTag : public FMassTag { GENERATED_BODY() };
USTRUCT() struct FMassStateIsAttackedTag : public FMassTag { GENERATED_BODY() };

// --- Hilfs-Tags ---
USTRUCT() struct FMassHasTargetTag : public FMassTag { GENERATED_BODY() }; // Wenn bHasValidTarget true ist
USTRUCT() struct FMassReachedDestinationTag : public FMassTag { GENERATED_BODY() }; // Von Movement gesetzt
//USTRUCT() struct FNeedsActorBindingInitTag : public FMassTag { GENERATED_BODY() }; // For PostInitProcessor
USTRUCT() struct FMassStateChargingTag : public FMassTag { GENERATED_BODY() }; 

USTRUCT() struct FMassStopGameplayEffectTag : public FMassTag { GENERATED_BODY() };

USTRUCT()
struct FMassChargeTimerFragment : public FMassFragment
{
	GENERATED_BODY()

	// Time when the charge effect should end (using game time seconds)
	UPROPERTY()
	float ChargeEndTime = 0.f;

	// The speed the unit should revert to after the charge
	UPROPERTY()
	float OriginalDesiredSpeed; // Or just float if you don't use FMassFactor for speed

	// Flag to indicate if the original speed was set (optional, for robustness)
	UPROPERTY()
	bool bOriginalSpeedSet = false;
};

USTRUCT()
struct FMassMovementOverrideFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement")
	bool bOverrideMaxSpeed = false;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float OverriddenMaxSpeed = 0.f;

	// You could add bOverrideAcceleration, OverriddenAcceleration, etc.
};


USTRUCT()
struct FMassGameplayEffectFragment : public FMassFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Ability")
	TSubclassOf<UGameplayEffect> FriendlyEffect;

	UPROPERTY(EditAnywhere, Category = "Ability")
	TSubclassOf<UGameplayEffect> EnemyEffect;

	UPROPERTY(EditAnywhere, Category = "Ability")
	float EffectRadius;
};

USTRUCT()
struct FMassGameplayEffectTargetFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ability")
	bool FriendlyEffectApplied = false;

	UPROPERTY(EditAnywhere, Category = "Ability")
	bool EnemyEffectApplied = false;
	
	UPROPERTY(EditAnywhere, Category = "Ability")
	float LastFriendlyEffectTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Ability")
	float LastEnemyEffectTime = 0.f;
	
	UPROPERTY(EditAnywhere, Category = "Ability")
	float FriendlyEffectCoolDown = 20.f;
	
	UPROPERTY(EditAnywhere, Category = "Ability")
	float EnemyEffectCoolDown = 20.f;
};

USTRUCT()
struct FMassWorkerStatsFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Time in seconds it takes this worker to extract one 'unit' of resource. */
	UPROPERTY(EditAnywhere, Category = "Worker")
	float ResourceExtractionTime = 2.0f;

	// Target info for GoToBuild state, populated externally (e.g., by signal handler)
	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	FVector BuildAreaPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	float BuildAreaArrivalDistance = 300.f;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	FVector BasePosition = FVector::ZeroVector; // Populated by external logic

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	bool BaseAvailable = false;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	float BaseArrivalDistance = 300.f;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	bool BuildingAreaAvailable = false;
	
	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	bool BuildingAvailable = false;
	
	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	float BuildTime = 5.f;
	
	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	FVector ResourcePosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	bool ResourceAvailable = false;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	float ResourceArrivalDistance = 300.f;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	bool UpdateMovement = true;

	UPROPERTY(VisibleAnywhere, Category="Worker|State", Transient)
	bool AutoMining = true;
};

USTRUCT()
struct FUnitStateFragment : public FMassFragment
{
	GENERATED_BODY()
	// Could store target entity handle here if attacking/following
	UPROPERTY()
	FMassEntityHandle TargetEntity;
	
};

struct FMassSightSignalPayload
{
	FMassEntityHandle TargetEntity;
	FMassEntityHandle DetectorEntity;
	FName            SignalName;

	// Constructor when you have both
	FMassSightSignalPayload(FMassEntityHandle InTarget, FMassEntityHandle InSource, FName InSignal)
		: TargetEntity(InTarget)
		, DetectorEntity(InSource)
		, SignalName(InSignal)
	{}
};

struct FMassSignalPayload
{
	FMassEntityHandle TargetEntity;
	FName SignalName; // Use FName for the signal identifier

	// Constructor using FName
	FMassSignalPayload(FMassEntityHandle InEntity, FName InSignalName)
		: TargetEntity(InEntity), SignalName(InSignalName)
	{}
};

struct FActorTransformUpdatePayload
{
	TWeakObjectPtr<AActor> ActorPtr;
	FTransform NewTransform;
	int32 InstanceIndex = INDEX_NONE;
	bool bUseSkeletal = true;

	FActorTransformUpdatePayload(AActor* InActor, const FTransform& InTransform, bool bInUseSkeletal, int32 InInstanceIndex = INDEX_NONE)
		: ActorPtr(InActor), NewTransform(InTransform), InstanceIndex(InInstanceIndex), bUseSkeletal(bInUseSkeletal)
	{}
};
//----------------------------------------------------------------------//
//  AI State Fragment
//----------------------------------------------------------------------//
USTRUCT()
struct FMassAIStateFragment : public FMassFragment
{
    GENERATED_BODY()
	
    /** Timer, der für Aktionen innerhalb des aktuellen Zustands verwendet wird (z.B. Attack-Cooldown, Pause-Dauer, Zeit im Idle-Zustand). Wird bei Zustandswechsel zurückgesetzt. */
    UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
    float StateTimer = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	bool HasAttacked = false;

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	FName PlaceholderSignal = NAME_None;
	
	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	FVector StoredLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	bool SwitchingState = false;

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	float BirthTime = TNumericLimits<float>::Max();

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	float DeathTime = TNumericLimits<float>::Max();
	/** How many overlaps this target has *per team* (any overlap). */

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	bool CanMove = true;

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	bool IsInitialized = true;

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	bool CanAttack = true;
	//UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	//TSet<FMassEntityHandle> LastSeenTargets; 
};


USTRUCT()
struct FMassSightFragment : public FMassFragment
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Transient, Category = RTSUnitTemplate)
	TMap<int32, int32> TeamOverlapsPerTeam;

	/** How many overlaps this target has *per team* from detectors that can see invisibles. */
	UPROPERTY(VisibleAnywhere, Transient, Category = RTSUnitTemplate)
	TMap<int32, int32> DetectorOverlapsPerTeam;
};

//----------------------------------------------------------------------//
//  AI Target Fragment
//----------------------------------------------------------------------//
USTRUCT()
struct FMassAITargetFragment : public FMassFragment
{
    GENERATED_BODY()

    /** Das aktuelle Ziel-Entity. Kann ungültig sein (TargetEntity.IsSet() == false). */
    UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
    FMassEntityHandle TargetEntity;

    /** Die letzte bekannte Position des Ziels. Nützlich, wenn das Ziel aus der Sichtweite gerät. */
    UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
    FVector LastKnownLocation = FVector::ZeroVector;

    /** Gibt an, ob aktuell ein gültiges Ziel verfolgt/angevisiert wird. Wird von einem TargetAcquisitionProcessor gesetzt. */
    UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
    bool bHasValidTarget = false;

	UPROPERTY(VisibleAnywhere, Category = "AI", Transient)
	bool IsFocusedOnTarget = false;
	/** The set of entities this detector currently believes it can see. */
	TSet<FMassEntityHandle> PreviouslySeen;

	/** The set we’ll build fresh each tick. */
	TSet<FMassEntityHandle> CurrentlySeen;
	
	UPROPERTY(VisibleAnywhere, Category = "AI|Ability", Transient)
	FVector AbilityTargetLocation = FVector::ZeroVector;
};

//----------------------------------------------------------------------//
//  Combat Stats / Attributes Fragment
//----------------------------------------------------------------------//
USTRUCT()
struct FMassCombatStatsFragment : public FMassFragment
{
    GENERATED_BODY()

    // --- Kernattribute ---
    UPROPERTY(EditAnywhere, Category = "Stats")
    float Health = 100.f;

    UPROPERTY(EditAnywhere, Category = "Stats")
    float MaxHealth = 100.f;

    UPROPERTY(EditAnywhere, Category = "Stats")
    float AttackRange = 150.f;

    UPROPERTY(EditAnywhere, Category = "Stats")
    float AttackDamage = 10.f;

    /** Angriffe pro Sekunde. Wird genutzt, um Cooldown/Pausendauer zu berechnen. */
    UPROPERTY(EditAnywhere, Category = "Stats")
    float AttackDuration = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Stats")
	float IsAttackedDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Stats")
	float CastTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Stats")
	bool IsInitialized = true;
	
    UPROPERTY(EditAnywhere, Category = "Stats")
    float RunSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "Stats")
	float RotationSpeed = 360.0f;
	
    UPROPERTY(EditAnywhere, Category = "Stats")
    int32 TeamId = 0;

    // --- Zusätzliche Attribute (Beispiele basierend auf deinem Code) ---
    UPROPERTY(EditAnywhere, Category = "Stats")
    float Armor = 0.f;

    UPROPERTY(EditAnywhere, Category = "Stats")
    float MagicResistance = 0.f;

    UPROPERTY(EditAnywhere, Category = "Stats")
    float Shield = 0.f; // Aktueller Schildwert

    UPROPERTY(EditAnywhere, Category = "Stats")
    float MaxShield = 0.f; // Maximaler Schildwert

    /** Sichtweite für die Zielerfassung. */
    UPROPERTY(EditAnywhere, Category = "Stats")
    float SightRadius = 2000.f;

    /** Distanz, ab der ein Ziel verloren wird. */
    UPROPERTY(EditAnywhere, Category = "Stats")
    float LoseSightRadius = 2500.f;

    /** Dauer der Pause zwischen Angriffen (könnte auch aus AttackSpeed berechnet werden). */
    UPROPERTY(EditAnywhere, Category = "Stats")
    float PauseDuration = 0.5f;

     /** Flag, ob die Einheit Projektile verwendet. */
     UPROPERTY(EditAnywhere, Category = "Stats")
     bool bUseProjectile = false;
};

//----------------------------------------------------------------------//
//  Agent Characteristics Fragment
//----------------------------------------------------------------------//
USTRUCT()
struct FMassAgentCharacteristicsFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Characteristics")
    bool bIsFlying = false;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float FlyHeight = 500.f;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float LastGroundLocation = 0.f;
	
    UPROPERTY(EditAnywhere, Category = "Characteristics")
    bool bCanOnlyAttackFlying = true;

    UPROPERTY(EditAnywhere, Category = "Characteristics")
    bool bCanOnlyAttackGround = true;

    UPROPERTY(EditAnywhere, Category = "Characteristics")
    bool bIsInvisible = false;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	bool bCanBeInvisible = false;
	
    UPROPERTY(EditAnywhere, Category = "Characteristics")
    bool bCanDetectInvisible = false;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float DespawnTime = 5.f;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	bool RotatesToMovement = true;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	bool RotatesToEnemy = true;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float RotationSpeed = 15.f;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	FTransform PositionedTransform;

	UPROPERTY(EditAnywhere, Category = "Characteristics")
	float CapsuleHeight = 88;
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Characteristics")
    // bool bIsOnPlattform = false; // Dein Plattform-Flag
};


//----------------------------------------------------------------------//
//  Patrol Fragment
//----------------------------------------------------------------------//
USTRUCT()
struct FMassPatrolFragment : public FMassFragment
{
    GENERATED_BODY()

    /** Index des aktuellen Wegpunkts in einer (hier nicht definierten) Wegpunktliste oder INDEX_NONE, wenn keine Patrouille aktiv/konfiguriert ist. */
    UPROPERTY(VisibleAnywhere, Category = "AI|Patrol", Transient)
    int32 CurrentWaypointIndex = INDEX_NONE;

    /** Die Zielposition des aktuellen Wegpunkts (wird vom Patrol-Logik-Prozessor gesetzt). */
    UPROPERTY(VisibleAnywhere, Category = "AI|Patrol", Transient)
    FVector TargetWaypointLocation = FVector::ZeroVector;

    /** Soll die Patrouille loopen? */
    UPROPERTY(EditAnywhere, Category = "AI|Patrol")
    bool bLoopPatrol = true;

    /** Soll zufällig im Radius um den Wegpunkt patrouilliert werden? (ersetzt PatrolCloseToWaypoint) */
    UPROPERTY(EditAnywhere, Category = "AI|Patrol")
    bool bPatrolRandomAroundWaypoint = false;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol")
	bool bSetUnitsBackToPatrol = false;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol")
	float SetUnitsBackToPatrolTime = 3.f;
    /** Radius für zufällige Patrouille um den Wegpunkt. */
    UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(EditCondition="bPatrolRandomAroundWaypoint"))
    float RandomPatrolRadius = 500.f;

	UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(EditCondition="bPatrolRandomAroundWaypoint"))
	float IdleChance = 70.f;
	
    /** Minimale/Maximale Idle-Zeit bei zufälliger Patrouille. */
    UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(EditCondition="bPatrolRandomAroundWaypoint"))
    float RandomPatrolMinIdleTime = 2.0f;
	
    UPROPERTY(EditAnywhere, Category = "AI|Patrol", meta=(EditCondition="bPatrolRandomAroundWaypoint"))
    float RandomPatrolMaxIdleTime = 5.0f;

	
    // Hier könnte auch eine Referenz auf eine FMassEntityHandle Liste mit Waypoint-Entities stehen
    // oder eine TArray<FVector> mit Positionen, je nachdem wie du Waypoints verwaltest.
};

inline void UpdateMoveTarget(FMassMoveTargetFragment& MoveTarget, const FVector& TargetLocation, float Speed, UWorld* World)
{
	if (!World)
	{
		// Log the error and exit
		//UE_LOG(LogTemp, Error, TEXT("UChaseStateProcessor::UpdateMoveTarget: World is null! Cannot update MoveTarget."));
		return;
	}
    
	// --- Modify the Fragment ---
	MoveTarget.CreateNewAction(EMassMovementAction::Move, *World); // Wichtig: Aktion neu erstellen!
	MoveTarget.Center = TargetLocation;
	MoveTarget.DesiredSpeed.Set(Speed);
	MoveTarget.IntentAtGoal = EMassMovementAction::Stand; // Anhalten, wenn Ziel erreicht (oder was immer gewünscht ist)
	//MoveTarget.SlackRadius = 50.f; // Standard-Akzeptanzradius für Bewegung (ggf. anpassen)

	//UE_LOG(LogTemp, Log, TEXT("MoveTarget.Center: %s"), *MoveTarget.Center.ToString());
	FVector PreNormalizedForward = (TargetLocation - MoveTarget.Center);
	MoveTarget.Forward = PreNormalizedForward.GetSafeNormal();
}


inline void StopMovement(FMassMoveTargetFragment& MoveTarget, UWorld* World)
{
	// Sicherheitscheck für World Pointer
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("StopMovement: World is null!"));
		return;
	}

	// Modifiziere das übergebene Fragment direkt
	MoveTarget.CreateNewAction(EMassMovementAction::Stand, *World); // Wichtig: Aktion neu erstellen!
	MoveTarget.DesiredSpeed.Set(0.f);
	// Andere Felder wie Center, SlackRadius etc. bleiben unverändert, sind aber für Stand egal.
}

inline void SetNewRandomPatrolTarget(FMassPatrolFragment& PatrolFrag, FMassMoveTargetFragment& MoveTarget, FMassAIStateFragment* StateFragPtr, UNavigationSystemV1* NavSys, UWorld* World, float Speed)
{
	FVector BaseWaypointLocation = PatrolFrag.TargetWaypointLocation; // Muss korrekt gesetzt sein!
	if (BaseWaypointLocation == FVector::ZeroVector)
	{
		// Fallback oder Fehlerbehandlung, wenn keine Basisposition bekannt ist
		MoveTarget.CreateNewAction(EMassMovementAction::Stand, *World); // Anhalten
		MoveTarget.DesiredSpeed.Set(0.f);
		return;
	}

	FNavLocation RandomPoint;
	bool bSuccess = false;
	int Attempts = 0;
	constexpr int MaxAttempts = 5;

	while (!bSuccess && Attempts < MaxAttempts)
	{
		// Finde zufälligen Punkt im Radius um den Basis-Wegpunkt
		bSuccess = NavSys->GetRandomReachablePointInRadius(BaseWaypointLocation, PatrolFrag.RandomPatrolRadius, RandomPoint);
		Attempts++;
	}

	if (bSuccess)
	{
		StateFragPtr->StoredLocation = RandomPoint.Location;
		UpdateMoveTarget(MoveTarget, RandomPoint.Location, Speed, World);
	}
	
}

inline bool DoesEntityHaveTag(const FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* TagType)
{
	if (!EntityManager.IsEntityValid(Entity)) // Optional: Check entity validity first
	{
		UE_LOG(LogTemp, Warning, TEXT("No EntityManager FOUND!!!!!!"));
		return false;
	}

	// 1. Get the entity's archetype handle (use Unsafe if you know the entity is valid and built)
	const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntityUnsafe(Entity);
	// Or safer: const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(Entity);

	if (!ArchetypeHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("No ArchetypeHandle FOUND!!!!!!"));
		return false; // Should not happen for a valid, built entity, but good practice
	}

	// 2. Get the composition descriptor for the archetype
	const FMassArchetypeCompositionDescriptor& Composition = EntityManager.GetArchetypeComposition(ArchetypeHandle);

	// 3. Check if the tag is present in the composition's tag bitset
	return Composition.Tags.Contains(*TagType);
}

template<typename FragmentType>
bool DoesEntityHaveFragment(
   const FMassEntityManager& EntityManager,
   FMassEntityHandle         Entity)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return false;
	}

	const FMassArchetypeHandle ArchetypeHandle =
	   EntityManager.GetArchetypeForEntity(Entity); // Note: Consider if GetArchetypeForEntity (safer) is more appropriate if Entity might not be fully initialized.
	if (!ArchetypeHandle.IsValid())
	{
		return false;
	}

	const FMassArchetypeCompositionDescriptor& Composition =
	   EntityManager.GetArchetypeComposition(ArchetypeHandle);

	// Get the UScriptStruct pointer for the FragmentType
	const UScriptStruct* FragmentStruct = FragmentType::StaticStruct();

	// Ensure the struct pointer is valid before dereferencing
	if (FragmentStruct)
	{
		// Dereference the pointer to pass a reference to Contains
		return Composition.Fragments.Contains(*FragmentStruct);
	}

	// If StaticStruct() somehow returned nullptr, the fragment isn't valid or found.
	return false;
}

template<typename FragmentType>
const FragmentType* TryGetFragmentDataPtr(const FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return nullptr;
	}

	const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(Entity);
	if (!ArchetypeHandle.IsValid())
	{
		return nullptr;
	}

	const FMassArchetypeCompositionDescriptor& Composition = EntityManager.GetArchetypeComposition(ArchetypeHandle);
	const UScriptStruct* FragmentStruct = FragmentType::StaticStruct();

	if (!FragmentStruct || !Composition.Fragments.Contains(*FragmentStruct))
	{
		return nullptr;
	}

	return EntityManager.GetFragmentDataPtr<FragmentType>(Entity);
}

inline bool IsFullyValidTarget(FMassEntityManager& EM, FMassEntityHandle H)
{
	return H.IsSet()
		&& EM.IsEntityValid(H)
		&& DoesEntityHaveFragment<FTransformFragment>(EM,H)
		&& DoesEntityHaveFragment<FMassCombatStatsFragment>(EM,H)
		&& DoesEntityHaveFragment<FMassAgentCharacteristicsFragment>(EM,H);
}