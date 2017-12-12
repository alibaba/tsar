LUACJSON = lua-cjson
LUASOCKET = luasocket-2.0.2
DIRS = $(LUACJSON) $(LUASOCKET)

all:
	for i in $(DIRS); do tar --no-same-owner -zxf $$i.tar.gz; done
	for i in $(DIRS); do make -C $$i; done

install:
	@make -C $(LUACJSON) LUA_CMODULE_DIR=/usr/local/tsar/modules LUA_MODULE_DIR=/usr/local/tsar/modules install
	@make -C $(LUASOCKET) INSTALL_TOP_SHARE=/usr/local/tsar/lualib INSTALL_TOP_LIB=/usr/local/tsar/lualib install

clean:
	for i in $(DIRS); do make -C $$i clean; done
