
#include "Characters/Camera/ExtendedCameraBase.h"

#include "Characters/Unit/BuildingBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "GameModes/ResourceGameMode.h"
#include "GameStates/ResourceGameState.h"
#include "GAS/GAS.h"
#include "Widgets/AbilityChooser.h"
#include "Widgets/ResourceWidget.h"
#include "Widgets/TalentChooser.h"
#include "Widgets/UnitWidgetSelector.h"

AExtendedCameraBase::AExtendedCameraBase(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	if (RootComponent == nullptr) {
		RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	}
	
	CreateCameraComp();

	ControlWidgetComp = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("ControlWidget"));
	ControlWidgetComp->AttachToComponent(RootScene, FAttachmentTransformRules::KeepRelativeTransform);

	TalentChooser = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("TalentChooser"));
	TalentChooser->AttachToComponent(RootScene, FAttachmentTransformRules::KeepRelativeTransform);
	
	AbilityChooser = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("AbilityChooser"));
	AbilityChooser->AttachToComponent(RootScene, FAttachmentTransformRules::KeepRelativeTransform);

	WidgetSelector = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("WidgetSelector"));
	WidgetSelector->AttachToComponent(RootScene, FAttachmentTransformRules::KeepRelativeTransform);

	ResourceWidget = ObjectInitializer.CreateDefaultSubobject<UWidgetComponent>(this, TEXT("ResourceWidget"));
	ResourceWidget->AttachToComponent(RootScene, FAttachmentTransformRules::KeepRelativeTransform);
	
	
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
	
}

// BeginPlay implementation
void AExtendedCameraBase::BeginPlay()
{
	// Call the base class BeginPlay
	Super::BeginPlay();
	SetupResourceWidget();

}

void AExtendedCameraBase::SetupResourceWidget()
{
	if(!ResourceWidget) return;
	
	if (IsOwnedByLocalPlayer()) // This is a pseudo-function, replace with actual ownership check
	{
		ResourceWidget->SetVisibility(true);
		UResourceWidget* ResourceBar = Cast<UResourceWidget>(ResourceWidget->GetUserWidgetObject());

		if(ResourceBar)
		{
			ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
			
			if(CameraControllerBase)
			{
				ResourceBar->SetTeamId(CameraControllerBase->SelectableTeamId);
				ResourceBar->StartUpdateTimer();
			}
		}
	}
	else
	{
		ResourceWidget->SetVisibility(false);
	}
}

// Tick implementation
void AExtendedCameraBase::Tick(float DeltaTime)
{
	// Call the base class Tick
	Super::Tick(DeltaTime);
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

		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Q_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 999);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_E_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 101010);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_T_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 12);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_P_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 14);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_O_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 15);
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D1, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 13);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Scroll_D2, ETriggerEvent::Triggered, this, &AExtendedCameraBase::SwitchControllerStateMachine, 13);

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
		
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Alt_Pressed, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Alt_Pressed, 0);
		EnhancedInputComponentBase->BindActionByTag(InputConfig, GameplayTags.InputTag_Alt_Released, ETriggerEvent::Triggered, this, &AExtendedCameraBase::Input_Alt_Released, 0);
		

	}

}

void AExtendedCameraBase::SetUserWidget(AUnitBase* SelectedActor)
{

	UTalentChooser* TalentBar= Cast<UTalentChooser>(TalentChooser->GetUserWidgetObject());
	UAbilityChooser* AbilityBar= Cast<UAbilityChooser>(AbilityChooser->GetUserWidgetObject());
	
	if(!TalentBar) return;

	if(SelectedActor)
	{
		if (TalentBar) {
			TalentBar->SetVisibility(ESlateVisibility::Visible);
			TalentBar->SetOwnerActor(SelectedActor);
			TalentBar->CreateClassUIElements();
			TalentBar->StartUpdateTimer();
		}

		if (AbilityBar) {
			AbilityBar->SetVisibility(ESlateVisibility::Visible);
			AbilityBar->SetOwnerActor(SelectedActor);
			AbilityBar->StartUpdateTimer();
		}
		
	}else
	{
		TalentBar->StopTimer();
		AbilityBar->StopTimer();
		TalentBar->SetVisibility(ESlateVisibility::Collapsed);
		AbilityBar->SetVisibility(ESlateVisibility::Collapsed);
		
	}

}

