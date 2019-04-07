# main compiler
# CC := gcc

INCDIR := include
SRCDIR := src
BUILDDIR := build
BINDIR := bin
LIBDIR := lib
PYLIBDIR := lib/python

SRCEXT := c
SOURCES := $(shell find $(SRCDIR) -maxdepth 1 -type f \( -iname "*.$(SRCEXT)" ! -iname "*main-*.$(SRCEXT)" \) )
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))

INC := $(shell find $(INCDIR) -maxdepth 1 -type d -exec echo -I {}  \;)

PYINC := "-I/usr/include/python2.7"
INC += $(PYINC)



LIB_FI2C_NAME := fasti2c
SOURCE_LIB_FI2C := $(SRCDIR)/lib/fast_i2c.$(SRCEXT)
OBJECT_LIB_FI2C := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCE_LIB_FI2C:.$(SRCEXT)=.o))
LIB_FI2C := lib$(LIB_FI2C_NAME).so

LIB_C2D_NAME := color2duty
SOURCE_LIB_C2D := $(SRCDIR)/lib/color2duty.$(SRCEXT)
OBJECT_LIB_C2D := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCE_LIB_C2D:.$(SRCEXT)=.o))
LIB_C2D := lib$(LIB_C2D_NAME).so

LIB_PCA_NAME := pcadriver
SOURCE_LIB_PCA := $(SRCDIR)/lib/pcadriver.$(SRCEXT)
OBJECT_LIB_PCA := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCE_LIB_PCA:.$(SRCEXT)=.o))
LIB_PCA := lib$(LIB_PCA_NAME).so




APP0 := rgbdriver
SOURCE_APP0 := $(SRCDIR)/main.$(SRCEXT)
OBJECT_APP0 := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCE_APP0:.$(SRCEXT)=.o))
LIB_APP0 := -lpcadriver -lfasti2c -lcolor2duty -lm -pthread -lz
TARGET_APP0 := $(APP0)





TARGET_LIBS = $(LIB_FI2C) $(LIB_C2D) $(LIB_PCA)
TARGET_APPS = $(TARGET_APP0)
LDFLAGS += -L$(LIBDIR)


BUILDROOT_PATH := /root/source

TOOLCHAIN_NAME := toolchain-mipsel_24kc_gcc-5.4.0_musl-1.1.16
TARGET_NAME := target-mipsel_24kc_musl-1.1.16

STAGING_DIR_RELATIVE := staging_dir
TOOLCHAIN_RELATIVE := $(STAGING_DIR_RELATIVE)/$(TOOLCHAIN_NAME)
TARGET_RELATIVE := $(STAGING_DIR_RELATIVE)/$(TARGET_NAME)

TOOLCHAIN := $(BUILDROOT_PATH)/$(TOOLCHAIN_RELATIVE)
TOOLCHAIN_BIN := $(BUILDROOT_PATH)/$(TOOLCHAIN_RELATIVE)/bin

TOOLCHAIN_INCLUDE := $(BUILDROOT_PATH)/$(TOOLCHAIN_RELATIVE)/include
TOOLCHAIN_LIB := $(BUILDROOT_PATH)/$(TOOLCHAIN_RELATIVE)/lib
TOOLCHAIN_USR_INCLUDE := $(BUILDROOT_PATH)/$(TOOLCHAIN_RELATIVE)/usr/include
TOOLCHAIN_USR_LIB := $(BUILDROOT_PATH)/$(TOOLCHAIN_RELATIVE)/usr/lib

# define the target paths
TARGET_B_RELATIVE := $(BUILDROOT_PATH)/$(TARGET_RELATIVE)

TARGET_INCLUDE := $(BUILDROOT_PATH)/$(TARGET_RELATIVE)/include
TARGET_LIB := $(BUILDROOT_PATH)/$(TARGET_RELATIVE)/lib
TARGET_USR_INCLUDE := $(BUILDROOT_PATH)/$(TARGET_RELATIVE)/usr/include
TARGET_USR_LIB := $(BUILDROOT_PATH)/$(TARGET_RELATIVE)/usr/lib

# define the compilers and such
CC := $(TOOLCHAIN_BIN)/mipsel-openwrt-linux-gcc
CXX := $(TOOLCHAIN_BIN)/mipsel-openwrt-linux-g++
LD := $(TOOLCHAIN_BIN)/mipsel-openwrt-linux-ld

TOOLCHAIN_AR := $(TOOLCHAIN_BIN)/mipsel-openwrt-linux-ar
TOOLCHAIN_RANLIB := $(TOOLCHAIN_BIN)/mipsel-openwrt-linux-ranlib

