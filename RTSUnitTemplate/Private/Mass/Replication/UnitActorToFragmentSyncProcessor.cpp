// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Replication/UnitActorToFragmentSyncProcessor.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Actors/EffectArea.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/MassUnitVisualFragments.h"
#include "MassEntitySubsystem.h"
#include "GAS/AttributeSetBase.h"
#include "Mass/MassActorBindingComponent.h"


UUnitActorToFragmentSyncProcessor::UUnitActorToFragmentSyncProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All; // Allow Client and Server execution
	bRequiresGameThreadExecution = true; // we touch AActors
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UUnitActorToFragmentSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassVisualEffectFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitActorToFragmentSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [&, this](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		const TArrayView<FMassCombatStatsFragment> CombatStatsList = ChunkContext.GetMutableFragmentView<FMassCombatStatsFragment>();
		const TArrayView<FMassAgentCharacteristicsFragment> CharacteristicsList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
		const TArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
		const TArrayView<FMassVisibilityFragment> VisibilityList = ChunkContext.GetMutableFragmentView<FMassVisibilityFragment>();
		const TArrayView<FMassVisualEffectFragment> VisualEffectList = ChunkContext.GetMutableFragmentView<FMassVisualEffectFragment>();
		const TArrayView<FEffectAreaImpactFragment> ImpactList = ChunkContext.GetMutableFragmentView<FEffectAreaImpactFragment>();

		for (int32 EntityIndex = 0; EntityIndex < ChunkContext.GetNumEntities(); ++EntityIndex)
		{
			AActor* Actor = ActorList[EntityIndex].GetMutable();
			if (!Actor) continue;

			if (const AUnitBase* Unit = Cast<AUnitBase>(Actor))
			{
				// Sync Combat Stats
				if (Unit->Attributes)
				{
					CombatStatsList[EntityIndex].Health = Unit->Attributes->GetHealth();
					CombatStatsList[EntityIndex].Shield = Unit->Attributes->GetShield();
					CombatStatsList[EntityIndex].MaxHealth = Unit->Attributes->GetMaxHealth();
					CombatStatsList[EntityIndex].MaxShield = Unit->Attributes->GetMaxShield();
					CombatStatsList[EntityIndex].TeamId = Unit->TeamId;

					CombatStatsList[EntityIndex].AttackDamage = Unit->Attributes->GetAttackDamage();
					CombatStatsList[EntityIndex].AttackRange = Unit->Attributes->GetRange();
					CombatStatsList[EntityIndex].RunSpeed = Unit->Attributes->GetRunSpeed();
					CombatStatsList[EntityIndex].Armor = Unit->Attributes->GetArmor();
					CombatStatsList[EntityIndex].MagicResistance = Unit->Attributes->GetMagicResistance();
				}

				CombatStatsList[EntityIndex].PauseDuration = Unit->PauseDuration;
				CombatStatsList[EntityIndex].AttackDuration = Unit->AttackDuration;
				CombatStatsList[EntityIndex].bUseProjectile = Unit->UseProjectile;
				CombatStatsList[EntityIndex].CastTime = Unit->CastTime;
				CombatStatsList[EntityIndex].IsInitialized = Unit->IsInitialized;

				if (Unit->MassActorBindingComponent)
				{
					if (!AIStateList[EntityIndex].bHasExtendedLoseSight)
					{
						CombatStatsList[EntityIndex].SightRadius = Unit->MassActorBindingComponent->SightRadius;
						CombatStatsList[EntityIndex].LoseSightRadius = Unit->MassActorBindingComponent->LoseSightRadius;
					}
				}

				// Sync Characteristics
				CharacteristicsList[EntityIndex].FlyHeight = Unit->FlyHeight;
				CharacteristicsList[EntityIndex].bIsFlying = Unit->IsFlying;
				CharacteristicsList[EntityIndex].bCanOnlyAttackFlying = Unit->CanOnlyAttackFlying;
				CharacteristicsList[EntityIndex].bCanOnlyAttackGround = Unit->CanOnlyAttackGround;
				CharacteristicsList[EntityIndex].bCanDetectInvisible = Unit->CanDetectInvisible;
				const FVector CurrentActorScale = Unit->GetActorScale3D();
				// Prüfe auf Änderung, um unnötige Dirty-Flags zu vermeiden
				
				if (!CharacteristicsList[EntityIndex].Scale.Equals(CurrentActorScale, 0.001f))
				{
					CharacteristicsList[EntityIndex].Scale = CurrentActorScale;
					CharacteristicsList[EntityIndex].PositionedTransform.SetScale3D(CurrentActorScale);
					CharacteristicsList[EntityIndex].bTransformDirty = true;
				}

				
				// Sync AI State
				AIStateList[EntityIndex].CanAttack = Unit->CanAttack;
				AIStateList[EntityIndex].CanMove = Unit->CanMove;
				AIStateList[EntityIndex].HoldPosition = Unit->bHoldPosition;

				// Sync Visibility Stats
				if (VisibilityList.Num() > 0 && Unit->Attributes)
				{
					VisibilityList[EntityIndex].LastHealth = Unit->Attributes->GetHealth();
					VisibilityList[EntityIndex].LastShield = Unit->Attributes->GetShield();
				}
			}
			else if (const AEffectArea* Area = Cast<AEffectArea>(Actor))
			{
				if (ImpactList.Num() > 0)
				{
					ImpactList[EntityIndex].StartScaleTime = Area->StartScaleTime;
					ImpactList[EntityIndex].VisualRotationOffset = Area->VisualRotationOffset;
					ImpactList[EntityIndex].TeamId = Area->TeamId;
				}

				if (CombatStatsList.Num() > 0)
				{
					CombatStatsList[EntityIndex].TeamId = Area->TeamId;
				}
			}
		}
	});
}
