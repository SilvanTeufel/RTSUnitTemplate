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
	bool bShouldLog = (LogThrottle % 60 == 0);
	bool bSyncFromCDO = (LogThrottle % 120 == 0); // Sync from CDO every 120 frames
	LogThrottle++;
	
	// Check if query is valid and has matching entities
	int32 NumEntities = EntityQuery.GetNumMatchingEntities(EntityManager);
	if (NumEntities > 0 && bShouldLog && EntityManager.GetWorld())
	{
		FString NetModeStr = (EntityManager.GetWorld()->GetNetMode() == NM_Client) ? TEXT("[CLIENT]") : TEXT("[SERVER]");
		UE_LOG(LogTemp, Warning, TEXT("%s UMassProjectileMovementProcessor::Execute - Matching Entities: %d"), *NetModeStr, NumEntities);
	}

	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([this, &EntityManager, bShouldLog, bSyncFromCDO](FMassExecutionContext& Context)
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
					// Robust sync for Niagara
					Visual.Niagara_A_RelativeTransform = (CDO->Niagara_A) ? CDO->Niagara_A->GetRelativeTransform() : CDO->Niagara_A_Start_Transform;
					Visual.Niagara_B_RelativeTransform = (CDO->Niagara_B) ? CDO->Niagara_B->GetRelativeTransform() : CDO->Niagara_B_Start_Transform;
					// Speed is tricky as it might be from attributes. 
					// We only sync speed if it wasn't an override or if we want CDO changes to affect all.
					// Let's assume CDO changes should affect all for now, unless specific attribute logic is needed.
				}
			}

			Projectile.LifeTime += DeltaTime;

			if (Projectile.bIsHoming)
			{
				Projectile.bFollowTarget = true;
			}
			
			if (bShouldLog && i == 0 && EntityManager.GetWorld())
			{
				FString NetModeStr = (EntityManager.GetWorld()->GetNetMode() == NM_Client) ? TEXT("[CLIENT]") : TEXT("[SERVER]");
				UE_LOG(LogTemp, Warning, TEXT("%s Projectile %d: LifeTime %f, Speed %f, ArcHeight %f, Pos %s, Target %s, bIsHoming: %d, bFollowTarget: %d"), 
					*NetModeStr, Context.GetEntity(i).Index, Projectile.LifeTime, Projectile.Speed, Projectile.ArcHeight, *Transform.GetLocation().ToString(), *Projectile.TargetLocation.ToString(), Projectile.bIsHoming, Projectile.bFollowTarget);
                
                if (Projectile.bIsHoming)
                {
                    UE_LOG(LogTemp, Warning, TEXT("%s Projectile %d: HomingRadius: %f, HomingRotSpeed: %f, CurrentOffset: %s"), 
                        *NetModeStr, Context.GetEntity(i).Index, Projectile.HomingMaxSpiralRadius, Projectile.HomingRotationSpeed, *Projectile.HomingOffset.ToString());
                }
                
                if (Visual.ISMComponent.IsValid())
                {
                    UE_LOG(LogTemp, Warning, TEXT("%s Projectile %d ISM Status: Visible: %d, Hidden: %d, Mesh: %s"), 
                        *NetModeStr, Context.GetEntity(i).Index, Visual.ISMComponent->IsVisible(), Visual.ISMComponent->bHiddenInGame, 
                        Visual.ISMComponent->GetStaticMesh() ? *Visual.ISMComponent->GetStaticMesh()->GetName() : TEXT("None"));
                }
                
                if (Visual.Niagara_A.IsValid())
                {
                    UE_LOG(LogTemp, Warning, TEXT("%s Projectile %d Niagara_A WorldPos: %s, Relative: %s"), 
                        *NetModeStr, Context.GetEntity(i).Index, *Visual.Niagara_A->GetComponentLocation().ToString(), *Visual.Niagara_A_RelativeTransform.GetLocation().ToString());
                }
                
                if (Visual.Niagara_B.IsValid())
                {
                    UE_LOG(LogTemp, Warning, TEXT("%s Projectile %d Niagara_B WorldPos: %s, Relative: %s"), 
                        *NetModeStr, Context.GetEntity(i).Index, *Visual.Niagara_B->GetComponentLocation().ToString(), *Visual.Niagara_B_RelativeTransform.GetLocation().ToString());
                }
			}

			if (Projectile.LifeTime >= Projectile.MaxLifeTime)
			{
				// Deactivate projectile
				Context.Defer().RemoveTag<FMassProjectileActiveTag>(Context.GetEntity(i));
				if (Visual.ISMComponent.IsValid())
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
				continue;
			}

			FVector CurrentLocation = Transform.GetLocation();
			FVector TargetLocation = Projectile.TargetLocation;

			// UE_LOG(LogTemp, Warning, TEXT("Projectile %d: Speed %f, CurrentLoc %s, TargetLoc %s, DeltaTime %f"), i, Projectile.Speed, *CurrentLocation.ToString(), *TargetLocation.ToString(), DeltaTime);

			// If following target entity, update TargetLocation
			if (Projectile.bFollowTarget && Projectile.TargetEntity.IsValid())
			{
				if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Projectile.TargetEntity))
				{
					TargetLocation = TargetTransform->GetTransform().GetLocation();
					Projectile.TargetLocation = TargetLocation;
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
						DesiredRadius *= (DistanceToTarget / 500.f);
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
					
					if (bShouldLog && i == 0)
					{
						FString NetModeStr = (EntityManager.GetWorld()->GetNetMode() == NM_Client) ? TEXT("[CLIENT]") : TEXT("[SERVER]");
						UE_LOG(LogTemp, Warning, TEXT("%s Homing Details: Radius=%f, Angle=%f, Offset=%s"), 
							*NetModeStr, DesiredRadius, CurrentAngle, *NewOffset.ToString());
					}
				}

				if (Projectile.ArcHeight > 0.f)
			{
				// Arc Movement logic
				Projectile.ArcTravelTime += DeltaTime;
				float TotalDistance = FVector::Dist(Projectile.ArcStartLocation, Projectile.TargetLocation); // Use current target for arc distance calc
				if (TotalDistance > 0.f)
                {
                    float CurrentDist = (Projectile.Speed * 10.f) * Projectile.ArcTravelTime;
                    float Alpha = FMath::Clamp(CurrentDist / TotalDistance, 0.f, 1.f);
                    
                    NewLocation = FMath::Lerp(Projectile.ArcStartLocation, Projectile.TargetLocation, Alpha);
                    float Height = 4.0f * Projectile.ArcHeight * Alpha * (1.0f - Alpha);
                    NewLocation.Z += Height;
                }
			}
			else
			{
				// Linear / Homing Movement logic
				FVector FlightDirection;
				if (Projectile.bFollowTarget && Projectile.bIsHoming)
				{
					// We already calculated HomingOffset above
					const FVector CurrentTarget = TargetLocation + Projectile.HomingOffset;
					FlightDirection = (CurrentTarget - CurrentLocation).GetSafeNormal();
				}
				else if (Projectile.bFollowTarget)
				{
					// Simple Homing with offset interpolation
					const FVector CurrentTarget = TargetLocation + Projectile.HomingOffset;
					FlightDirection = (CurrentTarget - CurrentLocation).GetSafeNormal();
					
					// Gradually reduce the offset
					Projectile.HomingOffset = FMath::VInterpTo(Projectile.HomingOffset, FVector::ZeroVector, DeltaTime, Projectile.HomingInterpSpeed);
				}
				else
				{
					// Direct flight to target location
					FlightDirection = (TargetLocation - CurrentLocation).GetSafeNormal();
				}

				float MoveDist = (Projectile.Speed * 10.f) * DeltaTime;
				float DistToTarget = FVector::Dist(CurrentLocation, TargetLocation);

				if (MoveDist >= DistToTarget && !Projectile.bFollowTarget)
				{
					NewLocation = TargetLocation;
				}
				else
				{
					NewLocation = CurrentLocation + FlightDirection * MoveDist;
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
										WallUnit->HandleProjectileImpact(ShooterActor, Projectile.WallImpactLocation, Projectile.ProjectileClass);
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
				if (!MoveDir.IsNearlyZero())
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

					if (bShouldLog && i == 0)
					{
						UE_LOG(LogTemp, Warning, TEXT("Projectile %d Rotation: %s, Speed: %f"), 
							Context.GetEntity(i).Index, *FinalQuat.Rotator().ToString(), Projectile.Speed);
					}
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
                if (bShouldLog && i == 0)
                {
                    bool bCompVisible = Visual.ISMComponent->IsVisible();
                    bool bCompHidden = Visual.ISMComponent->bHiddenInGame;
                    UE_LOG(LogTemp, Log, TEXT("Updating ISM %p Index %d. Visible: %d, Hidden: %d"), 
                        Visual.ISMComponent.Get(), Visual.InstanceIndex, bCompVisible, bCompHidden);
                }

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
