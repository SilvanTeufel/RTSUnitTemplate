// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/UnitStatsTooltipWidget.h"

#include "Actors/UnitPreviewStage.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GAS/AttributeSetBase.h"
#include "GAS/AutoAbility.h"
#include "GAS/GameplayAbilityBase.h"
#include "GameFramework/GameStateBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"

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
	bHasSnapshot = false;
	SnapshotPreview.Reset();
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

void UUnitStatsTooltipWidget::SetStatsSnapshot(TSubclassOf<AUnitBase> UnitClass, const FUnitStatsValues& Values, AActor* PreviewSource, bool bFillHeaderTexts)
{
	Unit.Reset();
	Snapshot = Values;
	SnapshotPreview = PreviewSource;
	SnapshotClass = UnitClass;
	bHasSnapshot = true;
	const AUnitBase* Defaults = UnitClass ? UnitClass->GetDefaultObject<AUnitBase>() : nullptr;
	if (bFillHeaderTexts && Defaults)
	{
		auto SetText = [this](const TCHAR* Var, const FText& Value)
		{
			if (FTextProperty* Prop = FindFProperty<FTextProperty>(GetClass(), Var))
			{
				Prop->SetPropertyValue_InContainer(this, Value);
			}
		};
		SetText(TEXT("Name"), FText::FromString(Defaults->Name));
		SetText(TEXT("TypeName"), Defaults->Type);
		SetText(TEXT("Description"), Defaults->Description);
	}
	Refresh();
	UpdatePreview();
}

void UUnitStatsTooltipWidget::UpdateStatsSnapshot(const FUnitStatsValues& Values)
{
	if (bHasSnapshot)
	{
		Snapshot = Values;
		Refresh();
	}
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
	AActor* Shown = nullptr;
	if (bOnScreen)
	{
		if (Unit.IsValid() && !IsUnitGone(Unit.Get()))
		{
			Shown = Unit.Get();
		}
		else if (bHasSnapshot)
		{
			Shown = SnapshotPreview.Get();
		}
	}
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

FText UUnitStatsTooltipWidget::BuildAbilitiesText(const AUnitBase* InUnit)
{
	if (!InUnit)
	{
		return FText::GetEmpty();
	}
	auto Clean = [](FString Name)
	{
		Name.ReplaceInline(TEXT("\n"), TEXT(" "));
		Name.TrimStartAndEndInline();
		Name.RemoveFromEnd(TEXT(":"));
		return Name.TrimStartAndEnd();
	};
	TArray<FString> Abilities;
	for (const TSubclassOf<UGameplayAbilityBase>& Class : InUnit->DefaultAbilities)
	{
		const UGameplayAbilityBase* CDO = Class ? Class->GetDefaultObject<UGameplayAbilityBase>() : nullptr;
		const FString Name = CDO ? Clean(CDO->AbilityName) : FString();
		if (!Name.IsEmpty() && !Name.StartsWith(TEXT("Ability X")))
		{
			Abilities.AddUnique(Name);
		}
	}
	TArray<FString> Lines;
	if (Abilities.Num() > 0)
	{
		const int32 Shown = FMath::Min(Abilities.Num(), 8);
		FString Line = FString::Printf(TEXT("Abilities: %s"), *FString::Join(TArrayView<const FString>(Abilities.GetData(), Shown), TEXT(", ")));
		if (Abilities.Num() > Shown)
		{
			Line += FString::Printf(TEXT(" (+%d)"), Abilities.Num() - Shown);
		}
		Lines.Add(Line);
	}
	TArray<FString> Auto;
	const AGameStateBase* GameState = InUnit->GetWorld() ? InUnit->GetWorld()->GetGameState() : nullptr;
	const float Now = GameState ? (float)GameState->GetServerWorldTimeSeconds() : 0.f;
	for (const FAutoAbilityStatus& S : InUnit->AutoAbilityStatus)
	{
		const UAutoAbility* A = S.Ability ? S.Ability->GetDefaultObject<UAutoAbility>() : nullptr;
		if (!A)
		{
			continue;
		}
		const float Left = S.ReadyAt - Now;
		Auto.Add(Left > 0.5f ? FString::Printf(TEXT("%s (%d s)"), *A->GetAutoName().ToString(), FMath::CeilToInt(Left)) : A->GetAutoName().ToString());
	}
	if (Auto.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("Auto abilities: %s"), *FString::Join(Auto, TEXT(", "))));
	}
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText UUnitStatsTooltipWidget::BuildUnitStatsText(const AUnitBase* InUnit)
{
	FUnitStatsValues Values;
	return CaptureUnitStats(InUnit, Values) ? BuildStatsText(Values) : FText::GetEmpty();
}

