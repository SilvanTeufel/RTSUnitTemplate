// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "StoryTriggerQueueSubsystem.generated.h"

class UStoryWidgetBase;
class USoundBase;
class UTexture2D;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FStoryQueueItem
{
	GENERATED_BODY()

	// Widget class to instantiate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TSubclassOf<UStoryWidgetBase> WidgetClass = nullptr;

	// Text to show
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	FText Text;

	// Optional image (hard reference)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<UTexture2D> Image = nullptr;

	// Optional material (hard reference; takes precedence over Image if set)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	// Optional soft references to reduce cook size. If set, they take precedence over hard refs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TSoftObjectPtr<UTexture2D> ImageSoft;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TSoftObjectPtr<UMaterialInterface> MaterialSoft;

	// Center offsets in pixels
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	// float OffsetX = 0.f;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	// float OffsetY = 0.f;

	// Lifetime in seconds; <= 0 means persist indefinitely
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	float LifetimeSeconds = 0.f;

	// Sound to play when this item becomes active
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<USoundBase> Sound = nullptr;

	// When true, the close timer waits for Sound to finish (+ AudioEndExtraDelay) instead of LifetimeSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	bool bTillAudioEnds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	float AudioEndExtraDelay = 2.5f;

	// Played one after another after Sound (FollowUpGap seconds apart), in the same window - e.g. one
	// recording per sentence of a message. bTillAudioEnds then waits for the last of them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TArray<TObjectPtr<USoundBase>> FollowUpSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	float FollowUpGap = 0.3f;

	UPROPERTY(BlueprintReadWrite, Category = "StoryQueue")
	TWeakObjectPtr<UObject> TriggeringSource = nullptr;
};

UCLASS()
class RTSUNITTEMPLATE_API UStoryTriggerQueueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UStoryTriggerQueueSubsystem();

	// Enqueue a story item to be displayed sequentially for the local player
	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void EnqueueStory(const FStoryQueueItem& Item);

	/**
	 * False when the player turned the story popups off (GameUserSettings.ini [StoryQueue] ShowStoryWidgets,
	 * written by the cassette widget's story button). Stories are then skipped: no window, no voice line;
	 * their sources still get OnStoryFinished, so music they lowered comes back up.
	 */
	UFUNCTION(BlueprintPure, Category="StoryQueue")
	static bool AreStoryWidgetsEnabled();

	// Force remove the currently active widget, if any
	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void ClearActive();

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	float GetGlobalSoundMultiplier() const { return GlobalSoundMultiplier; }

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	float GetDefaultSoundVolume() const { return DefaultSoundVolume; }

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void SetDefaultSoundVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category="StoryQueue")
	float GetMasterVolume() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StoryQueue")
	TObjectPtr<class USoundClass> MasterSoundClass;

	/**
	 * Next random row of Table, drawn like from a shuffled deck: every row comes once, in random
	 * order, before any row repeats, and a new round never starts with the row that ended the last
	 * one. The deck lives for the whole session, so a map change does not start the round over.
	 * NAME_None = no table or no rows.
	 */
	FName DrawRandomRow(const class UDataTable* Table);

	/** DrawRandomRow of the game instance of WorldContext; plain RandRange without one. */
	static FName DrawRandomRowFor(const UObject* WorldContext, const class UDataTable* Table);

protected:
	virtual void Deinitialize() override;

private:
	// Queue of pending items
	UPROPERTY(Transient)
	TArray<FStoryQueueItem> Pending;

	UPROPERTY(Transient)
	FStoryQueueItem CurrentItem;

	bool bIsStoryActive = false;

	/** One shuffled deck per table (DrawRandomRow). Remaining is drawn from the back. */
	struct FRandomRowDeck
	{
		TArray<FName> Remaining;
		FName Last;
	};
	TMap<FSoftObjectPath, FRandomRowDeck> RandomRowDecks;

	/**
	 * Alle Quellen, die seit der letzten Ruhephase eine Story eingereiht haben.
	 *
	 * OnStoryFinished ging bisher NUR an CurrentItem.TriggeringSource. Wer die Musik beim
	 * Ausloesen leiser dreht (BP_StoryTriggerActor_AH ruft SaveAndLowerVolume) und auf
	 * OnStoryFinished wartet, um sie zurueckzudrehen, blieb damit stumm, sobald zwischendurch
	 * eine andere Quelle die aktive wurde - die Musik blieb bis zum Levelwechsel leise.
	 *
	 * Deshalb wird jede einreihende Quelle gemerkt und beim Leerlaufen der Warteschlange
	 * benachrichtigt. RestoreVolume ist idempotent (bVolumeLowered), doppelte Meldungen
	 * schaden also nicht.
	 */
	TArray<TWeakObjectPtr<UObject>> QuellenSeitRuhe;

	/** Meldet allen gemerkten Quellen das Ende und leert die Liste. */
	void StoryEndeAnAlleMelden();

	float GlobalSoundMultiplier = 1.0f;
	float DefaultSoundVolume = 1.0f;

	// Currently active widget
	UPROPERTY(Transient)
	TObjectPtr<UStoryWidgetBase> ActiveWidget = nullptr;

	/** Used when an entry resolves to no display time at all (no sound and LifetimeSeconds 0).
	 *  Without this the close timer was never armed and the queue stalled forever. */
	float FallbackDisplaySeconds = 5.0f;

	FTimerHandle ActiveTimerHandle;
	FTimerHandle NextStoryTimerHandle;

	/**
	 * Seconds after the voice line ends until the music is allowed back up (OnStoryFinished), if no
	 * further story is waiting. Before 05.10.2026 the music only returned when the widget closed -
	 * audio end + AudioEndExtraDelay (2.5 s by default) - which felt like a long gap.
	 */
	float MusicRestoreDelay = 0.5f;
	FTimerHandle MusicRestoreTimerHandle;
	void OnMusicRestoreDue();

	/** CurrentItem.FollowUpSounds: the next one to play and its timer. */
	int32 NextFollowUp = 0;
	FTimerHandle FollowUpTimerHandle;
	void PlayNextFollowUp();

	void TryPlayNext();
	/** Popups turned off: drops what is waiting and, unless a story is still showing, reports the end next tick. */
	void SkipDisabledStories();
	/** Ende der Pause zwischen zwei Storys: gibt den Pausen-Timer frei, dann TryPlayNext. */
	void OnNextStoryDelayFinished();
	void OnActiveLifetimeFinished();
};
