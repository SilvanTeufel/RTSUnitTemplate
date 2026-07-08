// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/AttributeTreeWidget.h"
#include "Widgets/SAttributeTreeWidget.h"
#include "Characters/Unit/LevelUnit.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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
	Super::ReleaseSlateResources(bReleaseChildren);
	MyTree.Reset();
}

void UAttributeTreeWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	RefreshNodes();
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
