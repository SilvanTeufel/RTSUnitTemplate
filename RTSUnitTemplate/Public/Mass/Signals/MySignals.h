// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h" // Or Engine minimal header
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "MySignals.generated.h"

/** Signal raised when a unit controlled by UnitMovementProcessor reaches its destination */
USTRUCT(BlueprintType) // Add BlueprintType if you might want to use it in BP/StateTree UI
struct FUnitReachedDestinationSignal // No FMassSignal inheritance needed
{
	GENERATED_BODY()

	// You can still add payload data if needed
	//UPROPERTY()
	//FVector Location;
};

namespace UnitSignals
{
	//const FName ReachedDestination = FName("UnitReachedDestination");
	const FName Idle(TEXT("Idle"));
	const FName Chase(TEXT("Chase"));
	const FName Attack(TEXT("Attack"));
	const FName Dead(TEXT("Dead"));
	const FName PatrolIdle(TEXT("PatrolIdle"));
	const FName PatrolRandom(TEXT("PatrolRandom"));
	const FName Pause(TEXT("Pause"));
	const FName Run(TEXT("Run"));
	const FName Casting(TEXT("Cast"));
	const FName IsAttacked(TEXT("IsAttacked"));
	// Define other signal names here if needed
	const FName MeleeAttack(TEXT("MeeleAttack"));
	const FName RangedAttack(TEXT("RangedAttack"));
	const FName UnitInDetectionRange(TEXT("UnitInDetectionRange"));
	const FName UnitSpawned(TEXT("UnitSpawned"));
	const FName SetUnitToChase(TEXT("SetUnitToChase"));
	const FName StartDead(TEXT("StartDead"));
	const FName EndDead(TEXT("EndDead"));
	const FName SyncUnitBase(TEXT("SyncAttributes"));
	const FName RemoveDeadUnit(TEXT("RemoveDeadUnit"));
	const FName PISwitcher(TEXT("PISwitcher"));

	const FName SetUnitStatePlaceholder(TEXT("IdlePatrolSwitcher"));
	const FName ClientSetToPlaceholder(TEXT("ClientSetToPlaceholder"));
	
	const FName SyncCastTime(TEXT("SyncCastTime"));
	const FName EndCast(TEXT("EndCast"));
	
	// Repair-specific signals
	const FName GoToRepair(TEXT("GoToRepair"));
	const FName Repair(TEXT("Repair"));
	const FName SyncRepairTime(TEXT("SyncRepairTime"));
	
	// Worker Signals
	const FName GoToBase(TEXT("GoToBase"));
	const FName GoToBuild(TEXT("GoToBuild"));
	const FName Build(TEXT("Build"));
	const FName GoToResourceExtraction(TEXT("GoToResourceExtraction"));
	const FName ResourceExtraction(TEXT("ResourceExtraction"));
	const FName GetResource(TEXT("GetResource"));
	const FName ReachedBase(TEXT("ReachedBase"));
	const FName GetClosestBase(TEXT("StartBuildAction"));
	const FName SpawnBuildingRequest(TEXT("SpawnBuildingRequest"));

	const FName UpdateWorkerMovement(TEXT("UpdateWorkerMovement"));
	// FOG OF WAR
	const FName UnitEnterSight(TEXT("UnitEnterSight"));
	const FName UnitExitSight(TEXT("UnitExitSight"));
	const FName UpdateFogMask(TEXT("UpdateFogMask"));
	const FName UpdateSelectionCircle(TEXT("UpdateSelectionCircle"));

	const FName UseRangedAbilitys(TEXT("UseRangedAbilitys"));

	const FName InitUnit(TEXT("InitUnit"));
	// Follow feature signals removed (replaced by FriendlyTargetEntity-driven processors)
	// Client-side navigation mirror signals
}

USTRUCT()
struct FUnitPresenceSignal
{
	GENERATED_BODY()

	// Using UPROPERTY can help with debugging/reflection but adds minor overhead.
	// Remove UPROPERTY() if not needed.
	
	UPROPERTY()
	FMassEntityHandle SignalerEntity; // The entity broadcasting this signal

	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector; // Use NetQuantize for potential network relevance

	UPROPERTY()
	int32 TeamId = -1;

	UPROPERTY()
	bool bIsInvisible = false;

	UPROPERTY()
	bool bIsFlying = false;

	// Add other relevant data that detectors might need without accessing fragments directly
	// e.g., maybe a faction identifier, or specific target type flags
};
// Add other custom signal structs here if needed

