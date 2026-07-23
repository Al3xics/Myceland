// Copyright Myceland Team, All Rights Reserved.

#include "Core/ML_GameInstance.h"
#include "Save System/ML_SaveSubsystem.h"

void UML_GameInstance::Init()
{
	Super::Init();

	// GameInstance subsystems (incl. UML_SaveSubsystem, which loads/creates the save in its
	// own Initialize) are brought up inside Super::Init(), so the save is ready to read here.
	if (const UML_SaveSubsystem* SaveSys = GetSaveSubsystem())
	{
		ProgressionState = SaveSys->GetProgressionState();
	}
}

void UML_GameInstance::SetProgressionState(EML_ProgressionState NewState)
{
	if (ProgressionState == NewState) return;

	ProgressionState = NewState;

	if (UML_SaveSubsystem* SaveSys = GetSaveSubsystem())
	{
		SaveSys->SetProgressionState(NewState);
	}
}

UML_SaveSubsystem* UML_GameInstance::GetSaveSubsystem() const
{
	return GetSubsystem<UML_SaveSubsystem>();
}
