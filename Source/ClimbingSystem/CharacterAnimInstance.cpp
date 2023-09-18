// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterAnimInstance.h"
#include "ClimbingSystemCharacter.h"
#include "CLSMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	PlayerCharacter = Cast<AClimbingSystemCharacter>(TryGetPawnOwner());
	if (IsValid(PlayerCharacter))
	{
		PlayerMovementComponent = PlayerCharacter->GetCharacterMovement<UCLSMovementComponent>();
	}
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!(IsValid(PlayerCharacter) && IsValid(PlayerMovementComponent)))
	{
		PlayerCharacter = Cast<AClimbingSystemCharacter>(TryGetPawnOwner());
		if (IsValid(PlayerCharacter))
		{
			PlayerMovementComponent = PlayerCharacter->GetCharacterMovement<UCLSMovementComponent>();
		}

		return;
	}
	GetGroundSpeed();
	GetAirSpeedSpeed();
	GetIsFalling();
	GetSholdMove();
	GetIsClimbing();
	GetClimbVelocity();
}

void UCharacterAnimInstance::GetGroundSpeed()
{
	GroundSpeed = UKismetMathLibrary::VSizeXY(PlayerCharacter->GetVelocity());
}

void UCharacterAnimInstance::GetAirSpeedSpeed()
{
	AirSpeed = PlayerCharacter->GetVelocity().Z;
}

void UCharacterAnimInstance::GetSholdMove()
{
	bShouldMove = PlayerMovementComponent->GetCurrentAcceleration().Size() > 0.f && GroundSpeed > 5.f && !bIsFalling;
}

void UCharacterAnimInstance::GetIsFalling()
{
	bIsFalling = PlayerMovementComponent->IsFalling();
}

void UCharacterAnimInstance::GetIsClimbing()
{
	bIsClimbing = PlayerMovementComponent->IsClimbing();
}

void UCharacterAnimInstance::GetClimbVelocity()
{
	ClimbVelocity = PlayerMovementComponent->GetUnrotatedClimbVelocity();
}

