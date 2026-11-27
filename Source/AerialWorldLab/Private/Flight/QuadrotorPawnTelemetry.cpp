// Human-readable aircraft status, separate from simulation state updates.
#include "Flight/QuadrotorPawn.h"

#include "Components/BoxComponent.h"

FString AQuadrotorPawn::GetControlStatus() const
{
    const FVector Rates =
        Body->GetComponentQuat().UnrotateVector(Body->GetPhysicsAngularVelocityInRadians()) *
        (180.0F / PI);
    const TCHAR *Armed = TEXT("DISARMED");
    if (MotorState == EMotorState::Armed)
    {
        Armed = TEXT("ARMED");
    }
    else if (MotorState == EMotorState::Failsafe)
    {
        Armed = TEXT("FAILSAFE / MOTORS OFF");
    }
    const TCHAR *HoldStatus = TEXT("off");
    if (bHoldingPosition)
    {
        HoldStatus = TEXT("on");
    }
    const TCHAR *MotorSaturationStatus = TEXT("no");
    if (bMotorSaturated)
    {
        MotorSaturationStatus = TEXT("yes");
    }
    const TCHAR *InputSaturationStatus = TEXT("no");
    if (FMath::Max(FMath::Max(FMath::Abs(ForwardInputTarget), FMath::Abs(LateralInputTarget)),
                   FMath::Max(FMath::Abs(YawInputTarget), FMath::Abs(ThrottleInputTarget))) >=
        0.999F)
    {
        InputSaturationStatus = TEXT("yes");
    }
    const TCHAR *CollectiveDescription = TEXT("direct");
    if (PilotControls->CollectiveMode == ECollectiveMode::Incremental)
    {
        CollectiveDescription = TEXT("latched");
    }
    FString Out = FString::Printf(
        TEXT("%s | %s | %s | %s\nInput pitch/roll/yaw: %+.2f %+.2f %+.2f | speed %.2f m/s | "
             "vertical %+.2f m/s\nPosition hold: %s | motor saturation: %s | input saturation: "
             "%s\n"),
        *GetFlightModeName(), *GetInputProfileName(), *GetViewModeName(), Armed, ForwardInput,
        LateralInput, YawInput, GetVelocityMetersPerSecond().Size2D(),
        GetVelocityMetersPerSecond().Z, HoldStatus, MotorSaturationStatus, InputSaturationStatus);
    if (IsManualMode())
    {
        Out += FString::Printf(TEXT("Collective %.1f%% (%s) | rates command/measured deg/s\nRoll "
                                    "%.1f/%.1f Pitch %.1f/%.1f Yaw %.1f/%.1f\n"),
                               PilotControls->Throttle * 100, CollectiveDescription,
                               FMath::RadiansToDegrees(CommandedBodyRate.X), Rates.X,
                               FMath::RadiansToDegrees(CommandedBodyRate.Y), Rates.Y,
                               FMath::RadiansToDegrees(CommandedBodyRate.Z), Rates.Z);
    }
    else
    {
        Out += FString::Printf(
            TEXT("Velocity command X/Y/Z %.2f %.2f %.2f m/s | actual %.2f %.2f %.2f m/s\nAltitude "
                 "target %.2f m\nClimb request %+.2f (not collective)\n"),
            CommandedVelocity.X, CommandedVelocity.Y, CommandedVelocity.Z,
            GetVelocityMetersPerSecond().X, GetVelocityMetersPerSecond().Y,
            GetVelocityMetersPerSecond().Z, AltitudeTargetMeters, ThrottleInput);
    }
    if (MotorState == EMotorState::Failsafe)
    {
        Out += TEXT("MOTORS OFF: reconnect, land/reset, then deliberately re-arm.\n");
    }
    else if (MotorState == EMotorState::Disarmed)
    {
        Out += FString::Printf(
            TEXT("DISARMED: release sticks on ground; hold %s for %.1fs to arm.\n"),
            *PilotControls->ActionBindings(EPilotAction::Arm), PilotControls->HoldSeconds);
    }
    else if (bAwaitingNeutralInput)
    {
        Out +=
            TEXT("INPUT HELD: center sticks (direct collective: match the displayed throttle).\n");
    }
    else if (bTakeoffRequired)
    {
        Out += TEXT("ARMED, WAITING FOR TAKEOFF: press ") +
               PilotControls->ActionBindings(EPilotAction::Takeoff) + TEXT(".\n");
    }
    if (!PilotControls->Status.IsEmpty())
    {
        Out += PilotControls->Status + TEXT("\n");
    }
    return Out;
}
