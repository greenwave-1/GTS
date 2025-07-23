#!/bin/bash
# this script will make a zip for uploading to github as a release.
# this script expects to be ran from the root directory of the project,
# and expects 1 argument, a version string

if [ $# -ne 1 ]
then
  echo "Provide a version number and nothing else"
  exit 1
fi

#FILENAME=`basename "$PWD"`
DATE=`date -I`

make release version=$1 -j$(nproc)

mkdir -p release/wii/apps/GTS
cp wii-homebrew-channel-data/meta.xml release/wii/apps/GTS/
cp wii-homebrew-channel-data/icon.png release/wii/apps/GTS/

mv GTS_wii.dol release/wii/apps/GTS/boot.dol
mv GTS_gc.dol release/GTS_GC.dol

sed -i 's/VERSION/'$1'/g' release/wii/apps/GTS/meta.xml
sed -i 's/DATE/'$DATE'/g' release/wii/apps/GTS/meta.xml

make clean

cd release/wii/
zip ../wii.zip apps/GTS/*