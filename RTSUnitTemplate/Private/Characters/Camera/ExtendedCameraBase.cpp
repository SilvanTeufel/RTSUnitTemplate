// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Camera/ExtendedCameraBase.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Actors/WinLoseConfigActor.h"

#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/WorkingUnitBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameModes/ResourceGameMode.h"
#include "GameStates/ResourceGameState.h"
#include "GameStates/UpgradeGameState.h"
#include "GAS/GAS.h"
#include "Widgets/AbilityChooser.h"
#include "Widgets/ResourceWidget.h"
#include "Widgets/TaggedUnitSelector.h"
#include "Widgets/TalentChooser.h"
#include "Widgets/UnitWidgetSelector.h"
#include "Widgets/AttributeTreeWidget.h"
#include "Characters/Unit/LevelUnit.h"
#include "Widgets/SoundControlWidget.h"
#include "Widgets/WinConditionWidget.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

AExtendedCameraBase::AExtendedCameraBase(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	if (RootComponent == nullptr) {
		RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	}
	
	CreateCameraComp();
	
	GetCameraBaseCapsule()->BodyInstance.bLockXRotation = true;
	GetCameraBaseCapsule()->BodyInstance.bLockYRotation = true;
	GetCameraBaseCapsule()->BodyInstance.bLockZRotation = true;
	
	
		UCapsuleComponent* CComponent = GetCapsuleComponent();
		if (CComponent)
		{
			CComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);  // Enable both physics and overlap query
			CComponent->SetCollisionResponseToAllChannels(ECR_Ignore);  // Start by ignoring all channels
			CComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);  // Block other pawns (this can be adjusted based on your requirements)
			CComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  // Important: Block WorldStatic so it can walk on static objects like ground, walls, etc.
			CComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);  // ECR_Overlap Overlap with dynamic objects (adjust as needed)
		}

	
		UMeshComponent* CMesh = GetMesh();
		if(CMesh)
		{
			CMesh->AttachToComponent(GetCameraBaseCapsule(), FAttachmentTransformRules::KeepRelativeTransform);
			CMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // Typically, we use the capsule for physics and mesh for simple queries like overlap
			CMesh->SetCollisionResponseToAllChannels(ECR_Ignore);  // Start by ignoring all channels
			CMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);  // Overlap with other pawns
			CMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);  // Overlap with dynamic objects
		}
	
	bReplicates = true;
	SetNetUpdateFrequency(1);
	SetMinNetUpdateFrequency(1);
	SetReplicates(true);
	SetReplicatingMovement(false);

	for (int i = 0; i < 6; i++)
	{
		FKeyHoldTimes[i] = 0.f;
		bFKeyTagAssigned[i] = false;
		bFKeyPressed[i] = false;
	}
}

// BeginPlay implementation
void AExtendedCameraBase::BeginPlay()
{
	// Call the base class BeginPlay
	Super::BeginPlay();

	ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetController());
	if (MyPC)
	{
		MyPC->OnTeamIdChanged.AddDynamic(this, &AExtendedCameraBase::OnTeamIdChanged_Internal);
		
		// If team ID is already valid, trigger search
		if (MyPC->SelectableTeamId != -1)
		{
			OnTeamIdChanged_Internal(MyPC->SelectableTeamId);
		}
	}
}

void AExtendedCameraBase::OnTeamIdChanged_Internal(int32 NewTeamId)
{
	InitializeWinConditionDisplay();
}

bool AExtendedCameraBase::InitializeWinConditionDisplay()
{
	ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetController());
	int32 MyTeamId = MyPC ? MyPC->SelectableTeamId : -1;

	// Always bind to all config actors' changes
	for (TActorIterator<AWinLoseConfigActor> It(GetWorld()); It; ++It)
	{
		AWinLoseConfigActor* Config = *It;
		if (Config)
		{
			Config->OnWinConditionChanged.RemoveDynamic(this, &AExtendedCameraBase::OnWinConditionChanged);
			Config->OnWinConditionChanged.AddDynamic(this, &AExtendedCameraBase::OnWinConditionChanged);
			Config->OnTagProgressUpdated.RemoveDynamic(this, &AExtendedCameraBase::OnTagProgressUpdated);
			Config->OnTagProgressUpdated.AddDynamic(this, &AExtendedCameraBase::OnTagProgressUpdated);
		}
	}

	AWinLoseConfigActor* TargetConfig = AWinLoseConfigActor::GetWinLoseConfigForTeam(this, MyTeamId);
	if (TargetConfig)
	{
		float Delay = TargetConfig->InitialDisplayDelay;
		float Duration = TargetConfig->GameStartDisplayDuration;

		// Prolong initial delay while the LoadingWidget is active
		if (UWorld* World = GetWorld())
		{
			if (AResourceGameState* GS = World->GetGameState<AResourceGameState>())
			{
				const float Now = GS->GetServerWorldTimeSeconds();
				float ExtraDelay = 0.f;
				if (GS->MatchStartTime > 0.f)
				{
					ExtraDelay = FMath::Max(0.f, GS->MatchStartTime - Now);
				}
				else if (GS->LoadingWidgetConfig.Duration > 0.f)
				{
					// Fallback if MatchStartTime isn't set yet
					const float EndTime = GS->LoadingWidgetConfig.ServerWorldTimeStart + GS->LoadingWidgetConfig.Duration;
					ExtraDelay = FMath::Max(0.f, EndTime - Now);
				}
				Delay += ExtraDelay;
			}
		}

		GetWorldTimerManager().ClearTimer(InitialWinConditionDelayTimerHandle);
		
		if (Delay > 0)
		{
			TWeakObjectPtr<AExtendedCameraBase> WeakThis(this);
			GetWorldTimerManager().SetTimer(InitialWinConditionDelayTimerHandle, [WeakThis, Duration]()
			{
				if (AExtendedCameraBase* StrongThis = WeakThis.Get())
				{
					StrongThis->ShowWinConditionWidget(Duration);
				}
			}, Delay, false);
		}
		else
		{
			ShowWinConditionWidget(Duration);
		}
		return true;
	}
	return false;
}

void AExtendedCameraBase::ShowWinConditionWidget(float Duration)
{
	if (WinConditionWidget)
	{
		WinConditionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        
		GetWorldTimerManager().ClearTimer(WinConditionDisplayTimerHandle);
		if (Duration > 0)
		{
			GetWorldTimerManager().SetTimer(WinConditionDisplayTimerHandle, this, &AExtendedCameraBase::HideWinConditionWidget, Duration, false);
		}
	}
}

void AExtendedCameraBase::HideWinConditionWidget()
{
	if (WinConditionWidget != nullptr && TabMode != 3)
	{
		WinConditionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AExtendedCameraBase::OnWinConditionChanged(AWinLoseConfigActor* Config, EWinLoseCondition NewCondition)
{
	if (!Config) return;

	ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetController());
	int32 MyTeamId = MyPC ? MyPC->SelectableTeamId : -1;

	// Only show if the config is relevant to us
	if (Config->TeamId == MyTeamId || Config->TeamId == 0)
	{
		ShowWinConditionWidget(Config->WinConditionDisplayDuration);
	}
}

void AExtendedCameraBase::OnTagProgressUpdated(AWinLoseConfigActor* Config)
{
	if (!Config) return;

	ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetController());
	int32 MyTeamId = MyPC ? MyPC->SelectableTeamId : -1;

	// Only show if the config is relevant to us
	if (Config->TeamId == MyTeamId || Config->TeamId == 0)
	{
		ShowWinConditionWidget(Config->WinConditionDisplayDuration);
	}
}

void AExtendedCameraBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Slate-Inhalt am Viewport ueberlebt den Aktor - ohne das bliebe der Hinweis nach dem
	// Verlassen der Karte stehen.
	if (TabHintSlateWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(TabHintSlateWidget.ToSharedRef());
	}
	TabHintSlateWidget.Reset();

	Super::EndPlay(EndPlayReason);
}

void AExtendedCameraBase::ShowStartScreen()
{
	TabMode = 1;
	UpdateTabModeUI();
}