bool UUnitStatsTooltipWidget::CaptureUnitStats(const AUnitBase* InUnit, FUnitStatsValues& OutValues)
{
	const UAttributeSetBase* A = IsValid(InUnit) ? InUnit->Attributes : nullptr;
	if (!A)
	{
		return false;
	}
	OutValues.AutoAbilities.Reset();
	for (const FAutoAbilityStatus& S : InUnit->AutoAbilityStatus)
	{
		if (S.Ability)
		{
			OutValues.AutoAbilities.AddUnique(S.Ability);
		}
	}
	OutValues.Health = A->GetHealth();
	OutValues.MaxHealth = A->GetMaxHealth();
	OutValues.Shield = A->GetShield();
	OutValues.MaxShield = A->GetMaxShield();
	OutValues.AttackDamage = A->GetAttackDamage();
	OutValues.AttackInterval = InUnit->AttackDuration + InUnit->PauseDuration;
	OutValues.Range = A->GetRange();
	OutValues.RunSpeed = InUnit->CanMove ? A->GetRunSpeed() : -1.f;
	OutValues.Armor = A->GetArmor();
	OutValues.MagicResistance = A->GetMagicResistance();
	OutValues.Stamina = A->GetStamina();
	OutValues.AttackPower = A->GetAttackPower();
	OutValues.Willpower = A->GetWillpower();
	OutValues.Haste = A->GetHaste();
	OutValues.HealthRegeneration = A->GetHealthRegeneration();
	return true;
}

