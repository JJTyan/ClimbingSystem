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

#pragma region Overrides

public:
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;

protected:

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called after MovementMode has changed. Base implementation does special handling for starting certain modes, then notifies the CharacterOwner. */
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** @note Movement update functions should only be called through StartNewPhysics()*/
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

#pragma endregion

#pragma region ClimbTraces

	private:

	TArray <FHitResult> DoCapsuleTraceMultiByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug, bool bShowOneFrame);

	FHitResult DoLineTraceSingleByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug, bool bShowOneFrame);

#pragma endregion

#pragma region ClimbBPVariables
	//Types of surfaces that can be used for climbing
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	TArray<TEnumAsByte<EObjectTypeQuery> > ClimbSurfaceTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceRadius {50.f};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float ClimbCapsuleTraceHalfHeight {72.5f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	float ClimbingFriction {0.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float MaxBreakClimbDeceleration {400.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float MaxClimbAcceleration {300.f};
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float MaxClimbSpeed {100.f};

#pragma endregion

#pragma region ClimbCoreVariables

	TArray <FHitResult> ClimbTraceResults;

	FVector CurrentClimableSurfLocation;
	FVector CurrentClimableSurfNormal;

#pragma region ClimbCore

	//returns true if traced is at least one valid climable surface while filling ClimbTraceResults array
	bool TraceClimbSurfaces(bool bShowDebug = false, bool bShowOneFrame = true);

	FHitResult TraceFromEyes(float TraceDistance, float TraceStartOffset = 0, bool bShowDebug = false, bool bShowOneFrame = true);

	bool CanStartClimbing();

	void StartClimbing();
	void EndClimbing();

	void GetClimbSurfaceInfo();

	FQuat GetClimbRotation(float DeltaTime) const;

	void SnapToClimable(float DeltaTime);

#pragma endregion

public:
	void ToggleClimbing(bool bEnable);

	bool IsClimbing() const;
	
	
};
