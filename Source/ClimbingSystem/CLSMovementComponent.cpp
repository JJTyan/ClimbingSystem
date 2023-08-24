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

TArray <FHitResult> UCLSMovementComponent::DoCapsuleTraceMultiByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug /*= false*/)
{
	TArray<FHitResult> CapsuleHitResults;
	UKismetSystemLibrary::CapsuleTraceMultiForObjects(this, TraceStart, TraceEnd, ClimbCapsuleTraceRadius, ClimbCapsuleTraceHalfHeight, ClimbSurfaceTypes,
		false, TArray<AActor*>(), bShowDebug ? EDrawDebugTrace::Type::ForOneFrame : EDrawDebugTrace::Type::None, CapsuleHitResults, true);

	return CapsuleHitResults;
}

FHitResult UCLSMovementComponent::DoLineTraceSingleByObject(const FVector& TraceStart, const FVector& TraceEnd, bool bShowDebug /*= false*/)
{
	FHitResult LineTraceSingleResult;
	UKismetSystemLibrary::LineTraceSingleForObjects(this, TraceStart, TraceEnd, ClimbSurfaceTypes,
		false, TArray<AActor*>(), bShowDebug ? EDrawDebugTrace::Type::ForOneFrame : EDrawDebugTrace::Type::None, LineTraceSingleResult, true);
	
	return LineTraceSingleResult;
}

#pragma endregion

#pragma region ClimbCore

bool UCLSMovementComponent::CanStartClimbing()
{
	return TraceClimbSurfaces() && TraceFromEyes(100).bBlockingHit && !IsFalling();
}

void UCLSMovementComponent::ToggleClimbing(bool bEnable)
{
	if (bEnable)
	{
		if (CanStartClimbing())
		{
			Debug::Print("Entering Climbing State");
			MovementMode = MOVE_Custom;
			CustomMovementMode = (uint8)ECustomMovementMode::MOVE_Climb;
		}
	}
	else
	{
		Debug::Print("Exiting Climbing State");
		MovementMode = MOVE_Walking;
	}
	
}

bool UCLSMovementComponent::IsClimbing() const
{
	return (MovementMode == MOVE_Custom) && (CustomMovementMode == (uint8)ECustomMovementMode::MOVE_Climb);
}

bool UCLSMovementComponent::TraceClimbSurfaces()
{
	const FVector StratOffset { UpdatedComponent->GetForwardVector() * 30.f};
	const FVector StartTrace { UpdatedComponent->GetComponentLocation() + StratOffset};
	const FVector EndTrace = StartTrace + UpdatedComponent->GetForwardVector();
	ClimbTraceResults = DoCapsuleTraceMultiByObject(StartTrace, EndTrace, true);

	return !ClimbTraceResults.IsEmpty();
}

FHitResult UCLSMovementComponent::TraceFromEyes(float TraceDistance, float TraceStartOffset /*= 0*/)
{
	
	const FVector ComponentLocation{ UpdatedComponent->GetComponentLocation()};
	const FVector EyeHeightOffset{ UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset)};
	const FVector StartTraceLocation = ComponentLocation + EyeHeightOffset;
	const FVector EndTraceLocation = StartTraceLocation + (UpdatedComponent->GetForwardVector() * TraceDistance);

	return DoLineTraceSingleByObject(StartTraceLocation, EndTraceLocation, true);
}

#pragma endregion