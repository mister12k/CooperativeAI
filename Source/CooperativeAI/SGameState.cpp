// Fill out your copyright notice in the Description page of Project Settings.

#include "SGameState.h"


void ASGameState::SetWaveState(EWaveState NewState)
{
		EWaveState OldState = WaveState;

		WaveState = NewState;
}
