// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
#include "Actors/EffectArea.h"

#include "Mass/Projectile/MassProjectileMovementProcessor.h"
#include "LandscapeProxy.h"
#include "Actors/Projectile.h"

// BEHEBUNG (22.08.2026): Mindestgroesse der Trefferzone von Einheiten.
//
// Die Trefferpruefung nimmt den Radius aus dem Mass-Fragment, das ihn wiederum aus der
// Kollisionskapsel bezieht. Diese Kapseln stehen bei den Xeno-Einheiten deutlich kleiner als das,
// was der Spieler sieht: Needle-Wing hat CapsuleRadius 30 bei Mesh-Scale 2, MonsterFly 300 bei
// Kapsel-Scale 0.2 - also effektiv 60. Gemessen ueber 639 Fehlschuesse lag der Zielradius im
// Median bei 30 uu, waehrend das sichtbare Modell ein Vielfaches davon einnimmt. Der Spieler
// trifft das Insekt und die Pruefung sieht einen winzigen Zylinder in dessen Mitte vorbeiziehen.
//
// Die Kapseln selbst bleiben unangetastet - sie tragen Ausweichen, Klick-Auswahl, Pfadsuche und
// Einheitenabstaende. Stattdessen bekommt NUR die Projektil-Trefferpruefung eine Untergrenze.
// Als Konsolenvariable, damit sich der Wert im laufenden Spiel einstellen laesst.
static TAutoConsoleVariable<float> CVarRTS_MinProjectileHitRadius(
	TEXT("r.RTS.MinProjectileHitRadius"),
	150.0f,
	TEXT("Mindestradius der Trefferzone einer Einheit fuer Projektile (uu). 0 = aus, dann gilt allein die Kollisionskapsel."),
	ECVF_Default);

UMassProjectileImpactProcessor::UMassProjectileImpactProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true; // Signal subsystem might need game thread

	ExecutionOrder.ExecuteAfter.Add(UMassProjectileMovementProcessor::StaticClass()->GetFName());
}

void UMassProjectileImpactProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProjectileQuery.Initialize(EntityManager);
	ProjectileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ProjectileQuery.AddRequirement<FMassProjectileFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FMassProjectileVisualFragment>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.AddRequirement<FMassAllianceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ProjectileQuery.AddTagRequirement<FMassProjectileActiveTag>(EMassFragmentPresence::All);
	ProjectileQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	ProjectileQuery.RegisterWithProcessor(*this);

	UnitQuery.Initialize(EntityManager);
	UnitQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	UnitQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::Optional);
	UnitQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::Optional);
	UnitQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::Optional);
	UnitQuery.RegisterWithProcessor(*this);
}

void UMassProjectileImpactProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> Units;
	TArray<FVector> UnitLocations;
	TArray<FRotator> UnitRotations;
	TArray<FMassAgentCharacteristicsFragment> UnitCharFrags;
	TArray<int32> UnitTeams;
	TArray<bool> UnitIsDead;

	UnitQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& UnitContext)
	{
		TConstArrayView<FTransformFragment> TransformList = UnitContext.GetFragmentView<FTransformFragment>();
		TConstArrayView<FMassAgentCharacteristicsFragment> CharList = UnitContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		TConstArrayView<FMassCombatStatsFragment> CombatList = UnitContext.GetFragmentView<FMassCombatStatsFragment>();
		const bool bChunkIsDead = UnitContext.DoesArchetypeHaveTag<FMassStateDeadTag>();
		for (int32 i = 0; i < UnitContext.GetNumEntities(); ++i)
		{
			Units.Add(UnitContext.GetEntity(i));
			
			FVector Location = TransformList[i].GetTransform().GetLocation();
			
			// Z-Position für die Kollisionsprüfung anpassen, damit Projektile 
			// die Brust (oder Flughöhe) treffen statt über die Füße zu fliegen.
			if (CharList[i].bIsFlying)
			{
				Location.Z = CharList[i].FlyHeight + CharList[i].LastGroundLocation;
			}
			else
			{
				Location.Z = CharList[i].CapsuleHeight + CharList[i].LastGroundLocation;
			}

			UnitLocations.Add(Location);
			UnitRotations.Add(TransformList[i].GetTransform().Rotator());
			UnitCharFrags.Add(CharList[i]);
			UnitTeams.Add(CombatList[i].TeamId);
			UnitIsDead.Add(bChunkIsDead);
		}
	}));

	if (Units.Num() == 0)
	{
		// return; // We no longer return early if no units, because we might need to check landscape hits.
	}

	ProjectileQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ProjContext)
	{
		TConstArrayView<FTransformFragment> TransformList = ProjContext.GetFragmentView<FTransformFragment>();
		TArrayView<FMassProjectileFragment> ProjectileList = ProjContext.GetMutableFragmentView<FMassProjectileFragment>();
		TArrayView<FMassProjectileVisualFragment> VisualList = ProjContext.GetMutableFragmentView<FMassProjectileVisualFragment>();
		TConstArrayView<FMassAllianceFragment> AllianceList = ProjContext.GetFragmentView<FMassAllianceFragment>();

		for (int32 i = 0; i < ProjContext.GetNumEntities(); ++i)
		{
			const FVector& ProjPos = TransformList[i].GetTransform().GetLocation();
			FMassProjectileFragment& Projectile = ProjectileList[i];
			FMassEntityHandle ProjEntity = ProjContext.GetEntity(i);

			// --- NEW: Landscape Collision Check (Optimized) ---
			if (Projectile.bEnableLandscapeHit && Projectile.bHasLandscapeImpact)
			{
				float MoveDist = Projectile.Speed * 10.f * Context.GetDeltaTimeSeconds();
				FVector PrevPos = ProjPos - (Projectile.FlightDirection * MoveDist);
				
				// Use the center of flight if spiraling/homing (HomingOffset is the relative offset from center-line)
				FVector AdjustedProjPos = ProjPos - Projectile.HomingOffset;
				FVector AdjustedPrevPos = PrevPos - Projectile.HomingOffset;

				float DistSq = FMath::PointDistToSegmentSquared(Projectile.LandscapeImpactLocation, AdjustedPrevPos, AdjustedProjPos);
				
				// Check if we are within collision radius. 
				// We use a segment check now, so we don't need the SpeedBuffer in CheckRadius anymore, 
				// but we keep a generous safety margin.
				float CheckRadius = Projectile.CollisionRadius + 50.f;

				if (DistSq <= FMath::Square(CheckRadius))
				{
					// Trigger GroundHit on CDO (SERVER ONLY to avoid inaccuracy and CDO state pollution on client)
					if (Projectile.ProjectileClass && Context.GetWorld()->GetNetMode() != NM_Client)
					{
						if (AProjectile* ProjCDO = Projectile.ProjectileClass->GetDefaultObject<AProjectile>())
						{
							UObject* WorldCtx = Projectile.WorldContext.IsValid() ? Projectile.WorldContext.Get() : (UObject*)Context.GetWorld();
							ProjCDO->GroundHit(Projectile.LandscapeImpactLocation, WorldCtx, Projectile.AreaInfo, Projectile.TeamId);
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
				
				// Skip dead units unless they are the target
				if (UnitIsDead[j] && !bIsTarget)
				{
					continue;
				}

				bool bSameTeam = (Projectile.TeamId == UnitTeams[j]);
				const bool bIsAllied = (!AllianceList.IsEmpty() && (AllianceList[i].AlliedTeamsMask & (1LL << UnitTeams[j])));

				// Damage logic: Impact if different team OR if it's the specific target unit
				// Heal logic: Impact only if same team AND IsHealing is true
				bool bShouldImpact = false;
				if (Projectile.IsHealing)
				{
					bShouldImpact = bSameTeam && bIsTarget;
				}
				else
				{
					bShouldImpact = (!bSameTeam && !bIsAllied) || bIsTarget;
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
				//
				// BEHEBUNG (22.08.2026): Die Hoehe darf nicht ueber den Treffer entscheiden.
				//
				// Gemessen ueber 3684 Beinahe-Treffer des Spielers: das Projektil fliegt im Median
				// 152 uu UEBER dem Ziel (bis 550), weil das Schiff hoeher schwebt als die
				// Bodeneinheiten und der Schuss auf Schiffshoehe waagerecht laeuft. Der Zielradius
				// betraegt dabei nur 30-60 uu. In einer reinen 3D-Kugelpruefung frisst der
				// Hoehenanteil damit den halben Radius auf: 173 der 265 echten Beinahe-Treffer
				// (65%) waeren ohne den Z-Anteil ein Treffer gewesen.
				//
				// Der Spieler zielt in der Ebene, also wird auch in der Ebene geprueft - aus der
				// Kugel wird ein stehender Zylinder. Die Hoehe bleibt eine Schranke, nur eine
				// grosszuegigere: Trefferradius plus Koerperhoehe des Ziels. Damit trifft ein
				// Schuss weiterhin nicht quer durch mehrere Etagen, aber der Hoehenversatz
				// zwischen fliegendem Schuetzen und Bodenziel kostet keinen Treffer mehr.
				const FVector ZielDelta = ProjPos - UnitLocations[j];
				const float DistSqEbene = ZielDelta.X * ZielDelta.X + ZielDelta.Y * ZielDelta.Y;
				const float HoehenAbstand = FMath::Abs(ZielDelta.Z);
				float SpeedFactor = Projectile.Speed * 10.f * Context.GetDeltaTimeSeconds();
				float TargetCollisionRadius = UnitCharFrags[j].GetRadiusInDirection(ProjPos - UnitLocations[j], UnitRotations[j]);

				// Untergrenze anwenden - siehe CVarRTS_MinProjectileHitRadius oben.
				const float MindestZielRadius = CVarRTS_MinProjectileHitRadius.GetValueOnAnyThread();
				if (MindestZielRadius > 0.f)
				{
					TargetCollisionRadius = FMath::Max(TargetCollisionRadius, MindestZielRadius);
				}

				
				// Be more generous on the server to ensure damage application
				float SafetyMargin = (Context.GetWorld()->GetNetMode() < NM_Client) ? 50.f : 25.f;
				float CombinedRadius = TargetCollisionRadius + Projectile.CollisionRadius + SpeedFactor + SafetyMargin;

				const float HoehenToleranz = CombinedRadius + UnitCharFrags[j].CapsuleHeight;
				const bool bTrefferGeometrie = (DistSqEbene <= FMath::Square(CombinedRadius))
					&& (HoehenAbstand <= HoehenToleranz);

				if (bTrefferGeometrie)
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
										// Use the SHOOTER position (not the current projectile position) as the incoming direction so the
										// impact surface lands on the side FACING the shooter. ProjPos can overshoot PAST the target center
										// for large targets — e.g. the WorkArea-sized capsule/box of the extension ConstructionUnit — which
										// flips the surface to the FAR side (VFX on the opposite side). Fall back to ProjPos only if no shooter.
										FVector PreciseImpactPos = ShooterActor
											? FCollisionUtils::ComputeImpactSurfaceXY(ShooterActor, TargetActor)
											: FCollisionUtils::ComputeImpactSurfaceXY(ShooterActor, TargetActor, ProjPos);
										
										// If target is flying, ensure impact VFX stay at projectile height
										if (UnitCharFrags[j].bIsFlying)
										{
											PreciseImpactPos.Z = ProjPos.Z;
										}
										
										TargetUnit->HandleProjectileImpact(ShooterActor, PreciseImpactPos, Projectile.ProjectileClass, Projectile.Damage, Projectile.ProjectileEffect, Projectile.ProjectileEffect2, Projectile.ProjectileEffect3);

										// Shot by someone outside our sight: widen the victim's DETECTION range for a
										// few seconds so it can find the shooter, but only while it has no target of
										// its own (ApplyAttackedDetectionBonus enforces that). This is the case the
										// melee/ranged-start paths in UUnitStateProcessor cannot cover - the shooter
										// may be far outside SightRadius when the projectile finally lands.
										if (FMassAIStateFragment* VictimState = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Units[j]))
										{
											const FMassAITargetFragment* VictimTarget = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Units[j]);
											const FMassCombatStatsFragment* VictimStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Units[j]);
											if (VictimTarget && VictimStats)
											{
												ApplyAttackedDetectionBonus(*VictimState, *VictimTarget, *VictimStats);
											}
										}

										// Fire the projectile CDO's ImpactEvent once per hit unit (server-only, mirrors GroundHit).
										// This block only runs for a newly-hit unit (HitEntities dedup above), so it is one-shot per unit.
										if (Projectile.ProjectileClass)
										{
											if (AProjectile* ProjCDO = Projectile.ProjectileClass->GetDefaultObject<AProjectile>())
											{
												UObject* ImpactWorldCtx = Projectile.WorldContext.IsValid() ? Projectile.WorldContext.Get() : (UObject*)World;
												ProjCDO->ImpactEvent(PreciseImpactPos, ImpactWorldCtx, TargetActor, Projectile.TeamId);
											}
										}
									}
									else if (AEffectArea* EffectArea = Cast<AEffectArea>(TargetActor))
									{
										// Use the SHOOTER position (not the current projectile position) as the incoming direction so the
										// impact surface lands on the side FACING the shooter. ProjPos can overshoot PAST the target center
										// for large targets — e.g. the WorkArea-sized capsule/box of the extension ConstructionUnit — which
										// flips the surface to the FAR side (VFX on the opposite side). Fall back to ProjPos only if no shooter.
										FVector PreciseImpactPos = ShooterActor
											? FCollisionUtils::ComputeImpactSurfaceXY(ShooterActor, TargetActor)
											: FCollisionUtils::ComputeImpactSurfaceXY(ShooterActor, TargetActor, ProjPos);
										
										// If target is flying, ensure impact VFX stay at projectile height
										if (UnitCharFrags[j].bIsFlying)
										{
											PreciseImpactPos.Z = ProjPos.Z;
										}
										
										EffectArea->HandleProjectileImpact(ShooterActor, PreciseImpactPos, Projectile.ProjectileClass, Projectile.Damage, Projectile.ProjectileEffect, Projectile.ProjectileEffect2, Projectile.ProjectileEffect3);
									}
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
