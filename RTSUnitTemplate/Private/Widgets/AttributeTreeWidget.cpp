// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/AttributeTreeWidget.h"
#include "EngineUtils.h"
#include "Widgets/SAttributeTreeWidget.h"
#include "Characters/Unit/LevelUnit.h"
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Controller/PlayerController/CameraControllerBase.h"
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
		.OnGetHeaderText(FAttrTreeGetHeaderText::CreateUObject(this, &UAttributeTreeWidget::HandleGetHeaderText))
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

	// Ueber AktuellesZiel, damit der Baum auch dann gefuellt ist, wenn ihn niemand ueber
	// SetTargetUnit versorgt hat.
	const ALevelUnit* Ziel = AktuellesZiel();
	if (Ziel && Ziel->AttributeTreeDataTable)
	{
		const TArray<FName> RowNames = Ziel->AttributeTreeDataTable->GetRowNames();
		SlateNodes.Reserve(RowNames.Num());
		for (const FName& RowName : RowNames)
		{
			const FAttributeTreeNodeRow* Row = Ziel->AttributeTreeDataTable->FindRow<FAttributeTreeNodeRow>(RowName, TEXT("AttributeTreeUI"), /*bWarnIfMissing=*/false);
			if (!Row)
			{
				continue;
			}
			// KEIN Tagfilter mehr - der Baum zeigt ALLE Zeilen.
			//
			// Bis zum 11.09.2026 wurde hier nach DoesAttributeTreeNodeMatchUnit gefiltert. Damit sah
			// man immer nur den Teilbaum der gerade gewaehlten Einheit, also vier Aeste mit lauter
			// T1-Beschriftungen, und die uebrigen Stufen blieben unsichtbar. Gemeint ist aber EIN
			// Baum, der in der Mitte beginnt und fuenf Aeste hat: T0 Gebaeude, T1 bis T4 Einheiten.
			//
			// Der Tag am Knoten entscheidet weiterhin, WER die Aufwertung bekommt - das prueft
			// ALevelUnit::InvestInAttributeTreeNode fuer jede Einheit selbst. Er entscheidet nur
			// nicht mehr, was zu SEHEN ist.

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

	// Belegen, dass ALLE Zeilen gezeichnet werden und nicht nur der Teilbaum einer Stufe.
	// Ohne diese Zeile ist von aussen nicht zu unterscheiden, ob der Baum vollstaendig ist oder
	// ob der alte Tagfilter noch greift - genau das war der gemeldete Fehler.
	UE_LOG(LogTemp, Log, TEXT("[Attributbaum] Widget zeichnet %d Knoten aus '%s'."),
		SlateNodes.Num(),
		(Ziel && Ziel->AttributeTreeDataTable) ? *Ziel->AttributeTreeDataTable->GetName() : TEXT("-"));

	// Die Vertreter neu suchen lassen: die Zusammensetzung des Teams kann sich geaendert haben.
	VertreterProTag.Reset();

	MyTree->SetNodes(MoveTemp(SlateNodes));
}

ALevelUnit* UAttributeTreeWidget::AktuellesZiel() const
{
	if (IsValid(TargetUnit))
	{
		return TargetUnit;
	}

	if (ALevelUnit* Gemerkt = GefundenesZiel.Get())
	{
		return Gemerkt;
	}

	const UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return nullptr;
	}

	int32 MeinTeam = -1;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const ACameraControllerBase* Kamera = Cast<const ACameraControllerBase>(PC))
		{
			MeinTeam = Kamera->SelectableTeamId;
		}
	}

	ALevelUnit* Ersatz = nullptr;
	for (TActorIterator<ALevelUnit> It(Welt); It; ++It)
	{
		ALevelUnit* Einheit = *It;
		if (!IsValid(Einheit) || !Einheit->AttributeTreeDataTable)
		{
			continue;
		}
		if (MeinTeam >= 0 && Einheit->TeamId != MeinTeam)
		{
			continue;
		}

		// Bevorzugt eine mit freien Punkten - sonst zeigt der Baum die erstbeste mit 0 an,
		// obwohl anderswo im Team welche liegen.
		if (Einheit->AttributeTreePoints > 0)
		{
			GefundenesZiel = Einheit;
			return Einheit;
		}

		if (!Ersatz)
		{
			Ersatz = Einheit;
		}
	}

	GefundenesZiel = Ersatz;
	return Ersatz;
}

