#include "Mass/Signals/UnitClientGASSyncProcessor.h"
#include "Characters/Unit/UnitBase.h"
#include "GAS/AttributeSetBase.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"

UUnitClientGASSyncProcessor::UUnitClientGASSyncProcessor()
{
    bAutoRegisterWithProcessingPhases = true;
    ExecutionFlags = (int32)EProcessorExecutionFlags::Client;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bRequiresGameThreadExecution = true;
}

void UUnitClientGASSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
}

void UUnitClientGASSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    EntityQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context)
    {
        const int32 NumEntities = Context.GetNumEntities();
        auto ActorList = Context.GetMutableFragmentView<FMassActorFragment>();
        auto CombatList = Context.GetMutableFragmentView<FMassCombatStatsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            if (CombatList[i].bIsInitializedOnClient) continue;

            if (AUnitBase* Unit = Cast<AUnitBase>(ActorList[i].GetMutable()))
            {
                if (UAttributeSetBase* AS = const_cast<UAttributeSetBase*>(Unit->Attributes))
                {
                    // Synchronisiere statische Attribute aus dem Actor (der sie vom CDO oder Blueprint hat)
                    // Wir nutzen die Werte, die im Actor-Template definiert sind.
                    
                    AS->SetAttributeArmor(Unit->Armor);
                    AS->SetAttributeAttackDamage(Unit->AttackDamage);
                    AS->SetAttributeRange(Unit->AttackRange);
                    AS->SetAttributeRunSpeed(Unit->RunSpeed);
                    AS->SetAttributeMagicResistance(Unit->MagicResistance);
                    
                    // Auch Basis-Werte setzen falls nötig
                    AS->SetAttributeBaseHealth(Unit->MaxHealth);
                    AS->SetAttributeBaseAttackDamage(Unit->AttackDamage);
                    AS->SetAttributeBaseRunSpeed(Unit->RunSpeed);
                    
                    CombatList[i].bIsInitializedOnClient = true;
                    
                    // Wir setzen auch die Werte im Fragment, damit sie lokal konsistent sind
                    CombatList[i].Armor = Unit->Armor;
                    CombatList[i].AttackDamage = Unit->AttackDamage;
                    CombatList[i].AttackRange = Unit->AttackRange;
                    CombatList[i].RunSpeed = Unit->RunSpeed;
                    CombatList[i].MagicResistance = Unit->MagicResistance;
                }
            }
        }
    });
}
