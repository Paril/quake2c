# What is this?
It's 1996; Quake II is deep in development. They come to the realization that QC is quite limited and slow, and they consider moving to a native binary library to handle their game mods... except, they don't. They instead decide to improve QC and continue to build their next big game using QC as the game code handler.

That's the alternate reality I have constructed here. An alternate reality where Carmack & Co have switched to C++20 (somehow). Take a peek into a dimension full of VMs!

# Oh.. but why?
This started as a joke in the Quake Mapping & Quake Legacy Discords. I had an idea to construct a VM inside of a Q2 game DLL and natively load Quake 1 progs.dat's to be able to run Quake 1 mods inside of Quake 2. Sadly that idea never made it beyond initial testing (although it might still be mostly possible!!), but I then decided to simply convert Quake 2's code to QC and load that instead.

Those more familiar with QC can jump on in and create mods for Quake II without needing to touch any C code at all.

# I wanna make a progs - how do?
See the https://github.com/Paril/quake2c-progs repo for the actual progs source - this is just the game DLL!
