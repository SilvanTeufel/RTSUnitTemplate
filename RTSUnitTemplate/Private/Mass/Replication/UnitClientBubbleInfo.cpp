
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
	const float Pitch = (static_cast<float>(Item.PitchQuantized) / 65535.0f) * 360.0f;
	const float Yaw = (static_cast<float>(Item.YawQuantized) / 65535.0f) * 360.0f;
	const float Roll = (static_cast<float>(Item.RollQuantized) / 65535.0f) * 360.0f;
	FTransform Xf;
	Xf.SetLocation(Item.Location);
	Xf.SetRotation(FQuat(FRotator(Pitch, Yaw, Roll)));
	Xf.SetScale3D(Item.Scale);
	return Xf;
}

void FUnitReplicationItem::PostReplicatedAdd(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		const FTransform Xf = BuildTransformFromItem(*this);
		UnitReplicationCache::SetLatest(NetID, Xf);

		if (UWorld* World = InArraySerializer.OwnerBubble->GetWorld())
		{
			if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
			{
				if (UMassActorBindingComponent* MyBind = Cache->FindBindingByMassNetID(NetID.GetValue()))
				{
					if (AUnitBase* MyActor = Cast<AUnitBase>(MyBind->GetOwner()))
					{
						MyActor->ProjectileSpawnOffset = AIS_ProjectileSpawnOffset;
					}
				}
			}
		}

		// Synchronize fire counter to avoid spawning on join/initial replication
		LastServerProjectileFireCounter = AIS_ProjectileFireCounter;
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

		// Check for projectile fire events
		if (AIS_ProjectileFireCounter != LastServerProjectileFireCounter)
		{
			if (UWorld* World = InArraySerializer.OwnerBubble->GetWorld())
			{
				if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
				{
					if (UMassActorBindingComponent* MyBind = Cache->FindBindingByMassNetID(NetID.GetValue()))
					{
						if (AUnitBase* MyActor = Cast<AUnitBase>(MyBind->GetOwner()))
						{
							// Sync the replicated muzzle offset to the actor so predicted/multicast shots use the correct position
							MyActor->ProjectileSpawnOffset = AIS_ProjectileSpawnOffset;
						}
					}
				}
			}

			// Ring-safe delta for 8-bit counter
			uint8 RawDelta = (uint8)(AIS_ProjectileFireCounter - LastServerProjectileFireCounter);
			
			// Consume pending predictions first
			uint8 UsedFromPending = FMath::Min(RawDelta, PredictedPendingShots);
			PredictedPendingShots -= UsedFromPending;
			uint8 UseDelta = RawDelta - UsedFromPending;

			// Safety clamp to avoid spawning storms if counters desync heavily
			// At startup or after catch-up we intentionally only show the latest shot.
			const uint8 MaxBurst = 32;
			if (UseDelta > MaxBurst)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] PostReplicatedChange: NetID=%u had %d missed shots, spawning only latest (MaxBurst=%d)"), 
					NetID.GetValue(), UseDelta, MaxBurst);
				UseDelta = MaxBurst;
			}

			if (UseDelta > 0)
			{
				UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] PostReplicatedChange: NetID=%u, RawDelta=%d, Used=%d, Spawning=%d, PendingAfter=%d, Class=%s"), 
					NetID.GetValue(), RawDelta, UsedFromPending, UseDelta, PredictedPendingShots, 
					AIS_ProjectileClass ? *AIS_ProjectileClass->GetName() : TEXT("None"));
			}

			LastServerProjectileFireCounter = AIS_ProjectileFireCounter;
			// Clear client prediction latch/timer on any server fire counter change
			bPredictedLatch = false;
			PredictionTimer = 0.f;

			if (UseDelta > 0 && AIS_ProjectileClass)
			{
				if (UWorld* World = InArraySerializer.OwnerBubble->GetWorld())
				{
					if (UProjectileVisualManager* VisualManager = World->GetSubsystem<UProjectileVisualManager>())
					{
						// Resolve target location
						FVector TargetLoc = AIS_ProjectileTargetLocation;
 					if (TargetLoc.IsZero())
 					{
 						TargetLoc = AITargetLastKnownLocation;
 					}

 					// Startup/validation gates: skip visual spawn if unit not yet initialized or target invalid
 					if (!CS_IsInitialized)
 					{
 						UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] Skipping projectile spawn (NetID=%u): CS_IsInitialized=false"), NetID.GetValue());
 						return;
 					}
 					if (TargetLoc.IsNearlyZero())
 					{
 						UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] Skipping projectile spawn (NetID=%u): Invalid TargetLoc"), NetID.GetValue());
 						return;
 					}

						// Calculate spawn location (roughly from the shooter's position)
						FTransform SpawnXf = BuildTransformFromItem(*this);
						// Use replicated/predicted offset instead of hardcoded 50f forward, 100f up.
						const FVector SpawnPos = SpawnXf.GetLocation() + SpawnXf.TransformVector(AIS_ProjectileSpawnOffset);
						SpawnXf.SetLocation(SpawnPos);

						// Re-orient SpawnXf towards TargetLoc for non-homing or initial direction
						FVector ShootDir = (TargetLoc - SpawnPos).GetSafeNormal();
						if (!ShootDir.IsNearlyZero())
						{
							SpawnXf.SetRotation(FQuat(ShootDir.Rotation()));
						}

						// Resolve target entity handle if possible
						FMassEntityHandle TargetHandle;
						if (AIS_LastTargetNetID != 0)
						{
							if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
							{
								// Update cache if needed
								Cache->RebuildBindingCacheIfNeeded(0.5f);

								if (UMassActorBindingComponent* TargetBind = Cache->FindBindingByMassNetID(AIS_LastTargetNetID))
								{
									TargetHandle = TargetBind->GetEntityHandle();
								}
								else
								{
									UE_LOG(LogTemp, Warning, TEXT("[CLIENT] PostReplicatedChange: Could NOT resolve TargetNetID %u to EntityHandle!"), AIS_LastTargetNetID);
								}
							}
						}

						for (int32 i = 0; i < UseDelta; ++i)
						{
							// Add some local variation for multi-shot projectiles so they don't perfectly overlap
							float LocalInitialAngle = AIS_HomingInitialAngle;
							float LocalRotSpeed = AIS_HomingRotationSpeed;
							float LocalMaxRadius = AIS_HomingMaxSpiralRadius;
                            FTransform LocalSpawnXf = SpawnXf;
							FVector LocalTargetLoc = TargetLoc;

							if (UseDelta > 1 || AIS_bFollowTarget)
							{
								LocalInitialAngle += (i * (360.f / FMath::Max(1, (int32)UseDelta)));
								// Add a bit of random jitter
								LocalInitialAngle += FMath::RandRange(-10.f, 10.f);
								LocalRotSpeed *= FMath::RandRange(0.9f, 1.1f);
								LocalMaxRadius *= FMath::RandRange(0.9f, 1.1f);

                                if (UseDelta > 1)
                                {
                                    // Spatial jitter for bursts
                                    FVector Jitter = FMath::VRand() * 20.f;
                                    LocalSpawnXf.AddToTranslation(Jitter);

									// Apply spread to target location if multiple shots
									if (AIS_ProjectileSpread > 0.f)
									{
										const int32 MultiAngle = (i == 0) ? 0 : ((i & 1) ? 1 : -1);
										FVector SideDir = FVector::CrossProduct(FVector::UpVector, ShootDir).GetSafeNormal();
										LocalTargetLoc += SideDir * AIS_ProjectileSpread * MultiAngle;
									}
                                }
							}

							// Local copies for capture (if deferred)
							TSubclassOf<AProjectile> ProjectileClass = AIS_ProjectileClass;
							float Speed = AIS_ProjectileSpeed;
							int32 TeamId = CS_TeamId;
							bool bFollow = AIS_bFollowTarget;
							float Interp = AIS_HomingInterpSpeed;
							FVector LocalScale = AIS_ProjectileScale;

							// Direct call to VisualManager - it handles deferral internally now
							VisualManager->SpawnMassProjectile(
								ProjectileClass,
								LocalSpawnXf,
								nullptr, nullptr,
								LocalTargetLoc,
								FMassEntityHandle(),
								TargetHandle,
								Speed,
								TeamId,
								bFollow,
								LocalInitialAngle,
								LocalRotSpeed,
								LocalMaxRadius,
								Interp,
								nullptr,
								LocalScale,
								AIS_ProjectileDamage,
								AIS_ProjectileMaxPiercedTargets
							);
						}
					}
				}
			}
			else if (UseDelta > 0 && !AIS_ProjectileClass)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CLIENT] PostReplicatedChange: ProjectileClass is null for NetID=%u"), NetID.GetValue());
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
	bAlwaysRelevant = true;
	NetPriority = 0.5f; // Lower priority to allow important RPCs (like work area updates) to pass through first
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