# define the FLAGS
INCLUDE_LINES := -I $(TOOLCHAIN_USR_INCLUDE) -I $(TOOLCHAIN_INCLUDE) -I $(TARGET_USR_INCLUDE) -I $(TARGET_INCLUDE)
TOOLCHAIN_CFLAGS := -Os -pipe -mno-branch-likely -mips32r2 -mtune=24kc -fno-caller-saves -fno-plt -fhonour-copts -Wno-error=unused-but-set-variable -Wno-error=unused-result -msoft-float -mips16 -minterlink-mips16 -Wformat -Werror=format-security -fstack-protector -D_FORTIFY_SOURCE=1 -Wl,-z,now -Wl,-z,relro
# TOOLCHAIN_CFLAGS := -Os -pipe -mno-branch-likely -mips32r2 -mtune=34kc -fno-caller-saves -fhonour-copts -Wno-error=unused-but-set-variable -Wno-error=unused-result -msoft-float -mips16 -minterlink-mips16 -fpic
TOOLCHAIN_CFLAGS := $(TOOLCHAIN_CFLAGS) $(INCLUDE_LINES)

TOOLCHAIN_CXXFLAGS := $(TOOLCHAIN_CFLAGS)
# TOOLCHAIN_CXXFLAGS := -Os -pipe -mno-branch-likely -mips32r2 -mtune=34kc -fno-caller-saves -fhonour-copts -Wno-error=unused-but-set-variable -Wno-error=unused-result -msoft-float -mips16 -minterlink-mips16 -fpic
TOOLCHAIN_CXXFLAGS := $(TOOLCHAIN_CXXFLAGS) $(INCLUDE_LINES)

TOOLCHAIN_LDFLAGS := -L$(TOOLCHAIN_USR_LIB) -L$(TOOLCHAIN_LIB) -L$(TARGET_USR_LIB) -L$(TARGET_LIB)



CFLAGS += $(TOOLCHAIN_CFLAGS) $(INC) -fPIC -pthread
LDFLAGS += $(TOOLCHAIN_LDFLAGS)
CXXFLAGS += $(TOOLCHAIN_CXXFLAGS)

all: $(TARGET1) $(TARGET_LIBS) $(TARGET_APPS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@echo " Compiling $@"
	@mkdir -p $(dir $@)
	@STAGING_DIR=$(BUILDROOT_PATH)/$(STAGING_DIR_RELATIVE) \
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB_FI2C): $(OBJECT_LIB_FI2C)
	@echo " Compiling $@"
	@mkdir -p $(LIBDIR)
	STAGING_DIR=$(BUILDROOT_PATH)/$(STAGING_DIR_RELATIVE) \
	$(CC) -shared -o $@  $^
	@cp $@ $(LIBDIR)/

$(LIB_C2D): $(OBJECT_LIB_C2D)
	@echo " Compiling $@"
	@mkdir -p $(LIBDIR)
	@STAGING_DIR=$(BUILDROOT_PATH)/$(STAGING_DIR_RELATIVE) \
	$(CC) -shared -o $@  $^
	@cp $@ $(LIBDIR)/

$(LIB_PCA): $(OBJECT_LIB_PCA)
	@echo " Compiling $@"
	@mkdir -p $(LIBDIR)
	@STAGING_DIR=$(BUILDROOT_PATH)/$(STAGING_DIR_RELATIVE) \
	$(CC) -shared -o $@  $^
	@cp $@ $(LIBDIR)/

$(TARGET_APP0): $(OBJECT_APP0) $(TARGET_LIBS)
	@echo " Compiling $@"
	@mkdir -p $(BINDIR)
	@STAGING_DIR=$(BUILDROOT_PATH)/$(STAGING_DIR_RELATIVE) \
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $(TARGET_APP0) $(LIB_APP0)
	@cp $@ $(BINDIR)/

deploy:
	@echo " Deploying to $(OMEGA_USER)@$(OMEGA_ADDR):$(OMEGA_DEPLOY_PATH)"
	rsync -rlptDvhe ssh --chmod=+x $(BINDIR)/* "$(OMEGA_USER)@$(OMEGA_ADDR):$(OMEGA_DEPLOY_PATH)/$(BINDIR)/"
	rsync -rlptDvhe ssh $(LIBDIR)/* "$(OMEGA_USER)@$(OMEGA_ADDR):$(OMEGA_DEPLOY_PATH)/$(LIBDIR)/"

clean:
	@echo " Cleaning..."
	@rm -rf $(BUILDDIR) $(BINDIR) $(LIBDIR)

.PHONY: clean deploy
