// Fill out your copyright notice in the Description page of Project Settings.


#include "CLSMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"

#pragma region ClimbTraces

void UCLSMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType, ThisTickFunction);
}

void UCLSMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	UAnimInstance* playerAnimInstance{ GetCharacterOwner()->GetMesh()->GetAnimInstance() };
	playerAnimInstance->OnMontageEnded.AddDynamic(this, &ThisClass::OnClimbMontageEnded);
	playerAnimInstance->OnMontageBlendingOut.AddDynamic(this, &ThisClass::OnClimbMontageEnded);
}

FVector UCLSMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	UAnimInstance* playerAnimInstance{ GetCharacterOwner()->GetMesh()->GetAnimInstance() };
	if (IsFalling() && playerAnimInstance->IsAnyMontagePlaying())
	{
		return RootMotionVelocity;
	}
	else
	{
		return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity,CurrentVelocity);
	}
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
		OnEnterClimbStateDelegate.ExecuteIfBound();
	}
	else if (!IsClimbing() && PreviousMovementMode == MOVE_Custom && PreviousCustomMode == (uint8)ECustomMovementMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		StopMovementImmediately();
		OnExitClimbStateDelegate.ExecuteIfBound();
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

		//Check if we should stop climbing
		if (ShouldStopClimbing() || IsFloorReached())
		{
			EndClimbing();
			return;
		}

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

		if (IsLedgeReached())
		{
			EndClimbing();
			PlayClimbMontage(ClimbToLedge);
		}
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
			//Enter the climb state after transition anim finished
			PlayClimbMontage(IdleToClimb);
		}
		else if (CanStartDescending())
		{
			PlayClimbMontage(IdleToLedge);
		}
		else
		{
			bool canVault;
			FVector vaultStart;
			FVector vaultEnd;
			Tie(canVault, vaultStart, vaultEnd) = CanVault();
			if (canVault)
			{
				StartVaulting(vaultStart, vaultEnd);
			}
		}
	}

	if (!bEnable)
	{
		EndClimbing();
	}
}

bool UCLSMovementComponent::CanStartClimbing()
{
	return TraceClimbSurfaces(true,false) && TraceFromEyes(100,true,false).bBlockingHit && !IsFalling();
}

bool UCLSMovementComponent::CanStartDescending()
{
	/* to detect down ledge we need to perform 3 traces 
				* =>
				|	||
				|	\/
				|<==
	
	*/

	const float TRACE1_LENGTH {50.f};
	const float TRACE2_HEIGHT {400.f};
	const float TRACE3_LENGTH {75.f};

	if (IsFalling())
	{
		return false;
	}

	FHitResult Trace1HitResult{ TraceFromEyes(TRACE1_LENGTH) };

	if (Trace1HitResult.bBlockingHit)
	{
		return false;
	}

	const FVector Trace2Start { Trace1HitResult.TraceEnd };
	const FVector Trace2End{ Trace2Start + FVector::DownVector * TRACE2_HEIGHT };
	FHitResult Trace2HitResult{ DoLineTraceSingleByObject(Trace2Start,Trace2End,false,true) };

	if (Trace2HitResult.bBlockingHit)
	{
		return false;
	}

	const FVector Trace3Start{ Trace2HitResult.TraceEnd };
	const FVector Trace3End{ Trace3Start + -UpdatedComponent->GetForwardVector() * TRACE3_LENGTH };
	FHitResult Trace3HitResult{ DoLineTraceSingleByObject(Trace3Start,Trace3End,false,true) };

	if (Trace3HitResult.bBlockingHit)
	{
		return true;
	}

	return false;
}

TTuple<bool, FVector, FVector> UCLSMovementComponent::CanVault()
{
	/*we can vault an obstacle of limited height and width in front of character*/

	if (IsFalling())
	{
		return MakeTuple(false,FVector::ZeroVector, FVector::ZeroVector);
	}

	const FVector componentLocation {UpdatedComponent->GetComponentLocation()};
	const FVector forwardVec { UpdatedComponent->GetForwardVector()};
	const FVector upVec{ UpdatedComponent->GetUpVector() };
	const FVector downVec {-upVec};

	const float VERTICAL_OFFSET {100.f};
	const float TRACE_DISTANCE_VAULT_SURFACE {100.f};
	const float TRACE_DISTANCE_FLOOR {300.f};
	const float HORIZONTAL_STEP {100.f};
	const float MAX_VAULT_DISTANCE {500.f};

	/*
		We perform two traces - for vaulting surface and floor
		If for first trace (i=1) both traces returns true - then first trace (vault surface trace) returns valid vault start location

		if any of other iterations returns false for vault surface trace and true for floor surface trace - then floor trace returns end vault location

		if all other iterations return true for all traces - then we don't have a vaild end location (vaulting object is too large)
	*/

	for (int32 i = 1; i <= MAX_VAULT_DISTANCE / HORIZONTAL_STEP; ++i)
	{
		const FVector traceStart { componentLocation + upVec * VERTICAL_OFFSET + forwardVec * HORIZONTAL_STEP * i};
		FVector traceSurfaceEnd {traceStart + downVec * TRACE_DISTANCE_VAULT_SURFACE };
		FHitResult vaultSurfTrace {DoLineTraceSingleByObject(traceStart, traceSurfaceEnd, false,false)};

		traceSurfaceEnd = traceStart + downVec * TRACE_DISTANCE_FLOOR;
		FHitResult floorSurfTrace {DoLineTraceSingleByObject(traceStart, traceSurfaceEnd, false,false)};

		//check if we have a valid start vault location
		FVector startVault;
		FVector endVault;
		if (i==1)
		{
			if (vaultSurfTrace.bBlockingHit && floorSurfTrace.bBlockingHit)
			{
				startVault = vaultSurfTrace.ImpactPoint;
			}
			else
			{
				return MakeTuple(false, FVector::ZeroVector, FVector::ZeroVector);
			}
		}
		else
		{
			//check if we have a valid end vault location
			if (!vaultSurfTrace.bBlockingHit && floorSurfTrace.bBlockingHit)
			{
				endVault = floorSurfTrace.ImpactPoint;
				return MakeTuple(true, startVault, endVault);
			}
		}		
	}

	return MakeTuple(false, FVector::ZeroVector, FVector::ZeroVector);
}

