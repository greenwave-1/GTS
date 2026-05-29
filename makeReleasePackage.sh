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

make VERSION=$1

release_folder="release_$1"

mkdir -p $release_folder/wii/apps/GTS
mkdir $release_folder/elfs

cp wii-homebrew-channel-data/meta.xml $release_folder/wii/apps/GTS/
cp wii-homebrew-channel-data/icon.png $release_folder/wii/apps/GTS/

mv GTS_WII.dol $release_folder/wii/apps/GTS/boot.dol
mv GTS_GC.dol $release_folder/GTS_GC.dol

mv GTS_GC.elf $release_folder/elfs/
mv GTS_WII.elf $release_folder/elfs/

sed -i 's/VERSION/'$1'/g' $release_folder/wii/apps/GTS/meta.xml
sed -i 's/DATE/'$DATE'/g' $release_folder/wii/apps/GTS/meta.xml

make clean

cd $release_folder/wii/
zip ../wii.zip apps/GTS/*
