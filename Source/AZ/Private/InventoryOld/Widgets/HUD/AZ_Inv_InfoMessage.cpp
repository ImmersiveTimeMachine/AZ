// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Widgets/HUD/AZ_Inv_InfoMessage.h"

void UAZ_Inv_InfoMessage::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Text_Message->SetText(FText::GetEmpty());
	MessageHide();
}

void UAZ_Inv_InfoMessage::SetMessage(const FText& Message)
{
	Text_Message->SetText(Message);

	if (!bIsMessageActive)
	{
		MessageShow();
	}
	bIsMessageActive = true;

	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
	{
		MessageHide();
		bIsMessageActive = false;
	}, MessageLifeTime, false);
}
