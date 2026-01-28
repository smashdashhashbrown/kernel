#!/usr/bin/env bash

DOCKER_NAME=kerbuntu:12.04
WORK_DIR=/build

declare -i SHIFT_COUNT
SHIFT_COUNT=0

helpFunction()
{
   echo ""
   echo "Usage: $0 -D build_dir -I docker_image -W working_dir <command to run>"
   echo -e "\t-D directory to mount into /build"
   echo -e "\t-I Docker image and tag to use and build within (default: kerbuntu:12.04)"
   echo -e "\t-W working directory within docker (default: /build)"
   exit 1 # Exit script after printing help
}

while getopts "D:I:W:" opt
do
    case "$opt" in
        D ) BUILD_DIR="$OPTARG" && SHIFT_COUNT+=2 ;;
        I ) DOCKER_NAME="$OPTARG" && SHIFT_COUNT+=2 ;;
        W ) WORK_DIR="$OPTARG" && SHIFT_COUNT+=2 ;;
        ? ) helpFunction ;;
    esac
done

# Print helpFunction in case parameters are empty
if [ -z "$BUILD_DIR" ] || [ -z "$DOCKER_NAME" ] || [ -z "$WORK_DIR" ]
then
    echo "Necessary parameters are empty"
    helpFunction
fi

for i in $(seq 1 $SHIFT_COUNT)
do
    shift
done

docker run --rm -it -v $BUILD_DIR:/build -w $WORK_DIR $DOCKER_NAME $@
