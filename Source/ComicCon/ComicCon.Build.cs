// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ComicCon : ModuleRules
{
	public ComicCon(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(new string[] { "ComicCon" });
        
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Networking", "Sockets", "Json", "HTTP", "MediaPlate", "Niagara", "MediaPlate", "NDIIO", "MediaAssets" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
