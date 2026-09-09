#include "SpeedsterMovementComponent.h"
#include "Curves/CurveFloat.h"

USpeedsterMovementComponent::USpeedsterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USpeedsterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	BaseMaxWalkSpeed = MaxWalkSpeed;
	DefaultGroundFriction = GroundFriction;
	DefaultBrakingDeceleration = BrakingDecelerationWalking;
}

void USpeedsterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasValidData())
	{
		return;
	}

	const float CurrentSpeed2D = Velocity.Size2D();

	if (bIsSprinting)
	{
		SprintTime += DeltaTime;

		float CurveMultiplier = 1.0f;
		if (AccelerationCurve)
		{
			CurveMultiplier = AccelerationCurve->GetFloatValue(SprintTime);
		}
		CurveMultiplier = FMath::Clamp(CurveMultiplier, 1.0f, SprintMaxSpeedMultiplier);

		MaxWalkSpeed = BaseMaxWalkSpeed * CurveMultiplier;
	}
	else
	{
		SprintTime = 0.0f;
		MaxWalkSpeed = BaseMaxWalkSpeed;
	}

	// 超过高速阈值后降低摩擦力，做出滑行的惯性手感
	const float ThresholdSpeed = HighSpeedThreshold * BaseMaxWalkSpeed;

	if (CurrentSpeed2D > ThresholdSpeed)
	{
		GroundFriction = HighSpeedGroundFriction;
		BrakingDecelerationWalking = HighSpeedBrakingDeceleration;
	}
	else
	{
		GroundFriction = DefaultGroundFriction;
		BrakingDecelerationWalking = DefaultBrakingDeceleration;
	}
}

void USpeedsterMovementComponent::StartSprinting()
{
	bIsSprinting = true;
}

void USpeedsterMovementComponent::StopSprinting()
{
	bIsSprinting = false;
	// 速度会在下一帧 Tick 里恢复
}