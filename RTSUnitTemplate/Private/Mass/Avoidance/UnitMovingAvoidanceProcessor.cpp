// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.


#include "Mass/Avoidance/UnitMovingAvoidanceProcessor.h"
#include "Avoidance/MassAvoidanceProcessors.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "DrawDebugHelpers.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "VisualLogger/VisualLogger.h"
#include "Math/Vector2D.h"
#include "Logging/LogMacros.h"
#include "MassSimulationLOD.h"
#include "MassCommonFragments.h"
#include "MassDebugLogging.h"
#include "MassMovementFragments.h"
#include "MassNavigationSubsystem.h"
#include "MassNavigationFragments.h"
#include "MassNavigationUtils.h"
#include "Engine/World.h"
#include "MassDebugger.h"
#include "MassNavigationDebug.h"

#include "MassLODFragments.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "Mass/UnitMassTag.h"

namespace UE::UnitMassAvoidance
{
	namespace Tweakables
	{
		bool bEnableEnvironmentAvoidance = true;
		bool bEnableSettingsForExtendingColliders = true;
		bool bUseAdjacentCorridors = true;
		bool bUseDrawDebugHelpers = false;
		bool bEnableDetailedDebug = false;
	} // Tweakables

	FAutoConsoleVariableRef Vars[] = 
	{
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.EnableEnvironmentAvoidance"), Tweakables::bEnableEnvironmentAvoidance, TEXT("Set to false to disable avoidance forces for environment (for debug purposes)."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.EnableSettingsForExtendingColliders"), Tweakables::bEnableSettingsForExtendingColliders, TEXT("Set to false to disable using different settings for extending obstacles (for debug purposes)."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.UseAdjacentCorridors"), Tweakables::bUseAdjacentCorridors, TEXT("Set to false to disable usage of adjacent lane width."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.UseDrawDebugHelpers"), Tweakables::bUseDrawDebugHelpers, TEXT("Use debug draw helpers in addition to visual logs."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.EnableDetailedDebug"), Tweakables::bEnableDetailedDebug, TEXT("Display additional avoidance debug information."), ECVF_Cheat)
	};

	constexpr int32 MaxExpectedAgentsPerCell = 6;
	constexpr int32 MinTouchingCellCount = 4;
	constexpr int32 MaxObstacleResults = MaxExpectedAgentsPerCell * MinTouchingCellCount;

	static void FindCloseObstacles(const FVector& Center, const FVector::FReal SearchRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
									TArray<FMassNavigationObstacleItem, TFixedAllocator<MaxObstacleResults>>& OutCloseEntities, const int32 MaxResults)
	{
		OutCloseEntities.Reset();
		const FVector Extent(SearchRadius, SearchRadius, 0.);
		const FBox QueryBox = FBox(Center - Extent, Center + Extent);

		struct FSortingCell
		{
			int32 X;
			int32 Y;
			int32 Level;
			FVector::FReal SqDist;
		};
		TArray<FSortingCell, TInlineAllocator<64>> Cells;
		const FVector QueryCenter = QueryBox.GetCenter();
		
		for (int32 Level = 0; Level < AvoidanceObstacleGrid.NumLevels; Level++)
		{
			const FVector::FReal CellSize = AvoidanceObstacleGrid.GetCellSize(Level);
			const FNavigationObstacleHashGrid2D::FCellRect Rect = AvoidanceObstacleGrid.CalcQueryBounds(QueryBox, Level);
			for (int32 Y = Rect.MinY; Y <= Rect.MaxY; Y++)
			{
				for (int32 X = Rect.MinX; X <= Rect.MaxX; X++)
				{
					const FVector::FReal CenterX = (X + 0.5) * CellSize;
					const FVector::FReal CenterY = (Y + 0.5) * CellSize;
					const FVector::FReal DX = CenterX - QueryCenter.X;
					const FVector::FReal DY = CenterY - QueryCenter.Y;
					const FVector::FReal SqDist = DX * DX + DY * DY;
					FSortingCell SortCell;
					SortCell.X = X;
					SortCell.Y = Y;
					SortCell.Level = Level;
					SortCell.SqDist = SqDist;
					Cells.Add(SortCell);
				}
			}
		}

		Cells.Sort([](const FSortingCell& A, const FSortingCell& B) { return A.SqDist < B.SqDist; });

		for (const FSortingCell& SortedCell : Cells)
		{
			if (const FNavigationObstacleHashGrid2D::FCell* Cell = AvoidanceObstacleGrid.FindCell(SortedCell.X, SortedCell.Y, SortedCell.Level))
			{
				const TSparseArray<FNavigationObstacleHashGrid2D::FItem>&  Items = AvoidanceObstacleGrid.GetItems();
				for (int32 Idx = Cell->First; Idx != INDEX_NONE; Idx = Items[Idx].Next)
				{
					OutCloseEntities.Add(Items[Idx].ID);
					if (OutCloseEntities.Num() >= MaxResults)
					{
						return;
					}
				}
			}
		}
	}

	// Adapted from ray-capsule intersection: https://iquilezles.org/www/articles/intersectors/intersectors.htm
	static FVector::FReal ComputeClosestPointOfApproach(const FVector2D Pos, const FVector2D Vel, const FVector::FReal Rad, const FVector2D SegStart, const FVector2D SegEnd, const FVector::FReal TimeHoriz)
	{
		const FVector2D SegDir = SegEnd - SegStart;
		const FVector2D RelPos = Pos - SegStart;
		const FVector::FReal VelSq = FVector2D::DotProduct(Vel, Vel);
		const FVector::FReal SegDirSq = FVector2D::DotProduct(SegDir, SegDir);
		const FVector::FReal DirVelSq = FVector2D::DotProduct(SegDir, Vel);
		const FVector::FReal DirRelPosSq = FVector2D::DotProduct(SegDir, RelPos);
		const FVector::FReal VelRelPosSq = FVector2D::DotProduct(Vel, RelPos);
		const FVector::FReal RelPosSq = FVector2D::DotProduct(RelPos, RelPos);
		const FVector::FReal A = SegDirSq * VelSq - DirVelSq * DirVelSq;
		const FVector::FReal B = SegDirSq * VelRelPosSq - DirRelPosSq * DirVelSq;
		const FVector::FReal C = SegDirSq * RelPosSq - DirRelPosSq * DirRelPosSq - FMath::Square(Rad) * SegDirSq;
		const FVector::FReal H = FMath::Max(0., B*B - A*C); // b^2 - ac, Using max for closest point of arrival result when no hit.
		const FVector::FReal T = FMath::Abs(A) > SMALL_NUMBER ? (-B - FMath::Sqrt(H)) / A : 0.;
		const FVector::FReal Y = DirRelPosSq + T * DirVelSq;
		
		if (Y > 0. && Y < SegDirSq) 
		{
			return FMath::Clamp(T, 0., TimeHoriz);
		}
		else 
		{
			// caps
			const FVector2D CapRelPos = (Y <= 0.) ? RelPos : Pos - SegEnd;
			const FVector::FReal Cb = FVector2D::DotProduct(Vel, CapRelPos);
			const FVector::FReal Cc = FVector2D::DotProduct(CapRelPos, CapRelPos) - FMath::Square(Rad);
			const FVector::FReal Ch = FMath::Max(0., Cb * Cb - VelSq * Cc);
			const FVector::FReal T1 = VelSq > SMALL_NUMBER ? (-Cb - FMath::Sqrt(Ch)) / VelSq : 0.;
			return FMath::Clamp(T1, 0., TimeHoriz);
		}
	}