void AExtendedCameraBase::UpdateTabHint()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

	// Nur die Ansichten mit eigenem Fenster bekommen den Hinweis.
	//
	// ACHTUNG, hier lag der Fehler vom 19.09.2026: TabMode startet bei 1 (siehe Header und
	// ACustomControllerBase, das ebenfalls auf 1 zuruecksetzt), NICHT bei 0. Die Pruefung
	// "!= 0" blendete den Hinweis deshalb ausgerechnet auf der Startansicht ein.
	//
	// Modus 0 ist NICHT leer: der default-Zweig in UpdateTabModeUI zeigt dort Ability- und
	// TalentChooser. Nur Modus 1 ist die Startansicht mit der blossen Ressourcenleiste.
	const bool bShowHint = (TabMode != 1);

	if (!bShowHint)
	{
		if (TabHintSlateWidget.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(TabHintSlateWidget.ToSharedRef());
			TabHintSlateWidget.Reset();
		}
		return;
	}

	if (TabHintSlateWidget.IsValid())
	{
		return; // steht bereits
	}

	FSlateFontInfo HintFont;
	if (UObject* FontObject = TabHintFontPath.TryLoad())
	{
		HintFont = FSlateFontInfo(FontObject, TabHintFontSize);
	}
	else
	{
		HintFont = FCoreStyle::GetDefaultFontStyle("Regular", TabHintFontSize);
	}
	HintFont.LetterSpacing = 80;

	// Der Knopf braucht eine eigene Referenz auf den Aktor, die ihn nicht am Leben haelt.
	TWeakObjectPtr<AExtendedCameraBase> WeakSelf(this);

	TSharedRef<STextBlock> HintText =
		SNew(STextBlock)
		.Text(TabHintText)
		.Font(HintFont)
		.ColorAndOpacity(FSlateColor(TabHintColor));

	// Der Text schluckt keine Klicks, der Knopf darunter schon.
	HintText->SetVisibility(EVisibility::HitTestInvisible);

	TSharedRef<SWidget> HintBlock =
		SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(TabHintPadding)
		[
			SNew(SVerticalBox)

			// Zurueck zur Startansicht - auf JEDEM Tab-Screen erreichbar, ohne dass dafuer in
			// jedes HUD-Widget ein eigener Knopf muss.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ContentPadding(FMargin(14.f, 5.f))
				.ButtonColorAndOpacity(FLinearColor(0.010f, 0.015f, 0.027f, 0.85f))
				.OnClicked_Lambda([WeakSelf]() -> FReply
				{
					if (AExtendedCameraBase* Camera = WeakSelf.Get())
					{
						Camera->ShowStartScreen();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(TabBackText)
					.Font(HintFont)
					.ColorAndOpacity(FSlateColor(TabHintColor))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[
				HintText
			]
		];

	TabHintSlateWidget = HintBlock;

	// ZOrder hoch genug, um ueber den HUD-Panels zu liegen. Das MainHUD kommt mit AddToViewport()
	// ohne Angabe (= 0) herein, einzelne Fenster mit 1000. Mit den urspruenglichen 100 verschwand
	// der Hinweis hinter dem AbilityChooser der Control-Ansicht - genau so gemeldet.
	// Die Ladebilder liegen bei 9999 und bleiben damit weiterhin oben.
	GEngine->GameViewport->AddViewportWidgetContent(HintBlock, 5000);
}

void AExtendedCameraBase::ShowChoosersForTabMode(AUnitBase* TargetUnit)
{
	SetUserWidget(TargetUnit);

	// SetUserWidget blendet den AbilityChooser seit dem 19.09.2026 NICHT mehr ein: es haengt
	// ueber AControllerBase::SetWidgets am Auswahlknopf, und dadurch sprang der Chooser bei
	// jedem Klick auf eine Einheit auf. Ueber Tab ist das Aufklappen aber genau gewollt -
	// deshalb hier, und nur hier, ausdruecklich. Ohne diese Zeilen blieb die Ability-Ansicht
	// (Tab-Modus 0) leer, obwohl sie geblurrt war.
	if (TargetUnit && AbilityChooserWidget)
	{
		AbilityChooserWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void AExtendedCameraBase::UpdateTabModeUI()
{
	// Die Ressourcenleiste bleibt in JEDEM Tab-Modus sichtbar (02.09.2026). Vorher wurde sie
	// hier ausgeblendet und nur in Modus 1 wieder eingeschaltet - dadurch verschwand die
	// wichtigste Anzeige des Spiels, sobald man einmal weitergeschaltet hat.
	if (ResourceWidget)
	{
		ResourceWidget->SetVisibility(ESlateVisibility::Visible);
		ResourceWidget->StartUpdateTimer();
	}
	if (WinConditionWidget)
	{
		WinConditionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MapMenuWidget)
	{
		MapMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AttributeTreeWidget)
	{
		AttributeTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	HideControlWidget();
	SetUserWidget(nullptr);

	
	switch (TabMode)
	{
	case 1: // ResourceWidget
		if (ResourceWidget)
		{
			ResourceWidget->StartUpdateTimer();
			ResourceWidget->SetVisibility(ESlateVisibility::Visible);
			TabToggled = false;
		}
		break;
	case 2: // ControlWidget
		{
			ShowControlWidget();
			TabToggled = true;

			ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
			if (CameraControllerBase && CameraControllerBase->HUDBase && CameraControllerBase->HUDBase->SelectedUnits.Num())
			{
				AUnitBase* SelectedUnit = CameraControllerBase->HUDBase->SelectedUnits[0];
				ShowChoosersForTabMode(SelectedUnit);
			}
		}
		break;
	case 3:
		{
			// WinConditionWidget
			if (WinConditionWidget)
			{
				WinConditionWidget->SetVisibility(ESlateVisibility::Visible);
			}

			ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
			if (CameraControllerBase && CameraControllerBase->HUDBase && CameraControllerBase->HUDBase->SelectedUnits.Num())
			{
				AUnitBase* SelectedUnit = CameraControllerBase->HUDBase->SelectedUnits[0];
				ShowChoosersForTabMode(SelectedUnit);
			}
			TabToggled = true;
		}
		break;
	case 4:
		{
			// AttributeTreeWidget (radial attribute tree). Wired from the MainHUD BP (SetAttributeTreeWidget).
			if (AttributeTreeWidget)
			{
				ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
				AUnitBase* TargetUnit = nullptr;
				if (CameraControllerBase && CameraControllerBase->HUDBase && CameraControllerBase->HUDBase->SelectedUnits.Num())
				{
					TargetUnit = CameraControllerBase->HUDBase->SelectedUnits[0];
				}

				// No unit selected -> auto-target the first owned unit that has an attribute tree, so tab 4
				// always shows a populated tree instead of an empty panel.
				if (!TargetUnit)
				{
					const int32 MyTeam = CameraControllerBase ? CameraControllerBase->SelectableTeamId : -1;
					for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
					{
						AUnitBase* U = *It;
						if (U && U->AttributeTreeDataTable && (MyTeam < 0 || U->TeamId == MyTeam))
						{
							TargetUnit = U;
							break;
						}
					}
				}

				AttributeTreeWidget->SetTargetUnit(TargetUnit);
				AttributeTreeWidget->SetVisibility(ESlateVisibility::Visible);
			}
			TabToggled = true;
		}
		break;
	default:
		{
			// Modus 0 war bisher leer - eine Tab-Stufe, die nichts anzeigte. Hier stehen jetzt
			// AbilityChooser und TalentChooser. Zieleinheit wie in Modus 4: die Auswahl, sonst
			// die erste eigene Einheit mit einem Talentbaum, damit die Panels nie leer sind.
			ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
			AUnitBase* TargetUnit = nullptr;
			if (CameraControllerBase && CameraControllerBase->HUDBase && CameraControllerBase->HUDBase->SelectedUnits.Num())
			{
				TargetUnit = CameraControllerBase->HUDBase->SelectedUnits[0];
			}
			if (!TargetUnit)
			{
				const int32 MyTeam = CameraControllerBase ? CameraControllerBase->SelectableTeamId : -1;
				for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
				{
					AUnitBase* U = *It;
					if (U && U->AttributeTreeDataTable && (MyTeam < 0 || U->TeamId == MyTeam))
					{
						TargetUnit = U;
						break;
					}
				}
			}

			ShowChoosersForTabMode(TargetUnit);
			TabToggled = TargetUnit != nullptr;
		}
		break;
	}

	UpdateViewportBlur(TabToggled);
	UpdateTabHint();
}

void AExtendedCameraBase::CloseMapMenu()
{
	if (!MapMenuWidget) return;

	MapMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	BlockControls = false;
	UpdateViewportBlur(TabToggled);
}

void AExtendedCameraBase::UpdateViewportBlur(bool bEnable)
{
	if (!CameraComp)
	{
		return;
	}

	FPostProcessSettings& PP = CameraComp->PostProcessSettings;

	// Master designer switch: if blur is globally disabled, force the "off" path
	// regardless of what the caller requested (Tab / menus then never blur).
	const bool bApply = bEnable && BlurSettings.bBlurEnabled;

	// Keep every bOverride_* flag in lock-step with bApply so that turning the effect
	// off fully restores the camera's baseline post-process (nothing left half-overridden).
	PP.bOverride_DepthOfFieldFocalDistance   = bApply;
	PP.bOverride_DepthOfFieldFstop           = bApply;
	PP.bOverride_DepthOfFieldMinFstop        = bApply;
	PP.bOverride_DepthOfFieldSensorWidth     = bApply;
	PP.bOverride_DepthOfFieldDepthBlurAmount = bApply;
	PP.bOverride_DepthOfFieldDepthBlurRadius = bApply;

	// Mobile DoF transition-region overrides. Inert under the desktop cinematic DoF
	// method, but toggled consistently so their override state never desyncs.
	PP.bOverride_DepthOfFieldFocalRegion          = bApply;
	PP.bOverride_DepthOfFieldNearTransitionRegion = bApply;
	PP.bOverride_DepthOfFieldFarTransitionRegion  = bApply;

	if (bApply)
	{
		// --- Cinematic (Circle-of-Confusion) DoF ---
		PP.DepthOfFieldFocalDistance = BlurSettings.DepthOfFieldFocalDistance;
		PP.DepthOfFieldFstop         = BlurSettings.DepthOfFieldFstop;
		PP.DepthOfFieldMinFstop      = BlurSettings.DepthOfFieldMinFstop;
		PP.DepthOfFieldSensorWidth   = BlurSettings.DepthOfFieldSensorWidth;

		// --- Focal-plane-independent distance ("depth") blur ---
		PP.DepthOfFieldDepthBlurAmount = BlurSettings.DepthOfFieldDepthBlurAmount;
		PP.DepthOfFieldDepthBlurRadius = BlurSettings.DepthOfFieldDepthBlurRadius;

		// --- Mobile DoF transition regions (kept for completeness / old-look repro) ---
		PP.DepthOfFieldFocalRegion          = BlurSettings.DepthOfFieldFocalRegion;
		PP.DepthOfFieldNearTransitionRegion = BlurSettings.DepthOfFieldNearTransitionRegion;
		PP.DepthOfFieldFarTransitionRegion  = BlurSettings.DepthOfFieldFarTransitionRegion;
	}
	// When !bApply, all bOverride_* are false above, so the camera reverts to its baseline
	// post-process and the field values below are irrelevant — no need to zero them.
}

void AExtendedCameraBase::Client_UpdateWidgets_Implementation(UUnitWidgetSelector* NewWidgetSelector, UTaggedUnitSelector* NewTaggedSelector, UResourceWidget* NewResourceWidget)
{

	if (NewWidgetSelector)
	{
		UnitSelectorWidget = NewWidgetSelector;
	}
	

	if (NewTaggedSelector)
	{
		TaggedSelectorWidget = NewTaggedSelector;

	}

	if (NewResourceWidget)
	{
		ResourceWidget = NewResourceWidget;
	}

	UpdateTabModeUI();
}

bool AExtendedCameraBase::SetupResourceWidget(AExtendedControllerBase* CameraControllerBase)
{
		if(ResourceWidget)
		{
			if(CameraControllerBase)
			{
				ResourceWidget->SetTeamId(CameraControllerBase->SelectableTeamId);
				ResourceWidget->StartUpdateTimer();
				return true;
			}
		}
		return false;
}

// Tick implementation
void AExtendedCameraBase::Tick(float DeltaTime)
{
	// Call the base class Tick
	Super::Tick(DeltaTime);

	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if (CameraControllerBase)
	{
		for (int i = 0; i < 6; i++)
		{
			if (bFKeyPressed[i])
			{
				FKeyHoldTimes[i] += DeltaTime;
				if (FKeyHoldTimes[i] > TagTime && !bFKeyTagAssigned[i])
				{
					FGameplayTag Tag;
					switch(i)
					{
					case 0: Tag = CameraControllerBase->KeyTagF1; break;
					case 1: Tag = CameraControllerBase->KeyTagF2; break;
					case 2: Tag = CameraControllerBase->KeyTagF3; break;
					case 3: Tag = CameraControllerBase->KeyTagF4; break;
					case 4: Tag = CameraControllerBase->KeyTagF5; break;
					case 5: Tag = CameraControllerBase->KeyTagF6; break;
					}

					if (Tag.IsValid())
					{
						CameraControllerBase->Server_AssignTagToSelectedUnits(Tag, CameraControllerBase->SelectedUnits, CameraControllerBase->SelectableTeamId);
					}
					bFKeyTagAssigned[i] = true;
				}
			}
		}
	}
}


bool AExtendedCameraBase::IsOwnedByLocalPlayer()
{
	APlayerController* MyController = Cast<APlayerController>(GetController());
	return MyController && MyController->IsLocalPlayerController();
}

void AExtendedCameraBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponentBase* EIC = Cast<UEnhancedInputComponentBase>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}

	BindCameraInputActions(EIC);
	BindGameplayInputActions(EIC);
}

void AExtendedCameraBase::BindCameraInputActions(UEnhancedInputComponentBase* EnhancedInputComponentBase)
{
	if (!EnhancedInputComponentBase)
	{
		return;
	}

	const FGameplayTags& GameplayTags = FGameplayTags::Get();

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Space_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 7);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Space_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 8);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_W_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 1);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_W_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 111);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_S_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_S_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 222);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_A_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 3);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_A_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 333);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_D_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 4);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_D_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 444);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_X_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 5);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_X_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 555);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Y_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 6);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Y_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 666);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Q_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 9);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_E_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 10);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Q_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 999);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_E_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 101010);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_P_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 14);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D1, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 13);
	//EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D2, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 13);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Middle_Mouse_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 16);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Middle_Mouse_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 17);
}

