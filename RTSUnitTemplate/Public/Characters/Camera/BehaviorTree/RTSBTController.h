#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TimerManager.h"
#include "RTSBTController.generated.h"

// Forward declarations requested by user (these are engine classes)
class UBehaviorTree;
class UBlackboardComponent;
class UBehaviorTreeComponent;
class UBlackboardData;
class UInferenceComponent;
class APawn;

/**
 * ARTSBTController
 * A lightweight AIController that owns and runs a Behavior Tree for the RTS unit camera/agent.
 * - Uses UseBlackboard/RunBehaviorTree to wire AI owner + blackboard correctly
 * - Optionally polls a string blackboard key (SelectedActionJSON) each tick
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API ARTSBTController : public AAIController
{
	GENERATED_BODY()

public:
	ARTSBTController();

	// Team ID this AI belongs to. Used to gather game state from its perspective.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 OrchestratorTeamId = -1;

	// Assign your BT asset in defaults or in the editor (or from a BP derived from this controller)
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> StrategyBehaviorTree = nullptr;

	// Name of the string BB key that BT tasks will write the JSON action to
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	FName SelectedActionJSONKey = TEXT("SelectedActionJSON");

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	bool bDebug = false;

	// Enable controller-owned Blackboard updates. When false (default), expect a BT Service to feed the Blackboard.
	UPROPERTY(EditDefaultsOnly, Category = "AI|BlackboardUpdate")
	bool bEnableControllerBBUpdates = false;

	// Controls how the controller feeds Blackboard with live game state
	// - If true (default): use controller Tick to push BB at BlackboardUpdateInterval
	// - If false: use an internal repeating timer started in OnPossess
	UPROPERTY(EditDefaultsOnly, Category = "AI|BlackboardUpdate", meta=(EditCondition="bEnableControllerBBUpdates"))
	bool bUseTickForBBUpdates = true;

	// Interval for BB updates (seconds) both for Tick- and Timer-driven modes
	UPROPERTY(EditDefaultsOnly, Category = "AI|BlackboardUpdate", meta=(ClampMin="0.01", UIMin="0.01", EditCondition="bEnableControllerBBUpdates"))
	float BlackboardUpdateInterval = 0.1f;

	// If true, controller BB updates require a possessed pawn; if false (default), updates can run without a pawn (for orchestrator/controller-only setups)
	UPROPERTY(EditDefaultsOnly, Category = "AI|BlackboardUpdate", meta=(EditCondition="bEnableControllerBBUpdates"))
	bool bRequirePossessedPawnForBBUpdates = false;

 // Watchdog: if the BT Service doesn't tick within this timeout, automatically enable controller-owned BB updates
	bool bAutoFallbackToControllerBBUpdates = true;

	// Seconds without service pings before fallback engages
	float BBServiceWatchdogTimeout = 0.5f;

	// Optional: poll the BB each tick to consume actions written by BT tasks
	virtual void Tick(float DeltaSeconds) override;

	// Called by BT services to report a successful tick/push (used by the watchdog)
	UFUNCTION(BlueprintCallable, Category = "AI|BlackboardUpdate")
	void NotifyBBServiceTick();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Helper to fetch then clear the SelectedActionJSON once per tick
	UFUNCTION(BlueprintCallable, Category = "AI")
	FString ConsumeSelectedActionJSON();

private:
	// Try to obtain a valid blackboard (cached or from brain component)
	UBlackboardComponent* GetEffectiveBlackboard() const;

	// Cached components owned by the controller
	UPROPERTY(Transient)
	TObjectPtr<UBlackboardComponent> BlackboardComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTreeComponent> BehaviorComp = nullptr;

	// Periodic BB update (controller-owned) to feed Rule-Based BT from game state
	FTimerHandle BBUpdateTimerHandle;
	int32 BBPushTickCounter = 0;

	// Accumulator used when bUseTickForBBUpdates is true
	float BBUpdateAccumulator = 0.f;

	// Optional cache to avoid re-processing the same action
	FString LastActionJSON;

	// Throttle for early-return warnings
	double LastPawnWarnTime = -1000.0;
	double LastBBWarnTime = -1000.0;

	// Last time (seconds) we received a ping from the BT Service
	double LastBBServiceTickTime = -1.0;

	// Whether we already switched to controller-owned updates due to watchdog
	bool bControllerBBFallbackEngaged = false;

	// Pushes current game state into Blackboard; early-returns with logs when unavailable
	void PushBlackboardFromGameState_ControllerOwned();

	// Start controller-owned BB updates based on settings
	void StartControllerBBUpdates();

	// Internal helpers
	void PauseBBUpdates();
	void ResumeBBUpdates();
};
