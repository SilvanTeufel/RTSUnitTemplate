// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/RTSUnitTemplateSettings.h"
#include "Core/UnitData.h"
#include "Core/WorkerData.h"

class AWinLoseConfigActor;

/**
 * Turns the win conditions of a AWinLoseConfigActor into whole, spoken sentences for the narrator
 * (URTSUnitTemplateSettings, Narration). Pure text - no widgets, no world access.
 */
namespace WinConditionNarration
{
	/**
	 * The sentences for the current win condition: an opening (game start or "next stage"), then what to do.
	 * ResourceNames: display names of the resources (e.g. those of the goal widget); missing ones use the enum.
	 * Empty if there is nothing to explain (no win condition).
	 */
	RTSUNITTEMPLATE_API TArray<FString> BuildSentences(const AWinLoseConfigActor* Config, bool bGameStart,
		EWinConditionNarrationStyle Style, const TMap<EResourceType, FText>* ResourceNames);

	/** Puts sentences together into messages of at most MaxChars characters (a sentence is never split). */
	RTSUNITTEMPLATE_API TArray<FString> ToMessages(const TArray<FString>& Sentences, int32 MaxChars);
}