FText UUnitStatsTooltipWidget::BuildStatsText(const FUnitStatsValues& V)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Health: %s / %s"), *Num(V.Health), *Num(V.MaxHealth)));
	if (V.MaxShield > 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Shield: %s / %s"), *Num(V.Shield), *Num(V.MaxShield)));
	}
	Lines.Add(FString::Printf(TEXT("Damage: %s"), *Num(V.AttackDamage)));
	if (V.AttackInterval > 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Attack Speed: %s / s"), *Num(1.f / V.AttackInterval)));
	}
	Lines.Add(FString::Printf(TEXT("Range: %s"), *Num(V.Range)));
	if (V.RunSpeed >= 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Speed: %s"), *Num(V.RunSpeed)));
	}
	Lines.Add(FString::Printf(TEXT("Armor: %s"), *Num(V.Armor)));
	Lines.Add(FString::Printf(TEXT("Plasma Resistance: %s"), *Num(V.MagicResistance)));
	// Talent stats: Stamina is shown as Vitality, like in the game's texts.
	Lines.Add(FString::Printf(TEXT("Vitality: %s   Attack Power: %s"), *Num(V.Stamina), *Num(V.AttackPower)));
	Lines.Add(FString::Printf(TEXT("Willpower: %s   Haste: %s"), *Num(V.Willpower), *Num(V.Haste)));
	if (V.HealthRegeneration > 0.f)
	{
		Lines.Add(FString::Printf(TEXT("Health Regeneration: %s / s"), *Num(V.HealthRegeneration)));
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
	const bool bGone = bHasSnapshot
		? SnapshotPreview.IsStale()
		: (Unit.IsStale() || (Current && IsUnitGone(Current)));
	if (bCloseWhenUnitGone && bGone)
	{
		RemoveFromParent();
		return;
	}
	if (StatsText)
	{
		FText Text = Current ? BuildUnitStatsText(Current) : (bHasSnapshot ? BuildStatsText(Snapshot) : FText::GetEmpty());
		const bool bIcons = RefreshAbilityIcons(Current);
		const FText AbilitiesText = (Current && !bIcons) ? BuildAbilitiesText(Current) : FText::GetEmpty();
		if (!AbilitiesText.IsEmpty())
		{
			Text = FText::FromString(Text.ToString() + TEXT("\n\n") + AbilitiesText.ToString());
		}
		StatsText->SetText(Text);
		StatsText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

bool UUnitStatsTooltipWidget::RefreshAbilityIcons(const AUnitBase* Live)
{
	// Which abilities: the live unit, else the class defaults of the replay snapshot.
	const AUnitBase* Source = Live ? Live : (bHasSnapshot && SnapshotClass ? SnapshotClass->GetDefaultObject<AUnitBase>() : nullptr);
	TArray<const UGameplayAbilityBase*> Normal;
	TArray<TPair<const UGameplayAbilityBase*, float>> Auto;
	if (Source)
	{
		for (const TSubclassOf<UGameplayAbilityBase>& Class : Source->DefaultAbilities)
		{
			const UGameplayAbilityBase* CDO = Class ? Class->GetDefaultObject<UGameplayAbilityBase>() : nullptr;
			if (CDO && CDO->AbilityIcon && !CDO->AbilityName.StartsWith(TEXT("Ability X")))
			{
				Normal.AddUnique(CDO);
			}
		}
	}
	if (Live)
	{
		for (const FAutoAbilityStatus& S : Live->AutoAbilityStatus)
		{
			if (const UGameplayAbilityBase* CDO = S.Ability ? S.Ability->GetDefaultObject<UGameplayAbilityBase>() : nullptr)
			{
				Auto.Emplace(CDO, S.CooldownMultiplier);
			}
		}
	}
	else if (bHasSnapshot)
	{
		for (const TSubclassOf<UGameplayAbilityBase>& Class : Snapshot.AutoAbilities)
		{
			if (const UGameplayAbilityBase* CDO = Class ? Class->GetDefaultObject<UGameplayAbilityBase>() : nullptr)
			{
				Auto.Emplace(CDO, 1.f);
			}
		}
	}

	FString Signature;
	for (const UGameplayAbilityBase* A : Normal) { Signature += A->GetPathName() + TEXT(";"); }
	Signature += TEXT("|");
	for (const auto& A : Auto) { Signature += A.Key->GetPathName() + FString::Printf(TEXT("%.2f;"), A.Value); }

	if (!AbilityIconBox)
	{
		UPanelWidget* Parent = StatsText ? StatsText->GetParent() : nullptr;
		if (!Parent || !WidgetTree)
		{
			return false;
		}
		// The card itself must not catch clicks meant for the world under it - only the icons are hoverable
		// (the popup root is SelfHitTestInvisible, see ACustomControllerBase::ShowUnitStatsPopup).
		WidgetTree->ForEachWidget([](UWidget* W)
		{
			if (W && W->GetVisibility() == ESlateVisibility::Visible)
			{
				W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		});
		AbilityIconBox = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("AbilityIconBox"));
		AbilityIconBox->SetInnerSlotPadding(FVector2D(4.f, 4.f));
		Parent->InsertChildAt(Parent->GetChildIndex(StatsText) + 1, AbilityIconBox);
		AbilityIconSignature.Reset();
	}
	if (Signature == AbilityIconSignature)
	{
		return true;
	}
	AbilityIconSignature = Signature;
	AbilityIconBox->ClearChildren();

	auto AddIcon = [this](const UGameplayAbilityBase* Ability, bool bAuto, float CooldownMultiplier)
	{
		UTexture2D* Icon = Ability->AbilityIcon;
		FText Title;
		FText Body;
		if (bAuto)
		{
			if (const UAutoAbility* AutoAbility = Cast<UAutoAbility>(Ability))
			{
				Title = AutoAbility->GetAutoName();
				Body = AutoAbility->GetAutoDescription(CooldownMultiplier);
			}
		}
		if (Title.IsEmpty())
		{
			FString Name = Ability->AbilityName;
			Name.ReplaceInline(TEXT("\n"), TEXT(" "));
			Name.TrimStartAndEndInline();
			Name.RemoveFromEnd(TEXT(":"));
			Title = FText::FromString(Name.TrimStartAndEnd());
		}
		if (Body.IsEmpty())
		{
			Body = !Ability->Description.IsEmpty() ? Ability->Description : Ability->ToolTipText;
		}
		const FText Tip = FText::Format(NSLOCTEXT("UnitStats", "AbilityIconTip", "{0}{1}\n{2}"),
			bAuto ? NSLOCTEXT("UnitStats", "AutoPrefix", "[Auto] ") : FText::GetEmpty(), Title, Body);

		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(34.f);
		Size->SetHeightOverride(34.f);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetPadding(FMargin(2.f));
		Frame->SetBrushColor(bAuto ? FLinearColor(1.f, 0.72f, 0.15f, 0.95f) : FLinearColor(0.35f, 0.75f, 1.f, 0.55f));
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Icon)
		{
			Image->SetBrushFromTexture(Icon, false);
		}
		else
		{
			Image->SetColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
		}
		Frame->SetContent(Image);
		Size->SetContent(Frame);
		Size->SetToolTipText(Tip);
		Size->SetVisibility(ESlateVisibility::Visible);
		AbilityIconBox->AddChildToWrapBox(Size);
	};
	for (const UGameplayAbilityBase* A : Normal)
	{
		AddIcon(A, false, 1.f);
	}
	for (const auto& A : Auto)
	{
		AddIcon(A.Key, true, A.Value);
	}
	AbilityIconBox->SetVisibility((Normal.Num() + Auto.Num()) > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	return true;
}
