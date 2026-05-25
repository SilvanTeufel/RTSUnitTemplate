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
#include "Actors/Waypoint.h"
#include "GameModes/ResourceGameMode.h"
#include "Core/RTSUnitUtils.h"
using namespace RTSUnitUtils;


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
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassVisualEffectFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassPatrolFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassWorkerStatsFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAllianceFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
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
		const TArrayView<FMassPatrolFragment> PatrolList = ChunkContext.GetMutableFragmentView<FMassPatrolFragment>();
		const TArrayView<FMassAllianceFragment> AllianceList = ChunkContext.GetMutableFragmentView<FMassAllianceFragment>();
		const TArrayView<FTransformFragment> TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < ChunkContext.GetNumEntities(); ++EntityIndex)
		{
			AActor* Actor = ActorList[EntityIndex].GetMutable();
			if (!Actor) continue;

			if (const AUnitBase* Unit = Cast<AUnitBase>(Actor))
			{
				if (CombatStatsList.Num() > 0) SyncCombatStats(*Unit, CombatStatsList[EntityIndex]);
				if (CharacteristicsList.Num() > 0) SyncCharacteristics(*Unit, CharacteristicsList[EntityIndex]);
				if (AIStateList.Num() > 0 && CombatStatsList.Num() > 0) SyncAIState(*Unit, AIStateList[EntityIndex], CombatStatsList[EntityIndex]);
				if (VisibilityList.Num() > 0) SyncVisibility(*Unit, VisibilityList[EntityIndex]);
				if (AllianceList.Num() > 0) AllianceList[EntityIndex].AlliedTeamsMask = Unit->AlliedTeamsMask;
				
				// KORREKTUR: Zugriff über EntityManager für optionale Fragmente
				FMassEntityHandle EntityHandle = ChunkContext.GetEntity(EntityIndex);
				if (FMassPatrolFragment* PatrolFrag = EntityManager.GetFragmentDataPtr<FMassPatrolFragment>(EntityHandle))
				{
					SyncPatrol(*Unit, *PatrolFrag, EntityManager, EntityHandle);
				}

				if (FMassWorkerStatsFragment* WorkerStats = EntityManager.GetFragmentDataPtr<FMassWorkerStatsFragment>(EntityHandle))
				{
					if (Unit->IsWorker)
					{
						WorkerStats->BaseAvailable = IsValid(Unit->Base) && (Unit->Base->GetUnitState() != UnitData::Dead);
					}
				}
			
			}
			else if (const AEffectArea* Area = Cast<AEffectArea>(Actor))
			{
				if (ImpactList.Num() > 0)
				{
					SyncEffectArea(*Area, ImpactList[EntityIndex], 
						CombatStatsList.Num() > 0 ? &CombatStatsList[EntityIndex] : nullptr, 
						CharacteristicsList.Num() > 0 ? &CharacteristicsList[EntityIndex] : nullptr, 
						TransformList.Num() > 0 ? &TransformList[EntityIndex] : nullptr);
				}
			}
		}
	});
}

void UUnitActorToFragmentSyncProcessor::SyncCombatStats(const AUnitBase& Unit, FMassCombatStatsFragment& Stats)
{
	if (Unit.Attributes)
	{
		Stats.Health = Unit.Attributes->GetHealth();
		Stats.Shield = Unit.Attributes->GetShield();
		Stats.MaxHealth = Unit.Attributes->GetMaxHealth();
		Stats.MaxShield = Unit.Attributes->GetMaxShield();
		Stats.TeamId = Unit.TeamId;

		Stats.AttackDamage = Unit.Attributes->GetAttackDamage();
		Stats.AttackRange = Unit.Attributes->GetRange();
		Stats.RunSpeed = Unit.Attributes->GetRunSpeed();
		Stats.Armor = Unit.Attributes->GetArmor();
		Stats.MagicResistance = Unit.Attributes->GetMagicResistance();
	}

	Stats.PauseDuration = Unit.PauseDuration;
	Stats.AttackDuration = Unit.AttackDuration;
	Stats.bUseProjectile = Unit.UseProjectile;
	Stats.CastTime = Unit.CastTime;
	Stats.IsInitialized = Unit.IsInitialized;
	Stats.bIsInitializedOnClient = true;
	Stats.bCanMoveWhileAttacking = Unit.MassActorBindingComponent->CanMoveWhileAttacking;
	Stats.SightRadius = Unit.MassActorBindingComponent->SightRadius;
	Stats.LoseSightRadius = Unit.MassActorBindingComponent->LoseSightRadius;
}

