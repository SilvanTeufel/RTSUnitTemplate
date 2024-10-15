// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/PerformanceUnit.h"

#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Widgets/UnitTimerWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/SkeletalMesh.h" // For USkeletalMesh
#include "Engine/SkeletalMeshLODSettings.h" // To access LOD settings
#include "Engine/SkinnedAssetCommon.h"

APerformanceUnit::APerformanceUnit(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	// Initialize FogOfWarLight (PointLight or SpotLight)
	FogOfWarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FogOfWarLight"));
	// Attach the light component to the root or another component
	FogOfWarLight->SetupAttachment(RootComponent);

	// Set initial light properties
	FogOfWarLight->Intensity = 0.2f;             // Adjust intensity as needed
	FogOfWarLight->bUseInverseSquaredFalloff = false; // Avoid using inverse squared falloff for performance
	FogOfWarLight->LightFalloffExponent = 0.2f;     // Exponent for smoother falloff (optional)
	FogOfWarLight->AttenuationRadius = 500.f;      // Default range (will update this to SightRange later)
	FogOfWarLight->CastShadows = false;
	
}

void APerformanceUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	CheckHealthBarVisibility();
	CheckTimerVisibility();
	//VisibilityTick();
}

void APerformanceUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APerformanceUnit, HealthWidgetComp);
	DOREPLIFETIME(APerformanceUnit, TimerWidgetComp);
}

void APerformanceUnit::BeginPlay()
{
	Super::BeginPlay();
	

	AControllerBase* ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
	if(ControllerBase)
	{
		SetVisibility(false, ControllerBase->SelectableTeamId);
	}
	if (FogOfWarLight)
	{
		FogOfWarLight->SetVisibility(false);
	}
}

void APerformanceUnit::UpdateFogOfWarLight(int PlayerTeamId, float SightRange)
{
	bool IsFriendly = PlayerTeamId == TeamId;

	UE_LOG(LogTemp, Warning, TEXT("PlayerTeamId: %d"), PlayerTeamId);
	UE_LOG(LogTemp, Warning, TEXT("TeamId: %d"), TeamId);
	if (FogOfWarLight && IsFriendly)
	{
		// Set light range equal to SightRange
		FogOfWarLight->AttenuationRadius = SightRange;
		FogOfWarLight->SetVisibility(true);
		UE_LOG(LogTemp, Warning, TEXT("Light ON!"));
	}else if(FogOfWarLight && !IsFriendly)
	{
		//FogOfWarLight->AttenuationRadius = 0.f;
		//FogOfWarLight->SetVisibility(false, true);
		//FogOfWarLight->Intensity = 0.0f;  
		UE_LOG(LogTemp, Warning, TEXT("Light OFF!"));
	}
}

void APerformanceUnit::SetVisibility(bool IsVisible, int PlayerTeamId)
{
	bool IsFriendly = PlayerTeamId == TeamId;
	
	if (IsInViewport(GetActorLocation(), VisibilityOffset) && IsVisible && !IsFriendly)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Set Visible!"));
		USkeletalMeshComponent* SkelMesh = GetMesh();
		IsOnViewport = true;
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(true);
			SkelMesh->bPauseAnims = false;
		}
	}
	else if(!IsVisible && !IsFriendly)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Set Invisible!"));
		USkeletalMeshComponent* SkelMesh = GetMesh();
		IsOnViewport = false;
		if(SkelMesh)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Disabled Mesh!"));
			SkelMesh->SetVisibility(false);
			SkelMesh->bPauseAnims = true;
		}
	}
}

void APerformanceUnit::VisibilityTick()
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (IsOnViewport)
	{
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(true);
			SkelMesh->bPauseAnims = false;
		}
	}
	else
	{
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(false);
			SkelMesh->bPauseAnims = true;
		}
	}
}
void APerformanceUnit::CheckVisibility(int PlayerTeamId)
{


	if (IsInViewport(GetActorLocation(), VisibilityOffset) && PlayerTeamId == TeamId)
	{
		USkeletalMeshComponent* SkelMesh = GetMesh();
		IsOnViewport = true;
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(true);
			SkelMesh->bPauseAnims = false;
		}
	}
	else if(PlayerTeamId == TeamId)
	{
		USkeletalMeshComponent* SkelMesh = GetMesh();
		IsOnViewport = false;
		if(SkelMesh)
		{
			SkelMesh->SetVisibility(false);
			SkelMesh->bPauseAnims = true;
		}
	}
}


bool APerformanceUnit::IsInViewport(FVector WorldPosition, float Offset)
{
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(PlayerController, WorldPosition, ScreenPosition);

		int32 ViewportSizeX, ViewportSizeY;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		return ScreenPosition.X >= -Offset && ScreenPosition.X <= ViewportSizeX + Offset && ScreenPosition.Y >= -Offset && ScreenPosition.Y <= ViewportSizeY + Offset;
	}

	return false;
}

void APerformanceUnit::CheckHealthBarVisibility()
{
	if(HealthWidgetComp)
	if (UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject()))
	{
		if (IsOnViewport)
		{
			//HealthBarWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void APerformanceUnit::CheckTimerVisibility()
{
	if (UUnitTimerWidget* TimerWidget = Cast<UUnitTimerWidget>(TimerWidgetComp->GetUserWidgetObject())) // Assuming you have a UUnitBaseTimer class for the timer widget
	{
		if (IsOnViewport)
		{
			TimerWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			TimerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}