	static FVector::FReal ComputeClosestPointOfApproach(const FVector RelPos, const FVector RelVel, const FVector::FReal TotalRadius, const FVector::FReal TimeHoriz)
	{
		// Calculate time of impact based on relative agent positions and velocities.
		const FVector::FReal A = FVector::DotProduct(RelVel, RelVel);
		const FVector::FReal Inv2A = A > SMALL_NUMBER ? 1. / (2. * A) : 0.;
		const FVector::FReal B = FMath::Min(0., 2. * FVector::DotProduct(RelVel, RelPos));
		const FVector::FReal C = FVector::DotProduct(RelPos, RelPos) - FMath::Square(TotalRadius);
		// Using max() here gives us CPA (closest point on arrival) when there is no hit.
		const FVector::FReal Discr = FMath::Sqrt(FMath::Max(0., B * B - 4. * A * C));
		const FVector::FReal T = (-B - Discr) * Inv2A;
		return FMath::Clamp(T, 0., TimeHoriz);
	}

#if WITH_MASSGAMEPLAY_DEBUG
	using namespace UE::MassNavigation::Debug;
	
	// Colors
	static constexpr FColor Amber(255,179,0);
	static constexpr FColor Orange(251,140,0);
	static constexpr FColor OrangeRed(244,81,30);
	static constexpr FColor Cyan(0,172,193);
	static constexpr FColor Blue(2,155,229);
	static constexpr FColor Indigo(57,73,171);
	static constexpr FColor Yellow(253, 216, 53);
	static constexpr FColor Teal(0, 137, 123);
	static constexpr FColor Lime(172, 222, 51);

	static constexpr FColor CurrentAgentColor = Lime;
	static const	 FColor VelocityColor = FColor::Black;
	static constexpr FColor DesiredVelocityColor = Yellow;
	static constexpr FColor FinalSteeringForceColor = Teal;

	// Agents colors
	static constexpr FColor AgentsColor = Amber;
	static constexpr FColor AgentAvoidForceColor = Orange;
	static constexpr FColor AgentSeparationForceColor = OrangeRed;
	
	// Obstacles colors
	static constexpr FColor ObstacleColor = Cyan;	// edges
	static constexpr FColor ObstacleAvoidForceColor = Blue;
	static constexpr FColor ObstacleSeparationForceColor = Indigo;
	static const	FColor ObstacleContactNormalColor = FColor::Silver;

	// Ghost colors
	static const FColor GhostColor = FColor::Silver;
	static const FColor GhostSteeringForceColor = FColor::Silver;

	// Forces thickness
	static constexpr float AvoidThickness = 3.f;
	static constexpr float SeparationThickness = 3.f;
	static constexpr float SummedForcesThickness = 5.f;

	// Output force
	static constexpr float SteeringThickness = 8.f;
	static constexpr float SteeringArrowHeadSize = 12.f;

	// Height offsets
	static const FVector DebugAgentHeightOffset = FVector(0., 0., 175.);
	static const FVector DebugInputForceHeight = FVector(0., 0., 181.);
	static const FVector DebugAgentAvoidHeightOffset = FVector(0., 0., 182.);
	static const FVector DebugAgentSeparationHeightOffset = FVector(0., 0., 183.);
	static const FVector DebugOutputForcesHeight = FVector(0., 0., 184.);
	static const FVector DebugLowCylinderOffset = FVector(0., 0., 20.);
	
	// Local debug utils
	static void DebugDrawVelocity(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color)
	{
		// Different arrow than DebugDrawArrow()
		if (!Context.ShouldLogEntity())
		{
			return;
		}

		constexpr float Thickness = 3.f;
		constexpr FVector::FReal Pointyness = 1.8;
		const FVector Line = End - Start;
		const FVector UnitV = Line.GetSafeNormal();
		const FVector Perp = FVector::CrossProduct(UnitV, FVector::UpVector);
		const FVector Left = Perp - (Pointyness * UnitV);
		const FVector Right = -Perp - (Pointyness * UnitV);
		const FVector::FReal HeadSize = 0.08 * Line.Size();
		const UObject* LogOwner = Context.GetLogOwner(); 
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, Start, End, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, End, End + HeadSize * Left, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, End, End + HeadSize * Right, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(LogOwner, Context.Category, Log, End + HeadSize * Left, End + HeadSize * Right, Color, (int16)Thickness, TEXT(""));

		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugLine(Context.World, Start, End, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Left, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Right, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End + HeadSize * Left, End + HeadSize * Right, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
		}
	}

	static void DebugDrawForce(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color, const float Thickness)
	{
		DebugDrawArrow(Context, Start, End, Color, /*HeadSize*/4.f, Thickness);
	}

	static void DebugDrawSummedForce(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color)
	{
		DebugDrawArrow(Context, Start + FVector(0.,0.,1.), End + FVector(0., 0., 1.), Color, /*HeadSize*/8.f, SummedForcesThickness);
	}

#endif // WITH_MASSGAMEPLAY_DEBUG

} // namespace UE::MassAvoidance



UUnitMovingAvoidanceProcessor::UUnitMovingAvoidanceProcessor(): EntityQuery()
{
	// Execute in the same group as the old MassMovingAvoidanceProcessor
	ExecutionOrder.ExecuteInGroup = FName("Avoidance");

	// Execute after LOD and UnitMovementProcessor
	ExecutionOrder.ExecuteAfter.Add(FName("LOD"));
	ExecutionOrder.ExecuteAfter.Add(FName("UnitMovementProcessor"));

	// No need to execute before anything
	ExecutionOrder.ExecuteBefore.Empty();

	// Processing phase matches the original
	ProcessingPhase = EMassProcessingPhase::PrePhysics;

	// Make sure the processor is auto-registered
	bAutoRegisterWithProcessingPhases = true;

	// Runs on Server + Client + Standalone (same as default)
	ExecutionFlags = static_cast<uint8>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);

	// Does not require Game Thread
	bRequiresGameThreadExecution = true;
}

void UUnitMovingAvoidanceProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);

	// Call parent to set up all the standard requirements

	// Super::ConfigureQueries(EntityManager);
	// --- Copied from original UMassMovingAvoidanceProcessor ---
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavigationEdgesFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAvoidanceEntitiesToIgnoreFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddConstSharedRequirement<FMassMovingAvoidanceParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	// --- End of copy ---

