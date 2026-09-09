#include "SpeedsterCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/PostProcessVolume.h"


ASpeedsterCharacter::ASpeedsterCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0, 500.0, 0.0);
	MoveComp->JumpZVelocity = 700.f;
	MoveComp->AirControl = 0.35f;
	MoveComp->MaxWalkSpeed = BaseMaxWalkSpeed;
	MoveComp->MinAnalogWalkSpeed = 20.f;
	MoveComp->BrakingDecelerationWalking = 2000.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ASpeedsterCharacter::BeginPlay()
{
	Super::BeginPlay();

	BulletTimePPVolume = GetWorld()->SpawnActor<APostProcessVolume>();
	if (!BulletTimePPVolume)
	{
		return;
	}

	BulletTimePPVolume->bUnbound = true;
	BulletTimePPVolume->bEnabled = true;
	BulletTimePPVolume->Priority = 100.f;
	BulletTimePPVolume->BlendWeight = 0.f;

	FPostProcessSettings& PPS = BulletTimePPVolume->Settings;

	// 冷色调+低饱和+高对比+暗角，营造子弹时间视觉
	PPS.bOverride_SceneColorTint = true;
	PPS.SceneColorTint = FLinearColor(0.65f, 0.8f, 1.0f, 1.0f);

	PPS.bOverride_ColorSaturation = true;
	PPS.ColorSaturation = FVector4(0.4f, 0.45f, 0.65f, 1.0f);

	PPS.bOverride_ColorContrast = true;
	PPS.ColorContrast = FVector4(1.1f, 1.1f, 1.1f, 1.0f);

	PPS.bOverride_VignetteIntensity = true;
	PPS.VignetteIntensity = 0.35f;
}

void ASpeedsterCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ASpeedsterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Speedster] '%s' requires an Enhanced Input Component."), *GetNameSafe(this));
		return;
	}

	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASpeedsterCharacter::OnMove);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASpeedsterCharacter::OnLook);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ASpeedsterCharacter::OnSprintStart);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ASpeedsterCharacter::OnSprintStop);
	EnhancedInput->BindAction(TimeSlowAction, ETriggerEvent::Started, this, &ASpeedsterCharacter::OnTimeSlowStart);
	EnhancedInput->BindAction(TimeSlowAction, ETriggerEvent::Completed, this, &ASpeedsterCharacter::OnTimeSlowStop);
}

void ASpeedsterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// 速度够快才检测撞击，平时不用扫
	if (GetVelocity().Size() > 2000.f)
	{
		CheckSpeedImpact();
	}

	if (FollowCamera)
	{
		const float CurrentSpeed2D = GetVelocity().Size2D();
		// 速度越快 FOV 越大，90 ~ 120 之间
		const float TargetFOV = FMath::GetMappedRangeValueClamped(
			FVector2D(BaseMaxWalkSpeed, AbsoluteMaxSpeed),
			FVector2D(90.0f, 120.0f),
			CurrentSpeed2D);
		FollowCamera->SetFieldOfView(TargetFOV);
	}

	if (bIsSprinting)
	{
		CurrentSprintTime += DeltaTime;

		float Multiplier = 1.0f;
		if (AccelerationCurve)
		{
			Multiplier = AccelerationCurve->GetFloatValue(CurrentSprintTime);
		}
		Multiplier = FMath::Max(Multiplier, 1.0f);

		const float TargetSpeed = FMath::Min(BaseMaxWalkSpeed * Multiplier, AbsoluteMaxSpeed);
		MoveComp->MaxWalkSpeed = TargetSpeed;
	}

	UpdateBulletTimePostProcess(DeltaTime);
}

void ASpeedsterCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	if (!Controller)
	{
		return;
	}

	const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void ASpeedsterCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	if (!Controller)
	{
		return;
	}

	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void ASpeedsterCharacter::OnSprintStart(const FInputActionValue& Value) { StartSprinting(); }
void ASpeedsterCharacter::OnSprintStop(const FInputActionValue& Value) { StopSprinting(); }

void ASpeedsterCharacter::StartSprinting()
{
	bIsSprinting = true;
}

void ASpeedsterCharacter::StopSprinting()
{
	bIsSprinting = false;
	CurrentSprintTime = 0.f;  // 重置加速计时

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->MaxWalkSpeed = BaseMaxWalkSpeed;
	}
}

void ASpeedsterCharacter::OnTimeSlowStart(const FInputActionValue& Value)
{
	if (bIsTimeSlowed) return;

	bIsTimeSlowed = true;
	BulletTimeBlendTarget = 1.f;

	// 全局时间缩到 0.1，自己用 CustomTimeDilation 补回正常
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.1f);
	CustomTimeDilation = 10.0f;
}

void ASpeedsterCharacter::OnTimeSlowStop(const FInputActionValue& Value)
{
	if (!bIsTimeSlowed) return;

	bIsTimeSlowed = false;
	BulletTimeBlendTarget = 0.f;

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
	CustomTimeDilation = 1.0f;
}

void ASpeedsterCharacter::CheckSpeedImpact()
{
	FVector Start = GetActorLocation();
	FVector End = Start + GetVelocity().GetSafeNormal() * 150.f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_PhysicsBody,
		FCollisionShape::MakeSphere(80.f),
		Params
	);

	if (bHit)
	{
		UPrimitiveComponent* Component = Hit.GetComponent();
		if (Component && Component->IsSimulatingPhysics())
		{
			const float ObjectMass = Component->GetMass();
			const FVector MyVelocity = GetVelocity();

			UCharacterMovementComponent* MoveComp = GetCharacterMovement();
			const float PlayerMass = MoveComp ? MoveComp->Mass : 65.f;

			// 完全弹性碰撞，把物体弹飞，自己受到反向冲量
			const FVector ObjectVelocity = (2.f * PlayerMass / (PlayerMass + ObjectMass)) * MyVelocity;
			const FVector ObjectImpulse = ObjectMass * ObjectVelocity;

			Component->AddImpulseAtLocation(ObjectImpulse, Hit.Location);

			const FVector PlayerDeltaV = -ObjectImpulse / PlayerMass;
			if (MoveComp)
			{
				MoveComp->AddImpulse(PlayerDeltaV, true);
			}
		}
	}
}

void ASpeedsterCharacter::UpdateBulletTimePostProcess(float DeltaTime)
{
	if (!BulletTimePPVolume) return;

	// 后处理权重平滑过渡，避免画面突变
	const float Current = BulletTimePPVolume->BlendWeight;
	const float NewWeight = FMath::FInterpTo(Current, BulletTimeBlendTarget, DeltaTime, BulletTimeBlendSpeed);
	BulletTimePPVolume->BlendWeight = NewWeight;
}