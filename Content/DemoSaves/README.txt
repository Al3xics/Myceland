Read-only demo saves, packaged with the build.

Create one by playing to the point you want, then in the console:
    ml.ExportDemoSave Demo_A Demo - debut du Level 2

They are never written to or deleted at runtime: continuing one duplicates it
into a fresh normal slot first (see UML_SaveSubsystem::ContinueFromSlot).