#if WITH_MASSGAMEPLAY_DEBUG
	EntityQuery.DebugEnableEntityOwnerLogging();
#endif

	// ***** YOUR CUSTOMIZATION GOES HERE *****
	EntityQuery.AddTagRequirement<FMassDisableAvoidanceTag>(EMassFragmentPresence::None);
	
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitMovingAvoidanceProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	// --- Copied from original ---
	Super::InitializeInternal(Owner, EntityManager);

	World = Owner.GetWorld();
	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

void UUnitMovingAvoidanceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
QUICK_SCOPE_CYCLE_COUNTER(UMassMovingAvoidanceProcessor);

	if (!World || !NavigationSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager](FMassExecutionContext& Context)
	{
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		const double CurrentTime = World->GetTimeSeconds();
		
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
		const TConstArrayView<FMassNavigationEdgesFragment> NavEdgesList = Context.GetFragmentView<FMassNavigationEdgesFragment>();
		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassAvoidanceEntitiesToIgnoreFragment> EntitiesToIgnoreList = Context.GetFragmentView<FMassAvoidanceEntitiesToIgnoreFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
		const FMassMovingAvoidanceParameters& MovingAvoidanceParams = Context.GetConstSharedFragment<FMassMovingAvoidanceParameters>();
		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();

		const FVector::FReal InvPredictiveAvoidanceTime = 1. / MovingAvoidanceParams.PredictiveAvoidanceTime;

		// Arrays used to store close obstacles
		TArray<FMassNavigationObstacleItem, TFixedAllocator<UE::UnitMassAvoidance::MaxObstacleResults>> CloseEntities;

		// Used for storing sorted list or nearest obstacles.
		struct FSortedObstacle
		{
			FVector LocationCached;
			FVector Forward;
			FMassNavigationObstacleItem ObstacleItem;
			FVector::FReal SqDist;
		};
		TArray<FSortedObstacle, TFixedAllocator<UE::UnitMassAvoidance::MaxObstacleResults>> ClosestObstacles;

		// Potential contact between agent and environment. 
		struct FEnvironmentContact
		{
			FVector Position = FVector::ZeroVector;
			FVector Normal = FVector::ZeroVector;
			FVector::FReal Distance = 0.;				// Distance from agent center to contact position
			bool bAlongTheEdge = false;
			bool bBehindTheEdge = false;
		};
		TArray<FEnvironmentContact, TInlineAllocator<16>> Contacts;

		// Describes collider to avoid, collected from neighbour obstacles.
		struct FCollider
		{
			FVector Location = FVector::ZeroVector;
			FVector Velocity = FVector::ZeroVector;
			float Radius = 0.f;
			bool bCanAvoid = true;
			bool bIsMoving = false;
		};
		TArray<FCollider, TInlineAllocator<16>> Colliders;

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			// @todo: this check should eventually be part of the query (i.e. only handle moving agents).
			const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIt];
			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Animate || MoveTarget.GetCurrentAction() == EMassMovementAction::Stand)
			{
				continue;
			}

			FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			const FMassNavigationEdgesFragment& NavEdges = NavEdgesList[EntityIt];
			const FTransformFragment& Location = LocationList[EntityIt];
			const FMassVelocityFragment& Velocity = VelocityList[EntityIt];
			const FAgentRadiusFragment& RadiusFragment = RadiusList[EntityIt];
			
			TConstArrayView<FMassEntityHandle> EntitiesToIgnore =
				EntitiesToIgnoreList.IsEmpty() ?
				TConstArrayView<FMassEntityHandle>() :
					EntitiesToIgnoreList[EntityIt].EntitiesToIgnore;
			
			FMassForceFragment& Force = ForceList[EntityIt];

			// Smaller steering max accel makes the steering more "calm" but less opportunistic, may not find solution, or gets stuck.
			// Max contact accel should be quite a big bigger than steering so that collision response is firm. 
			const FVector::FReal MaxSteerAccel = MovementParams.MaxAcceleration;
			const FVector::FReal MaximumSpeed = MovementParams.MaxSpeed;

			const FVector AgentLocation = Location.GetTransform().GetTranslation();
			const FVector AgentVelocity = FVector(Velocity.Value.X, Velocity.Value.Y, 0.);
			
			const FVector::FReal AgentRadius = RadiusFragment.Radius;
			
			FVector SteeringForce = Force.Value;

			// Near start and end fades are used to subdue the avoidance at the start and end of the path.
			FVector::FReal NearStartFade = 1.;
			FVector::FReal NearEndFade = 1.;

			if (MoveTarget.GetPreviousAction() != EMassMovementAction::Move)
			{
				// Fade in avoidance when transitioning from other than move action.
				// I.e. the standing behavior may move the agents so close to each,
				// and that causes the separation to push them out quickly when avoidance is activated. 
				NearStartFade = FMath::Min((CurrentTime - MoveTarget.GetCurrentActionStartTime()) / MovingAvoidanceParams.StartOfPathDuration, 1.);
			}

			if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand)
			{
				// Estimate approach based on current desired speed.
				const FVector::FReal ApproachDistance = FMath::Max<FVector::FReal>(1., MovingAvoidanceParams.EndOfPathDuration * MoveTarget.DesiredSpeed.Get());
				NearEndFade = FMath::Clamp(MoveTarget.DistanceToGoal / ApproachDistance, 0., 1.);
			}
			
			const FVector::FReal NearStartScaling = FMath::Lerp<FVector::FReal>(MovingAvoidanceParams.StartOfPathAvoidanceScale, 1., NearStartFade);
			const FVector::FReal NearEndScaling = FMath::Lerp<FVector::FReal>(MovingAvoidanceParams.EndOfPathAvoidanceScale, 1., NearEndFade);
			
