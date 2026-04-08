#include "Mass/Abilitys/EffectAreaImpactProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Actors/EffectArea.h"
#include "Characters/Unit/UnitBase.h"

UMassEffectAreaImpactProcessor::UMassEffectAreaImpactProcessor()
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UMassEffectAreaImpactProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AreaQuery.Initialize(EntityManager);
	AreaQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadWrite);
	AreaQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	AreaQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	AreaQuery.AddTagRequirement<FMassEffectAreaImpactTag>(EMassFragmentPresence::All);
	AreaQuery.RegisterWithProcessor(*this);

	UnitQuery.Initialize(EntityManager);
	UnitQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	UnitQuery.RegisterWithProcessor(*this);
}

void UMassEffectAreaImpactProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> UnitEntities;
	TArray<FVector> UnitLocations;
	TArray<int32> UnitTeams;

	UnitQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& UnitContext)
	{
		const int32 NumEntities = UnitContext.GetNumEntities();
		TConstArrayView<FTransformFragment> TransformList = UnitContext.GetFragmentView<FTransformFragment>();
		TConstArrayView<FMassCombatStatsFragment> StatsList = UnitContext.GetFragmentView<FMassCombatStatsFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			UnitEntities.Add(UnitContext.GetEntity(i));
			UnitLocations.Add(TransformList[i].GetTransform().GetLocation());
			UnitTeams.Add(StatsList[i].TeamId);
		}
	});

	if (UnitEntities.Num() == 0) return;

	AreaQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& AreaContext)
	{
		const int32 NumEntities = AreaContext.GetNumEntities();
		TArrayView<FEffectAreaImpactFragment> ImpactList = AreaContext.GetMutableFragmentView<FEffectAreaImpactFragment>();
		TConstArrayView<FTransformFragment> TransformList = AreaContext.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment> ActorList = AreaContext.GetMutableFragmentView<FMassActorFragment>();

		float DeltaTime = AreaContext.GetDeltaTimeSeconds();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FEffectAreaImpactFragment& Impact = ImpactList[i];
			FVector AreaLocation = TransformList[i].GetTransform().GetLocation();
			AActor* AreaActor = ActorList[i].GetMutable();
			AEffectArea* EffectArea = Cast<AEffectArea>(AreaActor);

			Impact.ElapsedTime += DeltaTime;
			if (Impact.bIsRadiusScaling)
			{
				float Alpha = FMath::Clamp(Impact.ElapsedTime / Impact.TimeToEndRadius, 0.f, 1.f);
				Impact.CurrentRadius = FMath::Lerp(Impact.StartRadius, Impact.EndRadius, Alpha);
			}
			else
			{
				Impact.CurrentRadius = Impact.BaseRadius;
			}

			if (Impact.bScaleMesh && EffectArea && EffectArea->Mesh)
			{
				// Calculate scale based on mesh bounds to ensure visual matches logic
				FBoxSphereBounds LocalBounds = EffectArea->Mesh->CalcLocalBounds();
				float LocalRadius = LocalBounds.BoxExtent.X; // Assuming symmetric mesh in XY
    
				if (LocalRadius > 0.f)
				{
					float Scale = Impact.CurrentRadius / LocalRadius;
					EffectArea->Mesh->SetWorldScale3D(FVector(Scale));
				}
			}

			float RadiusSq = FMath::Square(Impact.CurrentRadius);

			for (int32 j = 0; j < UnitEntities.Num(); ++j)
			{
				FMassEntityHandle UnitEntity = UnitEntities[j];
				
				// Check if already hit
				bool bAlreadyHit = false;
				for (int32 k = 0; k < Impact.HitCount; ++k)
				{
					if (Impact.HitEntities[k] == UnitEntity)
					{
						bAlreadyHit = true;
						break;
					}
				}
				if (bAlreadyHit) continue;

				FVector UnitLocation = UnitLocations[j];
				if (FVector::DistSquared(AreaLocation, UnitLocation) <= RadiusSq)
				{
					int32 UnitTeam = UnitTeams[j];
					bool bShouldImpact = false;
					if (Impact.IsHealing)
					{
						if (UnitTeam == Impact.TeamId) bShouldImpact = true;
					}
					else
					{
						if (UnitTeam != Impact.TeamId) bShouldImpact = true;
					}

					if (bShouldImpact)
					{
						// Apply effects
						if (FMassActorFragment* UnitActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(UnitEntity))
						{
							if (AActor* UnitActor = UnitActorFrag->GetMutable())
							{
								if (AUnitBase* UnitBase = Cast<AUnitBase>(UnitActor))
								{
									if (UnitBase->GetUnitState() != UnitData::Dead)
									{
										UnitBase->ApplyInvestmentEffect(Impact.AreaEffectOne);
										UnitBase->ApplyInvestmentEffect(Impact.AreaEffectTwo);
										UnitBase->ApplyInvestmentEffect(Impact.AreaEffectThree);
										if (EffectArea)
										{
											EffectArea->ImpactEvent(UnitBase);
										}
										
										// Add to hit entities
										if (Impact.HitCount < FEffectAreaImpactFragment::MaxHitCount)
										{
											Impact.HitEntities[Impact.HitCount++] = UnitEntity;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	});
}
