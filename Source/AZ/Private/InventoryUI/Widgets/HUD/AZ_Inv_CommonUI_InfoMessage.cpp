// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InfoMessage.h"

#include "Components/TextBlock.h"

void UAZ_Inv_CommonUI_InfoMessage::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Text_Message->SetText(FText::GetEmpty());
	MessageHide();
}

void UAZ_Inv_CommonUI_InfoMessage::SetMessage(const FText& Message)
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
