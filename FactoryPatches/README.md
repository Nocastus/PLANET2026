# Factory Patch Bank

This folder is the source of truth for the patches baked into the ISHTAR
binary. The current contents are PLACEHOLDERS pending Beta2 curation.

## Workflow

1. Drop curated patch `.md` files into `FactoryPatches/<Category>/`.
   The subfolder name becomes the category shown in the Load menu.
2. Run `py Tools/bake_factory_patches.py` - this regenerates
   `Source/FactoryPatchData.h`.
3. Rebuild the plugin. The bank now ships inside the binary: no files to
   install, and factory patches are read-only by construction.

In the plugin, the Load button shows the factory categories as submenus,
with "Browse User Patches..." underneath for the Documents-folder library
(saving is unchanged and always goes to the user library).

Note: this folder is not scanned at runtime - editing it does nothing until
you re-run the baker and rebuild.
