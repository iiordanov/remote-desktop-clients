#!/bin/bash

usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP"
  exit 1
}

PRJ="$1"

if [ "$PRJ" != "bVNC" -a "$PRJ" != "freebVNC" \
  -a "$PRJ" != "aSPICE" -a "$PRJ" != "freeaSPICE" \
  -a "$PRJ" != "aRDP" -a "$PRJ" != "freeaRDP" ]
then
  usage
fi

ln -sf AndroidManifest.xml.$PRJ AndroidManifest.xml

generated_files="AbstractConnectionBean.java AbstractMetaKeyBean.java MetaList.java MostRecentBean.java SentTextBean.java"
for f in $generated_files
do
  rm -f src/com/iiordanov/*/$f
done

echo
echo "Now please switch to your IDE, select the bVNC project, refresh with F5,"
echo "clean and rebuild it to auto-generate the DAO objects with sqlitegen."
echo
echo "You must have sqlitegen installed as per the BUILDING file."
echo
echo "When the build in your IDE completes, switch back to this terminal and"
echo "press ENTER key for this script to continue executing."
echo
read CONTINUE

generated_files="AbstractConnectionBean.java AbstractMetaKeyBean.java MetaList.java MostRecentBean.java SentTextBean.java"

for f in $generated_files
do
  file=gen/com/iiordanov/$PRJ/$f
  if [ ! -f $file ]
  then
    echo "Could not find auto-generated file $file. Please try cleaning / rebuilding the project, and make sure sqlitegen is installed properly."
    echo
    exit 2
  else
    echo "Moving $f to src/com/iiordanov/$PRJ"
    mv $file src/com/iiordanov/$PRJ/
  fi
done

echo
echo "Now you can go back to your IDE and install $PRJ to your device, etc."
echo