void AExtendedCameraBase::BindGameplayInputActions(UEnhancedInputComponentBase* EnhancedInputComponentBase)
{
	if (!EnhancedInputComponentBase)
	{
		return;
	}

	const FGameplayTags& GameplayTags = FGameplayTags::Get();

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Tab_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Tab_Pressed, 0);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Tab_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Tab_Released, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_V_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_V_Pressed, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_LeftClick_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_LeftClick_Pressed, 0);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_LeftClick_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_LeftClick_Released, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_RightClick_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_RightClick_Pressed, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_G_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_G_Pressed, 0);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_A_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_A_Pressed, 0);
	// Needs the CPressed InputAction in ControlAsset + a C key row in IMC_Controls. BindActionByTag
	// returns silently when the tag has no entry in the InputConfig, so a missing asset row shows
	// up as "the key does nothing" with no log at all.
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_C_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_C_Pressed, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Shift_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Shift_Pressed, 0);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Shift_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Shift_Released, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Ctrl_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Ctrl_Pressed, 0);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Ctrl_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Ctrl_Released, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_R_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 18);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_T_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 12);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_O_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 15);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_1_Pressed, ETriggerEvent::Started, this, &AExtendedCameraBase::SwitchControllerStateMachine, 21);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_2_Pressed, ETriggerEvent::Started, this, &AExtendedCameraBase::SwitchControllerStateMachine, 22);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_3_Pressed, ETriggerEvent::Started, this, &AExtendedCameraBase::SwitchControllerStateMachine, 23);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_4_Pressed, ETriggerEvent::Started, this, &AExtendedCameraBase::SwitchControllerStateMachine, 24);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_5_Pressed, ETriggerEvent::Started, this, &AExtendedCameraBase::SwitchControllerStateMachine, 25);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_6_Pressed, ETriggerEvent::Started, this, &AExtendedCameraBase::SwitchControllerStateMachine, 26);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_1_Released, ETriggerEvent::Completed, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2121);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_2_Released, ETriggerEvent::Completed, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2222);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_3_Released, ETriggerEvent::Completed, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2323);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_4_Released, ETriggerEvent::Completed, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2424);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_5_Released, ETriggerEvent::Completed, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2525);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_6_Released, ETriggerEvent::Completed, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2626);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F1_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 27);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F2_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 28);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F3_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 29);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F4_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 30);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F5_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 31);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F6_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 32);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F1_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2727);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F2_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2828);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F3_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 2929);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F4_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 3030);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F5_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 3131);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_F6_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 3232);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Alt_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Alt_Pressed, 0);
	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Alt_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Alt_Released, 0);

	EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Esc_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Esc_Pressed, 0);
}

