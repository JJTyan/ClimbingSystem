// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CLSMovementComponent.generated.h"

UENUM(BlueprintType)
enum class ECustomMovementMode : uint8
{
	MOVE_Climb UMETA(DisplayName = "Climb Node")
};

/**
 * 
 */
UCLASS()
class CLIMBINGSYSTEM_API UCLSMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

#pragma region ClimbTraces

	TArray <FHitResult> DoCapsuleTraceMultiByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug = false);

	FHitResult DoLineTraceSingleByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug = false);

#pragma endregion

#pragma region ClimbBPVariables
	//Types of surfaces that can be used for climbing
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	TArray<TEnumAsByte<EObjectTypeQuery> > ClimbSurfaceTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceRadius {50.f};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceHalfHeight {72.5f};

#pragma endregion

#pragma region ClimbCoreVariables

	TArray <FHitResult> ClimbTraceResults;

#pragma region ClimbCore

	//returns true if traced is at least one valid climable surface while filling ClimbTraceResults array
	bool TraceClimbSurfaces();

	FHitResult TraceFromEyes(float TraceDistance, float TraceStartOffset = 0);

	bool CanStartClimbing();

#pragma endregion

public:
	void ToggleClimbing(bool bEnable);

	bool IsClimbing() const;
	
	
};
