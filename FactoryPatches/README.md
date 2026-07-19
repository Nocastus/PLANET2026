# Factory Patch Bank

This folder is the source of truth for the patches baked into the ISHTAR
binary. It is flat - no category subfolders.

## Layout rules

- Filenames carry a 2-digit order prefix: `01 Fable.md` ... `28 Init Patch.md`.
  The prefix sets the bank order; the Load menu shows the same numbers.
- Patch `01` is what a fresh instance opens with (currently Fable).
  Init Patch deliberately sits last.
- The `# Title` inside each file should match the filename minus the prefix -
  that title is what the GUI displays.
- Keep descriptions to roughly 55 characters or fewer: the patch-bar comment
  field cannot scroll, so longer text gets cut off.

## Workflow

1. Edit / add / renumber patch `.md` files here.
2. Run `py Tools/bake_factory_patches.py` - this regenerates
   `Source/FactoryPatchData.h`.
3. Rebuild the plugin (SharedCode first, then VST3 - see the build notes).
   The bank ships inside the binary: no files to install, and factory
   patches are read-only by construction.

In the plugin, the Load button shows the numbered factory list, with
"Browse User Patches..." underneath for the Documents-folder library
(saving is unchanged and always goes to the user library).

Note: this folder is not scanned at runtime - editing it does nothing until
you re-run the baker and rebuild.
