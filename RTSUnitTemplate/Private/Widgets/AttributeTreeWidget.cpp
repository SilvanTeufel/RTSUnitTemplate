// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/AttributeTreeWidget.h"
#include "Widgets/SAttributeTreeWidget.h"
#include "Characters/Unit/LevelUnit.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Brushes/SlateColorBrush.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

// A Slate brush material MUST have Material Domain = "User Interface". This is enforced only at shader
// permutation level (the Slate material shaders compile solely for MD_UI) and FSlateBrush's own ensure
// checks the CLASS only - so a Surface-domain material sails through, finds no shader, and draws
// NOTHING: no crash, no checkerboard, no log, indistinguishable from "no material set". That is
// undiagnosable in the Details panel, hence this guard.
// IsUIMaterial() is virtual on UMaterialInterface and UMaterialInstance forwards it to Parent, so this
// is correct for UMaterial, material instances and MIDs alike.
static void WarnIfNotUIMaterial(const FSlateBrush& Brush, const TCHAR* PropName, const UObject* Owner)
{
	if (UMaterialInterface* Mat = Cast<UMaterialInterface>(Brush.GetResourceObject()))
	{
		if (!Mat->IsUIMaterial())
		{
			UE_LOG(LogTemp, Warning, TEXT("%s (%s): Material '%s' has Material Domain != 'User Interface'. ")
				TEXT("Slate compiles no shader for it and will draw NOTHING (silently). Set Material Domain = User Interface."),
				*GetNameSafe(Owner), PropName, *Mat->GetName());
		}
	}
}

UAttributeTreeWidget::UAttributeTreeWidget()
{
	// Byte-identical to the brush the SWidget used to hard-code: DrawAs=Image, ImageType=NoImage,
	// Margin=0, TintColor=White. White * BackgroundColor == BackgroundColor, so existing content
	// renders exactly as before.
	BackgroundBrush = FSlateColorBrush(FLinearColor::White);
	// Non-zero base size so a later flip to Draw As = Box 9-slices sensibly. Inert for the default: the
	// 9-quad path is gated on DrawType != Image && Margin != 0, and tiling is NoTile.
	BackgroundBrush.ImageSize = FVector2f(64.f, 64.f);

	// No border until opted in -> preserves today's look exactly. Must be set explicitly: FSlateBrush's
	// default ctor is DrawAs=Image / ImageSize 32x32, which would paint an opaque white box over every
	// existing tree on first run.
	BorderBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	BorderBrush.TintColor = FSlateColor(FLinearColor::White);
	// Ready-to-use 9-slice: for a MATERIAL, ImageSize is the ONLY size source (no texture to measure)
	// and corners = ImageSize * Margin. At the engine's 32x32 default a large frame would get 8px
	// corners and read as "the border is broken"; at ImageSize 0 they collapse entirely.
	BorderBrush.ImageSize = FVector2f(256.f, 256.f);
	BorderBrush.Margin = FMargin(0.25f);
}

TSharedRef<SWidget> UAttributeTreeWidget::RebuildWidget()
{
	MyTree = SNew(SAttributeTreeWidget)
		.RingSpacing(RingSpacing)
		.NodeRadius(NodeRadius)
		.TooltipDelaySeconds(TooltipDelaySeconds)
		.LockedColor(LockedColor)
		.AvailableColor(AvailableColor)
		.PartialColor(PartialColor)
		.FullColor(FullColor)
		.LinkColor(LinkColor)
		.RingColor(RingColor)
		.BackgroundColor(BackgroundColor)
		.bFillViewport(bFillViewport)
		// Address of this UObject's own UPROPERTY - the UBorder pattern verbatim. Never a temporary:
		// the UPROPERTY is the GC anchor for whatever material/texture the brush holds.
		.BackgroundBrush(&BackgroundBrush)
		.BorderBrush(&BorderBrush)
		.BorderPadding(BorderPadding)
		.NodeFontSize(NodeFontSize)
		.CountFontSize(CountFontSize)
		.TooltipTitleFontSize(TooltipTitleFontSize)
		.TooltipBodyFontSize(TooltipBodyFontSize)
		.TooltipMaxWidth(TooltipMaxWidth)
		.TooltipPadding(TooltipPadding)
		.TooltipIconSize(TooltipIconSize)
		.TooltipIconGap(TooltipIconGap)
		.OnGetNodePoints(FAttrTreeGetNodePoints::CreateUObject(this, &UAttributeTreeWidget::HandleGetNodePoints))
		.OnGetAvailablePoints(FAttrTreeGetAvailablePoints::CreateUObject(this, &UAttributeTreeWidget::HandleGetAvailablePoints))
		.OnIsUnlocked(FAttrTreeIsUnlocked::CreateUObject(this, &UAttributeTreeWidget::HandleIsUnlocked))
		.OnInvest(FAttrTreeOnInvest::CreateUObject(this, &UAttributeTreeWidget::HandleInvest))
		.OnReset(FAttrTreeOnReset::CreateUObject(this, &UAttributeTreeWidget::HandleReset));

	RefreshNodes();
	return MyTree.ToSharedRef();
}

void UAttributeTreeWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	// Null the borrowed brush pointers BEFORE dropping our reference. Reached from UVisual::BeginDestroy,
	// i.e. while this UObject's brush memory is still fully valid. If anything else still holds a shared
	// ref to the SWidget it now paints its own fallback instead of reading freed UObject memory.
	if (MyTree.IsValid())
	{
		MyTree->SetPanelStyle(nullptr, nullptr, BackgroundColor, BorderPadding);
	}
	Super::ReleaseSlateResources(bReleaseChildren);
	MyTree.Reset();
}

void UAttributeTreeWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	// Re-push panel style so Details-panel edits live-update: UWidget::PostEditChangeProperty calls
	// SynchronizeProperties and never RebuildWidget, so construction-only SLATE_ARGUMENTs never refresh.
	// SCOPE, DELIBERATE: only the background/border style is pushed. The other style values (RingSpacing,
	// node colours, fonts, tooltip metrics) stay construction-only, so the Details panel IS inconsistent
	// by design. Extending it means one SWidget setter each, plus Invalidate(Layout) rather than Paint
	// for RingSpacing/NodeRadius - they feed ComputeDesiredSize and the brushes/fonts cached in Construct.
	if (MyTree.IsValid())
	{
		WarnIfNotUIMaterial(BackgroundBrush, TEXT("BackgroundBrush"), this);
		WarnIfNotUIMaterial(BorderBrush, TEXT("BorderBrush"), this);
		MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
	}
	RefreshNodes();
}

#if WITH_EDITOR
void UAttributeTreeWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Super drives SynchronizeProperties -> live update + the Material-Domain warning.
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UAttributeTreeWidget::SetBackgroundBrush(const FSlateBrush& InBrush)
{
	BackgroundBrush = InBrush;
	WarnIfNotUIMaterial(BackgroundBrush, TEXT("BackgroundBrush"), this);
	if (MyTree.IsValid())
	{
		MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
	}
}

void UAttributeTreeWidget::SetBackgroundBrushFromMaterial(UMaterialInterface* Material)
{
	BackgroundBrush.SetResourceObject(Material);
	WarnIfNotUIMaterial(BackgroundBrush, TEXT("BackgroundBrush"), this);
	if (MyTree.IsValid())
	{
		MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
	}
}

void UAttributeTreeWidget::SetBorderBrush(const FSlateBrush& InBrush)
{
	BorderBrush = InBrush;
	WarnIfNotUIMaterial(BorderBrush, TEXT("BorderBrush"), this);
	if (MyTree.IsValid())
	{
		MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
	}
}

void UAttributeTreeWidget::SetBorderBrushFromMaterial(UMaterialInterface* Material)
{
	BorderBrush.SetResourceObject(Material);
	WarnIfNotUIMaterial(BorderBrush, TEXT("BorderBrush"), this);
	if (MyTree.IsValid())
	{
		MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
	}
}

void UAttributeTreeWidget::SetBackgroundColor(FLinearColor InColor)
{
	BackgroundColor = InColor;
	if (MyTree.IsValid())
	{
		MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
	}
}

void UAttributeTreeWidget::SetBorderPadding(FMargin InPadding)
{
	BorderPadding = InPadding;
	if (MyTree.IsValid())
	{
		MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
	}
}

UMaterialInstanceDynamic* UAttributeTreeWidget::GetBackgroundDynamicMaterial()
{
	UMaterialInterface* Material = Cast<UMaterialInterface>(BackgroundBrush.GetResourceObject());
	if (!Material)
	{
		return nullptr;
	}
	// Cast-to-MID first and return it if we already wrapped: without this guard every call re-wraps and
	// leaks a MID chain. (UBorder::GetDynamicMaterial does exactly this.)
	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
	if (!DynamicMaterial)
	{
		// `this` (the UWidget) is the Outer - the Outer IS the keep-alive for the MID.
		DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
		BackgroundBrush.SetResourceObject(DynamicMaterial);
		if (MyTree.IsValid())
		{
			MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
		}
	}
	return DynamicMaterial;
}

