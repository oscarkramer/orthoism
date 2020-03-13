#!/bin/bash

inputImage=$1
outputImage=$2

ORTHOISM_HOME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
#ORTHOISM_EXE=/home/okramer/dev/orthoism/build/bin/foo
ORTHOISM_EXE="orthoism"
ORTHOISM_ICON=$ORTHOISM_HOME/pslogo.png

function launchZenity()
{
  zenity --progress \
  --title="ORTHOISM" --window-icon=$ORTHOISM_ICON \
  --text="Orthorectifying '$inputImage'" \
  --percentage=0 \
  --width=1000
  RESULT=$?
  if [[ "$RESULT" == 1 ]] ; then
    OPID=`pgrep -n $ORTHOISM_EXE`
    echo "Detected Cancel. Killing process $OPID"
    kill -9 $OPID
    pgrep $ORTHOISM_EXE
    zenity --error \
      --title="ORTHOISM" --window-icon=$ORTHOISM_ICON \
      --text="Process canceled." \
      --width=500
  elif [[ "$RESULT" != 0 ]]; then
    zenity --error \
      --title="ORTHOISM" --window-icon=$ORTHOISM_ICON \
      --text="Run-time error encountered." \
      --width=500
  fi
}

if [ -z "$inputImage" ]; then
  zenity --error \
    --title="ORTHOISM" --window-icon=$ORTHOISM_ICON \
    --text="No file names specified. Please provide input and output file names." \
    --width=500
  exit 1
fi
if [ ! -f "$inputImage" ]; then
  zenity --error \
    --title="ORTHOISM" --window-icon=$ORTHOISM_ICON \
    --text="Input file '$inputImage' not found. Check the path and try again." \
    --width=500
  exit 1
fi
if [ -z "$outputImage" ]; then
  zenity --error \
    --title="ORTHOISM" --window-icon=$ORTHOISM_ICON \
    --text="An output file name must be specified." \
    --width=500
  exit 1
fi

($ORTHOISM_EXE -z "$inputImage" "$outputImage") | tee /dev/tty | launchZenity

#$ORTHOISM_EXE "$inputImage" "$outputImage"

exit 0
