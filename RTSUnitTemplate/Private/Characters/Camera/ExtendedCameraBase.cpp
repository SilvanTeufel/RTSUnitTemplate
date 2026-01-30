
#include "Characters/Camera/ExtendedCameraBase.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Actors/WinLoseConfigActor.h"

#include "Characters/Unit/BuildingBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameModes/ResourceGameMode.h"
#include "GameStates/ResourceGameState.h"
#include "GAS/GAS.h"
#include "Widgets/AbilityChooser.h"
#include "Widgets/ResourceWidget.h"
#include "Widgets/TaggedUnitSelector.h"
#include "Widgets/TalentChooser.h"
#include "Widgets/UnitWidgetSelector.h"
#include "Widgets/SoundControlWidget.h"
#include "Widgets/WinConditionWidget.h"
#include "Blueprint/UserWidget.h"

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

	TabMode = 1;
	UpdateTabModeUI();

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
	
	// Optional fallback timer in case controller is not yet possessed
	FTimerHandle InitialShowTimer;
	GetWorldTimerManager().SetTimer(InitialShowTimer, [this]()
	{
		ACameraControllerBase* MyPC2 = Cast<ACameraControllerBase>(GetController());
		if (MyPC2 && MyPC2->SelectableTeamId != -1)
		{
			OnTeamIdChanged_Internal(MyPC2->SelectableTeamId);
		}
	}, 2.0f, false);
}

void AExtendedCameraBase::OnTeamIdChanged_Internal(int32 NewTeamId)
{
	AWinLoseConfigActor* BestConfig = nullptr;
	AWinLoseConfigActor* GlobalConfig = nullptr;

	for (TActorIterator<AWinLoseConfigActor> It(GetWorld()); It; ++It)
	{
		AWinLoseConfigActor* Config = *It;
		if (Config)
		{
			Config->OnWinConditionChanged.RemoveDynamic(this, &AExtendedCameraBase::OnWinConditionChanged);
			Config->OnWinConditionChanged.AddDynamic(this, &AExtendedCameraBase::OnWinConditionChanged);
                
			if (Config->TeamId == NewTeamId) BestConfig = Config;
			else if (Config->TeamId == 0) GlobalConfig = Config;
		}
	}

	AWinLoseConfigActor* TargetConfig = BestConfig ? BestConfig : GlobalConfig;
	if (TargetConfig)
	{
		ShowWinConditionWidget(TargetConfig->GameStartDisplayDuration);
	}
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

void AExtendedCameraBase::UpdateTabModeUI()
{
	if (ResourceWidget)
	{
		ResourceWidget->StopTimer();
		ResourceWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (WinConditionWidget)
	{
		WinConditionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MapMenuWidget)
	{
		MapMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
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
				SetUserWidget(SelectedUnit);
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
				SetUserWidget(SelectedUnit);
			}
			TabToggled = true;
		}
		break;
	default:{
			TabToggled = false;
		}
		break;
	}
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

void AExtendedCameraBase::SetupResourceWidget(AExtendedControllerBase* CameraControllerBase)
{
		if(ResourceWidget)
		{
			if(CameraControllerBase)
			{
				ResourceWidget->SetTeamId(CameraControllerBase->SelectableTeamId);
				ResourceWidget->StartUpdateTimer();
			}
		}
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
	
	UEnhancedInputComponentBase* EnhancedInputComponentBase = Cast<UEnhancedInputComponentBase>(PlayerInputComponent);

	if(EnhancedInputComponentBase)
	{
		check(EnhancedInputComponentBase);
		const FGameplayTags& GameplayTags = FGameplayTags::Get();

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Tab_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Tab_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Tab_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Tab_Released, 0);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_V_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_V_Pressed, 0);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_LeftClick_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_LeftClick_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_LeftClick_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_LeftClick_Released, 0);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_RightClick_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_RightClick_Pressed, 0);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_G_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_G_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_A_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_A_Pressed, 0);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Shift_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Shift_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Shift_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Shift_Released, 0);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Ctrl_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Ctrl_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Ctrl_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Ctrl_Released, 0);
		
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
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_R_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 18);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Q_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 999);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_E_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 101010);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_T_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 12);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_P_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 14);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_O_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 15);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D1, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 13);
		//EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D2, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 13);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Middle_Mouse_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 16);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Middle_Mouse_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 17);

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_1_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 21);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_2_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 22);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_3_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 23);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_4_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 24);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_5_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 25);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_6_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 26);

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
}

void AExtendedCameraBase::SetUserWidget(AUnitBase* SelectedActor)
{
	
	if(!TalentChooserWidget) return;

	if(SelectedActor)
	{
		if (TalentChooserWidget) {
			TalentChooserWidget->SetVisibility(ESlateVisibility::Visible);
			TalentChooserWidget->SetOwnerActor(SelectedActor);
			TalentChooserWidget->CreateClassUIElements();
			TalentChooserWidget->StartUpdateTimer();
		}

		if (AbilityChooserWidget) {
			AbilityChooserWidget->SetVisibility(ESlateVisibility::Visible);
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
	
	if (UnitSelectorWidget && !UnitSelectorWidget->ControllerBase)
	{
		UnitSelectorWidget->InitWidget(WithPC);
		return true;
	}
	
	return false;
}


bool AExtendedCameraBase::InitTaggedSelectorWidgetController(ACustomControllerBase* WithPC)
{
	if (!WithPC)
		return false;
	
	if (TaggedSelectorWidget && !TaggedSelectorWidget->ControllerBase)
	{
		TaggedSelectorWidget->InitWidget(WithPC);
		return true;
	}
	
	return false;
}


bool AExtendedCameraBase::InitAbiltiyChooserWidgetController(ACustomControllerBase* WithPC)
{
	if (!WithPC)
		return false;
	
	if (AbilityChooserWidget && !AbilityChooserWidget->ControllerBase)
	{
		AbilityChooserWidget->InitWidget(WithPC);
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
	
	CamController->ActivateKeyboardAbilitiesOnMultipleUnits(InputID);
	
}

void AExtendedCameraBase::Input_LeftClick_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
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
	if(BlockControls) return;
	
	TabMode = (TabMode + 1) % 4;

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
	if (MapMenuWidget)
	{
		if (MapMenuWidget->GetVisibility() == ESlateVisibility::Visible)
		{
			MapMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
			BlockControls = false;
		}
		else
		{
			ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
			if (CameraControllerBase && CameraControllerBase->HUDBase)
			{
				if (CameraControllerBase->HUDBase->SelectedUnits.Num() > 0)
				{
					CameraControllerBase->HUDBase->DeselectAllUnits();
					CameraControllerBase->SelectedUnits = CameraControllerBase->HUDBase->SelectedUnits;
					return;
				}
			}

			MapMenuWidget->SetVisibility(ESlateVisibility::Visible);
			BlockControls = true;
		}
	}
}

void AExtendedCameraBase::SwitchControllerStateMachine(const FInputActionValue& InputActionValue, int32 NewCameraState)
{
    if (BlockControls) return;

    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());

    if (CameraControllerBase)
    {

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