#if WITH_MASSGAMEPLAY_DEBUG
			const UE::UnitMassAvoidance::FDebugContext BaseDebugContext(Context, this, LogAvoidance, World, Entity, EntityIt);
			const UE::UnitMassAvoidance::FDebugContext VelocitiesDebugContext(Context, this, LogAvoidanceVelocities, World, Entity, EntityIt);
			const UE::UnitMassAvoidance::FDebugContext ObstacleDebugContext(Context, this, LogAvoidanceObstacles, World, Entity, EntityIt);
			const UE::UnitMassAvoidance::FDebugContext AgentDebugContext(Context, this, LogAvoidanceAgents, World, Entity, EntityIt);

			FColor EntityColor = FColor::White;
			if (BaseDebugContext.ShouldLogEntity(&EntityColor))
			{
				// Draw agent
				const FString Text = FString::Printf(TEXT("%i"), Entity.Index);
				DebugDrawCylinder(BaseDebugContext, AgentLocation, AgentLocation + UE::UnitMassAvoidance::DebugAgentHeightOffset, (AgentRadius+1.),
					UE::UnitMassAvoidance::CurrentAgentColor, Text);

				// Draw agent center
				DebugDrawSphere(BaseDebugContext, AgentLocation, 10.f, UE::UnitMassAvoidance::CurrentAgentColor);

				// Draw circle for agent in LogMassNavigation.
				const FVector ZOffset(0,0,25);
				UE_VLOG_WIRECIRCLE_THICK(this, LogMassNavigation, Log, AgentLocation + ZOffset, FVector::UpVector, AgentRadius, EntityColor, /*Thickness*/4,
					TEXT("%s"), *Entity.DebugGetDescription(), TEXT("%s"), *Entity.DebugGetDescription());

				// Draw current velocity (black)
				UE::UnitMassAvoidance::DebugDrawVelocity(VelocitiesDebugContext, AgentLocation + UE::UnitMassAvoidance::DebugInputForceHeight,
				                                     AgentLocation + UE::UnitMassAvoidance::DebugInputForceHeight + AgentVelocity, UE::UnitMassAvoidance::VelocityColor);

				// Draw initial steering force
				DebugDrawArrow(BaseDebugContext, AgentLocation + UE::UnitMassAvoidance::DebugInputForceHeight,
					AgentLocation + UE::UnitMassAvoidance:: DebugInputForceHeight + SteeringForce,
					UE::UnitMassAvoidance::CurrentAgentColor, UE::UnitMassAvoidance::SteeringArrowHeadSize, UE::UnitMassAvoidance::SteeringThickness);

				// Draw center
				DebugDrawSphere(BaseDebugContext, AgentLocation, /*Radius*/2.f, UE::UnitMassAvoidance::CurrentAgentColor);
			}
#endif // WITH_MASSGAMEPLAY_DEBUG

			FVector OldSteeringForce = FVector::ZeroVector;

			//////////////////////////////////////////////////////////////////////////
			// Environment avoidance.
			//
			
			if (!MoveTarget.bOffBoundaries && UE::UnitMassAvoidance::Tweakables::bEnableEnvironmentAvoidance)
			{
				const FVector::FReal EnvironmentSeparationAgentRadius = (RadiusFragment.Radius * MovingAvoidanceParams.SeparationRadiusScale) - (NavEdges.bExtrudedEdges ? AgentRadius : 0);
				const FVector::FReal EnvironmentPredictiveAvoidanceAgentRadius = (RadiusFragment.Radius * MovingAvoidanceParams.PredictiveAvoidanceRadiusScale) - (NavEdges.bExtrudedEdges ? AgentRadius : 0);
				
				const FVector DesiredAcceleration = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel);
				const FVector DesiredVelocity = UE::MassNavigation::ClampVector(AgentVelocity + DesiredAcceleration * DeltaTime, MaximumSpeed);

#if WITH_MASSGAMEPLAY_DEBUG
				// Draw desired velocity (yellow)
				UE::UnitMassAvoidance::DebugDrawVelocity(VelocitiesDebugContext, AgentLocation + UE::UnitMassAvoidance::DebugInputForceHeight,
					AgentLocation + UE::UnitMassAvoidance::DebugInputForceHeight + DesiredVelocity, UE::UnitMassAvoidance::DesiredVelocityColor);
