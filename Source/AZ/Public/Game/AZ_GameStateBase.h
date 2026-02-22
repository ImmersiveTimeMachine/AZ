// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "AZ_GameStateBase.generated.h"

UENUM(BlueprintType)
enum class EDifficultyLevel : uint8
{
	Beginner UMETA(DisplayName = "Beginner"),
	Normal   UMETA(DisplayName = "Normal"),
	Hard     UMETA(DisplayName = "Hard"),
	Expert   UMETA(DisplayName = "Expert")
};

UCLASS()
class AZ_API AAZ_GameStateBase : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	
	AAZ_GameStateBase();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Difficulty")
	EDifficultyLevel DifficultyLevel;

	UFUNCTION(BlueprintCallable, Category = "Difficulty")
	EDifficultyLevel GetDifficultyLevel() const { return DifficultyLevel; }
    
	UFUNCTION(BlueprintCallable, Category = "Difficulty")
	void SetDifficultyLevel(EDifficultyLevel NewDifficultyLevel);

protected:

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	// Optional: Add an OnRep function if you need to do something on clients when the difficulty changes
	UFUNCTION()
	virtual void OnRep_DifficultyLevel(EDifficultyLevel OldDifficultyLevel);
};