void UUnitActorToFragmentSyncProcessor::SyncCharacteristics(const AUnitBase& Unit, FMassAgentCharacteristicsFragment& Characteristics)
{
	bool bFlyParamChanged = false;
	if (Characteristics.FlyHeight != Unit.FlyHeight)
	{
		Characteristics.FlyHeight = Unit.FlyHeight;
		bFlyParamChanged = true;
	}

	if (Characteristics.bIsFlying != Unit.IsFlying)
	{
		Characteristics.bIsFlying = Unit.IsFlying;
		bFlyParamChanged = true;
	}

	if (bFlyParamChanged)
	{
		// Wenn sich Flugparameter aendern (besonders auf dem Client wichtig),
		// muessen wir die visuelle Position neu berechnen, falls es ein Gebaeude ist.
		// Bei Einheiten, die sich bewegen, macht das der MovementProcessor/PlacementProcessor ohnehin.
		// Aber Gebaeude haben StopMovement und werden sonst nie wieder angefasst.
		if (!Unit.CanMove)
		{
			FVector FinalLoc = Characteristics.PositionedTransform.GetLocation();
			const float NewZ = Characteristics.bIsFlying ? (Characteristics.LastGroundLocation + Characteristics.FlyHeight) : (Characteristics.LastGroundLocation + Characteristics.CapsuleHeight);
			
			if (!FMath::IsNearlyEqual(FinalLoc.Z, NewZ))
			{
				FinalLoc.Z = NewZ;
				Characteristics.PositionedTransform.SetLocation(FinalLoc);
				Characteristics.bTransformDirty = true;
			}
		}
	}
	
	Characteristics.bCanOnlyAttackFlying = Unit.CanOnlyAttackFlying;
	Characteristics.bCanOnlyAttackGround = Unit.CanOnlyAttackGround;
	Characteristics.bCanDetectInvisible = Unit.CanDetectInvisible;
	const FVector CurrentActorScale = Unit.GetActorScale3D();
	
	if (!Characteristics.Scale.Equals(CurrentActorScale, 0.001f))
	{
		Characteristics.Scale = CurrentActorScale;
		Characteristics.PositionedTransform.SetScale3D(CurrentActorScale);
		Characteristics.bTransformDirty = true;
	}
}

void UUnitActorToFragmentSyncProcessor::SyncAIState(const AUnitBase& Unit, FMassAIStateFragment& AIState, FMassCombatStatsFragment& CombatStats)
{
	AIState.CanAttack = Unit.CanAttack;
	AIState.CanMove = Unit.CanMove;
	AIState.HoldPosition = Unit.bHoldPosition;

	if (Unit.MassActorBindingComponent)
	{
		if (!AIState.bHasExtendedLoseSight)
		{
			CombatStats.SightRadius = Unit.MassActorBindingComponent->SightRadius;
			CombatStats.LoseSightRadius = Unit.MassActorBindingComponent->LoseSightRadius;
		}
	}
}

void UUnitActorToFragmentSyncProcessor::SyncVisibility(const AUnitBase& Unit, FMassVisibilityFragment& Visibility)
{
	if (Unit.Attributes)
	{
		Visibility.LastHealth = Unit.Attributes->GetHealth();
		Visibility.LastShield = Unit.Attributes->GetShield();
	}
}

void UUnitActorToFragmentSyncProcessor::SyncPatrol(const AUnitBase& Unit, FMassPatrolFragment& PatrolFrag, FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle)
{
	if (!Unit.HasAuthority()) return;
	
	if (Unit.NextWaypoint)
	{
		if (PatrolFrag.TargetWaypointLocation != Unit.NextWaypoint->GetActorLocation())
		{
			PatrolFrag.bLoopPatrol = Unit.NextWaypoint->PatrolCloseToWaypoint;
			PatrolFrag.RandomPatrolMinIdleTime = Unit.NextWaypoint->PatrolCloseMinInterval;
			PatrolFrag.RandomPatrolMaxIdleTime = Unit.NextWaypoint->PatrolCloseMaxInterval;
			PatrolFrag.TargetWaypointLocation = Unit.NextWaypoint->GetActorLocation();
			PatrolFrag.RandomPatrolRadius = (Unit.NextWaypoint->PatrolCloseOffset.X + Unit.NextWaypoint->PatrolCloseOffset.Y) / 2.f;
			PatrolFrag.IdleChance = Unit.NextWaypoint->PatrolCloseIdlePercentage;
		}
	}
}

