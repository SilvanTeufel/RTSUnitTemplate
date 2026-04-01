#include "Mass/Projectile/MassProjectileImpactProcessor.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "Mass/Signals/MySignals.h"
#include "MassCommands.h"
#include "Characters/Unit/UnitBase.h"

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
	static uint32 LogThrottle = 0;
	bool bShouldLog = (LogThrottle++ % 60 == 0);

	TArray<FMassEntityHandle> Units;
	TArray<FVector> UnitLocations;
	TArray<float> UnitRadii;
	TArray<int32> UnitTeams;

	UnitQuery.ForEachEntityChunk(EntityManager, Context, ([&](FMassExecutionContext& UnitContext)
	{
		TConstArrayView<FTransformFragment> TransformList = UnitContext.GetFragmentView<FTransformFragment>();
		TConstArrayView<FMassAgentCharacteristicsFragment> CharList = UnitContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		TConstArrayView<FMassCombatStatsFragment> CombatList = UnitContext.GetFragmentView<FMassCombatStatsFragment>();
		TConstArrayView<FMassActorFragment> ActorList = UnitContext.GetFragmentView<FMassActorFragment>();
		
		for (int32 i = 0; i < UnitContext.GetNumEntities(); ++i)
		{
			Units.Add(UnitContext.GetEntity(i));
			UnitLocations.Add(TransformList[i].GetTransform().GetLocation());
			UnitRadii.Add(CharList[i].CapsuleRadius);
			UnitTeams.Add(CombatList[i].TeamId);
		}
	}));

	if (Units.Num() == 0) return;

	ProjectileQuery.ForEachEntityChunk(EntityManager, Context, ([&](FMassExecutionContext& ProjContext)
	{
		TConstArrayView<FTransformFragment> TransformList = ProjContext.GetFragmentView<FTransformFragment>();
		TArrayView<FMassProjectileFragment> ProjectileList = ProjContext.GetMutableFragmentView<FMassProjectileFragment>();
		TArrayView<FMassProjectileVisualFragment> VisualList = ProjContext.GetMutableFragmentView<FMassProjectileVisualFragment>();

		for (int32 i = 0; i < ProjContext.GetNumEntities(); ++i)
		{
			const FVector& ProjPos = TransformList[i].GetTransform().GetLocation();
			FMassProjectileFragment& Projectile = ProjectileList[i];
			FMassEntityHandle ProjEntity = ProjContext.GetEntity(i);

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
					bShouldImpact = bSameTeam;
				}
				else
				{
					bShouldImpact = !bSameTeam || bIsTarget;
				}

				if (!bShouldImpact) continue; 

				// Enhanced distance check considering speed to prevent tunneling
				float DistSq = FVector::DistSquared(ProjPos, UnitLocations[j]);
				float SpeedFactor = Projectile.Speed * Context.GetDeltaTimeSeconds();
				float CollisionRadius = UnitRadii[j]; 
				
				// Be more generous on the server to ensure damage application
				float SafetyMargin = (Context.GetWorld()->GetNetMode() < NM_Client) ? 50.f : 25.f;
				float CombinedRadius = CollisionRadius + SpeedFactor + SafetyMargin;

				// DEBUG LOG (Throttled per projectile entity)
				if (bShouldLog && i == 0)
				{
					FString NetModeStr = (Context.GetWorld()->GetNetMode() == NM_Client) ? TEXT("[CLIENT]") : TEXT("[SERVER]");
					FString TargetStr = bIsTarget ? TEXT("(TARGET)") : TEXT("");
					UE_LOG(LogTemp, Warning, TEXT("%s Proj %d checking Unit %d %s: Dist %f, CombinedRad %f (Coll %f + Spd %f + %f), ProjTeam %d, UnitTeam %d"), 
						*NetModeStr, ProjEntity.Index, Units[j].Index, *TargetStr, FMath::Sqrt(DistSq), CombinedRadius, CollisionRadius, SpeedFactor, SafetyMargin, Projectile.TeamId, UnitTeams[j]);
				}

				if (DistSq <= FMath::Square(CombinedRadius))
				{
					// Impact!
					FString NetModeStr = (Context.GetWorld()->GetNetMode() == NM_Client) ? TEXT("[CLIENT]") : TEXT("[SERVER]");
					UE_LOG(LogTemp, Warning, TEXT("%s !!! IMPACT TRIGGERED !!! Proj %d -> Unit %d (Dist %f <= %f)"), 
						*NetModeStr, ProjEntity.Index, Units[j].Index, FMath::Sqrt(DistSq), CombinedRadius);
					
					Projectile.PiercedTargets++;
					
					if (Projectile.PiercedTargets >= Projectile.MaxPiercedTargets)
					{
						ProjContext.Defer().RemoveTag<FMassProjectileActiveTag>(ProjEntity);
						
						FMassProjectileVisualFragment& Visual = VisualList[i];
						if (Visual.ISMComponent.IsValid() && Visual.InstanceIndex != INDEX_NONE)
						{
							FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
							Visual.ISMComponent->UpdateInstanceTransform(Visual.InstanceIndex, HiddenTransform, true, true, true);
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
								UE_LOG(LogTemp, Warning, TEXT("[SERVER] Triggering HandleProjectileImpact for Unit %d"), Units[j].Index);
								FMassActorFragment* TargetActorFrag = EntityManager.IsEntityValid(Units[j]) ? EntityManager.GetFragmentDataPtr<FMassActorFragment>(Units[j]) : nullptr;
								FMassActorFragment* ShooterActorFrag = EntityManager.IsEntityValid(Projectile.ShooterEntity) ? EntityManager.GetFragmentDataPtr<FMassActorFragment>(Projectile.ShooterEntity) : nullptr;

								if (TargetActorFrag)
								{
									AActor* TargetActor = TargetActorFrag->GetMutable();
									AActor* ShooterActor = ShooterActorFrag ? ShooterActorFrag->GetMutable() : nullptr;

									if (AUnitBase* TargetUnit = Cast<AUnitBase>(TargetActor))
									{
										UE_LOG(LogTemp, Warning, TEXT("[SERVER] HandleProjectileImpact being called on %s by shooter %s"), *TargetUnit->GetName(), ShooterActor ? *ShooterActor->GetName() : TEXT("null"));
										TargetUnit->HandleProjectileImpact(ShooterActor, ProjPos, Projectile.ProjectileClass);
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
