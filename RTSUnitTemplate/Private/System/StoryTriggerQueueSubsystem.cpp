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

UStoryTriggerQueueSubsystem::UStoryTriggerQueueSubsystem()
{
}

void UStoryTriggerQueueSubsystem::Deinitialize()
{
	ClearActive();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextStoryTimerHandle);
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
	}

	QuellenSeitRuhe.Empty();
}

void UStoryTriggerQueueSubsystem::ClearActive()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActiveTimerHandle);
	}

	if (bIsStoryActive)
	{
		bIsStoryActive = false;
		GlobalSoundMultiplier = 1.0f;

		if (CurrentItem.TriggeringSource.IsValid())
		{
			if (AStoryTriggerActor* Actor = Cast<AStoryTriggerActor>(CurrentItem.TriggeringSource.Get()))
			{
				Actor->OnStoryFinished.Broadcast();
			}
			else if (UStoryTriggerComponent* Comp = Cast<UStoryTriggerComponent>(CurrentItem.TriggeringSource.Get()))
			{
				Comp->OnStoryFinished.Broadcast();
			}
		}
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

	if (Item.Sound)
	{
		UGameplayStatics::PlaySound2D(World, Item.Sound);
	}

	ActiveWidget = Widget;

	// Ab hier ist die Story sicher sichtbar und der Schliess-Timer wird unten bewaffnet -
	// erst jetzt darf die uebrige Tonkulisse leiser werden (siehe Hinweis oben).
	bIsStoryActive = true;
	if (Item.TriggeringSource.IsValid())
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
	if (Item.bTillAudioEnds && Item.Sound)
	{
		const float Dur = Item.Sound->GetDuration();
		if (Dur > 0.f && Dur < INDEFINITELY_LOOPING_DURATION)
		{
			DisplayTime = Dur + FMath::Max(0.f, Item.AudioEndExtraDelay);
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
}

void UStoryTriggerQueueSubsystem::OnActiveLifetimeFinished()
{
	ClearActive();

	// Add 3s delay before proceeding to next item
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(NextStoryTimerHandle, this, &UStoryTriggerQueueSubsystem::TryPlayNext, 3.0f, false);
	}
}
