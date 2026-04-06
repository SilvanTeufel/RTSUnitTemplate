// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Projectile/MassRotateToMouseProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Controller/PlayerController/ExtendedControllerBase.h"
#include "Mass/MassActorBindingComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

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
	// Performance optimization: skip execution if no units require rotation
	if (EntityQuery.GetNumMatchingEntities(EntityManager) == 0)
	{
		return;
	}

	UWorld* World = EntityManager.GetWorld();
	if (!World) return;

	AExtendedControllerBase* LocalPC = Cast<AExtendedControllerBase>(World->GetFirstPlayerController());
	FVector CurrentMouseHit = FVector::ZeroVector;
	bool bIsLocalUpdateNeeded = false;

	if (LocalPC && LocalPC->IsLocalController())
	{
		FHitResult Hit;
		if (LocalPC->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			CurrentMouseHit = Hit.Location;
			bIsLocalUpdateNeeded = true;
			LocalPC->Server_UpdateMouseLocation(CurrentMouseHit);
		}
	}

	// Get local player ID for comparison
	int32 LocalPlayerId = (LocalPC && LocalPC->PlayerState) ? LocalPC->PlayerState->GetPlayerId() : -2;

	// Cache PlayerId to PC for server-side logic
	TMap<int32, AExtendedControllerBase*> PlayerIdToPC;
	if (World->GetNetMode() != NM_Client)
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

	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([&](FMassExecutionContext& ChunkContext)
	{
		const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
		auto Transforms = ChunkContext.GetMutableFragmentView<FTransformFragment>();
		auto MouseFrags = ChunkContext.GetMutableFragmentView<FMassRotateToMouseFragment>();
		auto CombatStats = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
		auto Actors = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		auto Characteristics = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			FVector TargetLocation = FVector::ZeroVector;
			bool bFoundTarget = false;

			// 1. If this unit belongs to our PlayerId, use our local mouse hit
			if (MouseFrags[i].PlayerId == LocalPlayerId && bIsLocalUpdateNeeded)
			{
				TargetLocation = CurrentMouseHit;
				bFoundTarget = true;
				// Update fragment so server/others see it
				MouseFrags[i].TargetLocation = TargetLocation;
			}
			else if (World->GetNetMode() != NM_Client)
			{
				// 2. On Server, if we found a PC for this unit's PlayerId, use its replicated mouse location
				if (AExtendedControllerBase** PCPtr = PlayerIdToPC.Find(MouseFrags[i].PlayerId))
				{
					TargetLocation = (*PCPtr)->ReplicatedMouseLocation;
					bFoundTarget = true;
					MouseFrags[i].TargetLocation = TargetLocation;
				}
			}

			// 3. Fallback to replicated fragment location
			if (!bFoundTarget)
			{
				TargetLocation = MouseFrags[i].TargetLocation;
				if (!TargetLocation.IsNearlyZero())
				{
					bFoundTarget = true;
				}
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

					FQuat NewQuat = FQuat::Slerp(CurrentQuat, TargetQuat, FMath::Clamp(DeltaTime * RotSpeed, 0.f, 1.f));
					MassTransform.SetRotation(NewQuat);

					Characteristics[i].PositionedTransform = MassTransform;
					Characteristics[i].bTransformDirty = true;
					if (AActor* Actor = Actors[i].GetMutable())
					{
						Actor->SetActorRotation(NewQuat);
					}
				}
			}
		}
	}));
}
