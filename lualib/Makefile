LUACJSON = lua-cjson-2.1.0
LUASOCKET = luasocket-2.0.2
DIRS = $(LUACJSON) $(LUASOCKET)

INCLUDE_DIR ?= $(CURDIR)/../src/obj/include/luajit-2.0
INSTALL_DIR ?= /usr/local/tsar/lualib

all:
	for i in $(DIRS); do tar --no-same-owner -zxf $$i.tar.gz; done
	make -C $(LUACJSON) LUA_INCLUDE_DIR=$(INCLUDE_DIR)
	make -C $(LUASOCKET) LUAINC=-I$(INCLUDE_DIR)

install:
	@make -C $(LUACJSON) LUA_CMODULE_DIR=$(INSTALL_DIR) LUA_MODULE_DIR=$(INSTALL_DIR) install
	@make -C $(LUASOCKET) INSTALL_TOP_SHARE=$(INSTALL_DIR) INSTALL_TOP_LIB=$(INSTALL_DIR) install

clean:
	for i in $(DIRS); do make -C $$i clean; done
