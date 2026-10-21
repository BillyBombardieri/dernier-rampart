using UnrealBuildTool;

public class DernierRempartTarget : TargetRules
{
	public DernierRempartTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DernierRempart");
	}
}
