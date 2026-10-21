using UnrealBuildTool;

public class DernierRempartEditorTarget : TargetRules
{
	public DernierRempartEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DernierRempart");
	}
}
