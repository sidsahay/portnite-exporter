# Portnite Exporter

Author: Siddharth Sahay

Design: Exports arbitrary animation from "any" asset format (COLLADA and FBX tested) using Assimp

Screen Shot:

![Screen Shot](screenshot.png)

How To Play:

On Mac/Linux, install Assimp as recommended.
For Windows, there's precompiled assimp (assimp.zip) included here. extract that to nest-libs/windows/.
You can use "dist/game" to read the animation directly from the asset file, or "dist/export" to output a directory called "skeletal" with the animations converted to flat buffers so any game that uses this doesn't need Assimp.

Note: will probably break horribly. You have been warned.

Sources: Open Asset-Importer-Lib (Assimp): https://www.assimp.org

This game was built with [NEST](NEST.md).

