using UnrealBuildTool;

public class DernierRempart : ModuleRules
{
	public DernierRempart(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "AssetRegistry" });
		// Herbe et cailloux : maillages fabriqués au lancement (étape 16).
		PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "StaticMeshDescription" });
	}
}
