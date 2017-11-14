# libg8stor (0-stor legacy library)
```bash
git clone https://github.com/zero-os/lib0stor
cd lib0stor
```

Prepare dependencies
```bash
git submodule init
git submodule update
```

Install dependencies (all devel): `ssl snappy zlib python`

Compile library
```bash
cd src
make clean && make
```

Compile command-line tools
```bash
cd ../tools
make clean && make
```

Ensure library works with command-line
```bash
dd if=/dev/urandom of=/tmp/libstor-source bs=1M count=8
./g8stor-cli /tmp/libstor-source /tmp/libstor-verify
md5sum /tmp/libstor-*
```
