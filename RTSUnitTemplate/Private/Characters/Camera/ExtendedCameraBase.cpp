
#include "Characters/Camera/ExtendedCameraBase.h"

#include "Controller/CameraControllerBase.h"
#include "GAS/GAS.h"
#include "Widgets/AbilityChooser.h"
#include "Widgets/TalentChooser.h"

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
	
		GetCameraBaseCapsule()->BodyInstance.bLockXRotation = true;
		GetCameraBaseCapsule()->BodyInstance.bLockYRotation = true;
		GetCameraBaseCapsule()->BodyInstance.bLockZRotation = true;
	
		GetMesh()->AttachToComponent(GetCameraBaseCapsule(), FAttachmentTransformRules::KeepRelativeTransform);


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

	//SpawnTalentChooser();
	//SetTalentChooserLocation();
	// Your custom BeginPlay logic here
}

// Tick implementation
void AExtendedCameraBase::Tick(float DeltaTime)
{
	// Call the base class Tick
	Super::Tick(DeltaTime);

	//if(AutoAdjustTalentChooserPosition)
	//SetTalentChooserLocation();
	// Your custom Tick logic here
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
		}

		if (AbilityBar) {
			AbilityBar->SetVisibility(ESlateVisibility::Visible);
			AbilityBar->SetOwnerActor(SelectedActor);
			//AbilityBar->UsedAbilityPointsTextArray.Empty();
			//AbilityBar->InitializeButtonArray(AbilityBar->ButtonPreFixes[0], AbilityBar->OffensiveAbilityButtons);
			//AbilityBar->InitializeButtonArray(AbilityBar->ButtonPreFixes[1], AbilityBar->DefensiveAbilityButtons);
			//AbilityBar->InitializeButtonArray(AbilityBar->ButtonPreFixes[2], AbilityBar->AttackAbilityButtons);
			//AbilityBar->InitializeButtonArray(AbilityBar->ButtonPreFixes[3], AbilityBar->ThrowAbilityButtons);
		}
		
	}else
	{
		TalentBar->SetVisibility(ESlateVisibility::Collapsed);
		AbilityBar->SetVisibility(ESlateVisibility::Collapsed);
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

	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	if(CameraControllerBase)
	{
		CameraControllerBase->TPressed();
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
	
	ShowControlWidget();
}

void AExtendedCameraBase::Input_Tab_Released(const FInputActionValue& InputActionValue, int32 CamState)
{
	if(BlockControls) return;
	
	HideControlWidget();
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
	if(BlockControls) return;
	
	ACameraControllerBase* CameraControllerBase = Cast<ACameraControllerBase>(GetController());
	
	if(CameraControllerBase)
	{
		if(CameraControllerBase->IsStrgPressed)
			switch (NewCameraState)
			{
		case 0:
			{

			}break;
		case 1:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->WIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 111:
			{
				CameraControllerBase->WIsPressedState = 2;
			} break;
		case 2:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->SIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 222:
			{
				CameraControllerBase->SIsPressedState = 2;
			} break;
		case 3:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->AIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 333:
			{
				CameraControllerBase->AIsPressedState = 2;
			} break;
		case 4:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->DIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 444:
			{
				CameraControllerBase->DIsPressedState = 2;
			} break;
		case 5:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingInState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingInState = 1;
					SetCameraState(CameraData::ZoomIn);
				}
			}
				break;
		case 555:
			{
					CameraControllerBase->CamIsZoomingInState = 2;
			}
			break;
		case 6:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingOutState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingOutState = 1;
					SetCameraState(CameraData::ZoomOut);
				}
			}
			break;
		case 666:
			{
				CameraControllerBase->CamIsZoomingOutState = 2;
			}
			break;
		case 7: SetCameraState(CameraData::ZoomOutPosition); break;
		case 8: SetCameraState(CameraData::ZoomInPosition); break;
		case 9:
			{
				if(CameraControllerBase->LockCameraToCharacter)
					return;
				
				SetCameraState(CameraData::RotateLeft);
			} break;
		case 10:
			{
				if(CameraControllerBase->LockCameraToCharacter)
					return;
				
				SetCameraState(CameraData::RotateRight);
			} break;
		case 11: SetCameraState(CameraData::LockOnCharacter); break;
		case 12: SetCameraState(CameraData::ZoomToThirdPerson); break;
		case 14:
			{	SetCameraState(CameraData::OrbitAndMove); break;
				//CameraControllerBase->OrbitAtLocation(FVector(1000.f, -1000.f, 500.f), 0.033f);
			} break;
		case 15:
			{

				CameraControllerBase->SpawnMissileRain(4, FVector(1000.f, -1000.f, 1000.f));

				CameraControllerBase->SpawnEffectArea(3, FVector(1000.f, -1000.f, 10.f), FVector(5), CameraControllerBase->EffectAreaClass);
			} break;
		case 16:
			{
				CameraControllerBase->MiddleMouseIsPressed = true;
			} break;
		case 17:
			{
				CameraControllerBase->MiddleMouseIsPressed = false;
			} break;
		case 21:
			{
				
			} break;
		case 22:
			{
				
			} break;
		case 23:
			{
				
			} break;
		case 24:
			{
				
			} break;
		case 25:
			{
				
			} break;
		case 26:
			{
				
			} break;
		default:
			{
				//SetCameraState(CameraData::UseScreenEdges);
			}break;
		}

		if(!CameraControllerBase->IsStrgPressed)
		switch (NewCameraState)
		{
		case 1:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->WIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 111:
			{
				CameraControllerBase->WIsPressedState = 2;
			} break;
		case 2:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->SIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 222:
			{
				CameraControllerBase->SIsPressedState = 2;
			} break;
		case 3:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->AIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 333:
			{
				CameraControllerBase->AIsPressedState = 2;
			} break;
		case 4:
			{
				if(GetCameraState() == CameraData::OrbitAndMove)
				{
					CameraControllerBase->CamIsRotatingLeft = false;
					CameraControllerBase->CamIsRotatingRight = false;
				}
				
				CameraControllerBase->DIsPressedState = 1;
				CameraControllerBase->LockCameraToUnit = false;
				SetCameraState(CameraData::MoveWASD);
			} break;
		case 444:
			{
				CameraControllerBase->DIsPressedState = 2;
			} break;
		case 5:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingInState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingInState = 1;
					SetCameraState(CameraData::ZoomIn);
				}
			}
				break;
		case 555:
			{
					CameraControllerBase->CamIsZoomingInState = 2;
			}
			break;
		case 6:
			{
				if(CameraControllerBase->LockCameraToCharacter)
				{
					if(!CameraControllerBase->HoldZoomOnLockedCharacter) CameraControllerBase->CamIsZoomingOutState = 1;
				
					CameraControllerBase->HoldZoomOnLockedCharacter = !CameraControllerBase->HoldZoomOnLockedCharacter;
				
				} else
				{
					CameraControllerBase->CamIsZoomingOutState = 1;
					SetCameraState(CameraData::ZoomOut);
				}
			}
			break;
		case 666:
			{
				CameraControllerBase->CamIsZoomingOutState = 2;
			}
			break;
		case 8: SetCameraState(CameraData::ZoomInPosition); break;
		case 9:
			{
				CameraControllerBase->CamIsRotatingLeft = true;
				
				if(CameraControllerBase->LockCameraToCharacter ||
					CameraControllerBase->WIsPressedState ||
					CameraControllerBase->AIsPressedState ||
					CameraControllerBase->SIsPressedState ||
					CameraControllerBase->DIsPressedState)
					return;
				
				SetCameraState(CameraData::HoldRotateLeft);
			} break;
		case 10:
			{
				CameraControllerBase->CamIsRotatingRight = true;
				
				if(CameraControllerBase->LockCameraToCharacter ||
					CameraControllerBase->WIsPressedState ||
					CameraControllerBase->AIsPressedState ||
					CameraControllerBase->SIsPressedState ||
					CameraControllerBase->DIsPressedState)
					return;
				
				SetCameraState(CameraData::HoldRotateRight);
			} break;
		case 999:
			{
				CameraControllerBase->CamIsRotatingLeft = false;
			}
			break;
		case 101010:
			{
				CameraControllerBase->CamIsRotatingRight = false;
			}
			break;
		case 12:
			{
				CameraControllerBase->TPressed();
			}
			break;
		case 13:
			{
				float FloatValue = InputActionValue.Get<float>();

				if(CameraControllerBase->ScrollZoomCount <= 10.f)
				CameraControllerBase->ScrollZoomCount += FloatValue*2;
				
				if(CameraControllerBase->LockCameraToCharacter)
					return;
				
				if(FloatValue > 0)
				{
					SetCameraState(CameraData::ScrollZoomIn);
				}
				else
				{
					SetCameraState(CameraData::ScrollZoomOut);
				}
			}
			break;
		case 15:
			{
				UE_LOG(LogTemp, Warning, TEXT("15 pressed!"));

			} break;
		case 16:
			{
				CameraControllerBase->MiddleMouseIsPressed = true;
			} break;
		case 17:
			{
				CameraControllerBase->MiddleMouseIsPressed = false;
			} break;
		case 21:
			{
				for (AGASUnit* SelectedUnit : CameraControllerBase->SelectedUnits)
				{
					if (SelectedUnit)
					{
						UE_LOG(LogTemp, Warning, TEXT("Activating AbilityOne for unit: %s"), *SelectedUnit->GetName());
						OnAbilityInputDetected(EGASAbilityInputID::AbilityOne, SelectedUnit, SelectedUnit->DefaultAbilities);
					}
				}
			} break;
		case 22:
			{
				for (AGASUnit* SelectedUnit : CameraControllerBase->SelectedUnits)
				{
					if (SelectedUnit)
					{
						UE_LOG(LogTemp, Warning, TEXT("Activating AbilityTwo for unit: %s"), *SelectedUnit->GetName());
						OnAbilityInputDetected(EGASAbilityInputID::AbilityTwo, SelectedUnit, SelectedUnit->DefaultAbilities);
					}
				}
			} break;
		case 23:
			{
				for (AGASUnit* SelectedUnit : CameraControllerBase->SelectedUnits)
				{
					if (SelectedUnit)
					{
						UE_LOG(LogTemp, Warning, TEXT("Activating AbilityThree for unit: %s"), *SelectedUnit->GetName());
						OnAbilityInputDetected(EGASAbilityInputID::AbilityThree, SelectedUnit, SelectedUnit->DefaultAbilities);
					}
				}
			} break;
		case 24:
			{
				for (AGASUnit* SelectedUnit : CameraControllerBase->SelectedUnits)
				{
					if (SelectedUnit)
					{
						UE_LOG(LogTemp, Warning, TEXT("Activating AbilityFour for unit: %s"), *SelectedUnit->GetName());
						OnAbilityInputDetected(EGASAbilityInputID::AbilityFour, SelectedUnit, SelectedUnit->DefaultAbilities);
					}
				}
			} break;
		case 25:
			{
				for (AGASUnit* SelectedUnit : CameraControllerBase->SelectedUnits)
				{
					if (SelectedUnit)
					{
						UE_LOG(LogTemp, Warning, TEXT("Activating AbilityFive for unit: %s"), *SelectedUnit->GetName());
						OnAbilityInputDetected(EGASAbilityInputID::AbilityFive, SelectedUnit, SelectedUnit->DefaultAbilities);
					}
				}
			} break;
		case 26:
			{
				for (AGASUnit* SelectedUnit : CameraControllerBase->SelectedUnits)
				{
					if (SelectedUnit)
					{
						UE_LOG(LogTemp, Warning, TEXT("Activating AbilitySix for unit: %s"), *SelectedUnit->GetName());
						OnAbilityInputDetected(EGASAbilityInputID::AbilitySix, SelectedUnit, SelectedUnit->DefaultAbilities);
					}
				}
			} break;
		}
	}

}
