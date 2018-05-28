all:
	$(MAKE) -C src
	$(MAKE) -C python distutils
	$(MAKE) -C tools

install:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean
	$(MAKE) -C python clean
	$(MAKE) -C tools clean

mrproper:
	$(MAKE) -C src mrproper
	$(MAKE) -C python mrproper
	$(MAKE) -C tools mrproper