void AExtendedCameraBase::SetSelectorWidget(int Id, AUnitBase* SelectedActor)
{
	UUnitWidgetSelector* WSelector = Cast<UUnitWidgetSelector>(WidgetSelector->GetUserWidgetObject());
	
	if(WSelector && SelectedActor)
	{
		WSelector->SetButtonColours(Id);
		FString CharacterName = SelectedActor->Name + " / " + FString::FromInt(Id);
		if (WSelector->Name)
		{
			WSelector->Name->SetText(FText::FromString(CharacterName));
		}
	}
}

void AExtendedCameraBase::OnAbilityInputDetected(EGASAbilityInputID InputID, AGASUnit* SelectedUnit, const TArray<TSubclassOf<UGameplayAbilityBase>>& AbilitiesArray)
{

	if(SelectedUnit)
	{
		//UE_LOG(LogTemp, Warning, TEXT("OnAbilityInputDetected: Activating ability ID %d for unit: %s"), static_cast<int32>(InputID), *SelectedUnit->GetName());
		SelectedUnit->ActivateAbilityByInputID(InputID, AbilitiesArray);
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
		CameraControllerBase->LeftClickPressed();
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
		CameraControllerBase->LeftClickReleased();
	}
}

void AExtendedCameraBase::Input_RightClick_Pressed(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->RightClickPressed();
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
		CameraControllerBase->IsStrgPressed = true;
	}
}

void AExtendedCameraBase::Input_Ctrl_Released(const FInputActionValue& InputActionValue, int32 Camstate)
{
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->IsStrgPressed = false;
	}
}



void AExtendedCameraBase::Input_Tab_Pressed(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	TabToggled = !TabToggled;
	if(TabToggled)
	{
		ShowControlWidget();
		
		ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
		if(!CameraControllerBase || !CameraControllerBase->HUDBase) return;

		if(CameraControllerBase->HUDBase->SelectedUnits.Num())
		{ 
			AUnitBase* SelectedUnit = CameraControllerBase->HUDBase->SelectedUnits[0];
			SetUserWidget(SelectedUnit);
		}
		
		if (ResourceWidget)
		{
			UResourceWidget* ResourceBar= Cast<UResourceWidget>(ResourceWidget->GetUserWidgetObject());
			if(ResourceBar) ResourceBar->StartUpdateTimer();
			ResourceWidget->SetVisibility(true);
		}


	}
}

void AExtendedCameraBase::Input_Tab_Released(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	if(!TabToggled)
	{
		HideControlWidget();
		
		SetUserWidget(nullptr);

		/*
		if (ResourceWidget)
		{
			UResourceWidget* ResourceBar= Cast<UResourceWidget>(ResourceWidget->GetUserWidgetObject());
			if(ResourceBar) ResourceBar->StopTimer();
			ResourceWidget->SetVisibility(false);
		}
		*/
	}
}

