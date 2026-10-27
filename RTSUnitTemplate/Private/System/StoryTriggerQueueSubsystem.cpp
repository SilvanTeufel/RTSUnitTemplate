// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "System/StoryTriggerQueueSubsystem.h"
#include "Widgets/StoryWidgetBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Actors/StoryTriggerActor.h"
#include "Components/StoryTriggerComponent.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundBase.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"

UStoryTriggerQueueSubsystem::UStoryTriggerQueueSubsystem()
{
}

void UStoryTriggerQueueSubsystem::Deinitialize()
{
	ClearActive();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextStoryTimerHandle);
		World->GetTimerManager().ClearTimer(MusicRestoreTimerHandle);
		World->GetTimerManager().ClearTimer(FollowUpTimerHandle);
	}
	Pending.Empty();
	Super::Deinitialize();
}

void UStoryTriggerQueueSubsystem::SetDefaultSoundVolume(float Volume)
{
	DefaultSoundVolume = Volume;
}

void UStoryTriggerQueueSubsystem::SetMasterVolume(float Volume)
{
	if (MasterSoundClass)
	{
		MasterSoundClass->Properties.Volume = Volume;
	}
}

float UStoryTriggerQueueSubsystem::GetMasterVolume() const
{
	if (MasterSoundClass)
	{
		return MasterSoundClass->Properties.Volume;
	}
	return 1.0f;
}

void UStoryTriggerQueueSubsystem::EnqueueStory(const FStoryQueueItem& Item)
{
	Pending.Add(Item);
	UE_LOG(LogTemp, Verbose, TEXT("[StoryQueue] Eingereiht von %s (Ton %s), wartend %d, aktiv %d"), *GetNameSafe(Item.TriggeringSource.Get()),
		*GetNameSafe(Item.Sound), Pending.Num(), ActiveWidget ? 1 : 0);

	// Quelle merken - siehe QuellenSeitRuhe im Header. Wer beim Ausloesen die Musik leiser
	// dreht, muss das Ende auch dann erfahren, wenn zwischendurch eine andere Quelle die
	// aktive geworden ist.
	if (Item.TriggeringSource.IsValid())
	{
		QuellenSeitRuhe.AddUnique(Item.TriggeringSource);
	}

	TryPlayNext();
}

void UStoryTriggerQueueSubsystem::StoryEndeAnAlleMelden()
{
	UE_LOG(LogTemp, Verbose, TEXT("[StoryQueue] Ende an alle melden, %d Quellen seit Ruhe"), QuellenSeitRuhe.Num());
	TSet<UObject*> Gemeldet;
	for (const TWeakObjectPtr<UObject>& Quelle : QuellenSeitRuhe)
	{
		if (!Quelle.IsValid())
		{
			continue;
		}

		if (AStoryTriggerActor* Actor = Cast<AStoryTriggerActor>(Quelle.Get()))
		{
			Actor->OnStoryFinished.Broadcast();
		}
		else if (UStoryTriggerComponent* Comp = Cast<UStoryTriggerComponent>(Quelle.Get()))
		{
			Comp->OnStoryFinished.Broadcast();
		}
		Gemeldet.Add(Quelle.Get());
	}

	QuellenSeitRuhe.Empty();

	// A source that lowered the music can be gone by now (a boss with its trigger, a destroyed
	// actor) - then nobody restored it and the music stayed quiet until the level changed.
	// All trigger actors share the one cassette and RestoreVolume is idempotent, so every
	// trigger actor still in the world is told as well.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AStoryTriggerActor> It(World); It; ++It)
		{
			if (IsValid(*It) && !Gemeldet.Contains(*It))
			{
				It->OnStoryFinished.Broadcast();
			}
		}
	}
}

