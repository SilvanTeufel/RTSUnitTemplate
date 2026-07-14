// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "MassEntitySubsystem.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "MassCommonFragments.h"
#include "Mass/Replication/UnitReplicationCacheSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Mass/Projectile/ProjectileVisualManager.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/MassActorBindingComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Actors/Projectile.h"

// 0=Off, 1=Warn, 2=Verbose
static TAutoConsoleVariable<int32> CVarRTS_Bubble_LogLevel(
	TEXT("net.RTS.Bubble.LogLevel"),
	0,
	TEXT("Logging level for UnitClientBubbleInfo: 0=Off, 1=Warn, 2=Verbose."),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarRTS_Bubble_NetUpdateHz(
	TEXT("net.RTS.Bubble.NetUpdateHz"),
	5.0f,
	TEXT("Replication frequency (Hz) for the unit bubble. Default 5.0f to save bandwidth."),
	ECVF_Default);

// Implementierung der Fast Array Item Callbacks
static FTransform BuildTransformFromItem(const FUnitReplicationItem& Item)
{
	const uint16 YQ = (uint16)(Item.PackedBits & 0xFFFF);
	const float Yaw = (static_cast<float>(YQ) / 65535.0f) * 360.0f;
	FTransform Xf;
	Xf.SetLocation(Item.Location);
	Xf.SetRotation(FQuat(FRotator(0.f, Yaw, 0.f)));
	Xf.SetScale3D(FVector::OneVector);
	return Xf;
}

void FUnitReplicationItem::PostReplicatedAdd(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		const FTransform Xf = BuildTransformFromItem(*this);
		UnitReplicationCache::SetLatest(NetID, Xf);

		// Synchronize fire counter to avoid spawning on join/initial replication
		const uint8 CurrentFireCounter = (uint8)((AuxData >> 16) & 0xFF);
		LastServerProjectileFireCounter = CurrentFireCounter;
		PredictedPendingShots = 0;
		// Reset client-only prediction state
		PredictionTimer = 0.f;
		bPredictedLatch = false;
	}
}