void AExtendedCameraBase::SetUserWidget(AUnitBase* SelectedActor)
{
	
	if(!TalentChooserWidget) return;

	// Liegt der Attributbaum (Tab 4) vorn, darf eine Auswahl die Chooser NICHT hochholen.
	//
	// SetUserWidget lief bei jeder Auswahlaenderung und setzte beide Chooser hart auf Visible -
	// die Sichtbarkeit, die das HUD-Blueprint ueber die Tabs steuert, wurde damit ueberschrieben.
	// Beim schnellen Klicken im Attributbaum wechselt die Anzeigeeinheit staendig, und der
	// AbilityChooser sprang jedes Mal in den Vordergrund. Am 19.09.2026 genau so gemeldet.
	//
	// Owner und Aktualisierungstakt werden weiter gesetzt - nur die Sichtbarkeit bleibt dann in
	// der Hand der Tab-Umschaltung. Auf den Tabs 1 bis 3 aendert sich nichts.
	const bool bAttributeTreeInFront = AttributeTreeWidget
		&& AttributeTreeWidget->GetVisibility() != ESlateVisibility::Collapsed
		&& AttributeTreeWidget->GetVisibility() != ESlateVisibility::Hidden;

	if(SelectedActor)
	{
		if (TalentChooserWidget) {
			if (!bAttributeTreeInFront)
			{
				TalentChooserWidget->SetVisibility(ESlateVisibility::Visible);
			}
			TalentChooserWidget->SetOwnerActor(SelectedActor);
			TalentChooserWidget->CreateClassUIElements();
			TalentChooserWidget->StartUpdateTimer();
		}

		if (AbilityChooserWidget) {
			// Der AbilityChooser wird hier BEWUSST nicht mehr eingeblendet (19.09.2026).
			//
			// SetUserWidget haengt ueber AControllerBase::SetWidgets am Auswahlknopf
			// (USelectorButton::SetUnitSelectorId). Jeder Klick auf eine Einheit schaltete den
			// Chooser damit mit um - gemeldet als "der SelectButton soll den AbilityChooser
			// nicht toggeln". Seine Sichtbarkeit steuern jetzt ausschliesslich der eigene Knopf
			// im TaggedUnitSelector (UTaggedUnitSelector::ToggleAbilityChooser) und die
			// Tab-Umschaltung im HUD-Blueprint.
			//
			// Owner und Aktualisierungstakt werden weiter gesetzt, damit der Chooser beim
			// Aufklappen sofort die richtige Einheit zeigt.
			AbilityChooserWidget->SetOwnerActor(SelectedActor);
			AbilityChooserWidget->StartUpdateTimer();
		}

	}else
	{
		if (TalentChooserWidget) TalentChooserWidget->StopTimer();
		if (AbilityChooserWidget) AbilityChooserWidget->StopTimer();
		if (TalentChooserWidget) TalentChooserWidget->SetVisibility(ESlateVisibility::Collapsed);
		if (AbilityChooserWidget) AbilityChooserWidget->SetVisibility(ESlateVisibility::Collapsed);

	}

}

void AExtendedCameraBase::Server_InvestTeamAttributeTreeNode_Implementation(FName NodeId)
{
	UWorld* Welt = GetWorld();
	const ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetController());
	const int32 TeamId = MyPC ? MyPC->SelectableTeamId : -1;
	if (!Welt || TeamId < 1)
	{
		return;
	}

	AUpgradeGameState* GameStateRef = Welt->GetGameState<AUpgradeGameState>();
	const UDataTable* Table = GameStateRef ? GameStateRef->FindTeamAttributeTreeTable(TeamId) : nullptr;
	if (!GameStateRef || !Table)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Attributbaum] Investieren abgelehnt: %s fehlt (Team %d, Knoten '%s')."),
			!GameStateRef ? TEXT("GameState") : TEXT("Baumtabelle"), TeamId, *NodeId.ToString());
		return;
	}

	const FAttributeTreeNodeRow* Row = Table->FindRow<FAttributeTreeNodeRow>(
		NodeId, TEXT("Server_InvestTeamAttributeTreeNode"), /*bWarnIfMissing=*/false);
	if (!Row || !GameStateRef->IsTeamAttributeTreeNodeUnlocked(TeamId, NodeId))
	{
		return;
	}

	// Erst buchen, dann anwenden. Schlaegt das Buchen fehl (kein Punkt im Topf oder Knoten voll),
	// darf keine einzige Einheit die Aufwertung bekommen.
	if (!GameStateRef->InvestTeamAttributeTreeNode(TeamId, NodeId, Row->MaxPoints))
	{
		return;
	}

	int32 Applied = 0;
	int32 Considered = 0;
	for (TActorIterator<ALevelUnit> It(Welt); It; ++It)
	{
		ALevelUnit* Unit = *It;
		if (!IsValid(Unit) || Unit->TeamId != TeamId)
		{
			continue;
		}
		++Considered;
		if (Unit->ApplyAttributeTreeNodeFromTeam(NodeId))
		{
			++Applied;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Attributbaum] Team %d investiert in '%s': %d von %d Einheiten aufgewertet, Topf jetzt %d (ausgegeben %d)."),
		TeamId, *NodeId.ToString(), Applied, Considered,
		GameStateRef->GetTeamAttributeTreePoints(TeamId),
		GameStateRef->GetTeamUsedAttributeTreePoints(TeamId));
}

void AExtendedCameraBase::Server_ResetTeamAttributeTree_Implementation()
{
	UWorld* Welt = GetWorld();
	const ACameraControllerBase* MyPC = Cast<ACameraControllerBase>(GetController());
	const int32 TeamId = MyPC ? MyPC->SelectableTeamId : -1;
	if (!Welt || TeamId < 1)
	{
		return;
	}

	AUpgradeGameState* GameStateRef = Welt->GetGameState<AUpgradeGameState>();
	if (!GameStateRef)
	{
		return;
	}

	GameStateRef->ResetTeamAttributeTree(TeamId);

	int32 Cleared = 0;
	for (TActorIterator<ALevelUnit> It(Welt); It; ++It)
	{
		ALevelUnit* Unit = *It;
		if (!IsValid(Unit) || Unit->TeamId != TeamId)
		{
			continue;
		}
		Unit->ResetAttributeTree();
		++Cleared;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Attributbaum] Team %d zurueckgesetzt: %d Einheiten geleert, Topf jetzt %d."),
		TeamId, Cleared, GameStateRef->GetTeamAttributeTreePoints(TeamId));
}


void AExtendedCameraBase::SetSelectorWidget(int Id, AUnitBase* SelectedActor)
{

	if(UnitSelectorWidget)
	{
		UnitSelectorWidget->SetButtonColours(Id);
		FString CharacterName = SelectedActor->Name + " / " + FString::FromInt(Id);
		if (UnitSelectorWidget->Name)
		{
			UnitSelectorWidget->Name->SetText(FText::FromString(CharacterName));
		}
	}
}

void AExtendedCameraBase::UpdateSelectorWidget()
{
	if(UnitSelectorWidget)
	{
		UnitSelectorWidget->UpdateSelectedUnits();
	}
}

bool AExtendedCameraBase::InitUnitSelectorWidgetController(ACustomControllerBase* WithPC)
{
	if (!WithPC)
		return false;
	
	if (UnitSelectorWidget)
	{
		if (!UnitSelectorWidget->ControllerBase)
		{
			UnitSelectorWidget->InitWidget(WithPC);
		}
		return true;
	}
	
	return false;
}