UMaterialInstanceDynamic* UAttributeTreeWidget::GetBorderDynamicMaterial()
{
	UMaterialInterface* Material = Cast<UMaterialInterface>(BorderBrush.GetResourceObject());
	if (!Material)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
	if (!DynamicMaterial)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(Material, this);
		BorderBrush.SetResourceObject(DynamicMaterial);
		if (MyTree.IsValid())
		{
			MyTree->SetPanelStyle(&BackgroundBrush, &BorderBrush, BackgroundColor, BorderPadding);
		}
	}
	return DynamicMaterial;
}

void UAttributeTreeWidget::SetTargetUnit(ALevelUnit* InUnit)
{
	TargetUnit = InUnit;
	RefreshNodes();
}

void UAttributeTreeWidget::RefreshNodes()
{
	if (!MyTree.IsValid())
	{
		return;
	}

	TArray<FAttributeTreeSlateNode> SlateNodes;

	if (TargetUnit && TargetUnit->AttributeTreeDataTable)
	{
		const TArray<FName> RowNames = TargetUnit->AttributeTreeDataTable->GetRowNames();
		SlateNodes.Reserve(RowNames.Num());
		for (const FName& RowName : RowNames)
		{
			const FAttributeTreeNodeRow* Row = TargetUnit->AttributeTreeDataTable->FindRow<FAttributeTreeNodeRow>(RowName, TEXT("AttributeTreeUI"), /*bWarnIfMissing=*/false);
			if (!Row)
			{
				continue;
			}
			// Only show nodes that apply to this unit type (untagged, or matching TalentTag / UnitTags),
			// so one tree can hold unit-specific talents and each unit sees its own subtree.
			if (!TargetUnit->DoesAttributeTreeNodeMatchUnit(*Row))
			{
				continue;
			}

			FAttributeTreeSlateNode N;
			N.Id           = RowName;
			N.PrevId       = Row->PrevId;
			N.Ring         = Row->Ring;
			N.Angle        = Row->Angle;
			N.MaxPoints    = FMath::Max(1, Row->MaxPoints);
			N.DisplayName  = Row->DisplayName;
			N.ToolTipTitle = Row->ToolTipTitle;
			N.ToolTipText  = Row->ToolTipText;
			N.BaseColor    = Row->NodeColor;

			if (Row->Icon)
			{
				N.IconBrush.SetResourceObject(Row->Icon);
				N.IconBrush.ImageSize = FVector2D(NodeRadius * 1.6f, NodeRadius * 1.6f);
				N.IconBrush.DrawAs = ESlateBrushDrawType::Image;
				N.bHasIcon = true;
			}

			SlateNodes.Add(MoveTemp(N));
		}
	}

	MyTree->SetNodes(MoveTemp(SlateNodes));
}

int32 UAttributeTreeWidget::HandleGetNodePoints(FName NodeId) const
{
	return TargetUnit ? TargetUnit->GetAttributeTreeNodePoints(NodeId) : 0;
}

int32 UAttributeTreeWidget::HandleGetAvailablePoints() const
{
	return TargetUnit ? TargetUnit->LevelData.TalentPoints : 0;
}

bool UAttributeTreeWidget::HandleIsUnlocked(FName NodeId) const
{
	return TargetUnit ? TargetUnit->IsAttributeTreeNodeUnlocked(NodeId) : false;
}

void UAttributeTreeWidget::HandleInvest(FName NodeId)
{
	if (!TargetUnit)
	{
		return;
	}

	// Client-side gate to avoid pointless RPCs; the server re-validates authoritatively.
	if (!TargetUnit->CanInvestInAttributeTreeNode(NodeId))
	{
		return;
	}

	// Route through the player-owned camera pawn so the Server RPC has a valid owner.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(PC->GetPawn()))
		{
			Camera->Server_InvestAttributeTreeNode(TargetUnit, NodeId);
			return;
		}
	}

	// Standalone / listen-server fallback: only run the authoritative invest directly when we ARE
	// the authority (never mutate replicated state on a non-authoritative client).
	UWorld* World = GetWorld();
	if (TargetUnit->HasAuthority() || (World && World->GetNetMode() == NM_Standalone))
	{
		TargetUnit->InvestInAttributeTreeNode(NodeId);
	}
}

void UAttributeTreeWidget::HandleReset()
{
	if (!TargetUnit)
	{
		return;
	}

	// Route through the player-owned camera pawn so the Server RPC has a valid owner.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(PC->GetPawn()))
		{
			Camera->Server_ResetAttributeTree(TargetUnit);
			return;
		}
	}

	UWorld* World = GetWorld();
	if (TargetUnit->HasAuthority() || (World && World->GetNetMode() == NM_Standalone))
	{
		TargetUnit->ResetAttributeTree();
	}
}