#endif // WITH_MASSGAMEPLAY_DEBUG

				OldSteeringForce = SteeringForce;
				Contacts.Reset();

				const FVector::FReal AgentRadiusForEnvironment = NavEdges.bExtrudedEdges ? 0 : AgentRadius;

				int32 EdgeIndex = 0;
				
				// Collect potential contacts between agent and environment edges (obstacles).
				for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
				{
					const FVector EdgeDiff = Edge.End - Edge.Start;
					FVector EdgeDir = FVector::ZeroVector;
					FVector::FReal EdgeLength = 0.;
					EdgeDiff.ToDirectionAndLength(EdgeDir, EdgeLength);

					const FVector EdgeStartToAgent = AgentLocation - Edge.Start;
					const FVector::FReal DistAlongEdge = FVector::DotProduct(EdgeDir, EdgeStartToAgent);
					const FVector::FReal DistAwayFromEdge = FVector::DotProduct(Edge.LeftDir, EdgeStartToAgent);

					FVector ConPos = FVector::ZeroVector; 		// contact position on edge
					bool bBehindTheEdge = false;
					bool bAlongTheEdge = false;

					if (DistAwayFromEdge < 0)
					{
						bBehindTheEdge = true;

						if (DistAlongEdge < 0)
						{
							// Start corner
							ConPos = Edge.Start;
						}
						else if (DistAlongEdge > EdgeLength)
						{
							// End corner
							ConPos = Edge.End;
						}
						else
						{
							// Directly behind the edge
							bAlongTheEdge = true;
							ConPos = Edge.Start + EdgeDir * DistAlongEdge;
						}
					}
					else
					{
						// In front of the edge
						if (DistAlongEdge < 0)
						{
							// Start corner
							ConPos = Edge.Start;
						}
						else if (DistAlongEdge > EdgeLength)
						{
							// End corner
							ConPos = Edge.End;
						}
						else
						{
							// Directly in front of the edge
							bAlongTheEdge = true;
							ConPos = Edge.Start + EdgeDir * DistAlongEdge;
						}
					}
					
					// Add new contact
					FEnvironmentContact Contact;
					Contact.Position = ConPos;
					Contact.Normal = Edge.LeftDir;
					Contact.Distance = DistAwayFromEdge;
					Contact.bAlongTheEdge = bAlongTheEdge;
					Contact.bBehindTheEdge = bBehindTheEdge;
					Contacts.Add(Contact);

#if WITH_MASSGAMEPLAY_DEBUG
					if (UE::UnitMassAvoidance::Tweakables::bEnableDetailedDebug && ObstacleDebugContext.ShouldLogEntity())
					{
						if (bAlongTheEdge)
						{
							// Draw active edges
							FVector ZOffset = FVector(0., 0., 7.);
							DebugDrawLine(ObstacleDebugContext, ZOffset + Edge.Start, ZOffset + Edge.End, FColor::Yellow, /*Thickness=*/3.f,
								/*bPersistent*/false, *LexToString(EdgeIndex));

							// Draw environment separation distance
							const FVector SeparationOffset = MovingAvoidanceParams.EnvironmentSeparationDistance * Edge.LeftDir;
							DebugDrawLine(ObstacleDebugContext, ZOffset + Edge.Start + SeparationOffset, ZOffset + Edge.End + SeparationOffset, FColor::Yellow, /*Thickness=*/1.f);
						}
					
						if (bBehindTheEdge)
						{
							// Draw inactive eges
							FVector Offset = FVector(0., 0., 9.);
							DebugDrawLine(ObstacleDebugContext, Offset + Edge.Start, Offset + Edge.End, FColor::Silver, /*Thickness=*/2.f);	
						}
					}
#endif // WITH_MASSGAMEPLAY_DEBUG
					
					// Skip predictive avoidance when behind the edge.
					if (!bBehindTheEdge)
					{
						// Avoid edges
						
#if WITH_MASSGAMEPLAY_DEBUG
						if (UE::UnitMassAvoidance::Tweakables::bEnableDetailedDebug && ObstacleDebugContext.ShouldLogEntity())
						{
							// Draw environment predictive avoidance distance
							FVector ZOffset = FVector(0., 0., 7.);
							const FVector AvoidanceOffset = MovingAvoidanceParams.PredictiveAvoidanceDistance * Edge.LeftDir;
							DebugDrawLine(ObstacleDebugContext, ZOffset + Edge.Start + AvoidanceOffset, ZOffset + Edge.End + AvoidanceOffset,
								UE::UnitMassAvoidance::ObstacleAvoidForceColor, /*Thickness=*/1.f);
						}
#endif // WITH_MASSGAMEPLAY_DEBUG

						const FVector::FReal CPA = UE::UnitMassAvoidance::ComputeClosestPointOfApproach(FVector2D(AgentLocation), FVector2D(DesiredVelocity), AgentRadiusForEnvironment,
							FVector2D(Edge.Start), FVector2D(Edge.End), MovingAvoidanceParams.PredictiveAvoidanceTime);
						const FVector HitAgentPos = AgentLocation + DesiredVelocity * CPA;
						const FVector::FReal EdgeT = UE::MassNavigation::ProjectPtSeg(FVector2D(HitAgentPos), FVector2D(Edge.Start), FVector2D(Edge.End));
						const FVector HitObPos = FMath::Lerp(Edge.Start, Edge.End, EdgeT);

						// Calculate penetration at CPA
						FVector AvoidRelPos = HitAgentPos - HitObPos;
						AvoidRelPos.Z = 0.;	// @todo AT: ignore the z component for now until we clamp the height of obstacles
						const FVector::FReal AvoidDist = AvoidRelPos.Size();
						const FVector AvoidNormal = AvoidDist > UE_KINDA_SMALL_NUMBER ? (AvoidRelPos / AvoidDist) : Edge.LeftDir;

						const FVector::FReal AvoidPen = (EnvironmentPredictiveAvoidanceAgentRadius + MovingAvoidanceParams.PredictiveAvoidanceDistance) - AvoidDist;
						FVector::FReal AvoidMag = 1.;
						if (MovingAvoidanceParams.PredictiveAvoidanceDistance != 0.)
						{
							AvoidMag = FMath::Square(FMath::Clamp(AvoidPen / MovingAvoidanceParams.PredictiveAvoidanceDistance, 0., 1.));
						}

						const FVector::FReal AvoidMagDist = 1. + FMath::Square(1. - CPA * InvPredictiveAvoidanceTime);

						// Predictive avoidance against environment is tuned down towards the end of the path
						const FVector AvoidForce = AvoidNormal * AvoidMag * AvoidMagDist * MovingAvoidanceParams.EnvironmentPredictiveAvoidanceStiffness * NearEndScaling; 

						SteeringForce += AvoidForce;

#if WITH_MASSGAMEPLAY_DEBUG
						if (!AvoidForce.IsNearlyZero())
						{
							if (UE::UnitMassAvoidance::Tweakables::bEnableDetailedDebug)
							{
								// Draw contact normal
								DebugDrawArrow(ObstacleDebugContext, ConPos, ConPos + (Contact.Distance * Contact.Normal), UE::UnitMassAvoidance::ObstacleContactNormalColor, /*HeadSize=*/ 5.f);
								DebugDrawSphere(ObstacleDebugContext, ConPos, 2.5f, UE::UnitMassAvoidance::ObstacleContactNormalColor);
							}

							// Draw future hit pos with edge
							DebugDrawSphere(ObstacleDebugContext, HitAgentPos, 1.f, UE::UnitMassAvoidance::ObstacleAvoidForceColor);
							DebugDrawCircle(ObstacleDebugContext, HitAgentPos, AgentRadius, UE::UnitMassAvoidance::ObstacleAvoidForceColor);
							DebugDrawLine(ObstacleDebugContext, AgentLocation, HitAgentPos, UE::UnitMassAvoidance::ObstacleAvoidForceColor);

							// Draw individual predictive obstacle avoidance forces
							UE::UnitMassAvoidance::DebugDrawForce(ObstacleDebugContext, HitObPos, HitObPos + AvoidForce,
								UE::UnitMassAvoidance::ObstacleAvoidForceColor, UE::UnitMassAvoidance::AvoidThickness);
						}
#endif // WITH_MASSGAMEPLAY_DEBUG
					}

					EdgeIndex++;
					
				} // edge loop

#if WITH_MASSGAMEPLAY_DEBUG
				// Draw total steering force to avoid obstacles
				const FVector EnvironmentAvoidSteeringForce = SteeringForce - OldSteeringForce;
				UE::UnitMassAvoidance::DebugDrawSummedForce(ObstacleDebugContext,
					AgentLocation + UE::UnitMassAvoidance::DebugAgentAvoidHeightOffset,
					AgentLocation + UE::UnitMassAvoidance::DebugAgentAvoidHeightOffset + EnvironmentAvoidSteeringForce,
					UE::UnitMassAvoidance::ObstacleAvoidForceColor);

				if (UE::UnitMassAvoidance::Tweakables::bEnableDetailedDebug)
				{
					// Draw all contact points
					for (const FEnvironmentContact& Contact : Contacts) 
					{
						DebugDrawSphere(ObstacleDebugContext, Contact.Position + FVector(0.f, 0.f, Contact.bBehindTheEdge ? 10.f : 0.f),
							5.f, Contact.bBehindTheEdge ? FColor::Black: FColor::Cyan);					
					}
				}