bool AExtendedCameraBase::InitTaggedSelectorWidgetController(ACustomControllerBase* WithPC)
{
	if (!WithPC)
		return false;
	
	if (TaggedSelectorWidget)
	{
		if (!TaggedSelectorWidget->ControllerBase)
		{
			TaggedSelectorWidget->InitWidget(WithPC);
		}
		return true;
	}
	
	return false;
}


bool AExtendedCameraBase::InitAbiltiyChooserWidgetController(ACustomControllerBase* WithPC)
{
	if (!WithPC)
		return false;
	
	if (AbilityChooserWidget)
	{
		if (!AbilityChooserWidget->ControllerBase)
		{
			AbilityChooserWidget->InitWidget(WithPC);
		}
		return true;
	}
	
	return false;
}

void AExtendedCameraBase::OnAbilityInputDetected(EGASAbilityInputID InputID, AGASUnit* SelectedUnit, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{

	if(SelectedUnit && InputID != EGASAbilityInputID::None)
	{
		//UE_LOG(LogTemp, Warning, TEXT("OnAbilityInputDetected: Activating ability ID %d for unit: %s"), static_cast<int32>(InputID), *SelectedUnit->GetName());
		SelectedUnit->ActivateAbilityByInputID(InputID, AbilitiesArray, FHitResult(), Cast<APlayerController>(GetController()));
	}
}

void AExtendedCameraBase::ExecuteOnAbilityInputDetected(EGASAbilityInputID InputID, ACameraControllerBase* CamController)
{
	if(!CamController) return;
	
	if (AExtendedControllerBase* ExtController = Cast<AExtendedControllerBase>(CamController))
	{
		ExtController->SetAbilityInputHeld(InputID, true);
	}
	
	CamController->ActivateKeyboardAbilitiesOnMultipleUnits(InputID);
	
}

// ================================================================================================
// LUX-ANPASSUNG (16.08.2026) â€” Linksklick feuert, NUR in der Direktsteuerung.
// Ist eine CameraUnit gesetzt UND folgt sie nicht der Maus (CameraUnitMouseFollow == false),
// loest der Linksklick dieselbe Faehigkeit aus wie die Taste 1 (AbilityOne = erster Eintrag
// im aktiven AbilityArray). Die urspruengliche Klick-Routine (LeftClickPressedMass, JumpCamera,
// MoveToClick) haengt dann an SHIFT + Linksklick.
// Ohne CameraUnit oder mit CameraUnitMouseFollow == true bleibt alles unveraendert.
// Gemeinsame Bedingung fuer Press und Release, damit beide nie auseinanderlaufen.
// ================================================================================================
bool AExtendedCameraBase::LuxUseLeftClickAsAbility(ACameraControllerBase* CameraControllerBase) const
{
	return CameraControllerBase
		&& CameraControllerBase->CameraUnitWithTag
		&& CameraControllerBase->bUnitDirectControl
		&& !CameraControllerBase->CameraUnitMouseFollow
		&& !CameraControllerBase->IsShiftPressed;
}
// ===================== ENDE LUX-ANPASSUNG =======================================================

void AExtendedCameraBase::Input_LeftClick_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;

	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());

	// ============================================================================================
	// LUX-ANPASSUNG (16.08.2026) â€” siehe LuxUseLeftClickAsAbility() oben.
	// Original: hier stand direkt der LeftClickPressedMass/JumpCamera-Block.
	// ============================================================================================
	if (LuxUseLeftClickAsAbility(CameraControllerBase))
	{
		// ----------------------------------------------------------------------------------------
		// LUX-ANPASSUNG (28.08.2026) - Klick beim Zielen gehoert der zielenden Faehigkeit.
		// Steht der Ziel-Indikator einer Faehigkeit mit bIndicatorClicksAdvanceAbility (Granate),
		// zaehlt dieser Klick fuer SIE weiter, statt AbilityOne (den Schuss) zu starten. Siehe
		// ACameraControllerBase::LuxTryAdvanceIndicatorAbilityWithClick.
		// bLuxLeftClickWasAbility bleibt false: es wurde kein Halten begonnen, also darf das
		// Loslassen auch keines beenden.
		// ----------------------------------------------------------------------------------------
		if (CameraControllerBase->LuxTryAdvanceIndicatorAbilityWithClick())
		{
			bLuxLeftClickWasAbility = false;
			return;
		}
		// ===================== ENDE LUX-ANPASSUNG ===============================================

		// Identisch zum Tastendruck 1 (HandleState_AbilityOne). Setzt intern auch
		// SetAbilityInputHeld(AbilityOne, true), damit Dauerfeuer beim Halten laeuft.
		ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityOne, CameraControllerBase);
		bLuxLeftClickWasAbility = true;
		return;
	}
	bLuxLeftClickWasAbility = false;
	// ===================== ENDE LUX-ANPASSUNG ===================================================

	if(CameraControllerBase)
	{
		CameraControllerBase->LeftClickPressedMass();
		CameraControllerBase->JumpCamera();
	}

	if(CameraControllerBase && CameraControllerBase->IsShiftPressed)
	{
		SetCameraState(CameraData::MoveToClick);
	}

}

void AExtendedCameraBase::Input_LeftClick_Released(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;

	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());

	// ============================================================================================
	// LUX-ANPASSUNG (16.08.2026) â€” Gegenstueck zum Press-Zweig.
	// Wurde der Klick als Faehigkeit gewertet, muss beim Loslassen das Halten beendet werden -
	// genau wie beim Loslassen der Taste 1 (SwitchControllerStateMachine, case 2121). Sonst
	// feuert eine Dauerfeuer-Waffe endlos weiter. bLuxLeftClickWasAbility statt einer erneuten
	// Zustandspruefung, weil Shift oder die CameraUnit sich zwischen Druck und Loslassen
	// aendern koennen - sonst bliebe der Held-Zustand haengen.
	// Original: hier stand direkt der LeftClickReleasedMass-Aufruf.
	// ============================================================================================
	if (bLuxLeftClickWasAbility)
	{
		bLuxLeftClickWasAbility = false;
		if (AExtendedControllerBase* LuxExtPC = Cast<AExtendedControllerBase>(CameraControllerBase))
		{
			LuxExtPC->SetAbilityInputHeld(EGASAbilityInputID::AbilityOne, false);
		}
		return;
	}
	// ===================== ENDE LUX-ANPASSUNG ===================================================

	if(CameraControllerBase)
	{
		CameraControllerBase->LeftClickReleasedMass();
	}
}

void AExtendedCameraBase::Input_RightClick_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->RightClickPressedMass();
	}
}

void AExtendedCameraBase::Input_C_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;

	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->CycleGridFormationShape();
	}
}

void AExtendedCameraBase::Input_G_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->ToggleLockCamToCharacter();
	}
}

void AExtendedCameraBase::Input_A_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;

	/*
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->TPressed();
	} */
}

void AExtendedCameraBase::Input_Alt_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->AltIsPressed = true;
	}
}

void AExtendedCameraBase::Input_Alt_Released(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->AltIsPressed = false;
	}
}

void AExtendedCameraBase::Input_Ctrl_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->IsCtrlPressed = true;
	}
}

void AExtendedCameraBase::Input_Ctrl_Released(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->IsCtrlPressed = false;
	}
}



void AExtendedCameraBase::Input_Tab_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	if(BlockControls) return;

	TabMode = (TabMode + 1) % 5; // 0 = off, 1 = Resource, 2 = Control, 3 = WinCondition, 4 = AttributeTree

	UpdateTabModeUI();
}

void AExtendedCameraBase::Input_Tab_Released(const FInputActionValue& InputActionValue, int32 CamState)
{
	Input_Tab_Released_BP(CamState);
}

void AExtendedCameraBase::Input_V_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	ACustomControllerBase* CustomController = Cast<ACustomControllerBase>(GetController());
	if (CustomController)
	{
		CustomController->ShowFriendlyHealthbars();
	}
}

void AExtendedCameraBase::Input_Tab_Released_BP(int32 CamState)
{
}

void AExtendedCameraBase::Input_Shift_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	
	if(CameraControllerBase)
	{
		CameraControllerBase->ShiftPressed();
	}
}

