#include "Mass/States/AttackStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h" // Für Schadens-Signal

// Fragmente und Tags
#include "MassActorSubsystem.h"   
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassActorSubsystem.h"  
// Für Actor-Cast und Projektil-Spawn (Beispiel)
#include "MassArchetypeTypes.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"

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
        if (Composition.Tags.Contains<FMassStateIdleTag>())      { PresentTags += TEXT(" Idle"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateChaseTag>())     { PresentTags += TEXT(" Chase"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateAttackTag>())    { PresentTags += TEXT(" Attack"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStatePauseTag>())     { PresentTags += TEXT(" Pause"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateDeadTag>())      { PresentTags += TEXT(" Dead"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateRunTag>())       { PresentTags += TEXT(" Run"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateDetectTag>())    { PresentTags += TEXT(" Detect"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateCastingTag>())   { PresentTags += TEXT(" Casting"); bFoundTags = true; }

        // Patrol States
        if (Composition.Tags.Contains<FMassStatePatrolTag>())       { PresentTags += TEXT(" Patrol"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStatePatrolRandomTag>()) { PresentTags += TEXT(" PatrolRandom"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStatePatrolIdleTag>())   { PresentTags += TEXT(" PatrolIdle"); bFoundTags = true; }

        // Other States
        if (Composition.Tags.Contains<FMassStateEvasionTag>())    { PresentTags += TEXT(" Evasion"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateRootedTag>())     { PresentTags += TEXT(" Rooted"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassStateIsAttackedTag>()) { PresentTags += TEXT(" IsAttacked"); bFoundTags = true; }

        // Helper Tags
        if (Composition.Tags.Contains<FMassHasTargetTag>())         { PresentTags += TEXT(" HasTarget"); bFoundTags = true; }
        if (Composition.Tags.Contains<FMassReachedDestinationTag>()){ PresentTags += TEXT(" ReachedDestination"); bFoundTags = true; }

        // --- Add checks for any other custom tags you use ---
        // if (Composition.Tags.Contains<FMyCustomTag>()) { PresentTags += TEXT(" MyCustom"); bFoundTags = true; }


        if (!bFoundTags) { PresentTags += TEXT(" [None Found]"); }

        // --- Log the result ---
        // Use a specific log category if desired, otherwise LogTemp is fine for debugging.
        // Using LogOwner allows associating the log with a specific processor/object if needed.
        UE_LOG(LogMass, Log, TEXT("Entity [%d:%d] %s"), // Using LogMass category example
             Entity.Index, Entity.SerialNumber,
             *PresentTags);

        // Alternatively, stick to LogTemp if preferred:
        // UE_LOG(LogTemp, Log, TEXT("Entity [%d:%d] Archetype [%s] %s"), ... );
    }

} // End namespace UE::Mass::Debug (or anonymous namespace if preferred)

// Signal Definitionen (Beispiel - diese müssen irgendwo global definiert werden)
namespace UE::Mass::Signals
{
    const FName TakeDamage = FName(TEXT("TakeDamage"));
    // const FName TriggerEffect = FName(TEXT("TriggerEffect"));
    // const FName ActivateAbility = FName(TEXT("ActivateAbility"));
    // const FName ForceState = FName(TEXT("ForceState"));
}


UAttackStateProcessor::UAttackStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UAttackStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


void UAttackStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
  
    EntityQuery.RegisterWithProcessor(*this);
}

void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UE_LOG(LogTemp, Log, TEXT("UAttackStateProcessor::Execute!"));
    UWorld* World = Context.GetWorld(); // World für MoveTarget holen

    // Stelle sicher, dass das Signal Subsystem gültig ist
    if (!SignalSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("UAttackStateProcessor: SignalSubsystem is invalid!"));
        return;
    }

    // Set leeren, das speichert, wer in diesem Tick schon angegriffen hat
   //EntitiesThatAttackedThisTick.Reset();

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {


            
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
 
        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassVelocityFragment& Velocity = VelocityList[i];
        

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);


            UE::Mass::Debug::LogEntityTags(Entity, EntityManager, this);

            // 1. Sicherstellen, dass Einheit steht
            Velocity.Value = FVector::ZeroVector;
            // Rotation zum Ziel: Sollte idealerweise ein separater LookAtProcessor machen

            // 2. Ziel verloren oder ungültig? -> Zurück zu Idle/Chase
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                UE_LOG(LogTemp, Log, TEXT("TARGET NOT VALID ANYMORE!!!!!!!"));
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (!SignalSubsystem) continue;
                    
                SignalSubsystem->SignalEntity(UnitSignals::Chase, Entity);
                StateFrag.StateTimer = 0.f;
                continue;
            }

            // 3. Timer für Angriffszyklus prüfen
            StateFrag.StateTimer += DeltaTime;

            // Gesamtdauer des Angriffs (ohne Pause)
            const float AttackDuration = (Stats.AttackSpeed > KINDA_SMALL_NUMBER) ? (1.0f / Stats.AttackSpeed) : 1.0f;
            // Zeitpunkt innerhalb der AttackDuration, an dem der "Impact" stattfindet
            const float DamageApplicationTime = AttackDuration * DamageApplicationTimeFactor;

            // 4. Prüfen, ob der "Impact"-Zeitpunkt erreicht ist und wir noch nicht angegriffen haben
            if (StateFrag.StateTimer >= DamageApplicationTime) //&& !EntitiesThatAttackedThisTick.Contains(Entity)
            {
                // Prüfen, ob Ziel noch in Reichweite ist
                const float EffectiveAttackRange = Stats.AttackRange; // + Stats.AgentRadius; // Vereinfacht
                const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    if (Stats.bUseProjectile && !StateFrag.HasAttacked)
                    {
                        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                        if (!SignalSubsystem) continue;
                    
                        SignalSubsystem->SignalEntity(UnitSignals::RangedAttack, Entity);
                        StateFrag.HasAttacked = true;
                    }
                    else if (!StateFrag.HasAttacked)// Nahkampf
                    {
                        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                        if (!SignalSubsystem) continue;
                        
                        SignalSubsystem->SignalEntity(UnitSignals::MeleeAttack, Entity);
                        StateFrag.HasAttacked = true;
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Log, TEXT("Attack TO CHASE!!!!!!!"));
                    UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                      if (!SignalSubsystem)
                      {
                           continue; // Handle missing subsystem
                      }
                      SignalSubsystem->SignalEntity(
                      UnitSignals::Chase,
                      Entity);
                    StateFrag.StateTimer = 0.f;
                    continue;
                }
            } 

            // 5. Prüfen, ob die Angriffs-Aktion (ohne Pause) abgeschlossen ist
            if (StateFrag.StateTimer >= AttackDuration)
            {
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                   if (!SignalSubsystem)
                   {
                        continue; // Handle missing subsystem
                   }
                   SignalSubsystem->SignalEntity(
                   UnitSignals::Pause,
                   Entity);
                // Angriff beendet -> Wechsle zu Pause
                StateFrag.HasAttacked = false;
                StateFrag.StateTimer = 0.0f; // Timer für Pause zurücksetzen
                continue;
            }

            // 6. Im Attack-State bleiben
        }
    });
}


// --- Hilfsfunktionen ---

void UAttackStateProcessor::SendDamageSignal(FMassExecutionContext& Context, FMassEntityHandle AttackerEntity, FMassEntityHandle TargetEntity, float DamageAmount, bool bIsMagicDamage)
{
    if (SignalSubsystem && TargetEntity.IsSet() && DamageAmount > 0)
    {
        // Erstelle Payload mit Details
        FMassDamageSignalPayload Payload;
        Payload.DamageAmount = DamageAmount;
       /// Payload.bIsMagicDamage = bIsMagicDamage;
        //Payload.Instigator = AttackerEntity;

        // Sende Signal mit Payload an das Ziel-Entity
        // === KORREKTUR HIER: Argumente in richtiger Reihenfolge ===
        SignalSubsystem->SignalEntity(UE::Mass::Signals::TakeDamage, TargetEntity);
        // Reihenfolge: SignalName, Entity, Payload
         UE_LOG(LogTemp, Log, TEXT("Entity [%d] signaling damage [%.1f] to Entity [%d]"), AttackerEntity.Index, DamageAmount, TargetEntity.Index);
    }
}

void UAttackStateProcessor::SpawnProjectileFromActor(
    FMassEntityManager& EntityManager, // <--- EntityManager als Parameter
    FMassExecutionContext& Context,
    FMassEntityHandle AttackerEntity,
    FMassEntityHandle TargetEntity,
    AActor* AttackerActor) // AttackerActor kann non-const sein, wenn UnitBase->SpawnProjectile ihn nicht ändert
{
    AUnitBase* UnitBase = Cast<AUnitBase>(AttackerActor);
    if (UnitBase && TargetEntity.IsSet())
    {
        // Hole Ziel-Actor Fragment über den übergebenen EntityManager
        const FMassActorFragment* TargetActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(TargetEntity);

        if (TargetActorFrag && TargetActorFrag->IsValid()) // Prüfe auch IsValid()
        {
            // === KORREKTUR HIER: TargetActor als const deklarieren ===
            const AActor* TargetActor = TargetActorFrag->Get(); // Get() von const Fragment* liefert const AActor*

            // Cast zu AUnitBase* ist technisch ein const_cast, aber oft notwendig/akzeptiert in UE,
            // WENN SpawnProjectile den TargetUnitBase NICHT modifiziert.
            // Wenn SpawnProjectile einen const AUnitBase* akzeptiert, wäre das sicherer.
            AUnitBase* TargetUnitBase = const_cast<AUnitBase*>(Cast<AUnitBase>(TargetActor)); // const explizit entfernen!
            if(TargetUnitBase)
            {
                UnitBase->SpawnProjectile(TargetUnitBase, UnitBase); // Funktioniert jetzt, aber umgeht const-Sicherheit
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Entity [%d] target actor for projectile [%s] was not AUnitBase!"), AttackerEntity.Index, *GetNameSafe(TargetActor));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Entity [%d] target entity [%d] for projectile has no valid ActorFragment!"), AttackerEntity.Index, TargetEntity.Index);
            // Fallback: Position verwenden?
            // const FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
            // if(TargetTransformFrag) { UnitBase->SpawnProjectileAtLocation(TargetTransformFrag->GetTransform().GetLocation(), UnitBase); }
        }
    }
    else if (!UnitBase)
    {
        UE_LOG(LogTemp, Warning, TEXT("Entity [%d] tried to spawn projectile but AttackerActor [%s] was not AUnitBase!"), AttackerEntity.Index, *GetNameSafe(AttackerActor));
    }
}