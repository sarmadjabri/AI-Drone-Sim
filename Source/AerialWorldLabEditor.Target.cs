using UnrealBuildTool;

public class AerialWorldLabEditorTarget : TargetRules
{
    public AerialWorldLabEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        ExtraModuleNames.Add("AerialWorldLab");
    }
}
