// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/World.h"
#include "Characters/Camera/RL/InferenceComponent.h"   // EBrainMode, used in the panel API below
#include "RLRecorderSubsystem.generated.h"

struct FGameStateData;

/** Where a recorded decision came from. Kept as an enum so the trainer can filter or weight by source. */
UENUM(BlueprintType)
enum class ERLSampleSource : uint8
{
	/** The data-table rule AI. This is the bootstrap set: it already knows how the game works. */
	RuleBased   UMETA(DisplayName = "Rule-Based AI"),
	/** A human player's own keystrokes, for cloning their playstyle. */
	Human       UMETA(DisplayName = "Human Player"),
	/** An already-trained network playing, for self-play / iterative refinement. */
	Model       UMETA(DisplayName = "RL Model")
};

/** Everything a UI needs in one call, so the widget graph stays a single node instead of six. */
USTRUCT(BlueprintType)
struct RTSUNITTEMPLATE_API FRLRecordingStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|RL Recording")
	bool bRecording = false;

	/** Decisions written in the current session. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|RL Recording")
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AI|RL Recording")
	FString FilePath;

	UPROPERTY(BlueprintReadOnly, Category = "AI|RL Recording")
	FString Directory;

	/** Recordings already on disk, so a player can see their training set grow across sessions. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|RL Recording")
	int32 StoredFileCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AI|RL Recording")
	float StoredMegabytes = 0.f;

	/** Ready-made one-liner for a status label. */
	UPROPERTY(BlueprintReadOnly, Category = "AI|RL Recording")
	FText Summary;
};

/**
 * Records (state, action) pairs to disk so a network can be trained offline.
 *
 * This exists to replace the shared-memory bridge for the data-collection half of the RL loop. That bridge
 * needs a Global\ mapping, which means administrator rights for both the editor and the Python process, and
 * it only works while both run side by side. Writing to Saved/RLData instead needs no privileges and works
 * in a packaged build, so a player can record their own games and train from them afterwards.
 *
 * The format is newline-delimited JSON, one decision per line:
 *   {"t":12.34,"team":1,"src":"rule","s":[...21 floats...],"a":7}
 * State layout is exactly UInferenceComponent::StateToArray and the action is an ERTSAIAction value, which
 * is also the index into the model's output layer - so nothing has to be translated when training.
 */
