// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_Inv_WidgetUtils.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_WidgetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	static FVector2D GetWidgetPosition(UWidget* Widget);

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	static FVector2D GetWidgetSize(UWidget* Widget);
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	static bool IsWithinBounds(const FVector2D& BoundaryPos, const FVector2D& WidgetSize, const FVector2D& MousePos);
	
	static int32 GetIndexFromPosition(const FIntPoint& Position, const int32  Columns);
	static FIntPoint GetPositionFromIndex(const int32 Index, const int32 Columns);
	static FVector2D GetClampedWidgetPosition(const FVector2D& Boundary, const FVector2D& WidgetSize, const FVector2D& MousePos);
	
};
