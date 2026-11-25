// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Containers/Ticker.h"
#include "RTSPerfCapacityTest.generated.h"

class ACustomControllerBase;
class AUnitBase;

/**
 * Capacity test: how many own units keep the game at TargetFps while they march?
 *
 * WHY A SEPARATE TEST: RTSPerfTest selects all units and leaves them selected. With 600
 * selected units the selection rings (AHUDBase::DrawAllSelectedUnitsIndicators) cost a few ms on
 * the game thread, so "600 units at 48 fps" measured the ring drawing, not the simulation. This
 * test issues every march order through the normal selection, then DESELECTS again, so the
 * measured frames contain no selection indicators - the way a player plays with 600 units.
 *
 * Procedure (all inside ONE binary, so the numbers are comparable to each other):
 *   1. Warmup, fps cap and VSync off (restored at the end).
 *   2. Baseline at the current unit count: unselected, selected, unselected again. The two
 *      unselected windows give the drift, the selected one the cost of the selection rings.
 *   3. Spawn Step units, settle, measure - until the average fps falls below TargetFps or
 *      MaxUnits is reached.
 *
 * Unit kind (Kind=): ISM units and skeletal units are measured SEPARATELY - their costs differ a
 * lot (a skeletal unit is a moving actor with animation, an ISM unit an instance), and a mixed
 * count says nothing about either. With Kind=ism or Kind=skeletal the own movable units of the
 * other kind are removed first (KillSilently; camera units stay) and only that kind is spawned.
 * Kind=mixed keeps the map's units as they are.
 *
 * While the test runs RTS.Talents.PauseOnPoint is 0: an attribute tree point would otherwise
 * pause the game and open the tree in the middle of a step.
 *
 * Every window also checks that the units really move: the summed path length of the Mass
 * entities and - separately - of the visible skeletal actors. That catches the regression of
 * 18.09.2026, where a lost DispatchPendingUpdates call froze all skeletal units while ISM units
 * kept going and the frame rate looked BETTER.
 *
 * Output: "[PerfCapacity]" log lines, Saved/Profiling/RTSPerfCapacity_<time>.csv and a final
 * "[PerfCapacity] RESULT" line. With Quit=1 the process exits with code 0 (pass) or 3 (fail),
 * so a script can run it before a build.
 *
 * Console:  RTS.PerfTest.Capacity [Kind=ism|skeletal|mixed] [Start=0] [Step=50] [Max=2000] [Target=60] [Measure=12]
 *           [Settle=4] [Warmup=10] [Spread=4000] [Require=0] [Quit=0]
 *           RTS.PerfTest.Capacity.Stop
 */
UCLASS()
class RTSUNITTEMPLATE_API URTSPerfCapacityTest : public UObject
{
	GENERATED_BODY()

public:
	enum class EUnitKind : uint8 { Mixed, Ism, Skeletal };

	struct FSettings
	{
		EUnitKind Kind = EUnitKind::Ism;
		/** Fill up to this many own units before the first step (0 = as many as the map had). */
		int32 StartUnits = 0;
		int32 Step = 50;
		int32 MaxUnits = 2000;
		float TargetFps = 60.f;
		float WarmupSeconds = 10.f;
		float SettleSeconds = 4.f;
		float MeasureSeconds = 12.f;
		float OrderInterval = 6.f;
		float Distance = 9000.f;
		float Spread = 4000.f;
		/** Pass/fail gate: the test fails when fewer units than this reach TargetFps (0 = no gate). */
		int32 RequiredUnits = 0;
		bool bQuitWhenDone = false;
	};

	/** Starts the test for the given controller. A running test is stopped first. */
	static void Start(ACustomControllerBase* Controller, const FSettings& InSettings);
	static void Stop(const TCHAR* Reason);
	static bool IsRunning();

private:
	enum class EPhase : uint8 { Warmup, Clearing, Settle, Measure, Done };

	enum class EWindow : uint8 { BaselineUnselected, BaselineSelected, BaselineDrift, Step };

	struct FUnitTrack
	{
		FVector LastMass = FVector::ZeroVector;
		FVector LastActor = FVector::ZeroVector;
		double MassPath = 0.0;
		double ActorPath = 0.0;
		bool bSkeletal = false;
	};

	struct FWindowResult
	{
		FString Label;
		int32 Units = 0;
		bool bSelected = false;
		int32 Frames = 0;
		double Fps = 0.0;
		double FrameMs = 0.0;
		double FrameMsP95 = 0.0;
		double GameThreadMs = 0.0;
		double RenderThreadMs = 0.0;
		double GpuMs = 0.0;
		float MovedMassPct = 0.f;
		float MovedSkeletalPct = -1.f;   // -1 = no visible skeletal units in this window
		bool bMovementOk = true;
	};

	bool Tick(float DeltaTime);
	void BeginWindow(EWindow Window);
	void EndWindow();
	void EnterSettle();
	void IssueOrder();
	void SampleMovement(bool bAccumulate);
	void SelectAll();
	void DeselectAll();
	int32 CountOwnUnits() const;
	bool MatchesKind(const AUnitBase* Unit) const;
	static bool IsSkeletalUnit(const AUnitBase* Unit);
	static const TCHAR* KindName(EUnitKind Kind);
	/** Kills the own movable units of the other kind. Returns how many. */
	int32 RemoveOtherKind();
	/** Spawns Count own units of the test's kind around SpawnCenter. Returns how many came. */
	int32 SpawnUnits(int32 Count);
	void ApplyUncappedFrameRate();
	void RestoreFrameRate();
	void Finish(const FString& Reason);
	void WriteCsv(const FString& Verdict) const;
	ACustomControllerBase* GetController() const;

	FSettings Settings;
	TWeakObjectPtr<ACustomControllerBase> ControllerPtr;
	FTSTicker::FDelegateHandle TickerHandle;

	EPhase Phase = EPhase::Warmup;
	EWindow CurrentWindow = EWindow::BaselineUnselected;
	EWindow NextWindow = EWindow::BaselineUnselected;
	double PhaseStart = 0.0;
	double LastOrderTime = -1000.0;
	double LastMovementSample = 0.0;
	int32 OrderCount = 0;
	bool bKeepSelected = false;

	TArray<float> FrameMs;
	double SumGameMs = 0.0;
	double SumRenderMs = 0.0;
	double SumGpuMs = 0.0;
	TMap<TWeakObjectPtr<AUnitBase>, FUnitTrack> Tracks;

	TArray<FWindowResult> Results;
	int32 BestUnits = 0;
	double BestFps = 0.0;
	bool bMovementFailed = false;

	// Saved engine state, restored in RestoreFrameRate.
	float SavedMaxFps = 0.f;
	int32 SavedVSync = 0;
	bool bSavedSmoothFrameRate = false;
	int32 SavedPauseOnPoint = 1;
	bool bFrameRateChanged = false;

	FVector SpawnCenter = FVector::ZeroVector;
	int32 UnitsBeforeClearing = 0;
};
