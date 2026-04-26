
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
	const float Yaw = (static_cast<float>(Item.YawQuantized) / 65535.0f) * 360.0f;
	FTransform Xf;
	Xf.SetLocation(Item.Location);
	Xf.SetRotation(FQuat(FRotator(0.f, Yaw, 0.f)));
	Xf.SetScale3D(Item.Scale);
	return Xf;
}

void FUnitReplicationItem::PostReplicatedAdd(const FUnitReplicationArray& InArraySerializer)
{
	if (InArraySerializer.OwnerBubble && InArraySerializer.OwnerBubble->GetNetMode() == NM_Client)
	{
		const FTransform Xf = BuildTransformFromItem(*this);
		UnitReplicationCache::SetLatest(NetID, Xf);

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
				// Projectile data is now retrieved locally from the actor in the spawn block below
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
				UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] PostReplicatedChange: NetID=%u, RawDelta=%d, Used=%d, Spawning=%d, PendingAfter=%d"), 
					NetID.GetValue(), RawDelta, UsedFromPending, UseDelta, PredictedPendingShots);
			}

			LastServerProjectileFireCounter = AIS_ProjectileFireCounter;
			// Clear client prediction latch/timer on any server fire counter change
			bPredictedLatch = false;
			PredictionTimer = 0.f;

			if (UseDelta > 0)
			{
				if (UWorld* World = InArraySerializer.OwnerBubble->GetWorld())
				{
					TSubclassOf<AProjectile> ProjectileClass = nullptr;
					float ProjectileSpeed = 0.f;
					FVector ProjectileSpawnOffset = FVector::ZeroVector;

					AUnitBase* MyActor = nullptr;
					if (URTSWorldCacheSubsystem* Cache = World->GetSubsystem<URTSWorldCacheSubsystem>())
					{
						if (UMassActorBindingComponent* MyBind = Cache->FindBindingByMassNetID(NetID.GetValue()))
						{
							MyActor = Cast<AUnitBase>(MyBind->GetOwner());
							if (MyActor)
							{
								ProjectileClass = MyActor->ProjectileBaseClass;

								if (MyActor->Attributes)
								{
									ProjectileSpeed = MyActor->Attributes->GetProjectileSpeed();
								}
								
								if (ProjectileSpeed <= 0.f && ProjectileClass)
								{
									if (const AProjectile* ProjCDO = Cast<AProjectile>(ProjectileClass->GetDefaultObject()))
									{
										ProjectileSpeed = ProjCDO->MovementSpeed;
									}
								}
							}
						}
					}

					if (!ProjectileClass)
					{
						UE_LOG(LogTemp, Warning, TEXT("[CLIENT] PostReplicatedChange: ProjectileClass could not be resolved locally for NetID=%u"), NetID.GetValue());
						return;
					}

					if (UProjectileVisualManager* VisualManager = World->GetSubsystem<UProjectileVisualManager>())
					{
						// Resolve target location
						FVector TargetLoc = AIS_ProjectileTargetLocation;
						if (TargetLoc.IsZero())
						{
							TargetLoc = AITargetLastKnownLocation;
						}

						// Startup/validation gates: skip visual spawn if unit target invalid
						if (TargetLoc.IsNearlyZero())
						{
							UE_LOG(LogTemp, Verbose, TEXT("[CLIENT] Skipping projectile spawn (NetID=%u): Invalid TargetLoc"), NetID.GetValue());
							return;
						}

						// Calculate spawn location (roughly from the shooter's position)
						FTransform SpawnXf = BuildTransformFromItem(*this);
						if (MyActor)
						{
							FVector LocalSpawnPos = MyActor->GetProjectileSpawnLocation();
							SpawnXf.SetLocation(LocalSpawnPos);
						}
						else
						{
							const FVector SpawnPos = SpawnXf.TransformPosition(ProjectileSpawnOffset);
							SpawnXf.SetLocation(SpawnPos);
						}

						// Re-orient SpawnXf towards TargetLoc for non-homing or initial direction
						FVector ShootDir = (TargetLoc - SpawnXf.GetLocation()).GetSafeNormal();
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

						const AProjectile* ProjCDO = VisualManager->GetProjectileCDO(ProjectileClass);

						for (int32 i = 0; i < UseDelta; ++i)
						{
							// Add some local variation for multi-shot projectiles so they don't perfectly overlap
							float LocalInitialAngle = 0.f;
							float LocalRotSpeed = ProjCDO ? ProjCDO->HomingRotationSpeed : 360.f;
							float LocalMaxRadius = ProjCDO ? ProjCDO->HomingMaxSpiralRadius : 0.f;
							float Interp = ProjCDO ? ProjCDO->HomingInterpSpeed : 2.f;
							float FinalSpeed = ProjectileSpeed;
							FTransform LocalSpawnXf = SpawnXf;
							FVector LocalTargetLoc = TargetLoc;

							const bool bIsFollowTarget = (ReplicationBits & UnitReplicationBits::AIS_bFollowTarget) != 0 || (ProjCDO && ProjCDO->FollowTarget);
							bool bFinalFollow = bIsFollowTarget;
							if (ProjCDO && ProjCDO->HomingMissleCount > 0) bFinalFollow = true; // FIX: Homing für Bubble-Projektile erzwingen

							if (UseDelta > 1 || bFinalFollow)
							{
								LocalInitialAngle += (i * (360.f / FMath::Max(1, (int32)UseDelta)));
								// Add a bit of random jitter
								LocalInitialAngle += FMath::RandRange(-10.f, 10.f);
								
								if (ProjCDO && ProjCDO->HomingMissleCount > 0)
								{
									// Variationen vom Server übernehmen
									LocalRotSpeed *= FMath::RandRange(0.9f, 1.4f);
									if (FMath::RandBool()) LocalRotSpeed *= -1.f;
									LocalMaxRadius *= FMath::RandRange(0.8f, 1.2f);
									FinalSpeed += FMath::RandRange(-ProjCDO->HomingSpeedVariation, ProjCDO->HomingSpeedVariation);
								}
								else
								{
									LocalRotSpeed *= FMath::RandRange(0.9f, 1.1f);
									LocalMaxRadius *= FMath::RandRange(0.9f, 1.1f);
								}

								if (ProjCDO && ProjCDO->TwinProjectileDistance >= 10.f && UseDelta > 1)
								{
									FVector RightVector = LocalSpawnXf.GetRotation().GetRightVector();
									// Verschiebe jedes zweite Projektil in die entgegengesetzte Richtung
									float OffsetMult = (i % 2 == 0) ? 1.f : -1.f;
									LocalSpawnXf.AddToTranslation(RightVector * ProjCDO->TwinProjectileDistance * OffsetMult);
								}
								else if (UseDelta > 1)
								{
									// Spatial jitter for bursts
									FVector Jitter = FMath::VRand() * 20.f;
									LocalSpawnXf.AddToTranslation(Jitter);
								}
							}

							// Local copies for capture (if deferred)
							int32 TeamId = (int32)CS_TeamId;
							FVector LocalScale = FVector::OneVector;

							// Direct call to VisualManager - it handles deferral internally now
							VisualManager->SpawnMassProjectile(
								ProjectileClass,
								LocalSpawnXf,
								MyActor, nullptr,
								LocalTargetLoc,
								FMassEntityHandle(),
								TargetHandle,
								FinalSpeed,
								TeamId,
								bFinalFollow, // FIX angewendet
								LocalInitialAngle,
								LocalRotSpeed,
								LocalMaxRadius,
								Interp,
								nullptr,
								LocalScale,
								-1.f, // FIX: Nutze CDO-Schaden
								-1    // FIX: Nutze CDO-MaxPiercedTargets
							);
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
	bAlwaysRelevant = false;
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
