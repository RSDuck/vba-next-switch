
all: pkgcrypt.so

pkgcrypt.so: crypt.c setup.py
	@python setup.py build --compiler=mingw32
	@cp build/lib.*/pkgcrypt.so .

clean:
	rm -rf build pkgcrypt.so