void AExtendedCameraBase::Input_Shift_Released(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	
	if(CameraControllerBase)
	{
		CameraControllerBase->ShiftReleased();
	}
}

void AExtendedCameraBase::Input_Esc_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	if (MapMenuWidget)
	{
		if (MapMenuWidget->GetVisibility() == ESlateVisibility::Visible)
		{
			MapMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
			BlockControls = false;
			UpdateViewportBlur(TabToggled);
		}
		else
		{
			ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
			if (CameraControllerBase && CameraControllerBase->HUDBase)
			{
				// If we have selected units, cancel their actions before deselecting
				if (CameraControllerBase->HUDBase->SelectedUnits.Num() > 0)
				{
					for (AUnitBase* Unit : CameraControllerBase->HUDBase->SelectedUnits)
					{
						if (Unit)
						{
							if (Unit->IsAnyAbilityActive() && Unit->CurrentSnapshot.AbilityClass)
							{
								UGameplayAbilityBase* AbilityCDO = Unit->CurrentSnapshot.AbilityClass->GetDefaultObject<UGameplayAbilityBase>();
								if (AbilityCDO && !AbilityCDO->AbilityCanBeCanceled)
								{
									continue;
								}
							}

							// 1. Destroy any dragged work area (like a building placement)
							AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(Unit);
							if (Worker && Worker->CurrentDraggedWorkArea)
							{
								CameraControllerBase->DestroyDraggedArea(Worker);
							}

							// 2. Cancel the current ability and remove the AbilityIndicator
							CameraControllerBase->CancelCurrentAbility(Unit);
						}
					}

					// Clear selection
					CameraControllerBase->HUDBase->DeselectAllUnits();
					CameraControllerBase->SelectedUnits = CameraControllerBase->HUDBase->SelectedUnits;

					// Clear the camera's user widget (Ability/Talent choosers)
					SetUserWidget(nullptr);

					return;
				}
			}

			MapMenuWidget->SetVisibility(ESlateVisibility::Visible);
			if (AExtendedControllerBase* ExtPC = Cast<AExtendedControllerBase>(CameraControllerBase))
			{
				ExtPC->ClearHeldAbilityInputs();
			}
			BlockControls = true;
			UpdateViewportBlur(true);
		}
	}
}

void AExtendedCameraBase::SwitchControllerStateMachine(const FInputActionValue& InputActionValue, int32 NewCameraState)
{
    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
    if (!CameraControllerBase) return;

    // Ability key releases must run even while controls are blocked: if a menu opens while the key
    // is down, this release is the only thing that clears HeldAbilityInputs, and a stale entry
    // permanently blocks deselection in Client_ContinueSelectionAfterAbility.
    if (AExtendedControllerBase* ExtPC = Cast<AExtendedControllerBase>(CameraControllerBase))
    {
        switch (NewCameraState)
        {
        case 2121: ExtPC->SetAbilityInputHeld(EGASAbilityInputID::AbilityOne, false); return;
        case 2222: ExtPC->SetAbilityInputHeld(EGASAbilityInputID::AbilityTwo, false); return;
        case 2323: ExtPC->SetAbilityInputHeld(EGASAbilityInputID::AbilityThree, false); return;
        case 2424: ExtPC->SetAbilityInputHeld(EGASAbilityInputID::AbilityFour, false); return;
        case 2525: ExtPC->SetAbilityInputHeld(EGASAbilityInputID::AbilityFive, false); return;
        case 2626: ExtPC->SetAbilityInputHeld(EGASAbilityInputID::AbilitySix, false); return;
        }
    }

    // Everything below is a real control action and stays blocked while a menu is open.
    if (BlockControls) return;

    if (CameraControllerBase->AltIsPressed)
    	{  switch (NewCameraState)
    		{
    			case 0:
    				{

    				}break;
    			case 13:
    				{
    					HandleState_ScrollZoom(InputActionValue, CameraControllerBase);
    				}break;
    			case 21: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagAlt1, CameraControllerBase->SelectableTeamId); break; // ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilitySeven, CameraControllerBase);break;
    			case 22: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagAlt2, CameraControllerBase->SelectableTeamId); break;// ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityEight, CameraControllerBase); break;
    			case 23: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagAlt3, CameraControllerBase->SelectableTeamId); break; // ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityNine, CameraControllerBase); break;
    			case 24: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagAlt4, CameraControllerBase->SelectableTeamId); break;// ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityTen, CameraControllerBase); break;
    			case 25: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagAlt5, CameraControllerBase->SelectableTeamId); break;
    			case 26: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagAlt6, CameraControllerBase->SelectableTeamId); break;
    			default: break;
    		}
    	}
    	
        if (CameraControllerBase->IsCtrlPressed)
        {
            switch (NewCameraState)
            {
            	
            case 0:
            	{

            	}break;
            case 1: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrlW, CameraControllerBase->SelectableTeamId); break; break;
            case 111: HandleState_StopMoveW(CameraControllerBase); break;
            case 2: HandleState_MoveS(CameraControllerBase); break;
            case 222: HandleState_StopMoveS(CameraControllerBase); break;
            case 3: HandleState_MoveA(CameraControllerBase); break;
            case 333: HandleState_StopMoveA(CameraControllerBase); break;
            case 4: HandleState_MoveD(CameraControllerBase); break;
            case 444: HandleState_StopMoveD(CameraControllerBase); break;
            case 5: HandleState_ZoomIn(CameraControllerBase); break;
            case 555: HandleState_StopZoomIn(CameraControllerBase); break;
            case 6: HandleState_ZoomOut(CameraControllerBase); break;
            case 666: HandleState_StopZoomOut(CameraControllerBase); break;
            case 11: HandleState_LockOnCharacter(); break;
            case 14: HandleState_OrbitAndMove(); break;
            case 15: HandleState_SpawnEffects(CameraControllerBase); break;
            case 7:
            	{
            		CameraControllerBase->SpacePressed();
            		
            		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
            			SetCameraState(CameraData::ZoomOutPosition);
            	} break;
            case 8:
            	{
            		CameraControllerBase->SpaceReleased();
            		
            		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
            			SetCameraState(CameraData::ZoomInPosition);
            	} break;
            case 9: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrlQ, CameraControllerBase->SelectableTeamId); break;
            case 999: HandleState_StopRotateLeft(CameraControllerBase); break;
            case 10: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrlE, CameraControllerBase->SelectableTeamId); break;
            case 101010: HandleState_StopRotateRight(CameraControllerBase); break;
            case 13: 
                if (SwapScroll) HandleState_AbilityArrayIndex(InputActionValue, CameraControllerBase);
                else HandleState_ScrollZoom(InputActionValue, CameraControllerBase);
                break;
            case 18: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrlR, CameraControllerBase->SelectableTeamId); break;
            case 21: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrl1, CameraControllerBase->SelectableTeamId); break; // ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilitySeven, CameraControllerBase);break;
            case 22: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrl2, CameraControllerBase->SelectableTeamId); break;// ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityEight, CameraControllerBase); break;
            case 23: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrl3, CameraControllerBase->SelectableTeamId); break; // ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityNine, CameraControllerBase); break;
            case 24: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrl4, CameraControllerBase->SelectableTeamId); break;// ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityTen, CameraControllerBase); break;
            case 25: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrl5, CameraControllerBase->SelectableTeamId); break;
            case 26: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagCtrl6, CameraControllerBase->SelectableTeamId); break;
            case 27: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF1, CameraControllerBase->SelectableTeamId); break;
            case 28: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF2, CameraControllerBase->SelectableTeamId); break;
            case 29: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF3, CameraControllerBase->SelectableTeamId); break;
            case 30: CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF4, CameraControllerBase->SelectableTeamId); break;
            default: break;
            }
        }
        else
        {
            switch (NewCameraState)
            {
            case 1: HandleState_MoveW_NoStrg(CameraControllerBase); break;
            case 111: HandleState_StopMoveW_NoStrg(CameraControllerBase); break;
            case 2: HandleState_MoveS_NoStrg(CameraControllerBase); break;
            case 222: HandleState_StopMoveS_NoStrg(CameraControllerBase); break;
            case 3: HandleState_MoveA_NoStrg(CameraControllerBase); break;
            case 333: HandleState_StopMoveA_NoStrg(CameraControllerBase); break;
            case 4: HandleState_MoveD_NoStrg(CameraControllerBase); break;
            case 444: HandleState_StopMoveD_NoStrg(CameraControllerBase); break;
            case 5: HandleState_ZoomIn_NoStrg(CameraControllerBase); break;
            case 555: HandleState_StopZoomIn_NoStrg(CameraControllerBase); break;
            case 6: HandleState_ZoomOut_NoStrg(CameraControllerBase); break;
            case 666: HandleState_StopZoomOut_NoStrg(CameraControllerBase); break;
            case 7:
            	{
            		CameraControllerBase->SpacePressed();
            	} break;
            case 8:
            	{
            		CameraControllerBase->SpaceReleased();
            		
            		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
            			SetCameraState(CameraData::ZoomInPosition);
            	} break;
            case 9: HandleState_RotateLeft_NoStrg(CameraControllerBase); break;
            case 999: HandleState_StopRotateLeft_NoStrg(CameraControllerBase); break;
            case 10: HandleState_RotateRight_NoStrg(CameraControllerBase); break;
            case 101010: HandleState_StopRotateRight_NoStrg(CameraControllerBase); break;
            case 12: HandleState_TPressed(CameraControllerBase); break;
            case 13: 
                if (SwapScroll) HandleState_ScrollZoom(InputActionValue, CameraControllerBase);
                else HandleState_AbilityArrayIndex(InputActionValue, CameraControllerBase);
                break;
            case 16: HandleState_MiddleMousePressed(CameraControllerBase); break;
            case 17: HandleState_MiddleMouseReleased(CameraControllerBase); break;
            case 18: CameraControllerBase->SetHoldPositionOnSelectedUnits(); break;
            case 21: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityOne, CameraControllerBase);break;
            case 22: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityTwo, CameraControllerBase); break;
            case 23: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityThree, CameraControllerBase); break;
            case 24: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityFour, CameraControllerBase); break;
            case 25: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityFive, CameraControllerBase); break;
            case 26: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilitySix, CameraControllerBase); break;
            case 27:
                bFKeyPressed[0] = true;
                FKeyHoldTimes[0] = 0.f;
                bFKeyTagAssigned[0] = false;
                break;
            case 28:
                bFKeyPressed[1] = true;
                FKeyHoldTimes[1] = 0.f;
                bFKeyTagAssigned[1] = false;
                break;
            case 29:
                bFKeyPressed[2] = true;
                FKeyHoldTimes[2] = 0.f;
                bFKeyTagAssigned[2] = false;
                break;
            case 30:
                bFKeyPressed[3] = true;
                FKeyHoldTimes[3] = 0.f;
                bFKeyTagAssigned[3] = false;
                break;
            case 31:
                bFKeyPressed[4] = true;
                FKeyHoldTimes[4] = 0.f;
                bFKeyTagAssigned[4] = false;
                break;
            case 32:
                bFKeyPressed[5] = true;
                FKeyHoldTimes[5] = 0.f;
                bFKeyTagAssigned[5] = false;
                break;
            case 2727:
                if (bFKeyPressed[0]) {
                    CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF1, CameraControllerBase->SelectableTeamId);
                    bFKeyPressed[0] = false;
                }
                break;
            case 2828:
                if (bFKeyPressed[1]) {
                    CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF2, CameraControllerBase->SelectableTeamId);
                    bFKeyPressed[1] = false;
                }
                break;
            case 2929:
                if (bFKeyPressed[2]) {
                    CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF3, CameraControllerBase->SelectableTeamId);
                    bFKeyPressed[2] = false;
                }
                break;
            case 3030:
                if (bFKeyPressed[3]) {
                    CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF4, CameraControllerBase->SelectableTeamId);
                    bFKeyPressed[3] = false;
                }
                break;
            case 3131:
                if (bFKeyPressed[4]) {
                    CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF5, CameraControllerBase->SelectableTeamId);
                    bFKeyPressed[4] = false;
                }
                break;
            case 3232:
                if (bFKeyPressed[5]) {
                    CameraControllerBase->SelectUnitsWithTag(CameraControllerBase->KeyTagF6, CameraControllerBase->SelectableTeamId);
                    bFKeyPressed[5] = false;
                }
                break;
            default: break;
            }
        }
}

