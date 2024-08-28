// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Characters/Unit/PerformanceUnit.h"

#include "Net/UnrealNetwork.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "Widgets/UnitTimerWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h" // For USkeletalMesh
#include "Engine/SkeletalMeshLODSettings.h" // To access LOD settings
#include "Engine/SkinnedAssetCommon.h"

void APerformanceUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckVisibility();
	CheckHealthBarVisibility();
	CheckTimerVisibility();
}

void APerformanceUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APerformanceUnit, HealthWidgetComp);
	DOREPLIFETIME(APerformanceUnit, TimerWidgetComp);
}



void APerformanceUnit::SetLODCount(int32 LODCount)
{
	// Ensure the SkeletalMeshComponent is valid
	if (GetMesh())
	{
		//USkeletalMeshComponent* SkeletalMeshComponent = GetMesh();

		// Get the current Skeletal Mesh using GetSkinnedAsset()
		//USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(SkeletalMeshComponent->GetSkinnedAsset());


	

			// Modify the Skeletal Mesh for changes
			//SkeletalMesh->Modify();
			//SkeletalMesh->SetLODSettings(nullptr); // Clear LOD settings to manually change LOD count

			// Adjust LOD count
			//SkeletalMesh->GetLODInfoArray().SetNum(LODCount);
			GetMesh()->SetPredictedLODLevel(LODCount);
			// Optional: Log the new LOD count for debugging
			UE_LOG(LogTemp, Warning, TEXT("LOD Count set to: %d"), LODCount);
		
	}
	
}



void APerformanceUnit::CheckVisibility()
{
	if (IsInViewport(GetActorLocation(), VisibilityOffset))
	{
		GetMesh()->SetVisibility(true);
		
		if(Projectile) Projectile->SetVisibility(true);
	}
	else
	{
		GetMesh()->SetVisibility(false);
		if(Projectile) Projectile->SetVisibility(false);
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
	if (UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(HealthWidgetComp->GetUserWidgetObject()))
	{
		if (IsInViewport(HealthWidgetComp->GetComponentLocation(), VisibilityOffset))
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
		if (IsInViewport(TimerWidgetComp->GetComponentLocation(), VisibilityOffset))
		{
			TimerWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			TimerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}