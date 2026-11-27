// Stable settings layout indices; labels and editing use the same row names.
#pragma once
#include "CoreMinimal.h"

namespace Aether
{
    namespace PilotMenu
    {
        inline constexpr int32 GeneralPage = 0;
        inline constexpr int32 AxesPage = 1;
        inline constexpr int32 ModeTuningPage = 2;
        inline constexpr int32 ButtonsPage = 3;
        inline constexpr int32 ControllerMapPage = 4;
        inline constexpr int32 PageCount = 5;
        enum class EGeneralRow : int32
        {
            InputProfile = 0,
            FlightMode = 1,
            CollectiveMode = 2,
            ThrottleIncrease = 3,
            ThrottleDecrease = 4,
            PrecisionScale = 5,
            RightTriggerRole = 6,
            CameraSensitivity = 7,
            ArmHoldDuration = 8,
            ResetHoldDuration = 9,
            ProfileSlot = 10,
            SaveProfile = 11,
            LoadProfile = 12,
            RestoreDefaults = 13,
            LeftTriggerRole = 14,
            Count = 15
        };
        enum class EAxisRow : int32
        {
            SelectedAxis = 0,
            PhysicalSource = 1,
            Invert = 2,
            InnerDeadZone = 3,
            OuterDeadZone = 4,
            Center = 5,
            Minimum = 6,
            Maximum = 7,
            Sensitivity = 8,
            ExpoOffset = 9,
            Count = 10
        };
    } // namespace PilotMenu
} // namespace Aether