void UStoryTriggerQueueSubsystem::ClearActive()
{
	UE_LOG(LogTemp, Verbose, TEXT("[StoryQueue] ClearActive, wartend %d"), Pending.Num());
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActiveTimerHandle);
		World->GetTimerManager().ClearTimer(FollowUpTimerHandle);
	}

	// 03.10.2026: OnStoryFinished is NOT sent per item any more while more stories are waiting.
	// The source BPs restore the music on it - mid-queue that raised the music for the next
	// story, and the next story never lowered it again (it had broadcast OnStoryTriggered
	// already when it was enqueued). The end of the whole queue below notifies everyone.
	if (bIsStoryActive)
	{
		bIsStoryActive = false;
		GlobalSoundMultiplier = 1.0f;
	}

	if (ActiveWidget)
	{
		ActiveWidget->SetVisibility(ESlateVisibility::Collapsed);
		ActiveWidget = nullptr;
	}

	// Ist nichts mehr in der Warteschlange, hat die Erzaehlung wirklich geendet: dann bekommen
	// ALLE Quellen ihr OnStoryFinished, nicht nur die zuletzt aktive. Ohne das blieb die von
	// einer frueheren Quelle abgesenkte Musik leise (siehe QuellenSeitRuhe im Header).
	if (Pending.Num() == 0)
	{
		StoryEndeAnAlleMelden();
	}
}