void FUnitReplicationItem::PostReplicatedChange(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		UnitReplicationCache::SetLatest(NetID, BuildTransformFromItem(*this));

		const uint8 CurrentFireCounter = (uint8)((AuxData >> 16) & 0xFF);

		// Check for projectile fire events
		if (CurrentFireCounter != LastServerProjectileFireCounter)
		{
			// Ring-safe delta for 8-bit counter
			uint8 RawDelta = (uint8)(CurrentFireCounter - LastServerProjectileFireCounter);
			
			// Capture prediction state BEFORE modification to avoid race/logic errors
			uint8 OldPending = PredictedPendingShots;
			bool bWasLatch = bPredictedLatch;

			// Consume pending predictions first
			uint8 UsedFromPending = FMath::Min(RawDelta, PredictedPendingShots);
			PredictedPendingShots -= UsedFromPending;
			uint8 UseDelta = RawDelta - UsedFromPending;

			// Safety clamp to avoid spawning storms
			const uint8 MaxBurst = 32;
			if (UseDelta > MaxBurst) UseDelta = MaxBurst;

			LastServerProjectileFireCounter = CurrentFireCounter;
			bPredictedLatch = false;

			// Only reset prediction timer if we didn't predict these shots (server is ahead)
			// This prevents the client from being 'reset' to 0 while its local timer is already ticking for the next shot
			if (UseDelta > 0)
			{
				PredictionTimer = 0.f;
			}

			if (UseDelta > 0)
			{
				uint8 StyleIdx = (uint8)((ReplicationBits & UnitReplicationBits::AIS_StyleIndexMask) >> UnitReplicationBits::AIS_StyleIndexShift);
				
				// STRICTOR FILTER: For AutoAttacks, we almost always trust the client's prediction.
				// Even if UsedFromPending was 0, if the client was NEAR a fire event (PredictionTimer high),
				// we skip the server spawn to avoid double fire due to slight sync differences.
				if (StyleIdx == 0)
				{
					if (UsedFromPending > 0 || bWasLatch)
					{
						if (UseDelta <= 1)
						{
							return;
						}
						UseDelta--;
					}
					else if (PredictionTimer > 0.05f) // If we are already deep into the next cycle
					{
						// Server shot arrived while we are already preparing the next one locally.
						// This might be the confirmation for a shot we haven't officially 'latched' yet.
						if (UseDelta <= 1)
						{
							return;
						}
						UseDelta--;
					}
				}

				if (UWorld* World = InArraySerializer.OwnerBubble->GetWorld())
				{
					if (UProjectileVisualManager* VisualManager = World->GetSubsystem<UProjectileVisualManager>())
					{
						// Resolve target location
						FVector TgtLoc = FVector::ZeroVector;
						if (TagBits & UnitReplicationBits::Slot_ActionIsProjectile) TgtLoc = FVector(ActionLoc);
						else if (!(TagBits & UnitReplicationBits::Slot_TargetIsMove)) TgtLoc = FVector(TargetLoc);

						// Startup/validation gates
						if (TgtLoc.IsNearlyZero())
						{
							// Fallback to TargetLoc if ActionLoc is missing for some reason
							if (!(TagBits & UnitReplicationBits::Slot_TargetIsMove)) TgtLoc = FVector(TargetLoc);
							
							if (TgtLoc.IsNearlyZero()) return;
						}

						// 1. Resolve Style/Parameters
						TSubclassOf<AProjectile> ProjectileClass_Resolved = nullptr;
						float ProjectileSpeed_Resolved = 0.f;
						FVector ProjectileScale_Resolved = FVector::OneVector;
						float ProjectileSpread_Resolved = 0.f;
						int32 ProjectileCount_Resolved = 1;
						int32 MaxPiercedTargets_Resolved = 1;
						bool bIsBouncingNext_Resolved = false;
						bool bIsBouncingBack_Resolved = false;
						float ZOffset_Resolved = 0.f;
						FVector ProjectileSpawnOffset_Resolved = FVector::ZeroVector;
						bool bDisableAutoZOffset_Resolved = false;
						float TwinProjectileDistance_Resolved = 0.f;
						float HomingInitialAngle_Base = 0.f;
						float HomingRotationSpeed_Base = 0.f;
						float HomingMaxSpiralRadius_Base = 0.f;
						float HomingInterpSpeed_Base = 0.f;
						bool bStyleFollowTarget = false;

						if (StyleIdx > 0 && InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->ProjectileStyleRegistry.IsValidIndex(StyleIdx - 1))
						{
							const FProjectileStyle& Style = InArraySerializer.OwnerBubble->ProjectileStyleRegistry[StyleIdx - 1];
							ProjectileClass_Resolved = Style.ProjectileClass;
							ProjectileSpeed_Resolved = Style.Speed;
							ProjectileScale_Resolved = Style.Scale;
							ProjectileSpread_Resolved = Style.Spread;
							ProjectileCount_Resolved = Style.ProjectileCount;
							MaxPiercedTargets_Resolved = Style.MaxPiercedTargets;
							bIsBouncingNext_Resolved = Style.IsBouncingNext;
							bIsBouncingBack_Resolved = Style.IsBouncingBack;
							ZOffset_Resolved = Style.ZOffset;
							ProjectileSpawnOffset_Resolved = Style.SpawnOffset;
							bDisableAutoZOffset_Resolved = Style.DisableAutoZOffset;
							TwinProjectileDistance_Resolved = Style.TwinProjectileDistance;
							HomingInitialAngle_Base = Style.HomingInitialAngle;
							HomingRotationSpeed_Base = Style.HomingRotationSpeed;
							HomingMaxSpiralRadius_Base = Style.HomingMaxSpiralRadius;
							HomingInterpSpeed_Base = Style.HomingInterpSpeed;
							bStyleFollowTarget = Style.bFollowTarget;
						}

						AUnitBase* MyActor = nullptr;
						uint8 TeamId_Resolved = 0;
						if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
						{
							if (UMassActorBindingComponent* MyBind = Cache->FindBindingByMassNetID(NetID.GetValue()))
							{
								MyActor = Cast<AUnitBase>(MyBind->GetOwner());
								if (MyActor)
								{
									TeamId_Resolved = MyActor->TeamId;
									if (!ProjectileClass_Resolved)
									{
										ProjectileClass_Resolved = MyActor->ProjectileBaseClass;
										ProjectileSpeed_Resolved = MyActor->Attributes ? MyActor->Attributes->GetProjectileSpeed() : 0.f;
										ProjectileScale_Resolved = MyActor->ProjectileScale;
										
										const AProjectile* DefaultProjCDO = ProjectileClass_Resolved ? Cast<AProjectile>(ProjectileClass_Resolved->GetDefaultObject()) : nullptr;
										if (DefaultProjCDO)
										{
											if (ProjectileSpeed_Resolved <= 0.f) ProjectileSpeed_Resolved = DefaultProjCDO->MovementSpeed;
											TwinProjectileDistance_Resolved = DefaultProjCDO->TwinProjectileDistance;
											ProjectileCount_Resolved = (DefaultProjCDO->HomingMissleCount > 0) ? DefaultProjCDO->HomingMissleCount : 1;
											MaxPiercedTargets_Resolved = DefaultProjCDO->MaxPiercedTargets;
											HomingRotationSpeed_Base = DefaultProjCDO->HomingRotationSpeed;
											HomingMaxSpiralRadius_Base = DefaultProjCDO->HomingMaxSpiralRadius;
											HomingInterpSpeed_Base = DefaultProjCDO->HomingInterpSpeed;
											bStyleFollowTarget = DefaultProjCDO->FollowTarget || (DefaultProjCDO->HomingMissleCount > 0);
										}
									}
								}
							}
						}

						if (!ProjectileClass_Resolved) return;

						// Calculate base spawn transform
						FTransform BaseSpawnXf = BuildTransformFromItem(*this);
						if (MyActor)
						{
							BaseSpawnXf.SetLocation(MyActor->GetProjectileSpawnLocation(ProjectileSpawnOffset_Resolved));
						}
						else
						{
							BaseSpawnXf.SetLocation(BaseSpawnXf.TransformPosition(ProjectileSpawnOffset_Resolved));
						}

						// Determine HalfHeight for Auto Z-Offset
						float HalfHeight = 0.f;
						const uint32 TgtID = (TagBits & UnitReplicationBits::Slot_ActionIsProjectile) ? ActionID : 0;
						
						bool bEffectiveFollowTarget = bStyleFollowTarget || (StyleIdx == 0 && (ReplicationBits & UnitReplicationBits::AIS_bFollowTarget) != 0);
						if (!bDisableAutoZOffset_Resolved && bEffectiveFollowTarget)
						{
							if (TgtID != 0)
							{
								if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
								{
									if (UMassActorBindingComponent* TargetBind = Cache->FindBindingByMassNetID(TgtID))
									{
										if (AActor* TargetActor = TargetBind->GetOwner())
											HalfHeight = TargetActor->GetComponentsBoundingBox().GetSize().Z * 0.5f;
									}
								}
							}
						}

						FVector FinalTargetCenter = TgtLoc;
						FinalTargetCenter.Z += HalfHeight + ZOffset_Resolved;

						// Resolve target entity handle
						FMassEntityHandle TargetHandle;
						if (TgtID != 0)
						{
							if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
							{
								if (UMassActorBindingComponent* TargetBind = Cache->FindBindingByMassNetID(TgtID))
									TargetHandle = TargetBind->GetEntityHandle();
							}
						}

						FMassEntityHandle ShooterHandle;
						if (MyActor)
						{
							if (UMassActorBindingComponent* MyBind = MyActor->FindComponentByClass<UMassActorBindingComponent>())
								ShooterHandle = MyBind->GetEntityHandle();
						}

						// Handle Twin Projectiles
						TArray<FVector> SpawnPositions;
						if (TwinProjectileDistance_Resolved >= 10.f)
						{
							FVector DirToTarget = (FinalTargetCenter - BaseSpawnXf.GetLocation()).GetSafeNormal2D();
							FVector RightVector = DirToTarget.IsNearlyZero() ? BaseSpawnXf.GetRotation().GetRightVector() : FVector::CrossProduct(FVector::UpVector, DirToTarget);
							FVector RightOffset = RightVector * TwinProjectileDistance_Resolved;
							SpawnPositions.Add(BaseSpawnXf.GetLocation() - RightOffset); // Left
							SpawnPositions.Add(BaseSpawnXf.GetLocation() + RightOffset); // Right
						}
						else
						{
							SpawnPositions.Add(BaseSpawnXf.GetLocation());
						}

						FTransform IndividualSpawnXf_Base = BaseSpawnXf;
						IndividualSpawnXf_Base.SetScale3D(ProjectileScale_Resolved);

						for (uint8 d = 0; d < UseDelta; ++d)
						{
							for (const FVector& ActualSpawnPos : SpawnPositions)
							{
								for (int32 i = 0; i < ProjectileCount_Resolved; ++i)
								{
									int32 MultiAngle = (i == 0) ? 0 : ((i & 1) ? 1 : -1);
									FVector ToCenterDir = (FinalTargetCenter - BaseSpawnXf.GetLocation()).GetSafeNormal();
									FVector PerpOffsetDir = FRotator(0.f, MultiAngle * 90.f, 0.f).RotateVector(ToCenterDir);
									
									float ActualSpread = ProjectileSpread_Resolved;
									const AProjectile* ProjCDO_Spread = VisualManager->GetProjectileCDO(ProjectileClass_Resolved);
									bool bIsHoming_Spread = (ProjCDO_Spread && ProjCDO_Spread->HomingMissleCount > 0);

									if (!bIsHoming_Spread)
									{
										if (i == 0) ActualSpread = 0.f;
										else if (i == 1 || i == 2) ActualSpread = ProjectileSpread_Resolved;
										else if (i == 3 || i == 4) ActualSpread = ProjectileSpread_Resolved / 2.0f;
										else if (i == 5 || i == 6) ActualSpread = ProjectileSpread_Resolved + (ProjectileSpread_Resolved / 2.0f);
										else ActualSpread = ProjectileSpread_Resolved + (i / 2) * (ProjectileSpread_Resolved / 2.0f);
									}
									else if (ProjectileCount_Resolved <= 1)
									{
										ActualSpread = 0.f;
									}
									
									FVector SpreadOffset = PerpOffsetDir * ActualSpread;
									FVector IndividualTargetLoc = FinalTargetCenter + SpreadOffset;

									FTransform IndividualSpawnXf = IndividualSpawnXf_Base;
									IndividualSpawnXf.SetLocation(ActualSpawnPos);

									FVector Dir = (IndividualTargetLoc - ActualSpawnPos).GetSafeNormal();
									
									// Homing missile initial direction variance
									const AProjectile* ProjCDO = VisualManager->GetProjectileCDO(ProjectileClass_Resolved);
									bool bHoming = (ProjCDO && ProjCDO->HomingMissleCount > 0);
									if (bHoming && ProjectileCount_Resolved > 1)
									{
										float Angle = (360.0f / ProjectileCount_Resolved) * i;
										FVector Right, Up;
										Dir.FindBestAxisVectors(Right, Up);
										Dir = (Dir + (Right * FMath::Cos(FMath::DegreesToRadians(Angle)) + Up * FMath::Sin(FMath::DegreesToRadians(Angle))) * 0.1f).GetSafeNormal();
									}

									IndividualSpawnXf.SetRotation(FQuat(Dir.Rotation() + (MyActor ? MyActor->ProjectileRotationOffset : FRotator(0.f, 90.f, -90.f))));

									float FinalSpeed = ProjectileSpeed_Resolved;
									float InitialAngle = HomingInitialAngle_Base;
									float RotSpeed = HomingRotationSpeed_Base;
									float MaxRadius = HomingMaxSpiralRadius_Base;

									if (bHoming)
									{
										FinalSpeed += FMath::RandRange(-ProjCDO->HomingSpeedVariation, ProjCDO->HomingSpeedVariation);
										FinalSpeed = FMath::Max(FinalSpeed, 100.f);
										InitialAngle = FMath::RandRange(0.f, 360.f);
										RotSpeed *= FMath::RandRange(0.9f, 1.4f);
										if (FMath::RandBool()) RotSpeed *= -1.f;
										MaxRadius *= FMath::RandRange(0.8f, 1.2f);
									}

									VisualManager->SpawnMassProjectile(
										ProjectileClass_Resolved,
										IndividualSpawnXf,
										MyActor,
										nullptr, // Target Actor (Visual only)
										IndividualTargetLoc,
										ShooterHandle,
										TargetHandle,
										FinalSpeed,
										TeamId_Resolved,
										bStyleFollowTarget || (StyleIdx == 0 && (ReplicationBits & UnitReplicationBits::AIS_bFollowTarget) != 0),
										InitialAngle,
										RotSpeed,
										MaxRadius,
										HomingInterpSpeed_Base,
										nullptr,
										IndividualSpawnXf.GetScale3D(),
										-1.f, // Damage (resolved by impact handler)
										MaxPiercedTargets_Resolved,
										false // bIsPredicted
									);
								}
							}
						}
					}
				}
			}
		}
	}
}

