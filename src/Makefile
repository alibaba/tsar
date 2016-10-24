CC = gcc
SRC = tsar.c config.c debug.c framework.c output_file.c output_print.c output_db.c output_tcp.c output_nagios.c common.c tsar_lua_util.c
OBJ = $(patsubst %.c, %.o,$(SRC))
DEPS_DIR = deps

BIN = tsar
LUAJIT = $(notdir $(patsubst %.tar.gz,%,$(wildcard deps/LuaJIT*.tar.gz)))

ODIR = obj
DEPS = $(ODIR)/lib/libluajit-5.1.a

CFLAGS = -g -O2 -Wall -I../include -I$(ODIR)/include
LDFLAGS += -L$(ODIR)/lib -lluajit-5.1 -lm -ldl -rdynamic

all: $(BIN)

$(BIN): $(OBJ)
	@echo LINK $(BIN)
	$(CC) $^ -o tsar -I$(INCLUDE_DIR) $(CFLAGS) $(LDFLAGS)

$(OBJ): $(DEPS) | $(ODIR)

$(ODIR)/$(LUAJIT): $(DEPS_DIR)/$(LUAJIT).tar.gz | $(ODIR)
	@tar -C $(ODIR) -xf $<

$(ODIR)/lib/libluajit-5.1.a: $(ODIR)/$(LUAJIT) | $(ODIR)
	@echo Building LuaJIT...
	@$(MAKE) -C $< PREFIX=$(abspath $(ODIR)) BUILDMODE=static install

$(ODIR)/%.o : %.c
	@echo CC $<
	@$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR):
	@mkdir -p $@

clean:
	rm -rf *.o $(BIN) $(ODIR);
