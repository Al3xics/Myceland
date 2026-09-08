Read-only demo saves, packaged with the build.

Create one by playing to the point you want, then in the console:
    ml.ExportDemoSave Demo_Lvl_2 "Demo - Playtest Level 2"

They are never written to or deleted at runtime: continuing one duplicates it
into a fresh normal slot first (see UML_SaveSubsystem::ContinueFromSlot).