namespace UE::Mass::Debug // Optional: Use a namespace for organization
{
    /**
     * @brief Logs the tags currently present on a Mass Entity.
     * @param Entity The entity handle to inspect.
     * @param EntityManager A const reference to the entity manager for querying.
     * @param LogOwner Optional UObject context for the log category (can be nullptr).
     */
    static void LogEntityTags(const FMassEntityHandle& Entity, const FMassEntityManager& EntityManager, const UObject* LogOwner = nullptr)
    {
    
        FString PresentTags = TEXT("Tags:");
        bool bFoundTags = false;

        // --- Get Archetype and Composition ---
        const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntityUnsafe(Entity);
        if (!ArchetypeHandle.IsValid())
        {
            // Use default LogTemp or context-specific log category if owner provided
            UE_LOG(LogTemp, Warning, TEXT("Entity [%d:%d] has invalid archetype handle! Cannot log tags."), Entity.Index, Entity.SerialNumber);
            return;
        }

    
    	
        const FMassArchetypeCompositionDescriptor& Composition = EntityManager.GetArchetypeComposition(ArchetypeHandle);

        // --- Check all relevant tags using the Composition's Tag Bitset ---
        // Primary States
        const auto& Tags = Composition.GetTags();
        if (Tags.Contains<FMassStateIdleTag>())      { PresentTags += TEXT(" Idle"); bFoundTags = true; }
        if (Tags.Contains<FMassStateChaseTag>())     { PresentTags += TEXT(" Chase"); bFoundTags = true; }
        if (Tags.Contains<FMassStateAttackTag>())    { PresentTags += TEXT(" Attack"); bFoundTags = true; }
        if (Tags.Contains<FMassStatePauseTag>())     { PresentTags += TEXT(" Pause"); bFoundTags = true; }
        if (Tags.Contains<FMassStateDeadTag>())      { PresentTags += TEXT(" Dead"); bFoundTags = true; }
        if (Tags.Contains<FMassStateRunTag>())       { PresentTags += TEXT(" Run"); bFoundTags = true; }
        if (Tags.Contains<FMassStateDetectTag>())    { PresentTags += TEXT(" Detect"); bFoundTags = true; }
        if (Tags.Contains<FMassStateCastingTag>())   { PresentTags += TEXT(" Casting"); bFoundTags = true; }

        // Patrol States
        if (Tags.Contains<FMassStatePatrolTag>())       { PresentTags += TEXT(" Patrol"); bFoundTags = true; }
        if (Tags.Contains<FMassStatePatrolRandomTag>()) { PresentTags += TEXT(" PatrolRandom"); bFoundTags = true; }
        if (Tags.Contains<FMassStatePatrolIdleTag>())   { PresentTags += TEXT(" PatrolIdle"); bFoundTags = true; }

        // Other States
        if (Tags.Contains<FMassStateEvasionTag>())    { PresentTags += TEXT(" Evasion"); bFoundTags = true; }
        if (Tags.Contains<FMassStateRootedTag>())     { PresentTags += TEXT(" Rooted"); bFoundTags = true; }
        if (Tags.Contains<FMassStateIsAttackedTag>()) { PresentTags += TEXT(" IsAttacked"); bFoundTags = true; }

        // Helper Tags
        if (Tags.Contains<FMassHasTargetTag>())         { PresentTags += TEXT(" HasTarget"); bFoundTags = true; }
        if (Tags.Contains<FMassReachedDestinationTag>()){ PresentTags += TEXT(" ReachedDestination"); bFoundTags = true; }

   		if (Tags.Contains<FMassStateGoToBaseTag>())                { PresentTags += TEXT(" GoToBase"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateGoToBuildTag>())               { PresentTags += TEXT(" GoToBuild"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateGoToResourceExtractionTag>())  { PresentTags += TEXT(" GoToResourceExtraction"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateBuildTag>())                   { PresentTags += TEXT(" Build"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateResourceExtractionTag>())      { PresentTags += TEXT(" ResourceExtraction"); bFoundTags = true; }

   		// Repair States
   		if (Tags.Contains<FMassStateGoToRepairTag>())              { PresentTags += TEXT(" GoToRepair"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateRepairTag>())                  { PresentTags += TEXT(" Repair"); bFoundTags = true; }

   		// Utility/Helper tags
   		if (Tags.Contains<FMassStateStopMovementTag>())            { PresentTags += TEXT(" StopMovement"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateDisableObstacleTag>())         { PresentTags += TEXT(" DisableObstacle"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateDisableNavManipulationTag>())  { PresentTags += TEXT(" DisableNavManipulation"); bFoundTags = true; }
   		if (Tags.Contains<FMassStateChargingTag>())                { PresentTags += TEXT(" Charging"); bFoundTags = true; }
   		if (Tags.Contains<FMassStopGameplayEffectTag>())           { PresentTags += TEXT(" StopGameplayEffect"); bFoundTags = true; }
   		if (Tags.Contains<FMassStopUnitDetectionTag>())            { PresentTags += TEXT(" StopUnitDetection"); bFoundTags = true; }
   		if (Tags.Contains<FMassDisableAvoidanceTag>())             { PresentTags += TEXT(" DisableAvoidance"); bFoundTags = true; }

    	//if (Composition.Tags.Contains<FNeedsActorBindingInitTag>()){ PresentTags += TEXT(" ActorBindingInit"); bFoundTags = true; }
    	
    	// --- Add checks for any other custom tags you use ---
        // if (Composition.Tags.Contains<FMyCustomTag>()) { PresentTags += TEXT(" MyCustom"); bFoundTags = true; }

        if (!bFoundTags) { PresentTags += TEXT(" [None Found]"); }

        // --- Log the result ---
        // Use a specific log category if desired, otherwise LogTemp is fine for debugging.
        // Using LogOwner allows associating the log with a specific processor/object if needed.
        UE_LOG(LogTemp, Log, TEXT("Entity [%d:%d] %s"), // Using LogMass category example
             Entity.Index, Entity.SerialNumber,
             *PresentTags);
    	
        // Alternatively, stick to LogTemp if preferred:
        // UE_LOG(LogTemp, Log, TEXT("Entity [%d:%d] Archetype [%s] %s"), ... );
    }

} // End namespace UE::Mass::Debug (or anonymous namespace if preferred)