void UStoryTriggerQueueSubsystem::TryPlayNext()
{
	if (ActiveWidget)
	{
		return; // Already showing something
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(NextStoryTimerHandle))
	{
		return; // Waiting for delay
	}

	if (Pending.Num() == 0)
	{
		return;
	}

	CurrentItem = Pending[0];
	Pending.RemoveAt(0);

	FStoryQueueItem Item = CurrentItem;

	// Fallback: if no widget class provided, default to base class
	if (!Item.WidgetClass)
	{
		Item.WidgetClass = UStoryWidgetBase::StaticClass();
	}

	// BEHEBUNG (22.08.2026): Absenken der Lautstaerke erst setzen, wenn die Story auch
	// wirklich laeuft.
	//
	// Frueher standen bIsStoryActive = true und GlobalSoundMultiplier = LowerVolume schon hier,
	// also VOR zwei Ausstiegen: dem fehlenden PlayerController direkt darunter und dem Fall,
	// dass kein Widget zustande kommt. Wurde einer davon genommen, blieb die Lautstaerke auf
	// 0.4 stehen und es war weder ein Widget noch ein Schliess-Timer da, der sie je wieder
	// hochgesetzt haette - ClearActive() wird nur ueber diesen Timer erreicht. Die Musik blieb
	// dann bis zum Levelwechsel leise.
	//
	// Beides wird deshalb erst unten gesetzt, direkt beim Bewaffnen des Timers, der es auch
	// wieder abbaut. Zustand und sein Abbau gehoeren zusammen.

	// Ein Story-Widget gehoert in einen LOKALEN Viewport. GetPlayerController(World, 0) liefert
	// auf einem Dedicated Server aber den Controller des ENTFERNTEN Spielers - dort ist kein
	// Controller lokal. CreateWidget lehnt den dann ab ("Only Local Player Controllers can be
	// assigned to widgets") und die Story bleibt trotzdem als laufend vermerkt. Deshalb gezielt
	// den ersten lokalen Controller holen und ohne einen solchen gar nicht erst anfangen.
	APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	if (!PC)
	{
		return;
	}

	UStoryWidgetBase* Widget = nullptr;
	if (AExtendedCameraBase* Cam = Cast<AExtendedCameraBase>(PC->GetPawn()))
	{
		if (Cam->StoryWidget)
		{
			Widget = Cam->StoryWidget;
		}
	}

	if (!Widget)
	{
		Widget = CreateWidget<UStoryWidgetBase>(PC, Item.WidgetClass);
		if (Widget)
		{
			Widget->AddToViewport();
			Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

			int32 SizeX = 0, SizeY = 0;
			PC->GetViewportSize(SizeX, SizeY);
			const FVector2D CenterPos(0.5f * SizeX, 0.5f * SizeY);
			Widget->SetPositionInViewport(CenterPos, false);
		}
	}

	if (!Widget)
	{
		TryPlayNext();
		return;
	}

	// Resolve soft references (prefer soft over hard)
	UTexture2D* ResolvedTexture = Item.Image;
	UMaterialInterface* ResolvedMaterial = Item.Material;
	if (Item.MaterialSoft.IsValid() || Item.MaterialSoft.ToSoftObjectPath().IsValid())
	{
		ResolvedMaterial = Item.MaterialSoft.LoadSynchronous();
	}
	if (!ResolvedMaterial && (Item.ImageSoft.IsValid() || Item.ImageSoft.ToSoftObjectPath().IsValid()))
	{
		ResolvedTexture = Item.ImageSoft.LoadSynchronous();
	}

	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->StartStory(Item.Text, ResolvedTexture, ResolvedMaterial);

	// The length of all audio of this item: Sound, then each follow-up after FollowUpGap. 0 = none / looping.
	float AudioSeconds = 0.f;
	if (Item.Sound)
	{
		UGameplayStatics::PlaySound2D(World, Item.Sound);
		AudioSeconds = Item.Sound->GetDuration();
		const float Gap = FMath::Max(0.f, Item.FollowUpGap);
		for (const USoundBase* FollowUp : Item.FollowUpSounds)
		{
			const float Dur = FollowUp ? FollowUp->GetDuration() : 0.f;
			if (Dur > 0.f && Dur < INDEFINITELY_LOOPING_DURATION && AudioSeconds < INDEFINITELY_LOOPING_DURATION)
			{
				AudioSeconds += Gap + Dur;
			}
		}
		NextFollowUp = 0;
		World->GetTimerManager().ClearTimer(FollowUpTimerHandle);
		if (Item.FollowUpSounds.Num() > 0 && AudioSeconds > 0.f && Item.Sound->GetDuration() < INDEFINITELY_LOOPING_DURATION)
		{
			World->GetTimerManager().SetTimer(FollowUpTimerHandle, this, &UStoryTriggerQueueSubsystem::PlayNextFollowUp,
				FMath::Max(Item.Sound->GetDuration() + Gap, 0.01f), false);
		}
	}

	ActiveWidget = Widget;

	// Ab hier ist die Story sicher sichtbar und der Schliess-Timer wird unten bewaffnet -
	// erst jetzt darf die uebrige Tonkulisse leiser werden (siehe Hinweis oben).
	bIsStoryActive = true;
	if (!Item.Sound)
	{
		// Nothing is spoken: the other voices stay as loud as they are.
		GlobalSoundMultiplier = 1.0f;
	}
	else if (Item.TriggeringSource.IsValid())
	{
		if (AStoryTriggerActor* Actor = Cast<AStoryTriggerActor>(Item.TriggeringSource.Get()))
		{
			GlobalSoundMultiplier = Actor->LowerVolume;
		}
		else if (UStoryTriggerComponent* Comp = Cast<UStoryTriggerComponent>(Item.TriggeringSource.Get()))
		{
			GlobalSoundMultiplier = Comp->LowerVolume;
		}
	}
	else
	{
		GlobalSoundMultiplier = 0.4f; // Default lowering
	}

	// Close-timer: by default keep the widget open until the audio finishes (+ AudioEndExtraDelay).
	// Fall back to the fixed LifetimeSeconds when bTillAudioEnds is off, there is no sound, or the
	// sound loops indefinitely (GetDuration returns INDEFINITELY_LOOPING_DURATION).
	float DisplayTime = Item.LifetimeSeconds;
	World->GetTimerManager().ClearTimer(MusicRestoreTimerHandle);
	if (Item.bTillAudioEnds && Item.Sound)
	{
		const float Dur = AudioSeconds;
		if (Dur > 0.f && Dur < INDEFINITELY_LOOPING_DURATION)
		{
			DisplayTime = Dur + FMath::Max(0.f, Item.AudioEndExtraDelay);
			// The text may stay up a little longer for reading; the music comes back shortly after
			// the voice ends (the cassette fades it up).
			World->GetTimerManager().SetTimer(MusicRestoreTimerHandle, this,
				&UStoryTriggerQueueSubsystem::OnMusicRestoreDue, Dur + MusicRestoreDelay, false);
		}
	}

	// A close timer must ALWAYS be armed. FStoryQueueItem::LifetimeSeconds defaults to 0 and
	// bTillAudioEnds to true, so any entry without a sound (or with an indefinitely looping one)
	// ended up here with DisplayTime == 0. The old code then simply skipped the timer, ActiveWidget
	// stayed set forever, and TryPlayNext() bailed out at its first check - one silent entry
	// blocked the whole rest of the queue and nobody spoke again.
	if (DisplayTime <= 0.f)
	{
		DisplayTime = FallbackDisplaySeconds;
	}
	World->GetTimerManager().SetTimer(ActiveTimerHandle, this, &UStoryTriggerQueueSubsystem::OnActiveLifetimeFinished, DisplayTime, false);
	UE_LOG(LogTemp, Verbose, TEXT("[StoryQueue] Zeige von %s (Ton %s) fuer %.1f s, wartend %d"), *GetNameSafe(Item.TriggeringSource.Get()),
		*GetNameSafe(Item.Sound), DisplayTime, Pending.Num());
}