UCLASS()
class RTSUNITTEMPLATE_API URLRecorderSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Opens a new recording file under Saved/RLData and starts collecting.
	 * @param SessionLabel  Free-form label that becomes part of the file name; blank falls back to "session".
	 * @return false if a file could not be opened (the reason is logged).
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|RL Recording")
	bool StartRecording(const FString& SessionLabel);

	/** Flushes and closes the current file. Safe to call when not recording. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL Recording")
	void StopRecording();

	UFUNCTION(BlueprintPure, Category = "AI|RL Recording")
	bool IsRecording() const { return bIsRecording; }

	/** Number of decisions written since StartRecording. Meant for a progress readout in the UI. */
	UFUNCTION(BlueprintPure, Category = "AI|RL Recording")
	int32 GetSampleCount() const { return SampleCount; }

	/** Absolute path of the file being written, or empty when not recording. */
	UFUNCTION(BlueprintPure, Category = "AI|RL Recording")
	FString GetCurrentFilePath() const { return CurrentFilePath; }

	/** Directory all recordings go to, so a UI can list or open it. */
	UFUNCTION(BlueprintPure, Category = "AI|RL Recording")
	static FString GetRecordingDirectory();

	/** Convenience accessor so a widget does not need the game-instance plumbing. */
	UFUNCTION(BlueprintPure, Category = "AI|RL Recording", meta = (WorldContext = "WorldContextObject"))
	static URLRecorderSubsystem* Get(const UObject* WorldContextObject);

	/** Recording state plus what is already stored, for a status panel. */
	UFUNCTION(BlueprintPure, Category = "AI|RL Recording")
	FRLRecordingStatus GetStatus() const;

	/** Opens the recordings folder in the OS file browser - the shortest path from "recorded" to "trained". */
	UFUNCTION(BlueprintCallable, Category = "AI|RL Recording")
	static void OpenRecordingFolder();

	/** Deletes every .jsonl in the recording folder. Returns how many files went. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL Recording")
	int32 ClearRecordings();

	/** Only these sources are written. Lets a player record their own play without the AI polluting the set. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL Recording")
	void SetSourceFilter(ERLSampleSource Source, bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "AI|RL Recording")
	bool IsSourceEnabled(ERLSampleSource Source) const;

	/**
	 * Records one decision. Called from whichever brain produced it.
	 * A composite decision (select group, then press ability) is recorded as consecutive samples sharing the
	 * same state - that is what the network has to reproduce, one action per inference step.
	 */
	void RecordSample(int32 TeamId, const TArray<float>& State, int32 ActionIndex, ERLSampleSource Source);

	/** Convenience overload that does the state conversion, so callers holding a FGameStateData stay simple. */
	void RecordSampleFromGameState(int32 TeamId, const FGameStateData& GameState, int32 ActionIndex, ERLSampleSource Source);

	/** Shows or hides the configuration panel named by the rts.rl.ui.widget setting. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL Recording")
	void ToggleConfigWidget();

	// ---------------- In-game panel support ----------------
	// The RL panel belongs in the HUD, but only where it makes sense: you may configure the AI that plays
	// on YOUR side, and a spectator on team 0 may configure all of them. These give the widget graph the
	// three answers it needs without duplicating the actor search in Blueprint.

	/** Team ids in this world that are actually driven by an AI decider. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	TArray<int32> GetAITeamIds() const;

	/** The local player's team id (0 when spectating or unresolved). */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	int32 GetLocalPlayerTeamId() const;

	/**
	 * Teams the local player is allowed to configure: their own team when an AI plays on it, or every AI
	 * team when the player is on team 0. Empty means the panel should stay hidden.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	TArray<int32> GetConfigurableAITeamIds() const;

	/** True when the panel should be shown at all. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	bool ShouldShowRLPanel() const { return GetConfigurableAITeamIds().Num() > 0; }

	/** Brain mode currently used by the AI on that team. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	EBrainMode GetTeamBrainMode(int32 TeamId) const;

	/** Switches that team's AI between the rule/behaviour tree and the trained network. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	void SetTeamBrainMode(int32 TeamId, EBrainMode Mode);

	/** Human-readable one-liner for the panel, e.g. "Team 2 - Behavior Tree - model: none". */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	FString DescribeTeamAI(int32 TeamId) const;

	// The three calls below exist so the widget graph needs no loop of its own. Filling a combo box from
	// an array is a ForEach in Blueprint, and the graph DSL used to edit this widget cannot express one -
	// keeping the iteration in C++ makes each handler a single node.

	/** Clears the combo and fills it with the teams this player may configure, selecting the first. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	void PopulateAITeamCombo(class UComboBoxString* Combo) const;

	/** Team id behind the combo's current selection, or -1 when nothing sensible is selected. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	int32 GetSelectedTeamFromCombo(class UComboBoxString* Combo) const;

	/** Flips that team between rules and the network and returns the new description for the label. */
	UFUNCTION(BlueprintCallable, Category = "AI|RL UI")
	FString ToggleTeamBrainMode(int32 TeamId);

private:
	void FlushBuffer();
	void RegisterConsoleCommands();

	/** Applies the rts.ai.timescale speed-up once a game world exists. */
	void ApplyTimeScaleToWorld(UWorld* World, const UWorld::InitializationValues);

	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> ConfigWidget = nullptr;

	/** Console handles, so the commands disappear with the subsystem instead of dangling into the next PIE run. */
	TArray<IConsoleObject*> ConsoleCommands;

	/** Names this instance registered, so teardown can look them up instead of trusting stale pointers. */
	TArray<FString> RegisteredConsoleCommandNames;

	bool bIsRecording = false;
	int32 SampleCount = 0;
	FString CurrentFilePath;

	/** Lines are batched: one file write per decision would stall the game thread on long sessions. */
	TArray<FString> PendingLines;
	static constexpr int32 FlushThreshold = 64;

	/** Bitmask over ERLSampleSource; every source is enabled by default. */
	uint8 EnabledSourceMask = 0xFF;

	/** Cached directory listing for GetStatus, so a widget may poll it from Tick without hitting the disk. */
	mutable double CachedListingTime = 0.0;
	mutable int32 CachedFileCount = 0;
	mutable float CachedMegabytes = 0.f;

	FCriticalSection BufferLock;
};
