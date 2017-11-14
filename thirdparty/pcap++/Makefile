ifeq ($(wildcard mk/platform.mk),)
  $(error platform.mk not found! Please run configure script first)
endif

include mk/platform.mk

COMMONPP_HOME        := Common++
PACKETPP_HOME        := Packet++
PCAPPP_HOME          := Pcap++


UNAME := $(shell uname)


.SILENT:

all: libs
	@$(MKDIR) -p Dist/mk
	$(CP) mk/PcapPlusPlus.mk ./Dist/mk
	@echo 'Finished successfully building PcapPlusPlus'

# PcapPlusPlus libs only
libs:
	@$(RM) -rf Dist
	@cd $(COMMONPP_HOME)             && $(MAKE) all
	@cd $(PACKETPP_HOME)             && $(MAKE) all
	@cd $(PCAPPP_HOME)               && $(MAKE) all
	@$(MKDIR) -p Dist
	@$(MKDIR) -p Dist/header
	@$(CP) $(COMMONPP_HOME)/Lib/Release/* ./Dist
	@$(CP) $(PACKETPP_HOME)/Lib/* ./Dist
	@$(CP) $(PCAPPP_HOME)/Lib/* ./Dist
	@$(CP) $(COMMONPP_HOME)/header/* ./Dist/header
	@$(CP) $(PACKETPP_HOME)/header/* ./Dist/header
	@$(CP) $(PCAPPP_HOME)/header/* ./Dist/header
	@echo 'Finished successfully building PcapPlusPlus libs'
	@echo ' '

# Clean
clean:
	@cd $(COMMONPP_HOME)             && $(MAKE) clean
	@cd $(PACKETPP_HOME)             && $(MAKE) clean
	@cd $(PCAPPP_HOME)               && $(MAKE) clean
	@cd $(PACKETPP_TEST)             && $(MAKE) clean
	@cd $(PCAPPP_TEST)               && $(MAKE) clean
	@$(RM) -rf Dist
	@echo 'Finished successfully cleaning PcapPlusPlus'

ifndef WIN32
INSTALL_DIR=Dist

# Install
install: | $(INSTALL_DIR)
	@cd Dist && ../mk/$(INSTALL_SCRIPT)
	@echo 'Installation complete!'

# Uninstall
uninstall: | $(INSTALL_DIR)
	@cd Dist && ../mk/$(UNINSTALL_SCRIPT)
	@echo 'Uninstallation complete!'

$(INSTALL_DIR):
	@echo 'Please run make all first' && exit 1

endif
