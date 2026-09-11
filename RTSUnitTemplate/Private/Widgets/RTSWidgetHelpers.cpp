// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Widgets/RTSWidgetHelpers.h"

#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ComboBoxString.h"
#include "Engine/DataTable.h"

int32 URTSWidgetHelpers::SetComboBoxOptions(UComboBoxString* Box,
                                            const TArray<FString>& Options,
                                            bool bSelectFirst)
{
	if (!Box)
	{
		return 0;
	}

	Box->ClearOptions();
	for (const FString& O : Options)
	{
		Box->AddOption(O);
	}
	Box->RefreshOptions();

	if (bSelectFirst && Options.Num() > 0)
	{
		Box->SetSelectedOption(Options[0]);
	}

	UE_LOG(LogTemp, Log, TEXT("[Kartenfilter] %d Eintraege gesetzt."), Options.Num());
	return Options.Num();
}

int32 URTSWidgetHelpers::HighlightTabs(UUserWidget* Owner, const FString& Prefix,
                                       const FString& ActiveSuffix,
                                       FLinearColor Active, FLinearColor Inactive)
{
	if (!Owner || !Owner->WidgetTree || Prefix.IsEmpty())
	{
		return 0;
	}

	const FString AktiverName = Prefix + ActiveSuffix;
	int32 Gefunden = 0;

	Owner->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (!W)
		{
			return;
		}
		const FString Name = W->GetName();
		// Nur die Knoepfe selbst, nicht deren Beschriftungen: die heissen Prefix+X+"Label".
		if (!Name.StartsWith(Prefix) || Name.EndsWith(TEXT("Label")))
		{
			return;
		}
		if (!W->IsA<UButton>())
		{
			return;
		}
		++Gefunden;
		Cast<UButton>(W)->SetColorAndOpacity(Name.Equals(AktiverName) ? Active : Inactive);
	});

	UE_LOG(LogTemp, Log, TEXT("[Reiter] '%s' aktiv, %d Reiter eingefaerbt."),
		*AktiverName, Gefunden);
	return Gefunden;
}

FString URTSWidgetHelpers::TeamLabel(int32 TeamId)
{
	// Die beiden spielbaren Fraktionen haben Namen, alles darueber bleibt neutral.
	switch (TeamId)
	{
	case 1:  return TEXT("Xeno (Team 1)");
	case 2:  return TEXT("Singularians (Team 2)");
	default: return FString::Printf(TEXT("Team %d"), TeamId);
	}
}

int32 URTSWidgetHelpers::TeamIdFromLabel(const FString& Label)
{
	// Die Id steht am Ende in Klammern ("... (Team 2)") oder der Text ist schon eine Zahl.
	int32 Start = INDEX_NONE;
	if (Label.FindLastChar(TEXT(' '), Start) && Start + 1 < Label.Len())
	{
		const FString Rest = Label.Mid(Start + 1).Replace(TEXT(")"), TEXT(""));
		if (Rest.IsNumeric())
		{
			return FCString::Atoi(*Rest);
		}
	}
	return Label.IsNumeric() ? FCString::Atoi(*Label) : -1;
}

int32 URTSWidgetHelpers::SetTeamComboOptions(UComboBoxString* Box,
                                             const TArray<int32>& TeamIds, bool bSelectFirst)
{
	if (!Box)
	{
		return 0;
	}

	TArray<int32> Ids = TeamIds;
	if (Ids.Num() == 0)
	{
		// Keine Vorgabe an der Karte: alle vier zulassen, wie bisher.
		Ids = { 1, 2, 3, 4 };
	}

	TArray<FString> Namen;
	Namen.Reserve(Ids.Num());
	for (int32 Id : Ids)
	{
		Namen.Add(TeamLabel(Id));
	}

	UE_LOG(LogTemp, Log, TEXT("[Teamauswahl] %d Teams zur Wahl: %s"),
		Namen.Num(), *FString::Join(Namen, TEXT(", ")));
	return SetComboBoxOptions(Box, Namen, bSelectFirst);
}

namespace
{
	/** Sucht eine Eigenschaft ohne Ruecksicht auf Gross-/Kleinschreibung. */
	FProperty* FindePropertyLose(const UStruct* Struktur, const TCHAR* Name)
	{
		if (!Struktur)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Struktur); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Das MapsDataAsset einer Tabellenzeile: die erste Objektreferenz im Zeilenstruct. */
	UObject* HoleAssetAusZeile(const UScriptStruct* RowStruct, const uint8* RowData)
	{
		if (!RowStruct || !RowData)
		{
			return nullptr;
		}
		for (TFieldIterator<FObjectPropertyBase> It(RowStruct); It; ++It)
		{
			if (UObject* Wert = It->GetObjectPropertyValue_InContainer(RowData))
			{
				return Wert;
			}
		}
		return nullptr;
	}
}

int32 URTSWidgetHelpers::ApplyAllowedTeamsFromTable(UComboBoxString* TeamBox,
                                                    UDataTable* MapTable,
                                                    const FString& SelectedMapName,
                                                    bool bSelectFirst)
{
	if (!TeamBox || !MapTable || SelectedMapName.IsEmpty())
	{
		return 0;
	}

	const UScriptStruct* RowStruct = MapTable->GetRowStruct();
	for (const TPair<FName, uint8*>& Zeile : MapTable->GetRowMap())
	{
		UObject* Asset = HoleAssetAusZeile(RowStruct, Zeile.Value);
		if (!Asset)
		{
			continue;
		}

		// Name der Karte vergleichen (das Feld ist ein FText).
		FString Name;
		if (const FTextProperty* NameProp =
				CastField<FTextProperty>(FindePropertyLose(Asset->GetClass(), TEXT("mapName"))))
		{
			Name = NameProp->GetPropertyValue_InContainer(Asset).ToString();
		}
		if (!Name.Equals(SelectedMapName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TArray<int32> Teams;
		if (const FArrayProperty* ArrProp = CastField<FArrayProperty>(
				FindePropertyLose(Asset->GetClass(), TEXT("allowedTeamIds"))))
		{
			if (CastField<FIntProperty>(ArrProp->Inner))
			{
				FScriptArrayHelper Helper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(Asset));
				for (int32 i = 0; i < Helper.Num(); ++i)
				{
					Teams.Add(*reinterpret_cast<int32*>(Helper.GetRawPtr(i)));
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[Teamauswahl] Karte '%s' erlaubt %d Team(s)."),
			*SelectedMapName, Teams.Num());
		return SetTeamComboOptions(TeamBox, Teams, bSelectFirst);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Teamauswahl] Karte '%s' nicht in der Tabelle - Auswahl unveraendert."),
		*SelectedMapName);
	return 0;
}
