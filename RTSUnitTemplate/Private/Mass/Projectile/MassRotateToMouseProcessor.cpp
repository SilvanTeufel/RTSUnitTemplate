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

UMassRotateToMouseProcessor::UMassRotateToMouseProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UMassRotateToMouseProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	UE_LOG(LogTemp, Log, TEXT("UMassRotateToMouseProcessor::ConfigureQueries - World: %s"), EntityManager->GetWorld() ? *EntityManager->GetWorld()->GetName() : TEXT("None"));
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRotateToMouseFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassRotateToMouseProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	UE_LOG(LogTemp, Log, TEXT("UMassRotateToMouseProcessor::InitializeInternal - World: %s"), EntityManager->GetWorld() ? *EntityManager->GetWorld()->GetName() : TEXT("None"));
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
	UWorld* World = EntityManager.GetWorld();
	if (!World) return;

	static float LastLogTime = 0.f;
	float CurrentTime = World->GetTimeSeconds();
	bool bShouldLog = (CurrentTime - LastLogTime) >= 1.0f;
	if (bShouldLog)
	{
		LastLogTime = CurrentTime;
		//int32 EntityCount = EntityQuery.CalculateEntityCount(EntityManager);
		UE_LOG(LogTemp, Log, TEXT("UMassRotateToMouseProcessor::Execute: Running..."));
	}

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
			
			if (bShouldLog)
			{
				UE_LOG(LogTemp, Log, TEXT("UMassRotateToMouseProcessor::Execute: Local Mouse Hit at %s"), *CurrentMouseHit.ToString());
			}
		}
	}
	else if (bShouldLog)
	{
		UE_LOG(LogTemp, Log, TEXT("UMassRotateToMouseProcessor::Execute: No LocalPC or not LocalController."));
	}

	// Cache Team to PC for server-side logic
	TMap<int32, AExtendedControllerBase*> TeamToPC;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AExtendedControllerBase* PC = Cast<AExtendedControllerBase>(It->Get()))
		{
			TeamToPC.Add(PC->SelectableTeamId, PC);
		}
	}

	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([&](FMassExecutionContext& ChunkContext)
	{
		if (bShouldLog)
		{
			UE_LOG(LogTemp, Log, TEXT("UMassRotateToMouseProcessor::Execute: Processing chunk with %d entities"), ChunkContext.GetNumEntities());
		}
		
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

			if (bIsLocalUpdateNeeded && CombatStats[i].TeamId == LocalPC->SelectableTeamId)
			{
				TargetLocation = CurrentMouseHit;
				bFoundTarget = true;
			}
			else if (AExtendedControllerBase** PCPtr = TeamToPC.Find(CombatStats[i].TeamId))
			{
				TargetLocation = (*PCPtr)->ReplicatedMouseLocation;
				bFoundTarget = true;
			}

			if (bFoundTarget)
			{
				MouseFrags[i].TargetLocation = TargetLocation;

				if (bShouldLog && i == 0)
				{
					UE_LOG(LogTemp, Log, TEXT("UMassRotateToMouseProcessor::Execute: Entity %d rotating to %s (RotationSpeed=%.2f)"), i, *TargetLocation.ToString(), Characteristics[i].RotationSpeed);
				}
				
				FTransform& MassTransform = Transforms[i].GetMutableTransform();
				FVector Dir = TargetLocation - MassTransform.GetLocation();
				Dir.Z = 0.f;

				if (Dir.Normalize())
				{
					FQuat TargetQuat = Dir.ToOrientationQuat();
					FQuat CurrentQuat = MassTransform.GetRotation();
					FQuat NewQuat = FQuat::Slerp(CurrentQuat, TargetQuat, FMath::Clamp(DeltaTime * Characteristics[i].RotationSpeed, 0.f, 1.f));
					MassTransform.SetRotation(NewQuat);

					Characteristics[i].PositionedTransform = MassTransform;
					Characteristics[i].bTransformDirty = true;
					if (AActor* Actor = Actors[i].GetMutable())
					{
						Actor->SetActorRotation(NewQuat);
					}
				}
				else if (bShouldLog && i == 0) // Log only for first entity in chunk if it fails to normalize
				{
					UE_LOG(LogTemp, Warning, TEXT("UMassRotateToMouseProcessor::Execute: Failed to normalize direction for entity %d"), i);
				}
			}
			else if (bShouldLog && i == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("UMassRotateToMouseProcessor::Execute: No target found for entity %d (TeamId=%d)"), i, CombatStats[i].TeamId);
			}
		}
	}));
}