void UCLSMovementComponent::SetMotionWarpTarget(FName TargetName, const FVector& TargetValue)
{
	UMotionWarpingComponent* motionWarpingComponent {Cast<UMotionWarpingComponent>(CharacterOwner->GetDefaultSubobjectByName(TEXT("MotionWarpingComponent")))};
	motionWarpingComponent->AddOrUpdateWarpTargetFromLocation(TargetName, TargetValue);
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

FVector UCLSMovementComponent::GetUnrotatedClimbVelocity() const
{
	return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(),Velocity);
}

void UCLSMovementComponent::StartClimbing()
{
	SetMovementMode(MOVE_Custom, (uint8)ECustomMovementMode::MOVE_Climb);
}

void UCLSMovementComponent::EndClimbing()
{
	SetMovementMode(MOVE_Falling);
}

void UCLSMovementComponent::StartVaulting(const FVector& StartVault, const FVector& EndVault)
{
	SetMotionWarpTarget(FName(TEXT("VaultStartLocation")), StartVault);
	SetMotionWarpTarget(FName(TEXT("VaultEndLocation")), EndVault);
	
	StartClimbing();
	PlayClimbMontage(Vault);
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

bool UCLSMovementComponent::ShouldStopClimbing() const
{
	if (ClimbTraceResults.IsEmpty())
	{
		return true;
	}

	//check angle between surface normal and vertical direction
	const float dotProduct {(float)FVector::DotProduct(CurrentClimableSurfNormal,FVector::UpVector)};
	const float degree {FMath::RadiansToDegrees(FMath::Acos(dotProduct))};

	return degree <= 60.f;
}

bool UCLSMovementComponent::IsFloorReached()
{
	const FVector downDirection {-UpdatedComponent->GetUpVector()};
	const FVector startOffset { downDirection * 50.f};
	const FVector startTrace { UpdatedComponent->GetComponentLocation() + startOffset};
	const FVector endTrace { startTrace + downDirection};

	TArray<FHitResult>traceHits	{DoCapsuleTraceMultiByObject(startTrace, endTrace,true,true)};

	if (traceHits.IsEmpty())
	{
		return false;
	}

	for (const FHitResult& possibleHit : traceHits)
	{
		//filter out surfaces that are not horizontal
		//check angle surface normal and vertical direction. If it is small - then surface is floor
		const float dotProduct{ (float)FVector::DotProduct(possibleHit.ImpactNormal,FVector::UpVector) };
		const float degree{ FMath::RadiansToDegrees(FMath::Acos(dotProduct)) };
		const bool isMovingDown { GetUnrotatedClimbVelocity().Z < -10.f };

		if (degree < 10.f && isMovingDown)
		{
			return true;
		}
	}

	return false;
}

bool UCLSMovementComponent::IsLedgeReached()
{
	FHitResult ledgeHitTrace {TraceFromEyes(100.f,80.f,true)};

	if (!ledgeHitTrace.bBlockingHit)
	{
		const FVector walkingSurfaceTraceStart { ledgeHitTrace.TraceEnd};
		const FVector walkingSurfaceTraceEnd{ walkingSurfaceTraceStart + FVector::DownVector * 100.f };

		FHitResult walkingSurfaceHitResult {DoLineTraceSingleByObject(walkingSurfaceTraceStart,walkingSurfaceTraceEnd,false,true)};

		if (walkingSurfaceHitResult.bBlockingHit && GetUnrotatedClimbVelocity().Z > 10.f)
		{
			return true;
		}
	}
	return false;
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

void UCLSMovementComponent::PlayClimbMontage(UAnimMontage* AnimToPlay)
{
	check(AnimToPlay != nullptr);
	UAnimInstance* playerAnimInstance { GetCharacterOwner()->GetMesh()->GetAnimInstance() };

	//don't interrupt any animations
	if (playerAnimInstance->IsAnyMontagePlaying())
	{
		return;
	}

	playerAnimInstance->Montage_Play(AnimToPlay);

}

void UCLSMovementComponent::OnClimbMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* playerAnimInstance{ GetCharacterOwner()->GetMesh()->GetAnimInstance() };

	if (Montage == IdleToClimb || Montage == IdleToLedge)
	{
		StartClimbing();
	}
	else
	{
		SetMovementMode(MOVE_Walking);
	}
}

#pragma endregion