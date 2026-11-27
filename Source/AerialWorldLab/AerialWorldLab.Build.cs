using UnrealBuildTool;

public class AerialWorldLab : ModuleRules
{
    public AerialWorldLab(ReadOnlyTargetRules Target) : base(Target)
    {
        // Linux SDL owns the native window size hints used during fullscreen transitions.
        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
        }
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "ApplicationCore", "Json", "SlateCore" });
    }
}
