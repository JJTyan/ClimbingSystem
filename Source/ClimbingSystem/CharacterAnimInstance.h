// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CharacterAnimInstance.generated.h"

class AClimbingSystemCharacter;
class UCLSMovementComponent;

UCLASS()
class CLIMBINGSYSTEM_API UCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	AClimbingSystemCharacter* PlayerCharacter;
	UCLSMovementComponent* PlayerMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float GroundSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	float AirSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	bool bShouldMove;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	bool bIsFalling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	bool bIsClimbing;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
	FVector ClimbVelocity;

private:
	void GetGroundSpeed();
	void GetAirSpeedSpeed();
	void GetSholdMove();
	void GetIsFalling();
	void GetIsClimbing();
	void GetClimbVelocity();
};
