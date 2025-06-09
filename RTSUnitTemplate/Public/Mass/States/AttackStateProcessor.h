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
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
    float ExecutionInterval = 0.1f;
	
private:
    FMassEntityQuery EntityQuery;

    float TimeSinceLastRun = 0.0f;
    
    // Cached Subsystem Pointer
    UPROPERTY(Transient)
    TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
};