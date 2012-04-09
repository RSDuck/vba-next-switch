#!/bin/sh
FORMAT=libretro
START_DIR=`pwd`
BUILD_DIR_EXECUTABLES=`pwd`/wii/pkg

#******************
# PROGRAM FUNCTIONS
#******************

function clean()
{
	make -f Makefile.libretro clean
}

function make_libretro()
{
	make -f Makefile.libretro
}

function make_libretro_debug()
{
	make -f Makefile.libretro DEBUG=1
}

#******************
# DISPLAY FUNCTIONS
#******************

function title()
{
	echo ""
	echo "***********************"
	echo "COMPILER SCRIPT FOR $FORMAT"
	echo "***********************"
}

function display_clean()
{
	echo "clean          Clean the object files"
}

function display_make()
{
	echo "make           Compile libretro library"
}


function display_make_debug()
{
	echo "make_debug     Compile DEBUG libretro library "
}

function display_all_options()
{
	display_clean
	display_make
	display_make_debug
}

function display_usage()
{
	echo "Usage: compile_libretro.sh [options]"
	echo "Options:"
	display_all_options
}

#***********************
# MAIN CONTROL FLOW LOOP
#***********************

title
if [ ! -n "$1" ]; then
	display_usage
else
	for i in "$@"
	do
		if [ "$i" = "help" ]; then
			display_usage
		fi
		if [ "$i" = "clean" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_clean
			echo "*************************************"
			clean
		fi
		if [ "$i" = "make" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_make
			echo "*************************************"
			make_libretro
		fi
		if [ "$i" = "make_debug" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_make_debug
			echo "*************************************"
			make_libretro_debug
		fi
	done
fi
