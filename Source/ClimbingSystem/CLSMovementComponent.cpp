// Fill out your copyright notice in the Description page of Project Settings.


#include "CLSMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Character.h"
#include "CLDebugHelpers.h" 

#pragma region ClimbTraces

void UCLSMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType, ThisTickFunction);
}

float UCLSMovementComponent::GetMaxSpeed() const
{
	if (IsClimbing())
	{
		return MaxClimbSpeed;
	}
	else
	{
		return Super::GetMaxSpeed();
	}
}

float UCLSMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())
	{
		return MaxClimbAcceleration;
	}
	else
	{
		return Super::GetMaxAcceleration();
	}
}

void UCLSMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
	}
	else if (!IsClimbing() && PreviousMovementMode == MOVE_Custom && PreviousCustomMode == (uint8)ECustomMovementMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		StopMovementImmediately();
	}
}

void UCLSMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime,Iterations);

	if (IsClimbing())
	{
		if (deltaTime < MIN_TICK_TIME)
		{
			return;
		}

		//Process all the climable surfaces info
		TraceClimbSurfaces(true);
		GetClimbSurfaceInfo();

		//TODO Check if we should stop climbing
		
		RestorePreAdditiveRootMotionVelocity();

		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			//TODO define max speed and acceleration

			CalcVelocity(deltaTime, ClimbingFriction, true, MaxBreakClimbDeceleration);
		}

		ApplyRootMotionToVelocity(deltaTime);

		FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FVector Adjusted = Velocity * deltaTime;
		FHitResult Hit(1.f);

		SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);

		if (Hit.Time < 1.f)
		{
			//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}

		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
		}

		SnapToClimable(deltaTime);
	}
}

TArray <FHitResult> UCLSMovementComponent::DoCapsuleTraceMultiByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug, bool bShowOneFrame)
{
	TArray<FHitResult> CapsuleHitResults;

	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::Type::None;
	if (bShowDebug)
	{
		drawDebugType = bShowOneFrame ? EDrawDebugTrace::Type::ForOneFrame : EDrawDebugTrace::Type::ForDuration;
	}

	UKismetSystemLibrary::CapsuleTraceMultiForObjects(this, TraceStart, TraceEnd, ClimbCapsuleTraceRadius, ClimbCapsuleTraceHalfHeight, ClimbSurfaceTypes,
		false, TArray<AActor*>(), drawDebugType, CapsuleHitResults, true);

	return CapsuleHitResults;
}

FHitResult UCLSMovementComponent::DoLineTraceSingleByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug, bool bShowOneFrame)
{
	FHitResult LineTraceSingleResult;

	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::Type::None;
	if (bShowDebug)
	{
		drawDebugType = bShowOneFrame ? EDrawDebugTrace::Type::ForOneFrame : EDrawDebugTrace::Type::ForDuration;
	}

	UKismetSystemLibrary::LineTraceSingleForObjects(this, TraceStart, TraceEnd, ClimbSurfaceTypes,
		false, TArray<AActor*>(), drawDebugType, LineTraceSingleResult, true);
	
	return LineTraceSingleResult;
}

#pragma endregion

#pragma region ClimbCore

void UCLSMovementComponent::ToggleClimbing(bool bEnable)
{
	if (bEnable)
	{
		if (CanStartClimbing())
		{
			StartClimbing();
		}
	}
	else
	{
		EndClimbing();
	}
}

bool UCLSMovementComponent::CanStartClimbing()
{
	return TraceClimbSurfaces(true,false) && TraceFromEyes(100,true,false).bBlockingHit && !IsFalling();
}

bool UCLSMovementComponent::TraceClimbSurfaces(bool bShowDebug /*= false*/, bool bShowOneFrame /*= true*/)
{
	const FVector StratOffset{ UpdatedComponent->GetForwardVector() * 30.f };
	const FVector StartTrace{ UpdatedComponent->GetComponentLocation() + StratOffset };
	const FVector EndTrace = StartTrace + UpdatedComponent->GetForwardVector();
	ClimbTraceResults = DoCapsuleTraceMultiByObject(StartTrace, EndTrace, bShowDebug, bShowOneFrame);

	return !ClimbTraceResults.IsEmpty();
}

FHitResult UCLSMovementComponent::TraceFromEyes(float TraceDistance, float TraceStartOffset /*= 0*/, bool bShowDebug /*= false*/, bool bShowOneFrame /*= true*/)
{
	const FVector ComponentLocation{ UpdatedComponent->GetComponentLocation()};
	const FVector EyeHeightOffset{ UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset)};
	const FVector StartTraceLocation = ComponentLocation + EyeHeightOffset;
	const FVector EndTraceLocation = StartTraceLocation + (UpdatedComponent->GetForwardVector() * TraceDistance);

	return DoLineTraceSingleByObject(StartTraceLocation, EndTraceLocation, bShowDebug, bShowOneFrame);
}

bool UCLSMovementComponent::IsClimbing() const
{
	return (MovementMode == MOVE_Custom) && (CustomMovementMode == (uint8)ECustomMovementMode::MOVE_Climb);
}

void UCLSMovementComponent::StartClimbing()
{
	Debug::Print("Entering Climbing State");
	SetMovementMode(MOVE_Custom, (uint8)ECustomMovementMode::MOVE_Climb);

}

void UCLSMovementComponent::EndClimbing()
{
	Debug::Print("Exiting Climbing State");
	SetMovementMode(MOVE_Falling);
}

void UCLSMovementComponent::GetClimbSurfaceInfo()
{
	CurrentClimableSurfLocation = FVector::ZeroVector;
	CurrentClimableSurfNormal = FVector::ZeroVector;

	if (ClimbTraceResults.IsEmpty())
	{
		return;
	}

	for (const FHitResult& tracedHitResult : ClimbTraceResults)
	{
		CurrentClimableSurfLocation += tracedHitResult.ImpactPoint;
		CurrentClimableSurfNormal += tracedHitResult.ImpactNormal;
	}

	CurrentClimableSurfLocation /= ClimbTraceResults.Num();
	CurrentClimableSurfNormal = CurrentClimableSurfNormal.GetSafeNormal();

}

FQuat UCLSMovementComponent::GetClimbRotation(float DeltaTime) const
{
	const FQuat currentQuat {UpdatedComponent->GetComponentQuat()};

	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return currentQuat;
	}

	const FQuat targetQuat {FRotationMatrix::MakeFromX(-CurrentClimableSurfNormal).ToQuat()};

	//return interpolated result for smooth rotation
	return FMath::QInterpTo(currentQuat,targetQuat,DeltaTime,5.f);
}

void UCLSMovementComponent::SnapToClimable(float DeltaTime)
{
	const FVector currentLocation {UpdatedComponent->GetComponentLocation()};
	const FVector forwardVec { UpdatedComponent->GetForwardVector()};
	const FVector projectedCharToSurface {(CurrentClimableSurfLocation - currentLocation).ProjectOnTo(forwardVec)};

	//Vector that defines by how much we are going to move the component to snap to the wall
	const FVector snapOffset {-CurrentClimableSurfNormal * projectedCharToSurface.Length()};

	UpdatedComponent->MoveComponent(snapOffset * DeltaTime * MaxClimbSpeed, UpdatedComponent->GetComponentQuat(),true);
}

#pragma endregion