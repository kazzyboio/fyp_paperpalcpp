// Copyright Epic Games, Inc. All Rights Reserved.

#include "paperpalcppCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/KismetMathLibrary.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ApaperpalcppCharacter

ApaperpalcppCharacter::ApaperpalcppCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(35.f, 60.f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 825.0f;
	GetCharacterMovement()->AirControl = 1.0f;
	GetCharacterMovement()->MaxWalkSpeed = 650.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	
	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> characterMesh(TEXT("/Script/Engine.SkeletalMesh'/Game/PlayerCharacter/NewCHara/Skeleton_Player.Skeleton_Player'"));

	if (characterMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(characterMesh.Object);
	}


}

void ApaperpalcppCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
	
	// Iterate over all attached components to find the "Plane" and "Roll" skeletal meshes
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);
	for (USkeletalMeshComponent* MeshComponent : SkeletalMeshComponents)
	{
		// Check if the component's name matches "Plane" or "Roll"
		if (MeshComponent->GetName() == "Plane")
		{
			PlaneMesh = MeshComponent;
		}
		else if (MeshComponent->GetName() == "Roll")
		{
			RollMesh = MeshComponent;
		}
	}

	// Hide the found meshes
	if (PlaneMesh)
	{
		PlaneMesh->SetVisibility(false);
	}
	if (RollMesh)
	{
		RollMesh->SetVisibility(false);
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Using C++ character"));
	//GetCapsuleComponent()->SetSimulatePhysics(GetCapsuleComponent);

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ApaperpalcppCharacter::Tick(float deltaSeconds)
{
	delta = deltaSeconds;
}

//////////////////////////////////////////////////////////////////////////
// Input

void ApaperpalcppCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Gliding
		EnhancedInputComponent->BindAction(GlideAction, ETriggerEvent::Started, this, &ApaperpalcppCharacter::EnablePlane);
		EnhancedInputComponent->BindAction(GlideAction, ETriggerEvent::Completed, this, &ApaperpalcppCharacter::DisablePlane);

		// Sprinting
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ApaperpalcppCharacter::StartSprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ApaperpalcppCharacter::StopSprint);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ApaperpalcppCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ApaperpalcppCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ApaperpalcppCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

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

void ApaperpalcppCharacter::Look(const FInputActionValue& Value)
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

void ApaperpalcppCharacter::TogglePlane()
{
	if (isGliding == false)
	{
		EnablePlane();
	}
	else
	{
		DisablePlane();
	}
}

void ApaperpalcppCharacter::EnablePlane()
{
	if (CanGlide())
	{
		currentVelocity = GetCharacterMovement()->Velocity;
		isGliding = true;

		if (GetMesh())
		{
			GetMesh()->SetVisibility(false);
		}

		if (PlaneMesh)
		{
			PlaneMesh->SetVisibility(true);
		}

		GetCapsuleComponent()->SetCapsuleSize(55.f, 55.f);
		GetCharacterMovement()->Velocity = FVector(currentVelocity.X, currentVelocity.Y, -125.f);
		GetCharacterMovement()->GravityScale = 0;
		GetCharacterMovement()->AirControl = 10;
		GetCharacterMovement()->RotationRate = FRotator(0, 0, 175);
		GetCharacterMovement()->bUseControllerDesiredRotation = true;		
	}

}

void ApaperpalcppCharacter::DisablePlane()
{
		isGliding = false;

		if (GetMesh())
		{
			GetMesh()->SetVisibility(true);
		}

		if (PlaneMesh)
		{
			PlaneMesh->SetVisibility(false);
		}

		GetCapsuleComponent()->SetCapsuleSize(35.f, 60.f);
		GetCharacterMovement()->GravityScale = 1.f;
		GetCharacterMovement()->AirControl = 1.f;
		GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 600.f);
}

bool ApaperpalcppCharacter::CanGlide()
{
	FHitResult Hit;

	FVector TraceStart = GetActorLocation();
	FVector TraceEnd = GetActorLocation() + GetActorUpVector() * -1;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	TEnumAsByte<ECollisionChannel> TraceProperties = ECC_Visibility;

	GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, TraceProperties, QueryParams);

	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, Hit.bBlockingHit ? FColor::Blue : FColor::Red);

	if (Hit.bBlockingHit == false && GetCharacterMovement()->IsFalling() == true)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void ApaperpalcppCharacter::StartSprint()
{

	
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		if (RollMesh)
		{
			RollMesh->SetVisibility(true);
		}

		// Hide the player character mesh
		if (GetMesh())
		{
			GetMesh()->SetVisibility(false);
		}

		GetCharacterMovement()->MaxWalkSpeed = 1000.f;
		isSprinting = true;
		Crouch();
	}
}

void ApaperpalcppCharacter::StopSprint()
{
	if (RollMesh)
	{
		RollMesh->SetVisibility(false);
	}

	if (GetMesh())
	{
		GetMesh()->SetVisibility(true);
	}
	GetCharacterMovement()->MaxWalkSpeed = 650.f;
	isSprinting = false;
	UnCrouch();

}

void ApaperpalcppCharacter::DrainStamina()
{
	stamina = maxStamina - 1;

	if (stamina == 0)
	{
		StopSprint();
	}
}

void ApaperpalcppCharacter::RegenStamina()
{
}

void ApaperpalcppCharacter::StaminaChunk()
{
}

