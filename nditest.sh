#!/bin/bash

# SPDX-FileCopyrightText: 2024 Vizrt NDI AB
# SPDX-License-Identifier: MIT

#set -x

function ndi2mov () {
	echo "ndirx -s \"NDITEST (nditx)\" -c ${COUNT}"
	echo "ffmpeg -y -f rawvideo -vcodec rawvideo -pix_fmt p216le -s ${WIDTH}x${HEIGHT} -r ${RATE_N}/${RATE_D} -i - -movflags write_colr -c:v v210 -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range tv -metadata:s:v:0 \"encoder=Uncompressed 10-bit 4:2:2\" -c:a copy -vf setdar=16/9 -f mov $1"
	ndirx -s "NDITEST (nditx)" -c ${COUNT} | \
	ffmpeg -y -f rawvideo -vcodec rawvideo -pix_fmt p216le -s ${WIDTH}x${HEIGHT} -r ${RATE_N}/${RATE_D} -i - -movflags write_colr -c:v v210 -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range tv -metadata:s:v:0 "encoder=Uncompressed 10-bit 4:2:2" -c:a copy -vf setdar=16/9 -f mov $1
}

function mov2ndi () {
	echo "ffmpeg -i $1 -f image2pipe -vcodec rawvideo -pix_fmt p216le -"
	echo "nditx -m nditest -r ${RATE_N}/${RATE_D} -b ${BITRATE}  -s ${SHQMODE} -c ${COUNT} -w"
	ffmpeg -i $1 -f image2pipe -vcodec rawvideo -pix_fmt p216le - | \
	nditx -m nditest -r ${RATE_N}/${RATE_D} -b ${BITRATE}  -s ${SHQMODE} -c ${COUNT} -w

}

function isInt () {
	if [ -n "${1//[-0-9]/}" ] ; then
		echo "$1 does not look like an integer!"
		exit 1
	fi
}

function usage () {
	echo "Usage:"
	echo "$0 [-b bitrate]  [-s SHQ Mode] [-c framecount] [-g generations] -i inputfile -o output_base"
	echo "    bitrate : bitrate multiplier percent (default 100)"
	echo "    framecount : number of frames to transmit (default length of input clip)"
	echo "    generations : number of generations to process (default 1)"
	echo "    inputfile   : input v210 mov file"
	echo "    output_base : base name of output files, files will be named: output_base.genNN.mov"
}

# Default settings
BITRATE=100
COUNT=""
GENERATION=1
INPUT=""
OUTPUT=nditest.v210
SHQMODE=auto

OPTSTRING="b:c:g:i:o:s:"

while getopts ${OPTSTRING} opt; do
	case ${opt} in
		b) isInt ${OPTARG} ; BITRATE=${OPTARG} ;;
		c) isInt ${OPTARG} ; COUNT=${OPTARG} ;;
		g) isInt ${OPTARG} ; GENERATION=${OPTARG} ;;
		i) INPUT=${OPTARG} ;;
		o) OUTPUT=${OPTARG} ;;
		s) SHQMODE=${OPTARG} ;;
		?) echo "Argument parsing failed"
		   usage
		   exit 1
		   ;;
	esac
done


if [ -z "${INPUT}" -o ! -r "${INPUT}" ] ; then
	echo "Cannot read input file ${INPUT}"
	if [ -z "${INPUT}" ] ; then
		echo "Try: -i <input file>"
	fi
	exit 1
fi

case ${SHQMODE} in
	auto)	;;
	4:2:0)	;;
	4:2:2)	;;
	*)	echo "Unknown SpeedHQ mode!"
		echo "Try 4:2:2, 4:2:0, or auto"
		exit 1
		;;
esac

if [ $GENERATION -le 0 ] ; then
	echo "You must specify a positive integer number of generations!"
	exit 1
fi

WIDTH=`mediainfo --Inform='Video;%Width%' "${INPUT}"`
HEIGHT=`mediainfo --Inform='Video;%Height%' "${INPUT}"`
RATE_N=`mediainfo --Inform='Video;%FrameRate_Num%' "${INPUT}"`
RATE_D=`mediainfo --Inform='Video;%FrameRate_Den%' "${INPUT}"`

if [ -z "${COUNT}" ] ; then
	# No count specified as a command-line option,
	# so use the length of the input video
	COUNT=`mediainfo --Inform='Video;%FrameCount%' "${INPUT}"`
fi

echo "Input: ${COUNT} ${WIDTH}x${HEIGHT} frames @ ${RATE_N}/${RATE_D} fps"

GEN=1
SRC=${INPUT}
DST=$(printf "${OUTPUT}.gen%02i.mov" $GEN)

while [ $GEN -le ${GENERATION} ] ; do
	echo -n "Prossessing generation $GEN: "

	# Launch NDI receive process in the background
	ndi2mov ${DST} > ${DST}.ndirx.log 2>&1 &

	# Launch NDI send process and wait for it to finish
	mov2ndi ${INPUT} > ${SRC}.nditx.log 2>&1

	# Wait for the NDI receive process to finish
	wait

	# Make sure we recoreded the expected number of frames
	LEN=`mediainfo --Inform='Video;%FrameCount%' ${DST}`
	if [ $LEN -ne $COUNT ] ; then
		echo "Unexpected frame count ${LEN}, retrying"
		continue
	else
		echo "Done"
	fi

	let GEN+=1
	SRC=${DST}
	DST=$(printf "${OUTPUT}.gen%02i.mov" $GEN)

done
