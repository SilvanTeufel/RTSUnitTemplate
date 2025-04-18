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
#include "Characters/Unit/UnitBase.h"


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
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly); // Für Effekte/Projektile/XP

    EntityQuery.RegisterWithProcessor(*this);
}

void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Stelle sicher, dass das Signal Subsystem gültig ist
    if (!SignalSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("UAttackStateProcessor: SignalSubsystem is invalid!"));
        return;
    }

    // Set leeren, das speichert, wer in diesem Tick schon angegriffen hat
    EntitiesThatAttackedThisTick.Reset();

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
        const auto ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();
        TArrayView<FMassActorFragment> ActorFragments = Context.GetMutableFragmentView<FMassActorFragment>();
              
        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassVelocityFragment& Velocity = VelocityList[i];
            const FMassActorFragment& ActorFrag = ActorList[i]; // ReadOnly reicht meist
            AActor* AttackerActor = ActorFragments[i].GetMutable(); // Actor holen

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            // 1. Sicherstellen, dass Einheit steht
            Velocity.Value = FVector::ZeroVector;
            // Rotation zum Ziel: Sollte idealerweise ein separater LookAtProcessor machen

            // 2. Ziel verloren oder ungültig? -> Zurück zu Idle/Chase
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                ChunkContext.Defer().RemoveTag<FMassStateAttackTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStateIdleTag>(Entity); // Oder Chase, je nach Logik
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
            if (StateFrag.StateTimer >= DamageApplicationTime && !EntitiesThatAttackedThisTick.Contains(Entity))
            {
                // Prüfen, ob Ziel noch in Reichweite ist
                const float EffectiveAttackRange = Stats.AttackRange + Stats.AgentRadius; // Vereinfacht
                const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    if (Stats.bUseProjectile)
                    {
                        // === KORREKTER AUFRUF HIER ===
                        // Argumente: EntityManager, ChunkContext, Attacker-Entity, Ziel-Entity, Attacker-Actor
                        SpawnProjectileFromActor(EntityManager, ChunkContext, Entity, TargetFrag.TargetEntity, AttackerActor);
                        // =============================
                        
                    }
                    else // Nahkampf
                    {
                        // Schaden senden
                        // bool bIsMagic = Stats.bIsDoingMagicDamage; // Annahme: Flag im StatsFragment
                        bool bIsMagic = false; // Beispiel
                        float Damage = Stats.AttackDamage;
                        SendDamageSignal(ChunkContext, Entity, TargetFrag.TargetEntity, Damage, bIsMagic);

                        // Angreifer-spezifische Dinge über Actor auslösen
                        AUnitBase* AttackerUnitBase = Cast<AUnitBase>(AttackerActor);
                        if (AttackerUnitBase)
                        {
                            // Erfahrung erhöhen (Beispiel)
                            // AttackerUnitBase->LevelData.Experience++; // Vorsicht bei direktem Zugriff! Besser über Event/Signal

                            // Effekte/Sounds auslösen
                            AttackerUnitBase->ServerMeeleImpactEvent(); // Beispiel für RPC-Aufruf
                            // AttackerUnitBase->FireEffects(AttackerUnitBase->MeleeImpactVFX, ...); // Oder direkter Effekt

                            // Widget Update (wenn nötig) - Besser Event-basiert
                            // if (AttackerUnitBase->HealthWidgetComp) { ... }
                        }

                        // Ziel-spezifische Reaktionen auslösen (via Signal)
                        if (TargetFrag.TargetEntity.IsSet())
                        {
                             // SignalSubsystem->SignalEntity(TargetFrag.TargetEntity, UE::Mass::Signals::ActivateAbility, DefensiveAbilityPayload);
                             // SignalSubsystem->SignalEntity(TargetFrag.TargetEntity, UE::Mass::Signals::TriggerEffect, ImpactEffectPayload);
                             // SignalSubsystem->SignalEntity(TargetFrag.TargetEntity, UE::Mass::Signals::ForceState, IsAttackedStatePayload);
                             // UE_LOG(LogTemp, Log, TEXT("Signaling target reactions for Entity [%d]"), TargetFrag.TargetEntity.Index);
                        }
                    }
                    // Markieren, dass diese Entität in diesem Tick angegriffen hat
                    EntitiesThatAttackedThisTick.Add(Entity);
                }
                else // Ziel außer Reichweite gekommen während der Attack-Animation
                {
                    ChunkContext.Defer().RemoveTag<FMassStateAttackTag>(Entity);
                    ChunkContext.Defer().AddTag<FMassStateChaseTag>(Entity);
                    AUnitBase* AttackerUnitBase = Cast<AUnitBase>(AttackerActor);
                    AttackerUnitBase->SetUnitState(UnitData::Chase);
                    StateFrag.StateTimer = 0.f;
                    continue;
                }
            } // Ende Schadens-/Projektil-Anwendung

            // 5. Prüfen, ob die Angriffs-Aktion (ohne Pause) abgeschlossen ist
            if (StateFrag.StateTimer >= AttackDuration)
            {
                // Angriff beendet -> Wechsle zu Pause
                ChunkContext.Defer().RemoveTag<FMassStateAttackTag>(Entity);
                ChunkContext.Defer().AddTag<FMassStatePauseTag>(Entity);
                AUnitBase* AttackerUnitBase = Cast<AUnitBase>(AttackerActor);
                AttackerUnitBase->SetUnitState(UnitData::Pause);
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