void UUnitActorToFragmentSyncProcessor::SyncEffectArea(const AEffectArea& Area, FEffectAreaImpactFragment& Impact, FMassCombatStatsFragment* CombatStats, FMassAgentCharacteristicsFragment* Characteristics, FTransformFragment* TransformFragment)
{
	// Auf dem Client: Warten, bis der AreaIndex vom Server repliziert wurde
	if (!Area.HasAuthority() && Area.AreaIndex == INDEX_NONE)
	{
		return;
	}

	if (!Impact.bIsInitializedOnClient)
	{
		// Sicherheitsnetz: Falls SetupMassOnEffectArea zu früh durchkam (z.B. BaseRadius noch 0)
		if (!Area.HasAuthority() && Area.BaseRadius <= 0.1f)
		{
			return; 
		}

		// Einmaliger Sync der Radien und BP-Konstanten
        Impact.StartRadius = Area.StartRadius;
        Impact.EndRadius = Area.EndRadius;
        Impact.BaseRadius = Area.BaseRadius;
        Impact.TimeToEndRadius = Area.TimeToEndRadius;
        Impact.bScaleOnImpact = Area.bScaleOnImpact;
        Impact.bPulsate = Area.bPulsate;
        Impact.bDestroyOnImpact = Area.bDestroyOnImpact;
        Impact.bIsRadiusScaling = Area.bIsRadiusScaling;
        Impact.bScaleMesh = Area.ScaleMesh;
        Impact.IsHealing = Area.IsHealing;

        // Sync Gameplay Effects
        Impact.AreaEffectOne = Area.AreaEffectOne;
        Impact.AreaEffectTwo = Area.AreaEffectTwo;
        Impact.AreaEffectThree = Area.AreaEffectThree;

        // Sync Destruction/Spawn params from Actor/Component
        if (Area.MassBindingComponent)
        {
            Impact.HideActorTime = Area.MassBindingComponent->HideActorTime;
            Impact.DespawnTime = Area.MassBindingComponent->DespawnTime;

            if (Characteristics)
            {
                Characteristics->HideActorTime = Area.MassBindingComponent->HideActorTime;
                Characteristics->DespawnTime = Area.MassBindingComponent->DespawnTime;
            }
        }
        Impact.EarlySpawnTime = Area.EarlySpawnTime;
        Impact.SpawnRandomOffsetMin = Area.SpawnRandomOffsetMin;
        Impact.SpawnRandomOffsetMax = Area.SpawnRandomOffsetMax;

        // Existing syncs
        Impact.StartScaleTime = Area.StartScaleTime;
        Impact.VisualRotationOffset = Area.VisualRotationOffset;
        Impact.TeamId = Area.TeamId;

        if (CombatStats)
        {
            CombatStats->TeamId = Area.TeamId;
            // Hinweis: Health wird NICHT synchronisiert, da dies bereits in InitStats geschah
        }

        if (TransformFragment)
        {
            TransformFragment->GetMutableTransform().SetLocation(Area.GetActorLocation());
            TransformFragment->GetMutableTransform().SetRotation(Area.GetActorRotation().Quaternion());
        }

        Impact.bIsInitializedOnClient = true;

    }

    if (!Area.HasAuthority())
    {
        // Übertrage den replizierten Status vom Actor in das Fragment
        Impact.bIsScalingAfterImpact = Area.bIsScalingAfterImpact;
        Impact.bImpactScaleTriggered = Area.bImpactScaleTriggered;
        
        // Falls die Skalierung auf dem Client gerade erst durch Replikation gestartet wurde
        if (Impact.bIsScalingAfterImpact && Impact.ImpactScalingElapsedTime == 0.f)
        {
            Impact.RadiusAtImpactStart = Area.RadiusAtImpactStart;
        }
    }

    // Persistent safety check: If the transform is still at 0,0,0 but the actor has moved, re-sync.
    if (TransformFragment)
    {
        FVector FragLoc = TransformFragment->GetTransform().GetLocation();
        FVector ActorLoc = Area.GetActorLocation();
        if (FragLoc.IsNearlyZero() && !ActorLoc.IsNearlyZero())
        {
            TransformFragment->GetMutableTransform().SetLocation(ActorLoc);
            TransformFragment->GetMutableTransform().SetRotation(Area.GetActorRotation().Quaternion());
            
        }
    }
}