#endif // WITH_MASSGAMEPLAY_DEBUG
				
				// Process contacts to add edge separation force
				const FVector SteeringForceBeforeSeparation = SteeringForce;
				for (const FEnvironmentContact& Contact : Contacts) 
				{
					if (Contact.bAlongTheEdge && Contact.Distance > -AgentRadius)
					{
						// Separation force (stay away from obstacles if possible)
						const FVector::FReal SeparationPenalty = (EnvironmentSeparationAgentRadius + MovingAvoidanceParams.EnvironmentSeparationDistance) - Contact.Distance;

						FVector::FReal SeparationMag;
						if (MovingAvoidanceParams.EnvironmentSeparationDistance != 0.)
						{
							SeparationMag = UE::MassNavigation::Smooth(FMath::Clamp(SeparationPenalty / MovingAvoidanceParams.EnvironmentSeparationDistance, 0., 1.));
						}
						else
						{
							SeparationMag = 1.;
						}
						const FVector SeparationForce = Contact.Normal * MovingAvoidanceParams.EnvironmentSeparationStiffness * SeparationMag;
						
						SteeringForce += SeparationForce;

	#if WITH_MASSGAMEPLAY_DEBUG
						UE_VLOG(ObstacleDebugContext.GetLogOwner(), ObstacleDebugContext.Category, Log, TEXT("EnvironmentSeparationAgentRadius: %f, EnvironmentSeparationDistanc: %f, Distance: %f, SeparationPenalty: %f, SeparationMag: %f, SeparationForce: %s"),
							EnvironmentSeparationAgentRadius,
							MovingAvoidanceParams.EnvironmentSeparationDistance,
							Contact.Distance,
							SeparationPenalty,
							SeparationMag,
							*SeparationForce.ToCompactString()
							);
						
						// Draw contact normal
						if (!SeparationForce.IsNearlyZero())
						{
							// Draw individual separation forces
							const FVector ZOffset = FVector(0., 0., 7.);
							UE::UnitMassAvoidance::DebugDrawForce(ObstacleDebugContext, Contact.Position + ZOffset,
								Contact.Position + SeparationForce + ZOffset,
								UE::UnitMassAvoidance::ObstacleSeparationForceColor, UE::UnitMassAvoidance::SeparationThickness);
						}
	#endif // WITH_MASSGAMEPLAY_DEBUG
					}
				}
				
#if WITH_MASSGAMEPLAY_DEBUG
				// Draw total steering force to separate from close edges
				const FVector TotalSeparationForce = SteeringForce - SteeringForceBeforeSeparation;
				UE::UnitMassAvoidance::DebugDrawSummedForce(ObstacleDebugContext,
					AgentLocation + UE::UnitMassAvoidance::DebugAgentSeparationHeightOffset,
					AgentLocation + UE::UnitMassAvoidance::DebugAgentSeparationHeightOffset + TotalSeparationForce,
					UE::UnitMassAvoidance::ObstacleSeparationForceColor);

				// Display close obstacle edges
				if (ObstacleDebugContext.ShouldLogEntity())
				{
					for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
					{
						FVector Offset = FVector(0., 0., 5.);
						DebugDrawLine(ObstacleDebugContext, Offset + Edge.Start, Offset + Edge.End, UE::UnitMassAvoidance::ObstacleColor, /*Thickness=*/2.f);

						const FVector Middle = Offset + 0.5f * (Edge.Start + Edge.End);
						DebugDrawArrow(ObstacleDebugContext, Middle, Middle + 10. * Edge.LeftDir, UE::UnitMassAvoidance::ObstacleColor, /*HeadSize=*/2.f);
					}
				}
