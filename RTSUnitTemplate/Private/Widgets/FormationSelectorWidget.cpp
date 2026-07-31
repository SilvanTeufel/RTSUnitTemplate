// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/FormationSelectorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

void UFormationShapeButton::HandleClicked()
{
	OnShapeChosen.Broadcast(Shape);
}

UFormationSelectorWidget::UFormationSelectorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Ship usable out of the box: the six generated icons, in enum order. Everything stays
	// EditAnywhere, so a project can drop entries it does not want (VerticalLine and Staggered are
	// the usual candidates) or swap the artwork.
	struct FDefaultEntry { EGridShape Shape; const TCHAR* Path; };
	static const FDefaultEntry Defaults[] = {
		{ EGridShape::Square,       TEXT("/RTSUnitTemplate/RTSUnitTemplate/Images/T_Formation_Square.T_Formation_Square") },
		{ EGridShape::Staggered,    TEXT("/RTSUnitTemplate/RTSUnitTemplate/Images/T_Formation_Staggered.T_Formation_Staggered") },
		{ EGridShape::VerticalLine, TEXT("/RTSUnitTemplate/RTSUnitTemplate/Images/T_Formation_VLine.T_Formation_VLine") },
		{ EGridShape::Circle,       TEXT("/RTSUnitTemplate/RTSUnitTemplate/Images/T_Formation_Circle.T_Formation_Circle") },
		{ EGridShape::HalfCircle,   TEXT("/RTSUnitTemplate/RTSUnitTemplate/Images/T_Formation_HalfCircle.T_Formation_HalfCircle") },
		{ EGridShape::Triangle,     TEXT("/RTSUnitTemplate/RTSUnitTemplate/Images/T_Formation_Triangle.T_Formation_Triangle") },
	};

	for (const FDefaultEntry& Default : Defaults)
	{
		FFormationSelectorEntry Entry;
		Entry.Shape = Default.Shape;
		// A missing texture leaves Icon null; the button still works, it just shows no glyph.
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> Finder(Default.Path);
		Entry.Icon = Finder.Get();
		Shapes.Add(Entry);
	}
}

AControllerBase* UFormationSelectorWidget::GetRTSController() const
{
	return Cast<AControllerBase>(GetOwningPlayer());
}

void UFormationSelectorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebuildButtons();
	TryBindToController();
}

void UFormationSelectorWidget::NativeDestruct()
{
	if (AControllerBase* Controller = BoundController.Get())
	{
		Controller->OnGridFormationShapeChanged.RemoveDynamic(this, &UFormationSelectorWidget::HandleShapeChangedExternally);
	}
	BoundController.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}

	Super::NativeDestruct();
}

void UFormationSelectorWidget::TryBindToController()
{
	if (BoundController.IsValid())
	{
		return;
	}

	AControllerBase* Controller = GetRTSController();
	if (!Controller)
	{
		// The HUD can be constructed before the controller is ready; retry rather than give up,
		// otherwise the highlight would never follow the hotkey for the rest of the session.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(BindRetryTimerHandle, this,
				&UFormationSelectorWidget::TryBindToController, 0.5f, /*bLoop=*/true);
		}
		return;
	}

	Controller->OnGridFormationShapeChanged.AddDynamic(this, &UFormationSelectorWidget::HandleShapeChangedExternally);
	BoundController = Controller;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}

	RefreshSelection();
}

void UFormationSelectorWidget::HandleShapeChangedExternally(EGridShape NewShape)
{
	RefreshSelection();
}

void UFormationSelectorWidget::RebuildButtons()
{
	ShapeButtons.Reset();
	ShapeImages.Reset();

	if (!WidgetTree)
	{
		return;
	}

	// The WBP is allowed to omit the container - build one so the widget is usable with an empty
	// designer graph.
	if (!ButtonBox)
	{
		ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonBox"));
		if (!ButtonBox)
		{
			return;
		}
		if (!WidgetTree->RootWidget)
		{
			WidgetTree->RootWidget = ButtonBox;
		}
	}

	ButtonBox->ClearChildren();

	const FVector2D Size(FMath::Max(ButtonSize.X, 8.f), FMath::Max(ButtonSize.Y, 8.f));
	// Not named "Padding": that shadows UUserWidget::Padding and the build treats it as an error.
	const float SlotPadding = FMath::Max(ButtonPadding, 0.f);

	for (const FFormationSelectorEntry& Entry : Shapes)
	{
		UFormationShapeButton* Button = WidgetTree->ConstructWidget<UFormationShapeButton>(UFormationShapeButton::StaticClass());
		if (!Button)
		{
			continue;
		}
		Button->Shape = Entry.Shape;
		// Two hops on purpose: OnClicked has no payload, so the button re-broadcasts with its shape.
		Button->OnClicked.AddDynamic(Button, &UFormationShapeButton::HandleClicked);
		Button->OnShapeChosen.AddDynamic(this, &UFormationSelectorWidget::HandleShapeChosen);

		if (bOverrideButtonStyle)
		{
			// A dark plate so the white icons actually read. The engine default is a light grey
			// panel, which leaves an unselected (white, low-alpha) icon almost invisible.
			FButtonStyle Style = Button->GetStyle();
			Style.Normal.TintColor = FSlateColor(ButtonNormalColor);
			Style.Hovered.TintColor = FSlateColor(ButtonHoveredColor);
			Style.Pressed.TintColor = FSlateColor(ButtonPressedColor);
			Style.Disabled.TintColor = FSlateColor(ButtonNormalColor);
			Button->SetStyle(Style);
		}

		// Tooltip straight off the enum, so it always matches the UMETA DisplayNames.
		if (const UEnum* EnumPtr = StaticEnum<EGridShape>())
		{
			Button->SetToolTipText(EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Entry.Shape)));
		}

		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Icon)
		{
			if (Entry.Icon)
			{
				Icon->SetBrushFromTexture(Entry.Icon, false);
			}
			Icon->SetDesiredSizeOverride(Size);
			Button->AddChild(Icon);
		}

		if (UHorizontalBoxSlot* BoxSlot = ButtonBox->AddChildToHorizontalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(SlotPadding));
		}

		ShapeButtons.Add(Button);
		ShapeImages.Add(Icon);
	}

	RefreshSelection();
}

void UFormationSelectorWidget::RefreshSelection()
{
	const AControllerBase* Controller = GetRTSController();
	if (!Controller)
	{
		return;
	}

	const EGridShape Active = Controller->GridFormationShape;
	for (int32 i = 0; i < ShapeButtons.Num(); ++i)
	{
		UImage* Icon = ShapeImages.IsValidIndex(i) ? ShapeImages[i] : nullptr;
		if (!Icon || !ShapeButtons[i])
		{
			continue;
		}
		Icon->SetColorAndOpacity(ShapeButtons[i]->Shape == Active ? SelectedTint : UnselectedTint);
	}
}

void UFormationSelectorWidget::HandleShapeChosen(EGridShape Shape)
{
	if (AControllerBase* Controller = GetRTSController())
	{
		// Same entry point the C key uses: forces a slot rebuild and mirrors the shape to the
		// server for its off-navmesh re-solve.
		Controller->SetGridFormationShape(Shape);
		RefreshSelection();
	}
}
