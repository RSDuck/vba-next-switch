#!/bin/sh
FORMAT=PS3
BUILD_DIR_EXECUTABLES=`pwd`/ps3/pkg

#******************
# PROGRAM FUNCTIONS
#******************

function clean()
{
	make -f Makefile.ps3 clean
}

function clean_multiman()
{
	make -f ps3/Makefile-eboot-launcher clean MULTIMAN_SUPPORT=1
	clean_elf
	if [ -f "$BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.BIN" ] ; then
		echo "DELETE: $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.BIN exists, deleting.."
		rm $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.BIN
	else
		echo "SKIP: $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.BIN doesn't exist."
	fi
	make -f Makefile.ps3 clean MULTIMAN_SUPPORT=1
}

function clean_elf()
{
	if [ -f "$BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.ELF" ] ; then
		echo "DELETE: $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.ELF exists, deleting.."
		rm $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.ELF
	else
		echo "SKIP: $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.ELF doesn't exist."
	fi

	if [ -f "$BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.self" ] ; then
		echo "DELETE: $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.self exists, deleting.."
		rm $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.self
	else
		echo "SKIP: $BUILD_DIR_EXECUTABLES/USRDIR/EBOOT.self doesn't exist."
	fi

	if [ -f "$BUILD_DIR_EXECUTABLES/USRDIR/RELOAD.SELF" ] ; then
		echo "DELETE: $BUILD_DIR_EXECUTABLES/USRDIR/RELOAD.SELF exists, deleting.."
		rm $BUILD_DIR_EXECUTABLES/USRDIR/RELOAD.SELF
	else
		echo "SKIP: $BUILD_DIR_EXECUTABLES/USRDIR/RELOAD.SELF doesn't exist."
	fi
}

function make_ps3()
{
	make -f Makefile.ps3
}

function make_ps3_multiman()
{
	make -f ps3/Makefile-eboot-launcher
	make -f Makefile.ps3 MULTIMAN_SUPPORT=1
}

function make_ps3_pkg()
{
	make -f Makefile.ps3 pkg
}

function make_ps3_pkg_multiman()
{
	make -f ps3/Makefile-eboot-launcher pkg MULTIMAN_SUPPORT=1
	clean_elf
	make -f Makefile.ps3 pkg-signed MULTIMAN_SUPPORT=1
}

function make_ps3_eboot()
{
	make -f ps3/Makefile-eboot-launcher
}

function debug_elf()
{
	if [ -f "`pwd`/vba.ppu.elf" ] ; then
		ppu-addr2line -e vba.ppu.elf -f
	else
		echo "ERROR: ELF does not exist"
	fi
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
	echo "clean		Clean the object files"
}

function display_clean_multiman()
{
	echo "clean_multiman	Clean the object files (multiMAN-only"
}

function display_make()
{
	echo "make		Compile PS3 version"
}

function display_make_multiman()
{
	echo "make_multiman   Compile PS3 version (multiMAN-only)"
}

function display_make_eboot()
{
	echo "make_eboot	Compile PS3 eboot (multiMAN-only)"
}

function display_pkg()
{
	echo "pkg		Create PS3 PKG"
}

function display_pkg_multiman()
{
	echo "pkg_multiman	Create PS3 PKG (multiMAN-only)"
}

function display_clean_elf()
{
	echo "clean_elf       Clean pre-existing ELF binaries"
}

function display_debug_elf()
{
	echo "debug_elf       Debug ELF"
}

function display_all_options()
{
	display_clean
	display_clean_multiman
	display_clean_elf
	display_debug_elf
	display_make
	display_make_eboot
	display_make_multiman
	display_pkg
	display_pkg_multiman
}

function display_usage()
{
	echo "Usage: compile_ps3.sh [options]"
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
		if [ "$i" = "clean_elf" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_clean_elf
			echo "*************************************"
			clean_elf
		fi
		if [ "$i" = "clean_multiman" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_clean_multiman
			echo "*************************************"
			clean_multiman
		fi
		if [ "$i" = "debug_elf" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_debug_elf
			echo "*************************************"
			debug_elf
		fi
		if [ "$i" = "make" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_make
			echo "*************************************"
			make_ps3
		fi
		if [ "$i" = "make_multiman" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_make_multiman
			echo "*************************************"
			make_ps3_multiman
		fi
		if [ "$i" = "make_eboot" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_make_eboot
			echo "*************************************"
			make_ps3_eboot
		fi
		if [ "$1" = "pkg" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_pkg
			echo "*************************************"
			make_ps3_pkg
		fi
		if [ "$1" = "pkg_multiman" ]; then
			echo ""
			echo "*************************************"
			echo "DOING:"
			display_pkg_multiman
			echo "*************************************"
			make_ps3_pkg_multiman
		fi
	done
fi
