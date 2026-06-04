#---------------------------------------------------------------------------------------------------------------------
# Spronk Quest — GBA ROM build (Butano on devkitARM).
# Build with devkitPro's MSYS2 environment: `bash tools/build_rom.sh` (wraps this).
# See butano/template/Makefile for the full variable documentation.
#---------------------------------------------------------------------------------------------------------------------
TARGET      	:=  SpronkQuest
BUILD       	:=  build
LIBBUTANO   	:=  butano/butano
PYTHON      	:=  python
SOURCES     	:=  src src/logic src/engine src/game butano/common/src
INCLUDES    	:=  include butano/common/include
DATA        	:=
GRAPHICS    	:=  graphics butano/common/graphics
AUDIO       	:=
AUDIOBACKEND	:=  maxmod
AUDIOTOOL		:=
DMGAUDIO    	:=
DMGAUDIOBACKEND	:=  default
ROMTITLE    	:=  SPRONK QUEST
ROMCODE     	:=  SPRK
USERFLAGS   	:=
USERCXXFLAGS	:=
USERASFLAGS 	:=
USERLDFLAGS 	:=
USERLIBDIRS 	:=
USERLIBS    	:=
DEFAULTLIBS 	:=
STACKTRACE		:=
USERBUILD   	:=
EXTTOOL     	:=

#---------------------------------------------------------------------------------------------------------------------
# Export absolute butano path:
#---------------------------------------------------------------------------------------------------------------------
ifndef LIBBUTANOABS
	export LIBBUTANOABS	:=	$(realpath $(LIBBUTANO))
endif

#---------------------------------------------------------------------------------------------------------------------
# Include main makefile:
#---------------------------------------------------------------------------------------------------------------------
include $(LIBBUTANOABS)/butano.mak
