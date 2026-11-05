// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/UnitStatsTooltipWidget.h"

#include "Actors/UnitPreviewStage.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GAS/AttributeSetBase.h"

namespace
{
	bool IsUnitGone(const AUnitBase* InUnit)
	{
		return !IsValid(InUnit) || InUnit->GetUnitState() == UnitData::Dead;
	}

	FString Num(float Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = FMath::Abs(Value) < 10.f ? 1 : 0;
		return FText::AsNumber(Value, &Options).ToString();
	}
}

void UUnitStatsTooltipWidget::SetUnit(AUnitBase* InUnit, bool bFillHeaderTexts)
{
	Unit = InUnit;
	if (bFillHeaderTexts && InUnit)
	{
		// Blueprint variables by name, like the button Blueprints fill them on CreateWidget.
		auto SetText = [this](const TCHAR* Var, const FText& Value)
		{
			if (FTextProperty* Prop = FindFProperty<FTextProperty>(GetClass(), Var))
			{
				Prop->SetPropertyValue_InContainer(this, Value);
			}
		};
		SetText(TEXT("Name"), FText::FromString(InUnit->Name));
		SetText(TEXT("TypeName"), InUnit->Type);
		SetText(TEXT("Description"), InUnit->Description);
	}
	Refresh();
	UpdatePreview();
}

void UUnitStatsTooltipWidget::UpdatePreview()
{
	// Material from the class defaults as fallback, like the cassette widget's tooltip class.
	UMaterialInterface* Material = UnitPreviewMaterial ? UnitPreviewMaterial : GetClass()->GetDefaultObject<UUnitStatsTooltipWidget>()->UnitPreviewMaterial;
	if (UnitPreviewImage && Material)
	{
		UnitPreviewImage->SetBrushFromMaterial(Material);
	}
	// Only while constructed (on screen): the stage is destroyed again in NativeDestruct. Beside the selector's
	// stage, so neither lights the other's model.
	AUnitBase* Shown = bOnScreen && Unit.IsValid() && !IsUnitGone(Unit.Get()) ? Unit.Get() : nullptr;
	AUnitPreviewStage::UpdateWindow(this, PreviewStage, UnitPreviewBox, Material, UnitPreviewTextureParameter, Shown,
		UnitPreviewRotationSpeed, UnitPreviewCaptureRate, FVector(30000.f, 0.f, 150000.f));
}

void UUnitStatsTooltipWidget::NativeDestruct()
{
	bOnScreen = false;
	if (PreviewStage)
	{
		PreviewStage->Destroy();
		PreviewStage = nullptr;
	}
	Super::NativeDestruct();
}

FText UUnitStatsTooltipWidget::BuildUnitStatsText(const AUnitBase* InUnit)
{
	const UAttributeSetBase* A = IsValid(InUnit) ? InUnit->Attributes : nullptr;
	if (!A)
	{
		return FText::GetEmpty();
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Health: %s / %s"), *Num(A->GetHealth()), *Num(A->GetMaxHealth())));
	if (A->GetMaxShield() > 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Shield: %s / %s"), *Num(A->GetShield()), *Num(A->GetMaxShield())));
	}
	Lines.Add(FString::Printf(TEXT("Damage: %s"), *Num(A->GetAttackDamage())));
	const float AttackInterval = InUnit->AttackDuration + InUnit->PauseDuration;
	if (AttackInterval > 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Attack Speed: %s / s"), *Num(1.f / AttackInterval)));
	}
	Lines.Add(FString::Printf(TEXT("Range: %s"), *Num(A->GetRange())));
	if (InUnit->CanMove)
	{
		Lines.Add(FString::Printf(TEXT("Speed: %s"), *Num(A->GetRunSpeed())));
	}
	Lines.Add(FString::Printf(TEXT("Armor: %s"), *Num(A->GetArmor())));
	Lines.Add(FString::Printf(TEXT("Plasma Resistance: %s"), *Num(A->GetMagicResistance())));
	// Talent stats: Stamina is shown as Vitality, like in the game's texts.
	Lines.Add(FString::Printf(TEXT("Vitality: %s   Attack Power: %s"), *Num(A->GetStamina()), *Num(A->GetAttackPower())));
	Lines.Add(FString::Printf(TEXT("Willpower: %s   Haste: %s"), *Num(A->GetWillpower()), *Num(A->GetHaste())));
	if (A->GetHealthRegeneration() > 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Health Regeneration: %s / s"), *Num(A->GetHealthRegeneration())));
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

void UUnitStatsTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bOnScreen = true;
	SinceRefresh = 0.f;
	Refresh();
	UpdatePreview();
}

void UUnitStatsTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SinceRefresh += InDeltaTime;
	if (SinceRefresh >= RefreshInterval)
	{
		SinceRefresh = 0.f;
		Refresh();
	}
}

void UUnitStatsTooltipWidget::Refresh()
{
	AUnitBase* Current = Unit.Get();
	if (bCloseWhenUnitGone && (Unit.IsStale() || (Current && IsUnitGone(Current))))
	{
		RemoveFromParent();
		return;
	}
	if (StatsText)
	{
		const FText Text = Current ? BuildUnitStatsText(Current) : FText::GetEmpty();
		StatsText->SetText(Text);
		StatsText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
