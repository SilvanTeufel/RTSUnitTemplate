// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/DamageIndicator.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Misc/Paths.h" // Include for FPaths
#include "Fonts/SlateFontInfo.h" // Include for FSlateFontInfo
#include "GameModes/RTSGameModeBase.h"

void UDamageIndicator::NativeConstruct()
{
	Super::NativeConstruct();
	StartUpdateTimer();
}

/*
void UDamageIndicator::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	UpdateIndicator();
}
*/
void UDamageIndicator::UpdateIndicator()
{
	// Set a repeating timer to call NativeTick at a regular interval based on UpdateInterval
	if (!Damage )
		return;
	

	if (!Indicator)
		return;

	
	AControllerBase* ControllerBase = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
	ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
	
	if (ControllerBase)
	{
		
		if (RTSGameMode && RTSGameMode->AllUnits.Num() > MaxUnitCount )
		{
			SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
	}
	
	
	FNumberFormattingOptions Opts;
	Opts.SetMaximumFractionalDigits(0);
	
	Indicator->SetText(FText::AsNumber(Damage, &Opts));

	Indicator->SetOpacity(CalculateOpacity());
	Indicator->SetFont(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), CalculateTextSize()));
	Indicator->SetColorAndOpacity(CalculateTextColor());
}


void UDamageIndicator::StartUpdateTimer()
{
	// Set a repeating timer to call NativeTick at a regular interval based on UpdateInterval
	GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UDamageIndicator::UpdateIndicator, UpdateInterval, true);
}

float UDamageIndicator::CalculateOpacity()
{
	// Static variable to persist the opacity value across function calls
	const float OpacityIncrement = 0.01f; // Amount to increase the opacity per tick

	Opacity -= OpacityIncrement;
	if (Opacity > 1.0f)
	{
		Opacity = 1.0f; // Clamp the opacity to a maximum of 1.0
	}else if(Opacity < 0.0f)
	{
		Opacity = 0.0f; // Clamp the opacity to a maximum of 1.0
	}

	return Opacity;
}

float UDamageIndicator::CalculateTextSize()
{
	// Calculate the text size based on the damage value
	float ClampedDamage = FMath::Clamp(Damage, MinDamage, MaxDamage);
	float Alpha = (ClampedDamage - MinDamage) / (MaxDamage - MinDamage);
	return MinTextSize + (MaxTextSize - MinTextSize) * Alpha;
}

FLinearColor UDamageIndicator::CalculateTextColor()
{
	// Calculate the text color based on the damage value
	float ClampedDamage = FMath::Clamp(Damage, MinDamage, MaxDamage);
	float Alpha = (ClampedDamage - MinDamage) / (MaxDamage - MinDamage); 

	// Apply ColorOffset
	Alpha += ColorOffset;

	// Ensure Alpha stays within the 0 to 1 range
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	
	return FLinearColor::LerpUsingHSV(LowDamageColor, HighDamageColor, Alpha);
}
