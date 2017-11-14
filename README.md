# lib0stor (0-stor legacy library)

This is a legacy version of the library used to split and encrypt files on Zero-OS backend

## Dependencies
```bash
git clone https://github.com/maxux/lib0stor
cd lib0stor
```

Prepare dependencies
```bash
git submodule init
git submodule update
```

Install dependencies: `ssl snappy zlib python`

On ubuntu, you can install them via: `apt-get install build-essential libz-dev libssl-dev python3-dev libsnappy-dev`

## Compilation

The all-in-one compilation can be done with a simple `make` in the root directory.

### Library
```
make -C src
```

### Python binding
```
cd python
python3 setup.py build
cd ..
```

### Command Line Tool
```
make -C tool
```

## Test
You can ensure the library works as expected using the command-line tool:
```bash
# Generate random file
dd if=/dev/urandom of=/tmp/libstor-source bs=1M count=8

# Encrypt and decrypt the file
./g8stor-cli /tmp/libstor-source /tmp/libstor-verify

# Compare original and proceed version
md5sum /tmp/libstor-*
```