void AExtendedCameraBase::Input_Tab_Released_BP(int32 CamState)
{
	if(BlockControls) return;
	
	if(!TabToggled)
	{
		HideControlWidget();
		
		SetUserWidget(nullptr);

		if (ResourceWidget)
		{
			UResourceWidget* ResourceBar= Cast<UResourceWidget>(ResourceWidget->GetUserWidgetObject());
			if(ResourceBar) ResourceBar->StopTimer();
			ResourceWidget->SetVisibility(false);
		}
	}
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

void AExtendedCameraBase::SwitchControllerStateMachine(const FInputActionValue& InputActionValue, int32 NewCameraState)
{
    if (BlockControls) return;

    ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());

    if (CameraControllerBase)
    {
        if (CameraControllerBase->IsStrgPressed)
        {
            switch (NewCameraState)
            {
            	
            case 0:
            	{

            	}break;
            case 1: HandleState_MoveW(CameraControllerBase); break;
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
            		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
            			SetCameraState(CameraData::ZoomOutPosition);
            	} break;
            case 8:
            	{
            		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
            			SetCameraState(CameraData::ZoomInPosition);
            	} break;
            case 9: HandleState_RotateLeft(CameraControllerBase); break;
            case 999: HandleState_StopRotateLeft(CameraControllerBase); break;
            case 10: HandleState_RotateRight(CameraControllerBase); break;
            case 101010: HandleState_StopRotateRight(CameraControllerBase); break;
            case 13:
	            {
            		HandleState_AbilityArrayIndex(InputActionValue, CameraControllerBase); break;
	            } break;
            case 21: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilitySeven, CameraControllerBase);break;
            case 22: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityEight, CameraControllerBase); break;
            case 23: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityNine, CameraControllerBase); break;
            case 24: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityTen, CameraControllerBase); break;
            case 25: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityEleven, CameraControllerBase); break;
            case 26: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityTwelve, CameraControllerBase); break;
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
            case 8:
            	{
            		if(GetCameraState() != CameraData::LockOnCharacterWithTag)
            			SetCameraState(CameraData::ZoomInPosition);
            	} break;
            case 9: HandleState_RotateLeft_NoStrg(CameraControllerBase); break;
            case 999: HandleState_StopRotateLeft_NoStrg(CameraControllerBase); break;
            case 10: HandleState_RotateRight_NoStrg(CameraControllerBase); break;
            case 101010: HandleState_StopRotateRight_NoStrg(CameraControllerBase); break;
            case 12: HandleState_TPressed(CameraControllerBase); break;
            case 13: HandleState_ScrollZoom(InputActionValue, CameraControllerBase); break;
            case 16: HandleState_MiddleMousePressed(CameraControllerBase); break;
            case 17: HandleState_MiddleMouseReleased(CameraControllerBase); break;
            case 21: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityOne, CameraControllerBase);break;
            case 22: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityTwo, CameraControllerBase); break;
            case 23: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityThree, CameraControllerBase); break;
            case 24: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityFour, CameraControllerBase); break;
            case 25: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilityFive, CameraControllerBase); break;
            case 26: ExecuteOnAbilityInputDetected(EGASAbilityInputID::AbilitySix, CameraControllerBase); break;
            case 29: F3_Pressed(CameraControllerBase); break;
            case 30: F4_Pressed(CameraControllerBase); break;
            default: break;
            }
        }
    }
}

void AExtendedCameraBase::F3_Pressed(ACameraControllerBase* CameraControllerBase)
{
	CameraControllerBase->AddAbilityIndex(1);
}

void AExtendedCameraBase::F4_Pressed(ACameraControllerBase* CameraControllerBase)
{
	CameraControllerBase->AddAbilityIndex(-1);
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

    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveW_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    if (GetCameraState() == CameraData::LockOnCharacterWithTag)
        CameraControllerBase->CameraUnitWithTag->SetUnitState(UnitData::Idle);

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

    if (GetCameraState() == CameraData::LockOnCharacterWithTag)
        CameraControllerBase->CameraUnitWithTag->SetUnitState(UnitData::Idle);

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
    if (GetCameraState() == CameraData::LockOnCharacterWithTag)
        CameraControllerBase->CameraUnitWithTag->SetUnitState(UnitData::Idle);

    CameraControllerBase->AIsPressedState = 2;
}

void AExtendedCameraBase::HandleState_MoveD_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    CameraControllerBase->DIsPressedState = 1;
    CameraControllerBase->LockCameraToUnit = false;

    if (GetCameraState() != CameraData::LockOnCharacterWithTag)
        SetCameraState(CameraData::MoveWASD);
}

void AExtendedCameraBase::HandleState_StopMoveD_NoStrg(ACameraControllerBase* CameraControllerBase)
{
    if (GetCameraState() == CameraData::LockOnCharacterWithTag)
        CameraControllerBase->CameraUnitWithTag->SetUnitState(UnitData::Idle);

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
	float FloatValue = InputActionValue.Get<float>();
				
	if(FloatValue > 0)
	{
		CameraControllerBase->AddAbilityIndex(1);
	}
	else
	{
		CameraControllerBase->AddAbilityIndex(-1);
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