void FUnitReplicationItem::PreReplicatedRemove(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		UnitReplicationCache::Remove(NetID);
	}
}

namespace {
	struct FUnitReplicationCacheCleanup {
		FUnitReplicationCacheCleanup() {
			FWorldDelegates::OnWorldCleanup.AddStatic([](UWorld* World, bool, bool) {
				UnitReplicationCache::Clear();
			});
		}
	};
	static FUnitReplicationCacheCleanup GUnitReplicationCacheCleanup;
}

AUnitClientBubbleInfo::AUnitClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	// Aktiviere Replikation für diesen Actor
	bReplicates = true;
	//bAlwaysRelevant = true;
	//NetPriority = 1.0f;
	bAlwaysRelevant = false;
	NetPriority = 0.5f; 
	// Read desired replication rate (Hz) from CVAR; default 5
	float Hz = CVarRTS_Bubble_NetUpdateHz.GetValueOnGameThread();
	SetNetUpdateFrequency(Hz);

	// Setze Owner Pointer für Fast Array
	Agents.OwnerBubble = this;
}

void AUnitClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUnitClientBubbleInfo, Agents);
	DOREPLIFETIME(AUnitClientBubbleInfo, ProjectileStyleRegistry);
	DOREPLIFETIME(AUnitClientBubbleInfo, PlayerMouseDatas);
}

