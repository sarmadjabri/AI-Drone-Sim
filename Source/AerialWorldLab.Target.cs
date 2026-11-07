using UnrealBuildTool;

public class AerialWorldLabTarget : TargetRules
{
    public AerialWorldLabTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        ExtraModuleNames.Add("AerialWorldLab");
    }
}
