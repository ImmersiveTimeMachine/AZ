// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AZ_Inv_CharacterDisplay.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_CharacterDisplay : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	bool bIsDragging{false};
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;

	FVector2D CurrentPosition;
	FVector2D LastPosition;
};
