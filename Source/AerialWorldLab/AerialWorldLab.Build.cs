using UnrealBuildTool;

public class AerialWorldLab : ModuleRules
{
    public AerialWorldLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "ApplicationCore" });
    }
}
