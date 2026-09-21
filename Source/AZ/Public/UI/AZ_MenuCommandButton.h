#pragma once
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "UI/AZ_MenuRoutesComponent.h"
#include "AZ_MenuCommandButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_MenuCommandClicked, EAZ_MenuCommand, Command);

/** Small command-carrying button, so dynamically authored menus need no unsafe index callback. */
UCLASS()
class AZ_API UAZ_MenuCommandButton : public UButton
{
	GENERATED_BODY()
public:
	void Configure(EAZ_MenuCommand InCommand) { Command = InCommand; OnClicked.AddUniqueDynamic(this, &ThisClass::Clicked); }
	UPROPERTY() FAZ_MenuCommandClicked OnCommand;
private:
	EAZ_MenuCommand Command = EAZ_MenuCommand::Cancel;
	UFUNCTION() void Clicked() { OnCommand.Broadcast(Command); }
};
