# lib0stor (0-stor legacy library)

This is the library used to split and encrypt files on Zero-OS backend.

## Workflow
- Take a file
- Chunk this file (512 KB by default)
- Hash each chunk using blake2b hash (16 bytes)
- Encrypt each chunk using `xxtea` (using the blake2 hash as key)
- Hash this encrypted chunk (using blake2)
- Return the chunks with hash and key

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

Install dependencies: `libb2-dev snappy zlib python`

On ubuntu, you can install them via: `apt-get install build-essential libz-dev libb2-dev python3-dev libsnappy-dev`

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
python3 setup.py install
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
