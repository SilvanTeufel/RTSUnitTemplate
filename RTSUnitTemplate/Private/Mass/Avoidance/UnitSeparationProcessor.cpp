// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Avoidance/UnitSeparationProcessor.h"

#include "DrawDebugHelpers.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Mass/UnitMassTag.h"
#include "NavigationSystem.h"

namespace
{
	struct FClumpUnitInfo
	{
		FMassEntityHandle Entity;
		FVector Location = FVector::ZeroVector;
		FVector Forward = FVector::ForwardVector;
		int32 TeamId = INDEX_NONE;
		float CapsuleRadius = 0.f;
		FMassEntityHandle Target;
		bool bUseWorkerStrength = false;
	};

	static FVector Horizontal(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.f);
	}
}

UUnitSeparationProcessor::UUnitSeparationProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UUnitSeparationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateStopSeparationTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
	
	//EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
	//EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitSeparationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun -= ExecutionInterval;

	TArray<FClumpUnitInfo> Units;
	Units.Reserve(256);

	auto GatherFromQuery = [this, &EntityManager, &Units, &Context](FMassExecutionContext& LocalContext)
	{
		const int32 Num = LocalContext.GetNumEntities();
		const auto Transforms = LocalContext.GetFragmentView<FTransformFragment>();
		const auto CombatStats = LocalContext.GetFragmentView<FMassCombatStatsFragment>();
		const auto Characs = LocalContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		const auto Targets = LocalContext.GetFragmentView<FMassAITargetFragment>();
		const auto MoveTargets = LocalContext.GetFragmentView<FMassMoveTargetFragment>();

		UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Context.GetWorld());

		for (int32 i = 0; i < Num; ++i)
		{
			FVector Location = Transforms[i].GetTransform().GetLocation();
			if (NavSystem)
			{
				FNavLocation NavLoc;
				if (!NavSystem->ProjectPointToNavigation(Location, NavLoc, FVector(100.f, 100.f, 300.f)))
				{
					continue;
				}
			}

			FClumpUnitInfo Info;
			Info.Entity = LocalContext.GetEntity(i);
			Info.Location = Location;
			
			const FVector& TargetPos = MoveTargets[i].Center;
			FVector MoveDir = Horizontal(TargetPos - Info.Location);
			if (MoveDir.IsNearlyZero())
			{
				Info.Forward = Transforms[i].GetTransform().GetRotation().GetForwardVector();
			}
			else
			{
				Info.Forward = MoveDir.GetSafeNormal();
			}

			Info.TeamId = CombatStats[i].TeamId;
			Info.CapsuleRadius = Characs[i].CapsuleRadius;
			Info.Target = Targets[i].TargetEntity;
			
			Info.bUseWorkerStrength = DoesEntityHaveTag(EntityManager, Info.Entity, FMassStateResourceExtractionTag::StaticStruct());

			/*DoesEntityHaveTag(EntityManager, Info.Entity, FMassStateGoToBaseTag::StaticStruct()) || 
								   DoesEntityHaveTag(EntityManager, Info.Entity, FMassStateResourceExtractionTag::StaticStruct()) ||
								   DoesEntityHaveTag(EntityManager, Info.Entity, FMassStateGoToResourceExtractionTag::StaticStruct());
			*/
			Units.Add(Info);
		}
	};

	EntityQuery.ForEachEntityChunk(Context, GatherFromQuery);
	
	if (Debug)
	{
		UWorld* World = Context.GetWorld();
		if (Units.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UUnitSeparationProcessor: Processing %d units"), Units.Num());
		}
		for (const auto& Info : Units)
		{
			FVector DebugLocation = Info.Location + FVector(0, 0, 5.f);
			DrawDebugCircle(World, DebugLocation, Info.CapsuleRadius, 16, FColor::Green, false, ExecutionInterval * 2.f, 0, 2.f, FVector(1, 0, 0), FVector(0, 1, 0));
		}
	}

	if (Units.Num() <= 1)
	{
		return;
	}

	TMap<FMassEntityHandle, FVector> AccumPush;
	AccumPush.Reserve(Units.Num());

	for (int32 a = 0; a < Units.Num(); ++a)
	{
		const FClumpUnitInfo& A = Units[a];
		for (int32 b = a + 1; b < Units.Num(); ++b)
		{
			const FClumpUnitInfo& B = Units[b];

			const bool bSameTeam = (A.TeamId == B.TeamId);

			if (bOnlySeparateWhenSameTarget && bSameTeam)
			{
				if (A.Target != B.Target || !A.Target.IsSet())
				{
					continue;
				}
			}

			const FVector Delta = Horizontal(B.Location - A.Location);
			const float DistSq = Delta.SizeSquared();
			const float DistanceMultiplier = bSameTeam ? DistanceMultiplierFriendly : DistanceMultiplierEnemy;
			const float Desired = FMath::Max(1.f, A.CapsuleRadius + B.CapsuleRadius) * DistanceMultiplier;

			if (MaxCheckRadius > 0.f && DistSq > FMath::Square(MaxCheckRadius))
			{
				continue;
			}

			if (DistSq < Desired * Desired)
			{
				const float Dist = FMath::Sqrt(FMath::Max(1.f, DistSq));
				const FVector DirAB = (Dist > KINDA_SMALL_NUMBER) ? (Delta / Dist) : FVector(1, 0, 0);
				const float Overlap = Desired - Dist;
				
				const float StrengthA = A.bUseWorkerStrength ? RepulsionStrengthWorker : (bSameTeam ? RepulsionStrengthFriendly : RepulsionStrengthEnemy);
				const float StrengthB = B.bUseWorkerStrength ? RepulsionStrengthWorker : (bSameTeam ? RepulsionStrengthFriendly : RepulsionStrengthEnemy);

				const FVector RightA = FVector(-A.Forward.Y, A.Forward.X, 0.f);
				float LateralAmountA = DirAB | RightA;
				if (FMath::Abs(LateralAmountA) < KINDA_SMALL_NUMBER)
				{
					LateralAmountA = (A.Entity.Index < B.Entity.Index) ? 1.0f : -1.0f;
				}
				FVector PushA = RightA * (-LateralAmountA * Overlap * StrengthA);

				const FVector RightB = FVector(-B.Forward.Y, B.Forward.X, 0.f);
				float LateralAmountB = DirAB | RightB;
				if (FMath::Abs(LateralAmountB) < KINDA_SMALL_NUMBER)
				{
					LateralAmountB = (B.Entity.Index < A.Entity.Index) ? 1.0f : -1.0f;
				}
				FVector PushB = RightB * (LateralAmountB * Overlap * StrengthB);

				AccumPush.FindOrAdd(A.Entity) += PushA;
				AccumPush.FindOrAdd(B.Entity) += PushB;

				if (Debug)
				{
					UWorld* World = Context.GetWorld();
					FVector LineStartA = A.Location + FVector(0, 0, 5.f);
					FVector LineStartB = B.Location + FVector(0, 0, 5.f);
					DrawDebugLine(World, LineStartA, LineStartA + PushA * 0.1f, FColor::Red, false, ExecutionInterval * 2.f, 0, 2.f);
					DrawDebugLine(World, LineStartB, LineStartB + PushB * 0.1f, FColor::Red, false, ExecutionInterval * 2.f, 0, 2.f);
				}
			}
		}
	}

	if (AccumPush.Num() == 0)
	{
		return;
	}

	auto ApplyToQuery = [&AccumPush](FMassExecutionContext& LocalContext)
	{
		const int32 Num = LocalContext.GetNumEntities();
		auto ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
		for (int32 i = 0; i < Num; ++i)
		{
			const FMassEntityHandle Entity = LocalContext.GetEntity(i);
			if (const FVector* Push = AccumPush.Find(Entity))
			{
				ForceList[i].Value += Horizontal(*Push);
			}
		}
	};

	EntityQuery.ForEachEntityChunk(Context, ApplyToQuery);
}
