`textures.scf` defines what will be compiled into the binary

Image dimensions should be a multiple of 4. Round up and fill with alpha if necessary.

example: `<filepath="font.png" id="pvfont" colfmt=4 />`

filepath - self explanatory

id - the name that you can pass to TPL_GetTexture(tpl object, **the id**, destination texture object)

colfmt - color format, good table here:
https://wiki.tockdom.com/wiki/Image_Formats#Image_Formats