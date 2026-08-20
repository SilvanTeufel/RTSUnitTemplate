// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIExpansionSite.generated.h"

class UBillboardComponent;

/**
 * Handgesetzter Platz, an dem die KI eine Basis errichten darf.
 *
 * Die Expansionslogik in AExtendedControllerBase sucht diese Actors ueber den Tag
 * "AIExpansionSite". Frueher genuegte der Tag allein, weshalb jedes Team jeden Marker
 * benutzen durfte - die Xeno expandierten quer ueber die Karte. Ueber AllowedTeamIds
 * laesst sich jetzt pro Platz festlegen, wer dort bauen darf.
 *
 * Der Tag wird im Konstruktor gesetzt, ein von Hand getaggter Actor funktioniert aber
 * weiterhin: der zaehlt dann als "fuer alle Teams offen".
 */
UCLASS()
class RTSUNITTEMPLATE_API AAIExpansionSite : public AActor
{
	GENERATED_BODY()

public:
	AAIExpansionSite();

	/** Der Tag, ueber den die Expansionslogik diese Actors findet. */
	static const FName ExpansionSiteTag;

	/**
	 * Welche Teams hier eine Basis errichten duerfen.
	 *
	 * Leere Liste = jedes Team darf. Sonst nur die aufgefuehrten TeamIds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<int32> AllowedTeamIds;

	/** Leere AllowedTeamIds erlauben jedes Team; sonst muss die TeamId enthalten sein. */
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	bool IsTeamAllowed(int32 TeamId) const;

	/**
	 * Team-Pruefung fuer einen beliebigen Marker-Actor.
	 *
	 * Ist der Actor kein AAIExpansionSite (also nur von Hand getaggt), gilt er als fuer
	 * alle Teams offen - so bleiben bestehende Level unveraendert.
	 */
	static bool IsTeamAllowedForActor(const AActor* Marker, int32 TeamId);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	/** Nur zum Anfassen im Editor - im Spiel unsichtbar. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = RTSUnitTemplate)
	TObjectPtr<UBillboardComponent> Billboard;
#endif
};
