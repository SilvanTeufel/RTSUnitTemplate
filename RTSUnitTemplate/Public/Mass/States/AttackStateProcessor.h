// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
// === FÜGE DIESEN INCLUDE HINZU ===
#include "MassSignalTypes.h" // Enthält FMassSignalPayloadBase
// ================================
#include "Core/UnitData.h" // Dein Enum etc.
#include "MassEntityQuery.h"
#include "AttackStateProcessor.generated.h"

// Forward Decls für Fragmente, Tags und Systeme
struct FMassExecutionContext;
struct FMassStateAttackTag;
struct FMassAIStateFragment;
struct FMassAITargetFragment;
struct FMassCombatStatsFragment;
struct FMassAgentCharacteristicsFragment;
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
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
    float ExecutionInterval = 0.1f;

    /**
     * Hysterese fuer den Wechsel Angriff -> Chase/Run.
     *
     * Der Eintritt in den Angriff passiert bei Dist <= AttackRange, der Austritt lief bis
     * dahin bei exakt derselben Schwelle. Eine Einheit direkt an der Reichweitengrenze
     * kippte deshalb jeden Tick zwischen Angriff und Chase hin und her - sichtbar als
     * Zucken in der Animation, obwohl der Gegner unmittelbar daneben steht.
     * Der Angriff wird jetzt erst bei AttackRange * diesem Faktor abgebrochen.
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate, meta = (ClampMin = "1.0"))
    float AttackRangeHysteresis = 1.15f;

    /**
     * Laesst einen bereits begonnenen Angriff zu Ende laufen, wenn das Ziel dazwischen
     * stirbt, statt sofort abzubrechen.
     *
     * Ohne das kam eine Einheit mit langer AttackDuration (Siege-Kanone) im Getuemmel nie
     * zum Schuss: Ziel stirbt waehrend des Zielens -> Abbruch -> Zustandswechsel setzt
     * StateTimer auf 0 -> naechstes Ziel -> von vorn. Der Schuss geht jetzt auf die letzte
     * bekannte Position; danach greift der normale Pause-Uebergang.
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
    bool bFinishAttackOnTargetLoss = true;

    static float GetCombinedRadii(const FMassAgentCharacteristicsFragment& AttackerChar, const FTransform& AttackerTransform,
                                  const FMassAgentCharacteristicsFragment* TargetChar, const FTransform* TargetTransform,
                                  const FVector& TargetLocation);
	
    void ClientExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor);
    void ServerExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassAIStateFragment& StateFrag, const FMassAITargetFragment& TargetFrag, const FMassCombatStatsFragment& Stats, const FMassEntityHandle Entity, const int32 EntityIdx, AActor* Actor);

private:
    FMassEntityQuery EntityQuery;

    float TimeSinceLastRun = 0.0f;
    
    // Cached Subsystem Pointer
    UPROPERTY(Transient)
    TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

    float FollowAcceptanceMultiplier = 6.f;
};