#endif // WITH_MASSGAMEPLAY_DEBUG
			}

			//////////////////////////////////////////////////////////////////////////
			// Avoid close agents
			const FVector::FReal SeparationAgentRadius = RadiusFragment.Radius * MovingAvoidanceParams.SeparationRadiusScale;
			const FVector::FReal PredictiveAvoidanceAgentRadius = RadiusFragment.Radius * MovingAvoidanceParams.PredictiveAvoidanceRadiusScale;

			// Update desired velocity based on avoidance so far.
			const FVector DesAcc = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel);
			const FVector DesVel = UE::MassNavigation::ClampVector(AgentVelocity + DesAcc * DeltaTime, MaximumSpeed);

			// Find close obstacles
			const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid = NavigationSubsystem->GetObstacleGridMutable();

			UE::UnitMassAvoidance::FindCloseObstacles(AgentLocation, MovingAvoidanceParams.ObstacleDetectionDistance,
				AvoidanceObstacleGrid, CloseEntities, UE::UnitMassAvoidance::MaxObstacleResults);

			// Remove unwanted and find the closests in the CloseEntities
			const FVector::FReal DistanceCutOffSqr = FMath::Square(MovingAvoidanceParams.ObstacleDetectionDistance);
			ClosestObstacles.Reset();
			for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : CloseEntities)
			{
				// Skip self
				if (OtherEntity.Entity == Entity)
				{
					continue;
				}

				// Skip invalid entities.
				if (!EntityManager.IsEntityValid(OtherEntity.Entity))
				{
					UE_LOG(LogAvoidanceObstacles, VeryVerbose, TEXT("Close entity is invalid, skipped."));
					continue;
				}
				
				// Skip too far
				const FTransform& Transform = EntityManager.GetFragmentDataChecked<FTransformFragment>(OtherEntity.Entity).GetTransform();
				const FVector OtherLocation = Transform.GetLocation();
				
				const FVector::FReal SqDist = FVector::DistSquared(AgentLocation, OtherLocation);
				if (SqDist > DistanceCutOffSqr)
				{
					continue;
				}

				// Skip entities to ignore
				if (UNLIKELY(!EntitiesToIgnore.IsEmpty()) && EntitiesToIgnore.Contains(OtherEntity.Entity))
				{
					continue;
				}

				FSortedObstacle Obstacle;
				Obstacle.LocationCached = OtherLocation;
				Obstacle.Forward = Transform.GetRotation().GetForwardVector();
				Obstacle.ObstacleItem = OtherEntity;
				Obstacle.SqDist = SqDist;
				ClosestObstacles.Add(Obstacle);
			}
			ClosestObstacles.Sort([](const FSortedObstacle& A, const FSortedObstacle& B) { return A.SqDist < B.SqDist; });

			// Compute forces
			OldSteeringForce = SteeringForce;
			FVector TotalAgentSeparationForce = FVector::ZeroVector;

			// Fill collider list from close agents
			Colliders.Reset();
			for (int32 Index = 0; Index < ClosestObstacles.Num(); Index++)
			{
				constexpr int32 MaxColliders = 6;
				if (Colliders.Num() >= MaxColliders)
				{
					break;
				}

				FSortedObstacle& Obstacle = ClosestObstacles[Index];
				FMassEntityView OtherEntityView(EntityManager, Obstacle.ObstacleItem.Entity);

				const FMassVelocityFragment* OtherVelocityFragment = OtherEntityView.GetFragmentDataPtr<FMassVelocityFragment>();
				const FVector OtherVelocity = OtherVelocityFragment != nullptr ? OtherVelocityFragment->Value : FVector::ZeroVector; // Get velocity from FAvoidanceComponent

				// @todo: this is heavy fragment to access, see if we could handle this differently.
				const FMassMoveTargetFragment* OtherMoveTarget = OtherEntityView.GetFragmentDataPtr<FMassMoveTargetFragment>();
				const bool bCanAvoid = OtherMoveTarget != nullptr;
				const bool bOtherIsMoving = OtherMoveTarget ? OtherMoveTarget->GetCurrentAction() == EMassMovementAction::Move : true; // Assume moving if other does not have move target.
				
				// Check for colliders data
				if (EnumHasAnyFlags(Obstacle.ObstacleItem.ItemFlags, EMassNavigationObstacleFlags::HasColliderData))
				{
					if (const FMassAvoidanceColliderFragment* ColliderFragment = OtherEntityView.GetFragmentDataPtr<FMassAvoidanceColliderFragment>())
					{
						if (ColliderFragment->Type == EMassColliderType::Circle)
						{
							const FMassCircleCollider Circle = ColliderFragment->GetCircleCollider();
							
							FCollider& Collider = Colliders.Add_GetRef(FCollider{});
							Collider.Velocity = OtherVelocity;
							Collider.bCanAvoid = bCanAvoid;
							Collider.bIsMoving = bOtherIsMoving;
							Collider.Radius = Circle.Radius;
							Collider.Location = Obstacle.LocationCached;
						}
						else if (ColliderFragment->Type == EMassColliderType::Pill)
						{
							const FMassPillCollider Pill = ColliderFragment->GetPillCollider(); 

							FCollider& Collider = Colliders.Add_GetRef(FCollider{});
							Collider.Velocity = OtherVelocity;
							Collider.bCanAvoid = bCanAvoid;
							Collider.bIsMoving = bOtherIsMoving;
							Collider.Radius = Pill.Radius;
							Collider.Location = Obstacle.LocationCached + (Pill.HalfLength * Obstacle.Forward);

							if (Colliders.Num() < MaxColliders)
							{
								FCollider& Collider2 = Colliders.Add_GetRef(FCollider{});
								Collider2.Velocity = OtherVelocity;
								Collider2.bCanAvoid = bCanAvoid;
								Collider2.bIsMoving = bOtherIsMoving;
								Collider2.Radius = Pill.Radius;
								Collider2.Location = Obstacle.LocationCached + (-Pill.HalfLength * Obstacle.Forward);
							}
						}
					}
				}
				else
				{
					FCollider& Collider = Colliders.Add_GetRef(FCollider{});
					Collider.Location = Obstacle.LocationCached;
					Collider.Velocity = OtherVelocity;
					Collider.Radius = OtherEntityView.GetFragmentData<FAgentRadiusFragment>().Radius;
					Collider.bCanAvoid = bCanAvoid;
					Collider.bIsMoving = bOtherIsMoving;
				}
			}

			// Process colliders for avoidance
			for (const FCollider& Collider : Colliders)
			{
				bool bHasForcedNormal = false;
				FVector ForcedNormal = FVector::ZeroVector;

				if (Collider.bCanAvoid == false)
				{
					// If the other obstacle cannot avoid us, try to avoid the local minima they create between the wall and their collider.
					// If the space between edge and collider is less than MinClearance, make the agent to avoid the gap.
					const FVector::FReal MinClearance = 2. * AgentRadius * MovingAvoidanceParams.StaticObstacleClearanceScale;
					
					// Find the maximum distance from edges that are too close.
					FVector::FReal MaxDist = -1.;
					FVector ClosestPoint = FVector::ZeroVector;
					for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
					{
						const FVector Point = FMath::ClosestPointOnSegment(Collider.Location, Edge.Start, Edge.End);
						const FVector Offset = Collider.Location - Point;
						if (FVector::DotProduct(Offset, Edge.LeftDir) < 0.)
						{
							// Behind the edge, ignore.
							continue;
						}

						const FVector::FReal OffsetLength = Offset.Length();
						const bool bTooNarrow = (OffsetLength - Collider.Radius) < MinClearance; 
						if (bTooNarrow)
						{
							if (OffsetLength > MaxDist)
							{
								MaxDist = OffsetLength;
								ClosestPoint = Point;
							}
						}
					}

					if (MaxDist != -1.)
					{
						// Set up forced normal to avoid the gap between collider and edge.
						ForcedNormal = (Collider.Location - ClosestPoint).GetSafeNormal();
						bHasForcedNormal = true;
					}
				}

				FVector RelPos = AgentLocation - Collider.Location;
				RelPos.Z = 0.; // we assume we work on a flat plane for now
				const FVector RelVel = DesVel - Collider.Velocity;
				const FVector::FReal ConDist = RelPos.Size();
				const FVector ConNorm = ConDist > 0. ? RelPos / ConDist : FVector::ForwardVector;

				FVector SeparationNormal = ConNorm;
				if (bHasForcedNormal)
				{
					// The more head on the collisions is, the more we should avoid towards the forced direction.
					const FVector RelVelNorm = RelVel.GetSafeNormal();
					const FVector::FReal Blend = FMath::Max(0., -FVector::DotProduct(ConNorm, RelVelNorm));
					SeparationNormal = FMath::Lerp(ConNorm, ForcedNormal, Blend).GetSafeNormal();
				}

				// Care less about standing agents so that we can push through standing crowd.
				const FVector::FReal StandingScaling = Collider.bIsMoving ? 1. : MovingAvoidanceParams.StandingObstacleAvoidanceScale; 
				
				// Separation force (stay away from agents if possible)
				const FVector::FReal PenSep = (SeparationAgentRadius + Collider.Radius + MovingAvoidanceParams.ObstacleSeparationDistance) - ConDist;
				const FVector::FReal SeparationMag = FMath::Square(FMath::Clamp(PenSep / MovingAvoidanceParams.ObstacleSeparationDistance, 0., 1.));
				const FVector SepForce = SeparationNormal * MovingAvoidanceParams.ObstacleSeparationStiffness;
				const FVector SeparationForce = SepForce * SeparationMag * StandingScaling;

				SteeringForce += SeparationForce;
				TotalAgentSeparationForce += SeparationForce;

				// Calculate the closest point of approach based on relative agent positions and velocities.
				const FVector::FReal CPA = UE::UnitMassAvoidance::ComputeClosestPointOfApproach(RelPos, RelVel, PredictiveAvoidanceAgentRadius + Collider.Radius, MovingAvoidanceParams.PredictiveAvoidanceTime);

				// Calculate penetration at CPA
				const FVector AvoidRelPos = RelPos + RelVel * CPA;
				const FVector::FReal AvoidDist = AvoidRelPos.Size();
				const FVector AvoidConNormal = AvoidDist > UE_KINDA_SMALL_NUMBER ? (AvoidRelPos / AvoidDist) : FVector::ForwardVector;

				FVector AvoidNormal = AvoidConNormal;
				if (bHasForcedNormal)
				{
					// The more head on the predicted collisions is, the more we should avoid towards the forced direction.
					const FVector RelVelNorm = RelVel.GetSafeNormal();
					const FVector::FReal Blend = FMath::Max(0., -FVector::DotProduct(AvoidConNormal, RelVelNorm));
					AvoidNormal = FMath::Lerp(AvoidConNormal, ForcedNormal, Blend).GetSafeNormal();
				}
				
				const FVector::FReal AvoidPenetration = (PredictiveAvoidanceAgentRadius + Collider.Radius + MovingAvoidanceParams.PredictiveAvoidanceDistance) - AvoidDist; // Based on future agents distance
				const FVector::FReal AvoidMag = FMath::Square(FMath::Clamp(AvoidPenetration / MovingAvoidanceParams.PredictiveAvoidanceDistance, 0., 1.));
				const FVector::FReal AvoidMagDist = (1. - (CPA * InvPredictiveAvoidanceTime)); // No clamp, CPA is between 0 and PredictiveAvoidanceTime
				const FVector AvoidForce = AvoidNormal * AvoidMag * AvoidMagDist * MovingAvoidanceParams.ObstaclePredictiveAvoidanceStiffness * StandingScaling;

				SteeringForce += AvoidForce;

#if WITH_MASSGAMEPLAY_DEBUG
				// Display close agent
				UE::UnitMassAvoidance::DebugDrawCylinder(AgentDebugContext, Collider.Location,
					Collider.Location + UE::UnitMassAvoidance::DebugLowCylinderOffset, Collider.Radius, UE::UnitMassAvoidance::AgentsColor);

				if (bHasForcedNormal)
				{
					UE::UnitMassAvoidance::DebugDrawCylinder(BaseDebugContext, Collider.Location,
						Collider.Location + UE::UnitMassAvoidance::DebugAgentHeightOffset, Collider.Radius, FColor::Red);
				}

				// Draw agent contact separation force
				if (!SeparationForce.IsNearlyZero())
				{
					UE::UnitMassAvoidance::DebugDrawForce(AgentDebugContext,
						Collider.Location + UE::UnitMassAvoidance::DebugAgentSeparationHeightOffset,
						Collider.Location + UE::UnitMassAvoidance::DebugAgentSeparationHeightOffset + SeparationForce,
						UE::UnitMassAvoidance::AgentSeparationForceColor, UE::UnitMassAvoidance::SeparationThickness); 
				}
				
				if (AvoidForce.Size() > 0.)
				{
					// Draw agent vs agent hit positions
					const FVector HitPosition = AgentLocation + (DesVel * CPA);
					const FVector LeftOffset = PredictiveAvoidanceAgentRadius * UE::MassNavigation::GetLeftDirection(DesVel.GetSafeNormal(), FVector::UpVector);
					UE::UnitMassAvoidance::DebugDrawLine(AgentDebugContext, AgentLocation + UE::UnitMassAvoidance::DebugAgentHeightOffset + LeftOffset,
						HitPosition + UE::UnitMassAvoidance::DebugAgentHeightOffset + LeftOffset, UE::UnitMassAvoidance::CurrentAgentColor, 1.5f);
					UE::UnitMassAvoidance::DebugDrawLine(AgentDebugContext, AgentLocation + UE::UnitMassAvoidance::DebugAgentHeightOffset - LeftOffset,
						HitPosition + UE::UnitMassAvoidance::DebugAgentHeightOffset - LeftOffset, UE::UnitMassAvoidance::CurrentAgentColor, 1.5f);
					UE::UnitMassAvoidance::DebugDrawCylinder(AgentDebugContext, HitPosition,
						HitPosition + UE::UnitMassAvoidance::DebugAgentHeightOffset, PredictiveAvoidanceAgentRadius, UE::UnitMassAvoidance::CurrentAgentColor);

					const FVector OtherHitPosition = Collider.Location + (Collider.Velocity * CPA);
					const FVector OtherLeftOffset = Collider.Radius * UE::MassNavigation::GetLeftDirection(Collider.Velocity.GetSafeNormal(), FVector::UpVector);
					const FVector Left = UE::UnitMassAvoidance::DebugAgentHeightOffset + OtherLeftOffset;
					const FVector Right = UE::UnitMassAvoidance::DebugAgentHeightOffset - OtherLeftOffset;
					UE::UnitMassAvoidance::DebugDrawLine(AgentDebugContext, Collider.Location + Left, OtherHitPosition + Left, UE::UnitMassAvoidance::AgentsColor, 1.5f);
					UE::UnitMassAvoidance::DebugDrawLine(AgentDebugContext, Collider.Location + Right, OtherHitPosition + Right, UE::UnitMassAvoidance::AgentsColor, 1.5f);
					UE::UnitMassAvoidance::DebugDrawCylinder(AgentDebugContext, Collider.Location, Collider.Location + UE::UnitMassAvoidance::DebugAgentHeightOffset,
						AgentRadius, UE::UnitMassAvoidance::AgentsColor);
					UE::UnitMassAvoidance::DebugDrawCylinder(AgentDebugContext, OtherHitPosition, OtherHitPosition + UE::UnitMassAvoidance::DebugAgentHeightOffset,
						AgentRadius, UE::UnitMassAvoidance::AgentsColor);

					// Draw agent avoid force
					UE::UnitMassAvoidance::DebugDrawForce(AgentDebugContext,
						OtherHitPosition + UE::UnitMassAvoidance::DebugAgentAvoidHeightOffset,
						OtherHitPosition + UE::UnitMassAvoidance::DebugAgentAvoidHeightOffset + AvoidForce,
						UE::UnitMassAvoidance::AgentAvoidForceColor, UE::UnitMassAvoidance::AvoidThickness);
				}
#endif // WITH_MASSGAMEPLAY_DEBUG
			} // close entities loop

			SteeringForce *= NearStartScaling * NearEndScaling;

			Force.Value = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel); // Assume unit mass