ALevelUnit* UAttributeTreeWidget::VertreterFuer(const FAttributeTreeNodeRow& Row) const
{
	ALevelUnit* Anzeige = AktuellesZiel();

	// Knoten ohne Tag (die Wurzel) gelten fuer jeden - da genuegt die Anzeigeeinheit.
	if (!Row.UnitTag.IsValid())
	{
		return Anzeige;
	}

	// Passt die Anzeigeeinheit selbst, ist sie der beste Vertreter: dann stimmen Anzeige und
	// Wirkung ueberein.
	if (Anzeige && Anzeige->DoesAttributeTreeNodeMatchUnit(Row))
	{
		return Anzeige;
	}

	// Zwischenspeicher: die Suche laeuft sonst je Knoten und Bild ueber alle Aktoren.
	if (const TWeakObjectPtr<ALevelUnit>* Gemerkt = VertreterProTag.Find(Row.UnitTag))
	{
		if (ALevelUnit* Lebt = Gemerkt->Get())
		{
			return Lebt;
		}
		VertreterProTag.Remove(Row.UnitTag);
	}

	const UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return nullptr;
	}

	int32 MeinTeam = -1;
	if (const ACameraControllerBase* Kamera = Cast<const ACameraControllerBase>(GetOwningPlayer()))
	{
		MeinTeam = Kamera->SelectableTeamId;
	}

	// Bevorzugt eine Einheit mit freien Punkten - sonst zeigt der Ast 0 an, obwohl anderswo im
	// Team noch investiert werden koennte.
	ALevelUnit* Ersatz = nullptr;
	for (TActorIterator<ALevelUnit> It(Welt); It; ++It)
	{
		ALevelUnit* Einheit = *It;
		if (!IsValid(Einheit) || !Einheit->AttributeTreeDataTable)
		{
			continue;
		}
		if (MeinTeam >= 0 && Einheit->TeamId != MeinTeam)
		{
			continue;
		}
		if (!Einheit->DoesAttributeTreeNodeMatchUnit(Row))
		{
			continue;
		}
		if (Einheit->AttributeTreePoints > 0)
		{
			VertreterProTag.Add(Row.UnitTag, Einheit);
			return Einheit;
		}
		if (!Ersatz)
		{
			Ersatz = Einheit;
		}
	}
	if (Ersatz)
	{
		VertreterProTag.Add(Row.UnitTag, Ersatz);
	}
	return Ersatz;
}

ALevelUnit* UAttributeTreeWidget::VertreterFuerKnoten(FName NodeId) const
{
	const ALevelUnit* Anzeige = AktuellesZiel();
	if (!Anzeige || !Anzeige->AttributeTreeDataTable)
	{
		return nullptr;
	}
	const FAttributeTreeNodeRow* Row = Anzeige->AttributeTreeDataTable->FindRow<FAttributeTreeNodeRow>(
		NodeId, TEXT("AttributeTreeUI"), /*bWarnIfMissing=*/false);
	return Row ? VertreterFuer(*Row) : nullptr;
}

int32 UAttributeTreeWidget::HandleGetNodePoints(FName NodeId) const
{
	// Am Vertreter des Astes ablesen, nicht an der Anzeigeeinheit - siehe VertreterFuer.
	const ALevelUnit* Vertreter = VertreterFuerKnoten(NodeId);
	return Vertreter ? Vertreter->GetAttributeTreeNodePoints(NodeId) : 0;
}

int32 UAttributeTreeWidget::HandleGetAvailablePoints() const
{
	const ALevelUnit* Ziel = AktuellesZiel();
	// Der EIGENE Vorrat des Baums. Vorher stand hier LevelData.TalentPoints - derselbe Topf, den
	// AutoLevelUp() selbsttaetig leerraeumt; deshalb zeigte der Baum staendig 0.
	return Ziel ? Ziel->AttributeTreePoints : 0;
}

