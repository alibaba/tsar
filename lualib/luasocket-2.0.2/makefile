#------
# Load configuration
#
include config

#------
# Hopefully no need to change anything below this line
#
INSTALL_SOCKET_SHARE=$(INSTALL_TOP_SHARE)/socket
INSTALL_SOCKET_LIB=$(INSTALL_TOP_LIB)/socket
INSTALL_MIME_SHARE=$(INSTALL_TOP_SHARE)/mime
INSTALL_MIME_LIB=$(INSTALL_TOP_LIB)/mime

all clean:
	cd src; $(MAKE) $@

#------
# Files to install
#
TO_SOCKET_SHARE:= \
	http.lua \
	url.lua \
	tp.lua \
	ftp.lua \
	smtp.lua

TO_TOP_SHARE:= \
	ltn12.lua \
	socket.lua \
	mime.lua

TO_MIME_SHARE:= 

#------
# Install LuaSocket according to recommendation
#
install: all
	cd src; mkdir -p $(INSTALL_TOP_SHARE)
	cd src; $(INSTALL_DATA) $(TO_TOP_SHARE) $(INSTALL_TOP_SHARE)
	cd src; mkdir -p $(INSTALL_SOCKET_SHARE)
	cd src; $(INSTALL_DATA) $(TO_SOCKET_SHARE) $(INSTALL_SOCKET_SHARE)
	cd src; mkdir -p $(INSTALL_SOCKET_LIB)
	cd src; $(INSTALL_EXEC) $(SOCKET_SO) $(INSTALL_SOCKET_LIB)/core.$(EXT)
	#cd src; mkdir -p $(INSTALL_MIME_SHARE)
	#cd src; $(INSTALL_DATA) $(TO_MIME_SHARE) $(INSTALL_MIME_SHARE)
	cd src; mkdir -p $(INSTALL_MIME_LIB)
	cd src; $(INSTALL_EXEC) $(MIME_SO) $(INSTALL_MIME_LIB)/core.$(EXT)

#------
# End of makefile
#
