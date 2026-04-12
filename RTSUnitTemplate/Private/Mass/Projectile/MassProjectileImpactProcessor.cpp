#include "Mass/Projectile/MassProjectileImpactProcessor.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "Mass/Signals/MySignals.h"
#include "Core/CollisionUtils.h"
#include "MassCommands.h"
#include "Characters/Unit/UnitBase.h"

#include "LandscapeProxy.h"
#include "Actors/Projectile.h"

UMassProjectileImpactProcessor::UMassProjectileImpactProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true; // Signal subsystem might need game thread
}

void UMassProjectileImpactProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	UE_LOG(LogTemp, Log, TEXT("UMassProjectileImpactProcessor::ConfigureQueries"));
	ProjectileQuery.Initialize(EntityManager);
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FMassProjectileFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FMassProjectileVisualFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddTagRequirement<FMassProjectileActiveTag>(EMassFragmentPresence::All);
	ProjectileQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.RegisterWithProcessor(*this);

	UnitQuery.Initialize(EntityManager);
	UnitQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	UnitQuery.RegisterWithProcessor(*this);
}

void UMassProjectileImpactProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> Units;
	TArray<FVector> UnitLocations;
	TArray<FRotator> UnitRotations;
	TArray<FMassAgentCharacteristicsFragment> UnitCharFrags;
	TArray<int32> UnitTeams;

	UnitQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& UnitContext)
	{
		TConstArrayView<FTransformFragment> TransformList = UnitContext.GetFragmentView<FTransformFragment>();
		TConstArrayView<FMassAgentCharacteristicsFragment> CharList = UnitContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		TConstArrayView<FMassCombatStatsFragment> CombatList = UnitContext.GetFragmentView<FMassCombatStatsFragment>();
		
		for (int32 i = 0; i < UnitContext.GetNumEntities(); ++i)
		{
			Units.Add(UnitContext.GetEntity(i));
			UnitLocations.Add(TransformList[i].GetTransform().GetLocation());
			UnitRotations.Add(TransformList[i].GetTransform().Rotator());
			UnitCharFrags.Add(CharList[i]);
			UnitTeams.Add(CombatList[i].TeamId);
		}
	}));

	// UnitQuery and filling Units array...
	// We no longer return early if no units, because we might need to check landscape hits.

	ProjectileQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ProjContext)
	{
		TConstArrayView<FTransformFragment> TransformList = ProjContext.GetFragmentView<FTransformFragment>();
		TArrayView<FMassProjectileFragment> ProjectileList = ProjContext.GetMutableFragmentView<FMassProjectileFragment>();
		TArrayView<FMassProjectileVisualFragment> VisualList = ProjContext.GetMutableFragmentView<FMassProjectileVisualFragment>();

		for (int32 i = 0; i < ProjContext.GetNumEntities(); ++i)
		{
			const FVector& ProjPos = TransformList[i].GetTransform().GetLocation();
			FMassProjectileFragment& Projectile = ProjectileList[i];
			FMassEntityHandle ProjEntity = ProjContext.GetEntity(i);

			// --- NEW: Landscape Collision Check (Optimized) ---
			if (Projectile.bEnableLandscapeHit && Projectile.bHasLandscapeImpact)
			{
				float DistSq = FVector::DistSquared(ProjPos, Projectile.LandscapeImpactLocation);
				// Check if we are within collision radius, accounting for speed to avoid skipping
				float SpeedBuffer = Projectile.Speed * Context.GetDeltaTimeSeconds();
				float CheckRadius = Projectile.CollisionRadius + SpeedBuffer + 25.f;

				if (DistSq <= FMath::Square(CheckRadius))
				{
					// Trigger GroundHit on CDO
					if (Projectile.ProjectileClass)
					{
						if (AProjectile* ProjCDO = Projectile.ProjectileClass->GetDefaultObject<AProjectile>())
						{
							UObject* WorldCtx = Projectile.WorldContext.IsValid() ? Projectile.WorldContext.Get() : (UObject*)Context.GetWorld();
							ProjCDO->GroundHit(Projectile.LandscapeImpactLocation, WorldCtx);
						}
					}

					// Destroy Projectile
					EntityManager.Defer().DestroyEntity(ProjEntity);

					// Cleanup Visuals
					if (VisualList[i].InstanceIndex != INDEX_NONE && VisualList[i].ISMComponent.IsValid())
					{
						// Set Scale to 0 AND move far away to prevent ANY visual artifacts (including shadows)
						FTransform HiddenTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -1000000.f), FVector::ZeroVector);
						VisualList[i].ISMComponent->UpdateInstanceTransform(VisualList[i].InstanceIndex, HiddenTransform, true, true, true);
						VisualList[i].InstanceIndex = INDEX_NONE;
					}
					if (VisualList[i].Niagara_A.IsValid()) { VisualList[i].Niagara_A->Deactivate(); VisualList[i].Niagara_A->DestroyComponent(); }
					if (VisualList[i].Niagara_B.IsValid()) { VisualList[i].Niagara_B->Deactivate(); VisualList[i].Niagara_B->DestroyComponent(); }

					continue; // Move to next projectile
				}
			}

			// Check distance to all units
			for (int32 j = 0; j < Units.Num(); ++j)
			{
				bool bIsTarget = (Units[j] == Projectile.TargetEntity);
				bool bSameTeam = (Projectile.TeamId == UnitTeams[j]);

				// Damage logic: Impact if different team OR if it's the specific target unit
				// Heal logic: Impact only if same team AND IsHealing is true
				bool bShouldImpact = false;
				if (Projectile.IsHealing)
				{
					bShouldImpact = bSameTeam && bIsTarget;
				}
				else
				{
					bShouldImpact = !bSameTeam || bIsTarget;
				}

				if (!bShouldImpact) continue; 

				// --- NEW: Skip if already hit this unit (Impact only once per Unit) ---
				bool bAlreadyHit = false;
				for (uint8 HitIdx = 0; HitIdx < Projectile.HitCount; ++HitIdx)
				{
					if (Projectile.HitEntities[HitIdx] == Units[j])
					{
						bAlreadyHit = true;
						break;
					}
				}
				if (bAlreadyHit) continue;

				// Enhanced distance check considering speed to prevent tunneling
				float DistSq = FVector::DistSquared(ProjPos, UnitLocations[j]);
				float SpeedFactor = Projectile.Speed * Context.GetDeltaTimeSeconds();
				float TargetCollisionRadius = UnitCharFrags[j].GetRadiusInDirection(ProjPos - UnitLocations[j], UnitRotations[j]); 
				
				// Be more generous on the server to ensure damage application
				float SafetyMargin = (Context.GetWorld()->GetNetMode() < NM_Client) ? 50.f : 25.f;
				float CombinedRadius = TargetCollisionRadius + Projectile.CollisionRadius + SpeedFactor + SafetyMargin;

				if (DistSq <= FMath::Square(CombinedRadius))
				{
					// Impact!
					if (bIsTarget)
					{
						Projectile.bHasHitTarget = true;
					}
					
					// Register hit
					if (Projectile.HitCount < 16)
					{
						Projectile.HitEntities[Projectile.HitCount++] = Units[j];
					}

					Projectile.PiercedTargets++;
					
					if (Projectile.PiercedTargets >= Projectile.MaxPiercedTargets)
					{
						ProjContext.Defer().DestroyEntity(ProjEntity);
						
						FMassProjectileVisualFragment& Visual = VisualList[i];
						if (Visual.ISMComponent.IsValid() && Visual.InstanceIndex != INDEX_NONE)
						{
							// Set Scale to 0 AND move far away to prevent ANY visual artifacts (including shadows)
							FTransform HiddenTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -1000000.f), FVector::ZeroVector);
							Visual.ISMComponent->UpdateInstanceTransform(Visual.InstanceIndex, HiddenTransform, true, true, true);
							Visual.InstanceIndex = INDEX_NONE;
						}

						if (UNiagaraComponent* NC_A = Visual.Niagara_A.Get())
						{
							NC_A->Deactivate();
							NC_A->SetVisibility(false);
							NC_A->DestroyComponent();
						}

						if (UNiagaraComponent* NC_B = Visual.Niagara_B.Get())
						{
							NC_B->Deactivate();
							NC_B->SetVisibility(false);
							NC_B->DestroyComponent();
						}
					}

					if (UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>())
					{
						SignalSubsystem->SignalEntity(UnitSignals::ProjectileImpact, Units[j]);
						// Let's use the CDO's Impact directly if we are on server
						if (UWorld* World = EntityManager.GetWorld())
						{
							if (World->GetNetMode() < NM_Client)
							{
								FMassActorFragment* TargetActorFrag = EntityManager.IsEntityActive(Units[j]) ? EntityManager.GetFragmentDataPtr<FMassActorFragment>(Units[j]) : nullptr;
								FMassActorFragment* ShooterActorFrag = EntityManager.IsEntityActive(Projectile.ShooterEntity) ? EntityManager.GetFragmentDataPtr<FMassActorFragment>(Projectile.ShooterEntity) : nullptr;

								if (TargetActorFrag)
								{
									AActor* TargetActor = TargetActorFrag->GetMutable();
									AActor* ShooterActor = ShooterActorFrag ? ShooterActorFrag->GetMutable() : nullptr;

									if (AUnitBase* TargetUnit = Cast<AUnitBase>(TargetActor))
									{
										FVector PreciseImpactPos = FCollisionUtils::ComputeImpactSurfaceXY(ShooterActor, TargetActor, ProjPos);
										TargetUnit->HandleProjectileImpact(ShooterActor, PreciseImpactPos, Projectile.ProjectileClass, Projectile.Damage);
									}
									else
									{
										UE_LOG(LogTemp, Error, TEXT("[SERVER] TargetActor is not AUnitBase!"));
									}
								}
								else
								{
									UE_LOG(LogTemp, Error, TEXT("[SERVER] TargetActorFrag is missing for Unit %d"), Units[j].Index);
								}
							}
						}
					}
					
					break; 
				}
			}
		}
	}));
}