FString UAttributeTreeWidget::HandleGetHeaderText() const
{
	const ALevelUnit* Ziel = AktuellesZiel();
	if (!Ziel)
	{
		// Say WHY it is empty. "No unit selected" left open whether nothing was clicked or whether
		// no unit carries the table at all.
		return FString(TEXT("No unit with an attribute tree - assign the table on the unit"));
	}

	// Der Baum wirkt auf die ganze Fraktion, also zeigt die Kopfzeile den Teamstand.
	//
	// Die Punkte liegen je Einheit (ALevelUnit::AttributeTreePoints), vergeben werden sie aber im
	// selben Takt an alle. Angezeigt wird deshalb das MAXIMUM - das ist, was der naechste Klick
	// mindestens bei einer Einheit auszugeben hat. Wer weniger hat, bleibt bei dieser Aufwertung
	// einfach zurueck; so steht es auch in Server_InvestAttributeTreeNode.
	int32 TeamEinheiten = 0;
	int32 MaxVerfuegbar = Ziel->AttributeTreePoints;
	int32 MaxVerbraucht = Ziel->UsedAttributeTreePoints;
	if (const UWorld* Welt = GetWorld())
	{
		for (TActorIterator<ALevelUnit> It(Welt); It; ++It)
		{
			const ALevelUnit* Andere = *It;
			if (IsValid(Andere) && Andere->TeamId == Ziel->TeamId && Andere->AttributeTreeDataTable)
			{
				++TeamEinheiten;
				MaxVerfuegbar = FMath::Max(MaxVerfuegbar, Andere->AttributeTreePoints);
				MaxVerbraucht = FMath::Max(MaxVerbraucht, Andere->UsedAttributeTreePoints);
			}
		}
	}

	FString Takt;
	if (const ACameraControllerBase* Kamera = Cast<ACameraControllerBase>(GetOwningPlayer()))
	{
		if (Kamera->TalentPointInterval > 0.f && Kamera->TalentPointsPerInterval > 0)
		{
			const float Sekunden = Kamera->TalentPointInterval;
			Takt = (Sekunden >= 60.f)
				? FString::Printf(TEXT("   |   +%d every %.0f min"),
					Kamera->TalentPointsPerInterval, Sekunden / 60.f)
				: FString::Printf(TEXT("   |   +%d every %.0f s"),
					Kamera->TalentPointsPerInterval, Sekunden);
		}
		else
		{
			// Say it outright - otherwise you wait for points that never come.
			Takt = FString(TEXT("   |   no periodic grant"));
		}
	}

	// "Attribute points", not "talent points": these are two separate pools since 10.09.2026, and
	// the identical wording was part of the confusion.
	return FString::Printf(
		TEXT("Attribute points: %d available, %d spent   |   Team %d: %d units%s"),
		MaxVerfuegbar, MaxVerbraucht, Ziel->TeamId, TeamEinheiten, *Takt);
}

bool UAttributeTreeWidget::HandleIsUnlocked(FName NodeId) const
{
	const ALevelUnit* Vertreter = VertreterFuerKnoten(NodeId);
	return Vertreter ? Vertreter->IsAttributeTreeNodeUnlocked(NodeId) : false;
}

void UAttributeTreeWidget::HandleInvest(FName NodeId)
{
	// Der Vertreter des Astes, nicht die Anzeigeeinheit: sonst scheitert die Vorpruefung unten an
	// jedem Knoten, der zu einer anderen Stufe gehoert als die gerade gewaehlte Einheit.
	ALevelUnit* Ziel = VertreterFuerKnoten(NodeId);
	if (!Ziel)
	{
		Ziel = AktuellesZiel();
	}
	if (!Ziel)
	{
		return;
	}

	// Client-side gate to avoid pointless RPCs; the server re-validates authoritatively.
	if (!Ziel->CanInvestInAttributeTreeNode(NodeId))
	{
		return;
	}

	// Route through the player-owned camera pawn so the Server RPC has a valid owner.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(PC->GetPawn()))
		{
			Camera->Server_InvestAttributeTreeNode(Ziel, NodeId);
			return;
		}
	}

	// Standalone / listen-server fallback: only run the authoritative invest directly when we ARE
	// the authority (never mutate replicated state on a non-authoritative client).
	UWorld* World = GetWorld();
	if (Ziel->HasAuthority() || (World && World->GetNetMode() == NM_Standalone))
	{
		Ziel->InvestInAttributeTreeNode(NodeId);
	}
}

void UAttributeTreeWidget::HandleReset()
{
	ALevelUnit* Ziel = AktuellesZiel();
	if (!Ziel)
	{
		return;
	}

	// Route through the player-owned camera pawn so the Server RPC has a valid owner.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (AExtendedCameraBase* Camera = Cast<AExtendedCameraBase>(PC->GetPawn()))
		{
			Camera->Server_ResetAttributeTree(Ziel);
			return;
		}
	}

	UWorld* World = GetWorld();
	if (Ziel->HasAuthority() || (World && World->GetNetMode() == NM_Standalone))
	{
		Ziel->ResetAttributeTree();
	}
}
