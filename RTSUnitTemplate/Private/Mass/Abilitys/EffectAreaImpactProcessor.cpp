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
	AreaQuery.AddTagRequirement<FMassEffectAreaActiveTag>(EMassFragmentPresence::All);
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

	// UnitQuery.ForEachEntityChunk removed early return
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

	AreaQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& AreaContext)
	{
		const int32 NumEntities = AreaContext.GetNumEntities();
		TArrayView<FEffectAreaImpactFragment> ImpactList = AreaContext.GetMutableFragmentView<FEffectAreaImpactFragment>();
		TConstArrayView<FTransformFragment> TransformList = AreaContext.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment> ActorList = AreaContext.GetMutableFragmentView<FMassActorFragment>();

		float DeltaTime = AreaContext.GetDeltaTimeSeconds();
		bool bIsServer = AreaContext.GetWorld()->GetNetMode() != NM_Client;

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FEffectAreaImpactFragment& Impact = ImpactList[i];
			FVector AreaLocation = TransformList[i].GetTransform().GetLocation();
			AActor* AreaActor = ActorList[i].GetMutable();
			AEffectArea* EffectArea = Cast<AEffectArea>(AreaActor);

			Impact.ElapsedTime += DeltaTime;

			// Synchronize bIsScalingAfterImpact from Actor (for Clients)
			if (EffectArea && EffectArea->bIsScalingAfterImpact && !Impact.bIsScalingAfterImpact)
			{
				Impact.bIsScalingAfterImpact = true;
				Impact.ImpactScalingElapsedTime = 0.f;
				Impact.RadiusAtImpactStart = Impact.CurrentRadius;
			}

			if (EffectArea && EffectArea->bImpactScaleTriggered && !Impact.bImpactScaleTriggered)
			{
				Impact.bImpactScaleTriggered = true;
			}

			// 1. LifeTime Logic (Server-only for removal)
			if (bIsServer && Impact.MaxLifeTime > 0.f && Impact.ElapsedTime >= Impact.MaxLifeTime)
			{
				AreaContext.Defer().RemoveTag<FMassEffectAreaActiveTag>(AreaContext.GetEntity(i));
				continue;
			}

			// 2. Radius Calculation
			if (Impact.bIsScalingAfterImpact)
			{
				Impact.ImpactScalingElapsedTime += DeltaTime;
				float Alpha = (Impact.TimeToEndRadius > 0.f)
					? FMath::Clamp(Impact.ImpactScalingElapsedTime / Impact.TimeToEndRadius, 0.f, 1.f)
					: 1.f;
				Impact.CurrentRadius = FMath::Lerp(Impact.RadiusAtImpactStart, Impact.EndRadius, Alpha);
                
				if (Alpha >= 1.f)
				{
					Impact.bIsScalingAfterImpact = false;
					if (EffectArea)
					{
						EffectArea->bIsScalingAfterImpact = false;
					}

					if (bIsServer)
					{
						Impact.bImpactVFXTriggered = true;
						if (EffectArea)
						{
							EffectArea->bImpactVFXTriggered = true;
						}

						if (Impact.bDestroyOnImpact)
						{
							AreaContext.Defer().RemoveTag<FMassEffectAreaActiveTag>(AreaContext.GetEntity(i));
							continue;
						}
					}
				}
			}
			else if (Impact.bImpactScaleTriggered)
			{
				Impact.CurrentRadius = Impact.EndRadius;
			}
			else if (Impact.bPulsate)
			{
				// Pulsate between Start and EndRadius over TimeToEndRadius
				float Sine = (FMath::Sin(Impact.ElapsedTime * (2.0f * PI / Impact.TimeToEndRadius)) + 1.0f) / 2.0f;
				Impact.CurrentRadius = FMath::Lerp(Impact.StartRadius, Impact.EndRadius, Sine);
			}
			else if (Impact.bIsRadiusScaling)
			{
				float Alpha = FMath::Clamp(Impact.ElapsedTime / Impact.TimeToEndRadius, 0.f, 1.f);
				Impact.CurrentRadius = FMath::Lerp(Impact.StartRadius, Impact.EndRadius, Alpha);
			}
			else
			{
				Impact.CurrentRadius = Impact.BaseRadius;
			}

			// 3. Impact Logic (Server Only)
			if (bIsServer)
			{
				float RadiusSq = FMath::Square(Impact.CurrentRadius);
				bool bHitAny = false;

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
											bHitAny = true;
										}
									}
								}
							}
						}
					}
				}

				if (bHitAny)
				{
					// Start post-impact scale if configured
					if (Impact.bScaleOnImpact && !Impact.bImpactScaleTriggered)
					{
						Impact.bImpactScaleTriggered = true;
						Impact.bIsScalingAfterImpact = true;
						Impact.ImpactScalingElapsedTime = 0.f;
						Impact.RadiusAtImpactStart = Impact.CurrentRadius;
						if (EffectArea)
						{
							EffectArea->bIsScalingAfterImpact = true;
							EffectArea->bImpactScaleTriggered = true;
						}
					}
					else if (!Impact.bScaleOnImpact)
					{
						// Immediate VFX if no scaling
						Impact.bImpactVFXTriggered = true;
						if (EffectArea)
						{
							EffectArea->bImpactVFXTriggered = true;
						}

						if (Impact.bDestroyOnImpact)
						{
							AreaContext.Defer().RemoveTag<FMassEffectAreaActiveTag>(AreaContext.GetEntity(i));
						}
					}
				}
			}
		}
	});
}