void AExtendedCameraBase::HandleState_MoveW(ACameraControllerBase* CameraControllerBase)
{

	if (CameraControllerBase->CameraUnitWithTag) return;
	
    if (GetCameraState() == CameraData::OrbitAndMove)
    {
        CameraControllerBase->CamIsRotatingLeft = false;
        CameraControllerBase->CamIsRotatingRight = false;
    }

    CameraControllerBase->WIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;
    SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveW(ACameraControllerBase* CameraControllerBase)
{

	if (CameraControllerBase->CameraUnitWithTag) return;
	
    CameraControllerBase->WIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_MoveS(ACameraControllerBase* CameraControllerBase)
{
	if (CameraControllerBase->CameraUnitWithTag) return;

    if (GetCameraState() == CameraData::OrbitAndMove)
    {
        CameraControllerBase->CamIsRotatingLeft = false;
        CameraControllerBase->CamIsRotatingRight = false;
    }

    CameraControllerBase->SIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;
    SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveS(ACameraControllerBase* CameraControllerBase)
{

	if (CameraControllerBase->CameraUnitWithTag) return;
	
    CameraControllerBase->SIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_MoveA(ACameraControllerBase* CameraControllerBase)
{

	if (CameraControllerBase->CameraUnitWithTag) return;
	
    if (GetCameraState() == CameraData::OrbitAndMove)
    {
        CameraControllerBase->CamIsRotatingLeft = false;
        CameraControllerBase->CamIsRotatingRight = false;
    }

    CameraControllerBase->AIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;
    SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveA(ACameraControllerBase* CameraControllerBase)
{
	if (CameraControllerBase->CameraUnitWithTag) return;

    CameraControllerBase->AIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_MoveD(ACameraControllerBase* CameraControllerBase)
{

	if (CameraControllerBase->CameraUnitWithTag) return;
	
    if (GetCameraState() == CameraData::OrbitAndMove)
    {
        CameraControllerBase->CamIsRotatingLeft = false;
        CameraControllerBase->CamIsRotatingRight = false;
    }

    CameraControllerBase->DIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;
    SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveD(ACameraControllerBase* CameraControllerBase)
{

	if (CameraControllerBase->CameraUnitWithTag) return;
	
    CameraControllerBase->DIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_ZoomIn(ACameraControllerBase* CameraControllerBase)
{

    CameraControllerBase->CamIsZoomingInState = 1;
	
    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::ZoomIn);
}

void AExtendedCameraBase::HandleState_StopZoomIn(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->CamIsZoomingInState = 2;
}

void AExtendedCameraBase::HandleState_ZoomOut(ACameraControllerBase* CameraControllerBase)
{

    CameraControllerBase->CamIsZoomingOutState = 1;
    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::ZoomOut);
}

void AExtendedCameraBase::HandleState_StopZoomOut(ACameraControllerBase* CameraControllerBase)
{

    CameraControllerBase->CamIsZoomingOutState = 2;
}

void AExtendedCameraBase::HandleState_LockOnCharacter()
{
    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::LockOnCharacter);
}

void AExtendedCameraBase::HandleState_OrbitAndMove()
{
    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::OrbitAndMove);
}

void AExtendedCameraBase::HandleState_SpawnEffects(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->SpawnMissileRain(4, FVector(1000.f, -1000.f, 1000.f));
    CameraControllerBase->SpawnEffectArea(3, FVector(1000.f, -1000.f, 10.f), FVector(5), CameraControllerBase->EffectAreaClass);
}

void AExtendedCameraBase::HandleState_RotateLeft(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->CamIsRotatingLeft = true;
    if (!CameraControllerBase->LockCameraToCharacter &&
        !CameraControllerBase->WIsPressedState &&
        !CameraControllerBase->AIsPressedState &&
        !CameraControllerBase->SIsPressedState &&
        !CameraControllerBase->DIsPressedState)
    {
        if (GetCameraState() != CameraData::LockOnCharacterWithTag)
            SetCameraState(CameraData::HoldRotateLeft);
    }
}

void AExtendedCameraBase::HandleState_StopRotateLeft(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->CamIsRotatingLeft = false;
}

void AExtendedCameraBase::HandleState_RotateRight(ACameraControllerBase* CameraControllerBase)
{

    CameraControllerBase->CamIsRotatingRight = true;
    if (!CameraControllerBase->LockCameraToCharacter &&
        !CameraControllerBase->WIsPressedState &&
        !CameraControllerBase->AIsPressedState &&
        !CameraControllerBase->SIsPressedState &&
        !CameraControllerBase->DIsPressedState)
    {
        if (GetCameraState() != CameraData::LockOnCharacterWithTag)
            SetCameraState(CameraData::HoldRotateRight);
    }
}

void AExtendedCameraBase::HandleState_StopRotateRight(ACameraControllerBase* CameraControllerBase)
{

    CameraControllerBase->CamIsRotatingRight = false;
}


void AExtendedCameraBase::HandleState_MoveW_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->WIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;
	//CameraControllerBase->CameraUnitTimer = 100.f;
	
    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveW_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    //if (GetCameraState() == CameraData::LockOnCharacterWithTag)
    	//CameraControllerBase->SetUnitState_Replication( CameraControllerBase->CameraUnitWithTag, 0);
	
    CameraControllerBase->WIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_MoveS_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->SIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;

    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveS_NoStrg(ACameraControllerBase* CameraControllerBase)
{

    //if (GetCameraState() == CameraData::LockOnCharacterWithTag)
    	//CameraControllerBase->SetUnitState_Replication( CameraControllerBase->CameraUnitWithTag, 0);
	
    CameraControllerBase->SIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_MoveA_NoStrg(ACameraControllerBase* CameraControllerBase)
{

    CameraControllerBase->AIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;
	
    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveA_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    //if (GetCameraState() == CameraData::LockOnCharacterWithTag)
    	//CameraControllerBase->SetUnitState_Replication( CameraControllerBase->CameraUnitWithTag, 0);
	
    CameraControllerBase->AIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_MoveD_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->DIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;
	//CameraControllerBase->CameraUnitTimer = 100.f;
	
    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::MoveWASD);

}

void AExtendedCameraBase::HandleState_StopMoveD_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    //if (GetCameraState() == CameraData::LockOnCharacterWithTag)
    	//CameraControllerBase->SetUnitState_Replication( CameraControllerBase->CameraUnitWithTag, 0);

	//CameraControllerBase->CameraUnitTimer = 100.f;
    CameraControllerBase->DIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_ZoomIn_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->CamIsZoomingInState = 1;

    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::ZoomIn);
}

void AExtendedCameraBase::HandleState_StopZoomIn_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->CamIsZoomingInState = 2;
}

void AExtendedCameraBase::HandleState_ZoomOut_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->CamIsZoomingOutState = 1;

    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::ZoomOut);
}

