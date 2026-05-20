// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Projectile/MassRotateToMouseProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Mass/MassActorBindingComponent.h"
#include "Characters/Unit/SpawnerUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Mass/Replication/RTSWorldCacheSubsystem.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"

UMassRotateToMouseProcessor::UMassRotateToMouseProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UMassRotateToMouseProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRotateToMouseFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassRotateToMouseProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	if (UWorld* World = EntityManager->GetWorld())
	{
		if (UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>())
		{
			SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateMouseLocation)
				.AddUObject(this, &UMassRotateToMouseProcessor::HandleMouseUpdateSignal);
		}
	}
}

void UMassRotateToMouseProcessor::HandleMouseUpdateSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
	// This can be used for event-driven updates, but the main logic is in Execute for smoothness
}

void UMassRotateToMouseProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (EntityQuery.GetNumMatchingEntities() == 0)
	{
		return;
	}

	UWorld* World = EntityManager.GetWorld();
	if (!World) return;

	AExtendedControllerBase* LocalPC = Cast<AExtendedControllerBase>(World->GetFirstPlayerController());
	const bool bHasAuthority = (World->GetNetMode() != NM_Client);
	
	FVector CurrentMouseHit = FVector::ZeroVector;
	bool bIsLocalUpdateNeeded = false;

	if (LocalPC && LocalPC->IsLocalController())
	{
		FHitResult Hit;
		if (LocalPC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			CurrentMouseHit = Hit.Location;
			bIsLocalUpdateNeeded = true;
			LocalPC->UpdateMouseLocationWithThrottling(CurrentMouseHit);
		}
	}

	// Get local player ID for comparison
	int32 LocalPlayerId = (LocalPC && LocalPC->PlayerState) ? LocalPC->PlayerState->GetPlayerId() : -2;

	// Cache PlayerId to PC for server-side logic
	TMap<int32, AExtendedControllerBase*> PlayerIdToPC;
	if (bHasAuthority)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AExtendedControllerBase* PC = Cast<AExtendedControllerBase>(It->Get()))
			{
				if (PC->PlayerState)
				{
					PlayerIdToPC.Add(PC->PlayerState->GetPlayerId(), PC);
				}
			}
		}
	}

	// Throttling for Server and Logs
	static float LastLogTime = 0.0f;
	static float LastServerTickTime = 0.0f;
	const float CurrentTime = World->GetTimeSeconds();
	const bool bShouldLog = (CurrentTime - LastLogTime > 1.0f);
	const bool bIsServerTick = (CurrentTime - LastServerTickTime > 0.05f);

	if (bShouldLog) LastLogTime = CurrentTime;
	if (bIsServerTick && bHasAuthority) LastServerTickTime = CurrentTime;

	EntityQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ChunkContext)
	{
		const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
		auto Transforms = ChunkContext.GetMutableFragmentView<FTransformFragment>();
		auto Actors = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		auto Characteristics = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			AActor* Actor = Actors[i].GetMutable();
			ASpawnerUnit* SpawnerUnit = Cast<ASpawnerUnit>(Actor);
			int32 RotatorId = SpawnerUnit ? SpawnerUnit->ActiveRotationPlayerId : -1;

			FVector TargetLocation = FVector::ZeroVector;
			bool bFoundTarget = false;

			// 1. Local Owner: Use immediate local mouse position
			if (RotatorId == LocalPlayerId && bIsLocalUpdateNeeded)
			{
				TargetLocation = CurrentMouseHit;
				bFoundTarget = true;
			}
			// 2. Server (Authority): Use replicated mouse location from Controller
			else if (bHasAuthority && bIsServerTick)
			{
				if (AExtendedControllerBase** PCPtr = PlayerIdToPC.Find(RotatorId))
				{
					TargetLocation = (*PCPtr)->ReplicatedMouseLocation;
					bFoundTarget = true;
				}
			}
			// 3. Remote Client: Use shared mouse data from BubbleInfo
			else if (!bHasAuthority && RotatorId != -1 && RotatorId != LocalPlayerId)
			{
				if (URTSWorldCacheSubsystem* CacheSub = World->GetSubsystem<URTSWorldCacheSubsystem>())
				{
					if (AUnitClientBubbleInfo* Bubble = CacheSub->GetBubble(false))
					{
						for (const FPlayerMouseData& Data : Bubble->PlayerMouseDatas)
						{
							if (Data.PlayerId == RotatorId)
							{
								TargetLocation = Data.MouseLocation;
								bFoundTarget = true;
								break;
							}
						}
					}
				}
			}

			if (bShouldLog && i == 0) // Throttled Log (Once per second, first unit in chunk)
			{
				UE_LOG(LogTemp, Log, TEXT("[%s] RTM - Unit: %s, Rotator: %d, Local: %d, Target: %s"), 
					(bHasAuthority ? TEXT("Server") : TEXT("Client")), 
					Actor ? *Actor->GetName() : TEXT("None"), 
					RotatorId, LocalPlayerId, *TargetLocation.ToString());
			}

			if (bFoundTarget)
			{
				FTransform& MassTransform = Transforms[i].GetMutableTransform();
				FVector Dir = TargetLocation - MassTransform.GetLocation();
				Dir.Z = 0.f;

				if (Dir.Normalize())
				{
					FQuat TargetQuat = Dir.ToOrientationQuat();
					FQuat CurrentQuat = MassTransform.GetRotation();
					float RotSpeed = Characteristics[i].RotationSpeed;
					if (RotSpeed <= 0.01f) { RotSpeed = 15.0f; }

					// Smooth rotation
					FQuat NewQuat = FQuat::Slerp(CurrentQuat, TargetQuat, FMath::Clamp(DeltaTime * RotSpeed, 0.f, 1.f));
					MassTransform.SetRotation(NewQuat);
					
					
					Characteristics[i].PositionedTransform.SetRotation(MassTransform.GetRotation());
					Characteristics[i].bTransformDirty = true;
					if (Actor)
					{
						Actor->SetActorRotation(MassTransform.GetRotation());
					}
					
				}
			}
		}
	}));
}
