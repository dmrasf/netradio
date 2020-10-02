RM = rm -rf
SDIR = ./src/server
CDIR = ./src/client
STARGET = server
CTARGET = client

all:
	cd $(SDIR) && make
	mv $(SDIR)/$(STARGET) .
	cd $(SDIR) && make clean
	cd $(CDIR) && make
	mv $(CDIR)/$(CTARGET) .
	cd $(CDIR) && make clean

clean:
	$(RM) $(STARGET) $(CTARGET)
