#include "Mass/Projectile/MassProjectileMovementProcessor.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassEntityManager.h"
#include "MassEntityTypes.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Mass/Projectile/ProjectileVisualManager.h"
#include "Actors/Projectile.h"
#include "Engine/World.h"
#include "Characters/Unit/UnitBase.h"
#include "MassActorSubsystem.h"

UMassProjectileMovementProcessor::UMassProjectileMovementProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UMassProjectileMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	UE_LOG(LogTemp, Log, TEXT("UMassProjectileMovementProcessor::ConfigureQueries - World: %s"), EntityManager->GetWorld() ? *EntityManager->GetWorld()->GetName() : TEXT("None"));
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassProjectileFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassProjectileVisualFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassProjectileActiveTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassProjectileMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	static uint32 LogThrottle = 0;
	bool bSyncFromCDO = (LogThrottle % 120 == 0); // Sync from CDO every 120 frames
	LogThrottle++;
	
	EntityQuery.ForEachEntityChunk(Context, ([this, &EntityManager, bSyncFromCDO](FMassExecutionContext& Context)
	{
		UProjectileVisualManager* VisualManager = EntityManager.GetWorld()->GetSubsystem<UProjectileVisualManager>();
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FMassProjectileFragment> ProjectileList = Context.GetMutableFragmentView<FMassProjectileFragment>();
		TArrayView<FMassProjectileVisualFragment> VisualList = Context.GetMutableFragmentView<FMassProjectileVisualFragment>();

		for (int32 i = 0; i < Context.GetNumEntities(); ++i)
		{
			FTransform& Transform = TransformList[i].GetMutableTransform();
			FMassProjectileFragment& Projectile = ProjectileList[i];
			FMassProjectileVisualFragment& Visual = VisualList[i];

			if (bSyncFromCDO && VisualManager)
			{
				if (const AProjectile* CDO = VisualManager->GetProjectileCDO(Projectile.ProjectileClass))
				{
					// Update runtime variables from CDO (Requirement 3)
					Projectile.RotationSpeed = CDO->RotationSpeed;
					Projectile.RotationOffset = CDO->RotationOffset;
					Visual.VisualRelativeTransform = CDO->ISMComponent ? CDO->ISMComponent->GetRelativeTransform() : FTransform::Identity;
					Projectile.bRotateMesh = CDO->RotateMesh;
					Projectile.bDisableAnyRotation = CDO->DisableAnyRotation;
					Projectile.MaxLifeTime = CDO->MaxLifeTime;
                    if (Projectile.Damage >= 0.f) Projectile.Damage = CDO->Damage;
                    Projectile.IsHealing = CDO->IsHealing;
                    Projectile.bContinueAfterTarget = CDO->bContinueAfterTarget;
                    Projectile.ArcHeightDistanceFactor = CDO->ArcHeightDistanceFactor;
                    Projectile.ArcHeight = CDO->ArcHeight;
					// Robust sync for Niagara
					Visual.Niagara_A_RelativeTransform = (CDO->Niagara_A) ? CDO->Niagara_A->GetRelativeTransform() : CDO->Niagara_A_Start_Transform;
					Visual.Niagara_B_RelativeTransform = (CDO->Niagara_B) ? CDO->Niagara_B->GetRelativeTransform() : CDO->Niagara_B_Start_Transform;
					// Speed is tricky as it might be from attributes. 
					// We only sync speed if it wasn't an override or if we want CDO changes to affect all.
					// Let's assume CDO changes should affect all for now, unless specific attribute logic is needed.
				}
			}

			Projectile.LifeTime += DeltaTime;

			if (Projectile.LifeTime >= Projectile.MaxLifeTime)
			{
				// Destroy projectile entity
				Context.Defer().DestroyEntity(Context.GetEntity(i));

				// Cleanup visuals
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

				continue;
			}

			if (Projectile.bIsHoming)
			{
				Projectile.bFollowTarget = true;
			}
			
			if (Projectile.LifeTime < DeltaTime && EntityManager.GetWorld())
			{
				if (EntityManager.GetWorld()->GetNetMode() == NM_Client && Projectile.TargetLocation.IsZero())
				{
					UE_LOG(LogTemp, Warning, TEXT("[CLIENT] Projectile %d started with ZERO TargetLocation!"), Context.GetEntity(i).Index);
				}
			}


			FVector CurrentLocation = Transform.GetLocation();
			FVector TargetLocation = Projectile.TargetLocation;

            if (LogThrottle % 60 == 0) {
                FString NetModeStr = (EntityManager.GetWorld()->GetNetMode() == NM_Client) ? TEXT("[CLIENT]") : TEXT("[SERVER]");
                UE_LOG(LogTemp, Verbose, TEXT("%s Proj %d: Loc=%s, Tgt=%s, Dir=%s, bCont=%d, Speed=%.1f"), 
                    *NetModeStr, Context.GetEntity(i).Index, *CurrentLocation.ToString(), *TargetLocation.ToString(), *Projectile.FlightDirection.ToString(), Projectile.bContinueAfterTarget, Projectile.Speed);
            }

			// UE_LOG(LogTemp, Warning, TEXT("Projectile %d: Speed %f, CurrentLoc %s, TargetLoc %s, DeltaTime %f"), i, Projectile.Speed, *CurrentLocation.ToString(), *TargetLocation.ToString(), DeltaTime);

			// If following target entity, update TargetLocation
			if (Projectile.bFollowTarget && Projectile.TargetEntity.IsValid())
			{
				if (EntityManager.IsEntityActive(Projectile.TargetEntity))
				{
					if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Projectile.TargetEntity))
					{
						TargetLocation = TargetTransform->GetTransform().GetLocation();
						Projectile.TargetLocation = TargetLocation;
					}
				}
				else
				{
					// Target is gone, stop following but keep the last known location
					Projectile.bFollowTarget = false;
					Projectile.TargetEntity = FMassEntityHandle();
				}
			}

			FVector NewLocation = CurrentLocation;

				if (Projectile.bIsHoming && Projectile.bFollowTarget)
				{
					const FVector DirToTarget = (TargetLocation - CurrentLocation).GetSafeNormal();
					const float CurrentAngle = Projectile.HomingInitialAngle + Projectile.LifeTime * Projectile.HomingRotationSpeed;

					float DesiredRadius = Projectile.HomingMaxSpiralRadius;
					const float DistanceToTarget = FVector::Dist(CurrentLocation, TargetLocation);

					// Shrink as we get closer than 500 units
					if (DistanceToTarget < 500.f)
					{
						// Aggressively shrink to 0 when we are close to the target (e.g. 100 units) to prevent circling
						DesiredRadius *= FMath::Clamp((DistanceToTarget - 150.f) / 350.f, 0.f, 1.f);
					}
					// Grow during the first 0.1 seconds of flight
					const float GrowthTime = 0.1f;
					if (Projectile.LifeTime < GrowthTime)
					{
						DesiredRadius *= (Projectile.LifeTime / GrowthTime);
					}

					FVector Right, Up;
					DirToTarget.FindBestAxisVectors(Right, Up);

					FVector NewOffset = (Right * FMath::Cos(FMath::DegreesToRadians(CurrentAngle)) +
									Up * FMath::Sin(FMath::DegreesToRadians(CurrentAngle))) * DesiredRadius;
					
					Projectile.HomingOffset = NewOffset;
				}

				if (Projectile.ArcHeight > 0.f || Projectile.ArcHeightDistanceFactor > 0.f)
				{
					// Arc Movement logic
					Projectile.ArcTravelTime += DeltaTime;
					float TotalDistance = FVector::Dist(Projectile.ArcStartLocation, Projectile.TargetLocation); // Use current target for arc distance calc
					if (TotalDistance > 0.f)
					{
                        const float EffectiveArcHeight = Projectile.ArcHeight + (TotalDistance * Projectile.ArcHeightDistanceFactor);
						float CurrentDist = (Projectile.Speed * 10.f) * Projectile.ArcTravelTime;
						float Alpha = CurrentDist / TotalDistance;
                        
                        if (Alpha >= 1.0f && Projectile.bContinueAfterTarget)
                        {
                            // Transition to Linear flight
                            Projectile.FlightDirection = (Projectile.TargetLocation - Projectile.ArcStartLocation).GetSafeNormal();
                            if (Projectile.FlightDirection.IsNearlyZero())
                            {
                                Projectile.FlightDirection = Transform.GetRotation().GetForwardVector();
                            }
                            Projectile.ArcHeight = 0.f;
                            Projectile.ArcHeightDistanceFactor = 0.f;
                            Projectile.bIsHoming = false; // Stop homing when switching to linear
                            
                            // Extrapolate linearly past target
                            NewLocation = FMath::Lerp(Projectile.ArcStartLocation, Projectile.TargetLocation, Alpha);
                        }
                        else
                        {
                            if (Alpha > 1.0f) Alpha = 1.0f;

						    NewLocation = FMath::Lerp(Projectile.ArcStartLocation, Projectile.TargetLocation, Alpha);
                            
						    float Height = 4.0f * EffectiveArcHeight * Alpha * (1.0f - Alpha);
						    NewLocation.Z += Height;

						    // Add Homing Spiral offset to the Arc position if enabled
						    if (Projectile.bIsHoming)
						    {
							    NewLocation += Projectile.HomingOffset;
						    }
                        }
					}
				}
			else
			{
				// Linear / Homing Movement logic
				FVector LocalFlightDirection;
				if (Projectile.bFollowTarget)
				{
					const FVector CurrentTarget = TargetLocation + Projectile.HomingOffset;
					FVector NewFlightDir = (CurrentTarget - CurrentLocation).GetSafeNormal();
                    
                    bool bStopFollowing = false;
                    if (Projectile.bContinueAfterTarget && !Projectile.TargetEntity.IsValid())
                    {
                        // Check if reached static target location (oscillation/flip prevention)
                        if (NewFlightDir.IsNearlyZero() || (NewFlightDir | Projectile.FlightDirection) < -0.5f)
                        {
                            bStopFollowing = true;
                        }
                    }

                    if (bStopFollowing)
                    {
                        Projectile.bFollowTarget = false;
                        Projectile.bIsHoming = false;
                        LocalFlightDirection = Projectile.FlightDirection;
                    }
                    else
                    {
                        LocalFlightDirection = NewFlightDir;
                        Projectile.FlightDirection = LocalFlightDirection;
                    }
					
					// Gradually reduce the offset if not pure homing
                    if (!Projectile.bIsHoming)
                    {
					    Projectile.HomingOffset = FMath::VInterpTo(Projectile.HomingOffset, FVector::ZeroVector, DeltaTime, Projectile.HomingInterpSpeed);
                    }
				}
				else
				{
					// Direct flight to target location - keep original direction to allow flying past target
					LocalFlightDirection = Projectile.FlightDirection;
                    if (LocalFlightDirection.IsNearlyZero())
                    {
                        LocalFlightDirection = (TargetLocation - CurrentLocation).GetSafeNormal();
                        if (LocalFlightDirection.IsNearlyZero())
                        {
                            LocalFlightDirection = Transform.GetRotation().GetForwardVector();
                        }
                        Projectile.FlightDirection = LocalFlightDirection;
                    }
				}

				// ROBUSTHEIT: Falls das Projektil mit Speed 0 existiert, nutze eine Notfall-Geschwindigkeit (mind. 100 Einheiten/s)
				const float EffectiveSpeed = FMath::Max(Projectile.Speed, 10.f); 
				float MoveDist = (EffectiveSpeed * 10.f) * DeltaTime;
				float DistToTarget = FVector::Dist(CurrentLocation, TargetLocation);

				if (MoveDist >= DistToTarget && !Projectile.bFollowTarget && !Projectile.bContinueAfterTarget)
				{
					NewLocation = TargetLocation;
				}
				else
				{
					NewLocation = CurrentLocation + Projectile.FlightDirection * MoveDist;
				}
			}

			// EnergyWall Impact Check
			if (Projectile.bHasWallImpact && Projectile.LifeTime < Projectile.MaxLifeTime)
			{
				FVector DirToWall = Projectile.WallImpactLocation - CurrentLocation;
				float DistToWall = DirToWall.Size();
				float MoveDistThisFrame = FVector::Dist(NewLocation, CurrentLocation);

				if (MoveDistThisFrame >= DistToWall)
				{
					NewLocation = Projectile.WallImpactLocation;
					Projectile.LifeTime = Projectile.MaxLifeTime;

					// Trigger impact on server
					if (EntityManager.GetWorld()->GetNetMode() < NM_Client)
					{
						UE_LOG(LogTemp, Warning, TEXT("[SERVER] Wall Impact Triggered for Proj %d at location %s"), Context.GetEntity(i).Index, *Projectile.WallImpactLocation.ToString());
						AActor* WallActor = Projectile.WallActor.Get();
						if (WallActor)
						{
							AActor* ShooterActor = nullptr;
							if (Projectile.ShooterEntity.IsValid())
							{
								if (FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Projectile.ShooterEntity))
								{
									ShooterActor = ActorFragment->GetMutable();
								}
							}

							if (ShooterActor)
							{
								if (AUnitBase* ShootingUnit = Cast<AUnitBase>(ShooterActor))
								{
									// If the wall is actually a unit, trigger normal impact (damage)
									if (AUnitBase* WallUnit = Cast<AUnitBase>(WallActor))
									{
										WallUnit->HandleProjectileImpact(ShooterActor, Projectile.WallImpactLocation, Projectile.ProjectileClass, Projectile.Damage);
									}
									else if (APerformanceUnit* PerfShooter = Cast<APerformanceUnit>(ShootingUnit))
									{
										// It's just a wall or other non-unit actor -> Trigger effects but NO DAMAGE
										if (const AProjectile* CDO = Cast<AProjectile>(Projectile.ProjectileClass->GetDefaultObject()))
										{
											const FVector FromLoc = ShootingUnit->GetMassActorLocation();
											FVector Dir = (Projectile.WallImpactLocation - FromLoc);
											Dir.Z = 0.f;
											FRotator FaceRot = Dir.IsNearlyZero() ? FRotator::ZeroRotator : Dir.Rotation();

											PerfShooter->FireEffectsAtLocation(CDO->ImpactVFX, CDO->ImpactSound, CDO->ScaleImpactVFX, CDO->ScaleImpactSound, Projectile.WallImpactLocation, 2.0f, FaceRot);
										}
									}
								}
							}
						}
					}
				}
			}

			// Update Rotation
			if (!Projectile.bDisableAnyRotation)
			{
				FVector MoveDir = (NewLocation - CurrentLocation).GetSafeNormal();
				float DistSqToTarget = FVector::DistSquared(NewLocation, TargetLocation);

				// Stop updating orientation at the very end to prevent weird flips or circling visuals
				if (DistSqToTarget > FMath::Square(100.f) && !MoveDir.IsNearlyZero())
				{
					// 1. Base rotation from movement direction (facing direction)
					FQuat BaseQuat = MoveDir.ToOrientationQuat();
					
					// 2. Apply static rotation offset from Projectile class
					FQuat FinalQuat = BaseQuat * Projectile.RotationOffset.Quaternion();

					// 3. Apply continuous Eigenrotation (RotationSpeed) over time, only if bRotateMesh is active
					if (Projectile.bRotateMesh)
					{
						// Calculate total rotation since start based on LifeTime to keep it smooth
						// Using FRotator directly to ensure same behavior as AProjectile::Tick
						FRotator TotalEigenRotation(
							Projectile.RotationSpeed.X * Projectile.LifeTime, 
							Projectile.RotationSpeed.Y * Projectile.LifeTime, 
							Projectile.RotationSpeed.Z * Projectile.LifeTime
						);
						FinalQuat = FinalQuat * TotalEigenRotation.Quaternion();
					}
					
					Transform.SetRotation(FinalQuat);
				}
			}
			Transform.SetLocation(NewLocation);

			// Update Niagara
			if (UNiagaraComponent* NC_A = Visual.Niagara_A.Get())
			{
				FTransform FinalNiagaraTransform = Visual.Niagara_A_RelativeTransform * Transform;
				NC_A->SetWorldTransform(FinalNiagaraTransform);
			}

			if (UNiagaraComponent* NC_B = Visual.Niagara_B.Get())
			{
				FTransform FinalNiagaraTransform = Visual.Niagara_B_RelativeTransform * Transform;
				NC_B->SetWorldTransform(FinalNiagaraTransform);
			}

			// Update ISM
			if (Visual.ISMComponent.IsValid() && Visual.InstanceIndex != INDEX_NONE)
			{
				// Auf dem Client stellen wir sicher, dass die Sichtbarkeit aktiv bleibt
				if (EntityManager.GetWorld() && EntityManager.GetWorld()->GetNetMode() == NM_Client)
				{
					if (!Visual.ISMComponent->IsVisible() || Visual.ISMComponent->bHiddenInGame)
					{
						Visual.ISMComponent->SetHiddenInGame(false);
						Visual.ISMComponent->SetVisibility(true);
					}
				}

				Visual.ISMComponent->UpdateInstanceTransform(Visual.InstanceIndex, Visual.VisualRelativeTransform * Transform, true, true, true);
			}
		}
	}));
}
