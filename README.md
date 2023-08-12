# PortalSpawner
*Woooooooo portals!*

![portals in action](http://i.imgur.com/9loiCe6.gif)

# Overview:

The original purpose of this plugin is to bypass buggy map sections, or to create shortcuts in maps that need them. It's also good for lulz. 

Pretty much every possible entity you can think of will be able to go through portals, including weapon projectiles (grenades, hornets, displacers, etc.) and func_pushable. 

Portals can be saved and loaded. The data files can be found in your /svencoop/scripts/plugins/store/ folder. If you create a "PortalSpawner" folder inside /store/ then the files will be written there instead (keeps your /store/ folder clean).

# Usage:

Say `.ps` or type `.ps` in console to open the menu. The menu will stay open until you explicitly "Exit" it or if you die. Admin rights are required to use this.

**There are three types of portals you can spawn:**

1) Portal entrance (orange)
2) Portal exit (green)
3) Bidirectional portal (white-ish)

To spawn a portal without using the menu, use the following commands:

`.ps en` = spawns a portal entrance  
`.ps ex` = spawns a portal exit  
`.ps bi` = spawns a bidirectional portal  
You can type out ".ps enter" or ".ps exit" or ".ps bidirectional" if you really want to.

Portal entrances can only be linked to portal exits, and bidirectional portals can only be linked to other bidirectional portals. By default, portals will choose random destinations. You can link portals and change properties in the portal edit menu.

## Portal edit menu options:

1) Target - Choose a portal to link with (default is "Random")
2) Update Position - Press this to move the portal to your current position
3) Update Angles - The direction you want entities to be launched out
4) Exit speed - The speed you want entities to launch out of the portal (Default is "Same as input", which means no launching)
5) Rotate entities (Yes/No/Yaw only) - Whether to rotate entity look direction and velocity

# CVars:
```
as_command ps.autoload 1
```
`ps.autoload` disables auto-loading on map start if set to 0.

# Server Impact:

This plugin uses an inefficient method for detecting entities near portals. As you add more portals, you may notice an FPS drop or higher CPU usage. I'm planning to improve this if we ever get anything like an EntityCreated hook for AngelScript.

The maximum portals allowed (for now) is 32. Each teleport event creates 2 beam entitites that exist for half a second. The worst case scenario is that 96 extra entities exist at one time, which shouldn't be a problem unless a map is near 2000 entities (use cl_entitycount to check this).

4 sprites and 2 sounds are precached.

# Installation

1. Extract the `scripts` folder to your `svencoop_addon/` folder
1. Create a folder named `PortalSpawner` in `/svencoop/scripts/plugins/store/`.
   The plugin will save map portal files in here to keep your /store/ folder clean.
1. Add this to `svencoop/default_plugins.txt`
```
"plugin"
{
    "name" "PortalSpawner"
    "script" "PortalSpawner"
    "concommandns" "ps"
}
```