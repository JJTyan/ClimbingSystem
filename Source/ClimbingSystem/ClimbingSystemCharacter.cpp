// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingSystemCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "CLSMovementComponent.h"
#include "MotionWarpingComponent.h"

#include "CLDebugHelpers.h" 

//////////////////////////////////////////////////////////////////////////
// AClimbingSystemCharacter

void AClimbingSystemCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode /*= 0*/)
{
	Super::OnMovementModeChanged(PrevMovementMode,PreviousCustomMode);

	if (CLSMovementComponent->IsClimbing())
	{
		GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}

	//if we exiting climbing
	else if (!CLSMovementComponent->IsClimbing() && PrevMovementMode == MOVE_Custom && PreviousCustomMode == (uint8)ECustomMovementMode::MOVE_Climb)
	{
		GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		//restore vertical position of character in case we exited climbing at angle
		const FRotator exitClimbRotation {GetActorRotation()};
		const FRotator desiredRotation {0.f,exitClimbRotation.Yaw, exitClimbRotation.Roll};
		SetActorRotation(desiredRotation);
	}
}

AClimbingSystemCharacter::AClimbingSystemCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UCLSMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	CLSMovementComponent = GetCharacterMovement<UCLSMovementComponent>();

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void AClimbingSystemCharacter::BeginPlay()
{
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	CLSMovementComponent->OnEnterClimbStateDelegate.BindUObject(this,&ThisClass::OnPlayerEnteredClimbState);
	CLSMovementComponent->OnExitClimbStateDelegate.BindUObject(this,&ThisClass::OnPlayerExitedClimbState);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AClimbingSystemCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::GroundMovement);
		EnhancedInputComponent->BindAction(ClimbMoveAction, ETriggerEvent::Triggered, this, &ThisClass::ClimbingMovement);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);

		//Climbing
		EnhancedInputComponent->BindAction(ClimbAction, ETriggerEvent::Started, this, &ThisClass::OnClimbActionStarted);

		//Climbing
		EnhancedInputComponent->BindAction(ClimbHopAction, ETriggerEvent::Started, this, &ThisClass::OnHopActionStarted);
	}

}

void AClimbingSystemCharacter::GroundMovement(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AClimbingSystemCharacter::ClimbingMovement(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D MovementVector {Value.Get<FVector2D>()};

	//the direction where player will move when pressed Forward. Can be straight up or at angle depending on climbing sutface
	//cross prod of inversed surface normal (as normal is from surface to player and we need it other way) and right vector
	const FVector ForwardDirection{FVector::CrossProduct(-CLSMovementComponent->GetClimbSurfaceNormal(),GetRootComponent()->GetRightVector())};

	const FVector RightDirection{FVector::CrossProduct(-CLSMovementComponent->GetClimbSurfaceNormal(),-GetRootComponent()->GetUpVector()) };
	
	// add movement 
	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void AClimbingSystemCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AClimbingSystemCharacter::OnClimbActionStarted(const FInputActionValue& Value)
{
	CLSMovementComponent->ToggleClimbing(!CLSMovementComponent->IsClimbing());

}

void AClimbingSystemCharacter::OnHopActionStarted(const FInputActionValue& Value)
{
	Debug::Print("Hop action");
}

void AClimbingSystemCharacter::OnPlayerEnteredClimbState()
{
	AddInputMappingContext(ClimbingMappingContext,1);
}

void AClimbingSystemCharacter::AddInputMappingContext(UInputMappingContext* NewContext, int32 Priority)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(NewContext, Priority);
		}
	}
}

void AClimbingSystemCharacter::OnPlayerExitedClimbState()
{
	RemoveInputMappingContext(ClimbingMappingContext);
}

void AClimbingSystemCharacter::RemoveInputMappingContext(UInputMappingContext* Context)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(Context);
		}
	}
}