#if WITH_MASSGAMEPLAY_DEBUG
			const FVector AgentAvoidSteeringForce = SteeringForce - OldSteeringForce;

			// Draw total steering force to separate agents
			UE::UnitMassAvoidance::DebugDrawSummedForce(AgentDebugContext,
				AgentLocation + UE::UnitMassAvoidance::DebugAgentSeparationHeightOffset,
				AgentLocation + UE::UnitMassAvoidance::DebugAgentSeparationHeightOffset + TotalAgentSeparationForce,
				UE::UnitMassAvoidance::AgentSeparationForceColor);

			// Draw total steering force to avoid agents
			UE::UnitMassAvoidance::DebugDrawSummedForce(AgentDebugContext,
				AgentLocation + UE::UnitMassAvoidance::DebugAgentAvoidHeightOffset,
				AgentLocation + UE::UnitMassAvoidance::DebugAgentAvoidHeightOffset + AgentAvoidSteeringForce,
				UE::UnitMassAvoidance::AgentAvoidForceColor);

			// Draw final steering force adding
			UE::UnitMassAvoidance::DebugDrawArrow(BaseDebugContext, 
				AgentLocation + UE::UnitMassAvoidance::DebugOutputForcesHeight,
				AgentLocation + UE::UnitMassAvoidance::DebugOutputForcesHeight + Force.Value,
				UE::UnitMassAvoidance::FinalSteeringForceColor, UE::UnitMassAvoidance::SteeringArrowHeadSize, UE::UnitMassAvoidance::SteeringThickness);
#endif // WITH_MASSGAMEPLAY_DEBUG
		}
	});
}