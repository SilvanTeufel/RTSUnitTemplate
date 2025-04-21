#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
// === FÜGE DIESEN INCLUDE HINZU ===
#include "MassSignalTypes.h" // Enthält FMassSignalPayloadBase
// ================================
#include "Core/UnitData.h" // Dein Enum etc.
#include "AttackStateProcessor.generated.h"

// Forward Decls für Fragmente, Tags und Systeme
struct FMassExecutionContext;
struct FMassStateAttackTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassVelocityFragment;
struct FMassActorFragment;
struct FTransformFragment;
struct FMassStatePauseTag;
struct FMassStateChaseTag;
struct FMassStateIdleTag; // Falls Ziel verloren geht
class UMassSignalSubsystem; // Für Schadens-Signale

// Beispiel für eine Signal-Payload Struktur (optional, aber gut für klare Datenübergabe)
USTRUCT()
struct FMassDamageSignalPayload
{
    GENERATED_BODY()

    UPROPERTY()
    float DamageAmount = 0.f;

    UPROPERTY()
    bool bIsMagicDamage = false; // Oder DamageType Enum

    UPROPERTY()
    FMassEntityHandle Instigator; // Wer hat den Schaden verursacht?
};


UCLASS()
class RTSUNITTEMPLATE_API UAttackStateProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UAttackStateProcessor();

protected:
    virtual void ConfigureQueries() override;
    virtual void Initialize(UObject& Owner) override; // Für SignalSubsystem Cache
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery;

    // Hilfsfunktion für Schaden via Signal
    void SendDamageSignal(FMassExecutionContext& Context, FMassEntityHandle AttackerEntity, FMassEntityHandle TargetEntity, float DamageAmount, bool bIsMagicDamage);

    // Hilfsfunktion für Projektil via ActorFragment
    void SpawnProjectileFromActor(FMassEntityManager& EntityManager, // <--- EntityManager als Parameter
    FMassExecutionContext& Context,
    FMassEntityHandle AttackerEntity,
    FMassEntityHandle TargetEntity,
    AActor* AttackerActo);

    // Verhindert mehrfaches Auslösen des Schadens/Projektils pro Attack-State pro Tick
    //TSet<FMassEntityHandle> EntitiesThatAttackedThisTick;

    // Gehört eigentlich in Config/Stats-Fragment
    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float DamageApplicationTimeFactor = 0.5f; // Bei 50% der Angriffszeit Schaden anwenden/Projektil spawnen

    // Cached Subsystem Pointer
    UPROPERTY(Transient)
    TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};