void UStoryTriggerQueueSubsystem::PlayNextFollowUp()
{
	UWorld* World = GetWorld();
	if (!World || !ActiveWidget)
	{
		return;
	}
	// Skip empty or unplayable entries; play the next one and arm the timer for the one after it.
	while (CurrentItem.FollowUpSounds.IsValidIndex(NextFollowUp))
	{
		USoundBase* Sound = CurrentItem.FollowUpSounds[NextFollowUp++];
		const float Dur = Sound ? Sound->GetDuration() : 0.f;
		if (Dur <= 0.f || Dur >= INDEFINITELY_LOOPING_DURATION)
		{
			continue;
		}
		UGameplayStatics::PlaySound2D(World, Sound);
		if (CurrentItem.FollowUpSounds.IsValidIndex(NextFollowUp))
		{
			World->GetTimerManager().SetTimer(FollowUpTimerHandle, this, &UStoryTriggerQueueSubsystem::PlayNextFollowUp,
				Dur + FMath::Max(0.f, CurrentItem.FollowUpGap), false);
		}
		return;
	}
}

void UStoryTriggerQueueSubsystem::OnMusicRestoreDue()
{
	// Only when nothing else is waiting - otherwise the next line follows and the music stays down.
	// ClearActive later notifies again; RestoreVolume ignores the repeat.
	if (Pending.Num() == 0)
	{
		StoryEndeAnAlleMelden();
	}
}

void UStoryTriggerQueueSubsystem::OnNextStoryDelayFinished()
{
	// 19.10.2026: Der Pausen-Timer rief TryPlayNext direkt auf. Waehrend seines eigenen Rueckrufs
	// meldet IsTimerActive ihn aber noch als aktiv (Status Executing) - TryPlayNext stieg an der
	// Pausen-Pruefung aus, und die naechste Story blieb fuer immer liegen. Mit nicht leerer
	// Warteschlange kam auch StoryEndeAnAlleMelden nie: die Musik blieb leise (gesehen bei der
	// WinCondition-Ansage von Cmdr. Vael direkt nach einem Philosophy-Spruch).
	NextStoryTimerHandle.Invalidate();
	TryPlayNext();
}

void UStoryTriggerQueueSubsystem::OnActiveLifetimeFinished()
{
	ClearActive();

	// Add 3s delay before proceeding to next item
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(NextStoryTimerHandle, this, &UStoryTriggerQueueSubsystem::OnNextStoryDelayFinished, 3.0f, false);
	}
}