void AUnitClientBubbleInfo::OnRep_Agents()
{
	Agents.OwnerBubble = this;
	const int32 Level = CVarRTS_Bubble_LogLevel.GetValueOnGameThread();
	if (Level >= 1)
	{
		const int32 Count = Agents.Items.Num();
		//UE_LOG(LogTemp, Log, TEXT("[Bubble] OnRep_Agents: Items=%d (World=%s)"), Count, *GetWorld()->GetName());
		if (Level >= 2 && Count > 0)
		{
			const int32 MaxLog = FMath::Min(20, Count);
			FString IdList;
			for (int32 i = 0; i < MaxLog; ++i)
			{
				if (i > 0) IdList += TEXT(", ");
				IdList += FString::Printf(TEXT("%u"), Agents.Items[i].NetID.GetValue());
			}
			//UE_LOG(LogTemp, Log, TEXT("[Bubble] NetIDs[%d]: %s%s"), MaxLog, *IdList, (Count > MaxLog ? TEXT(" ...") : TEXT("")));
		}
	}
}

void AUnitClientBubbleInfo::BeginPlay()
{
	Super::BeginPlay();

	// Stelle sicher dass der Owner Pointer gesetzt ist
	Agents.OwnerBubble = this;

	const int32 Level = CVarRTS_Bubble_LogLevel.GetValueOnGameThread();
	if (Level >= 1)
	{
		const ENetMode Mode = GetNetMode();
		const TCHAR* ModeStr = (Mode == NM_DedicatedServer) ? TEXT("Server") : (Mode == NM_ListenServer ? TEXT("ListenServer") : (Mode == NM_Client ? TEXT("Client") : TEXT("Standalone")));
		//UE_LOG(LogTemp, Log, TEXT("[Bubble] BeginPlay in %s world %s, NetUpdateHz=%.1f"), ModeStr, *GetWorld()->GetName(), GetNetUpdateFrequency());
	}
}

#if WITH_EDITORONLY_DATA
void AUnitClientBubbleInfo::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

uint8 AUnitClientBubbleInfo::GetOrCreateStyleIndex(const FProjectileStyle& InStyle)
{
	if (!InStyle.ProjectileClass) return 0;

	for (int32 i = 0; i < ProjectileStyleRegistry.Num(); ++i)
	{
		if (ProjectileStyleRegistry[i].IsSameAs(InStyle))
		{
			return static_cast<uint8>(i + 1);
		}
	}

	if (ProjectileStyleRegistry.Num() < 127) // 7 bits used (25-31), allows indices 1-127. 0 is default.
	{
		int32 Idx = ProjectileStyleRegistry.Add(InStyle);
		return static_cast<uint8>(Idx + 1);
	}

	return 0;
}