void AExtendedCameraBase::HandleState_StopZoomOut_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->CamIsZoomingOutState = 2;
}

void AExtendedCameraBase::HandleState_RotateLeft_NoStrg(ACameraControllerBase* CameraControllerBase)
{

	if (GetCameraState() != CameraData::LockOnCharacterWithTag)
	{
		CameraControllerBase->CamIsRotatingLeft = true;
		SetCameraState(CameraData::HoldRotateLeft);
	}else
	{
		CameraControllerBase->CamIsRotatingRight = true;
	}
	
}

void AExtendedCameraBase::HandleState_StopRotateLeft_NoStrg(ACameraControllerBase* CameraControllerBase)
{
	
	if (GetCameraState() != CameraData::LockOnCharacterWithTag)
	{
		CameraControllerBase->CamIsRotatingLeft = false;
	}else
		CameraControllerBase->CamIsRotatingRight = false;
}

void AExtendedCameraBase::HandleState_RotateRight_NoStrg(ACameraControllerBase* CameraControllerBase)
{
	

	if (GetCameraState() != CameraData::LockOnCharacterWithTag)
	{
		CameraControllerBase->CamIsRotatingRight = true;
		SetCameraState(CameraData::HoldRotateRight);
	}
	else
	{
		CameraControllerBase->CamIsRotatingLeft = true;
	}
}

void AExtendedCameraBase::HandleState_StopRotateRight_NoStrg(ACameraControllerBase* CameraControllerBase)
{


	if (GetCameraState() != CameraData::LockOnCharacterWithTag)
	{
		CameraControllerBase->CamIsRotatingRight = false;
	}else
	{
		CameraControllerBase->CamIsRotatingLeft = false;
	}
}


void AExtendedCameraBase::HandleState_TPressed(ACameraControllerBase* CameraControllerBase)
{
	CameraControllerBase->TPressed();

}

void AExtendedCameraBase::HandleState_PPressed(ACameraControllerBase* CameraControllerBase)
{
    HandleState_OrbitAndMove();
}

void AExtendedCameraBase::HandleState_OPressed(ACameraControllerBase* CameraControllerBase)
{
    HandleState_SpawnEffects(CameraControllerBase);
}

void AExtendedCameraBase::HandleState_AbilityArrayIndex(const FInputActionValue& InputActionValue, ACameraControllerBase* CameraControllerBase)
{
				
	if(InputActionValue.Get<float>() > 0)
	{
		CameraControllerBase->AddAbilityIndex(1);
	}
	else
	{
		CameraControllerBase->AddAbilityIndex(-1);
	}
}

void AExtendedCameraBase::HandleState_AbilityUnitIndex(const FInputActionValue& InputActionValue, ACameraControllerBase* CameraControllerBase)
{
			
	if (InputActionValue.Get<float>() > 0)
	{
		CameraControllerBase->AbilityArrayIndex = 0;
		CameraControllerBase->AddToCurrentUnitWidgetIndex(1);
	}
	else
	{
		CameraControllerBase->AbilityArrayIndex = 0;
		CameraControllerBase->AddToCurrentUnitWidgetIndex(-1);
	}
}

void AExtendedCameraBase::HandleState_ScrollZoom(const FInputActionValue& InputActionValue, ACameraControllerBase* CameraControllerBase)
{
	float FloatValue = InputActionValue.Get<float>();

	if(CameraControllerBase->ScrollZoomCount <= 10.f)
		CameraControllerBase->ScrollZoomCount += FloatValue*2;
				
	if(CameraControllerBase->LockCameraToCharacter)
		return;
				
	if(FloatValue > 0)
	{
		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
			SetCameraState(CameraData::ScrollZoomIn);
	}
	else
	{
		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
			SetCameraState(CameraData::ScrollZoomOut);
	}
}

void AExtendedCameraBase::HandleState_MiddleMousePressed(ACameraControllerBase* CameraControllerBase)
{
	float MouseX, MouseY;
	CameraControllerBase->GetMousePosition(MouseX, MouseY);
	PreviousMouseLocation.X = MouseX;
	PreviousMouseLocation.Y = MouseY;
				
	CameraControllerBase->MiddleMouseIsPressed = true;
}

void AExtendedCameraBase::HandleState_MiddleMouseReleased(ACameraControllerBase* CameraControllerBase)
{


    CameraControllerBase->MiddleMouseIsPressed = false;
}

void AExtendedCameraBase::HandleState_AbilityOne(ACameraControllerBase* CameraControllerBase)
{

    ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityOne, CameraControllerBase);
}

void AExtendedCameraBase::HandleState_AbilityTwo(ACameraControllerBase* CameraControllerBase)
{


    ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityTwo, CameraControllerBase);
}

void AExtendedCameraBase::HandleState_AbilityThree(ACameraControllerBase* CameraControllerBase)
{
    ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityThree, CameraControllerBase);
}

void AExtendedCameraBase::HandleState_AbilityFour(ACameraControllerBase* CameraControllerBase)
{
   

    ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityFour, CameraControllerBase);
}

void AExtendedCameraBase::HandleState_AbilityFive(ACameraControllerBase* CameraControllerBase)
{

    ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityFive, CameraControllerBase);
}

void AExtendedCameraBase::HandleState_AbilitySix(ACameraControllerBase* CameraControllerBase)
{

    ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilitySix, CameraControllerBase);
}

