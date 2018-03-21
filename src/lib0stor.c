#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <snappy-c.h>
#include <zlib.h>
#include <math.h>
#include <time.h>
#include "openssl/sha.h"
#include "xxtea.h"
#include "lib0stor.h"

#define CHUNK_SIZE    1024 * 512    // 512 KB
#define SHA256LEN     (size_t) SHA256_DIGEST_LENGTH * 2

//
// buffer manager
//
static size_t file_length(FILE *fp) {
    size_t length;

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);

    fseek(fp, 0, SEEK_SET);

    return length;
}

static ssize_t file_load(char *filename, buffer_t *buffer) {
    if(!(buffer->fp = fopen(filename, "r"))) {
        perror("[-] fopen");
        return -1;
    }

    buffer->length = file_length(buffer->fp);
    printf("[+] filesize: %lu bytes\n", buffer->length);

    if(buffer->length == 0)
        return 0;

    if(!(buffer->data = malloc(sizeof(char) * buffer->chunksize))) {
        perror("[-] malloc");
        return 0;
    }

    return buffer->length;
}

buffer_t *bufferize(char *filename) {
    buffer_t *buffer;

    if(!(buffer = calloc(1, sizeof(buffer_t)))) {
        perror("[-] malloc");
        return NULL;
    }

    buffer->chunksize = CHUNK_SIZE;
    if(file_load(filename, buffer) < 0)
        return NULL;

    // file empty, nothing to do.
    if(buffer->length == 0) {
        fprintf(stderr, "[-] file is empty, nothing to do.\n");
        free(buffer);
        return NULL;
    }

    buffer->chunks = ceil(buffer->length / (float) buffer->chunksize);

    // if the file is smaller than a chunks, hardcoding 1 chunk.
    if(buffer->length < buffer->chunksize)
        buffer->chunks = 1;

    return buffer;
}

buffer_t *buffer_writer(char *filename) {
    buffer_t *buffer;

    if(!(buffer = calloc(1, sizeof(buffer_t)))) {
        perror("[-] malloc");
        return NULL;
    }

    if(!(buffer->fp = fopen(filename, "w"))) {
        perror("[-] fopen");
        free(buffer);
        return NULL;
    }

    return buffer;
}

const unsigned char *buffer_next(buffer_t *buffer) {
    // resize chunksize if it's smaller than the remaining
    // amount of data
    if(buffer->current + buffer->chunksize > buffer->length)
        buffer->chunksize = buffer->length - buffer->current;

    // loading this chunk in memory
    if(fread(buffer->data, buffer->chunksize, 1, buffer->fp) != 1) {
        perror("[-] fread");
        return NULL;
    }

    buffer->current += buffer->chunksize;

    return (const unsigned char *) buffer->data;
}

void buffer_free(buffer_t *buffer) {
    fclose(buffer->fp);
    free(buffer->data);
    free(buffer);
}

//
// hashing
//
static char *sha256hex(unsigned char *hash) {
    char *buffer = calloc((SHA256_DIGEST_LENGTH * 2) + 1, sizeof(char));

    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf((char *) buffer + (i * 2), "%02x", hash[i]);

    return buffer;
}

static char *sha256(const unsigned char *buffer, size_t length) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;

    SHA256_Init(&sha256);
    SHA256_Update(&sha256, buffer, length);
    SHA256_Final(hash, &sha256);

    return sha256hex(hash);
}

//
// chunks manager
//
chunk_t *chunk_new(char *id, char *cipher, unsigned char *data, size_t length) {
    chunk_t *chunk;

    if(!(chunk = malloc(sizeof(chunk_t)))) {
        perror("[-] malloc");
        return NULL;
    }

    chunk->id = id;
    chunk->cipher = cipher;

    chunk->data = data;
    chunk->length = length;

    return chunk;
}

void chunk_free(chunk_t *chunk) {
    free(chunk->id);
    free(chunk->cipher);
    free(chunk->data);
    free(chunk);
}

//
// encryption and decryption
//
// encrypt a buffer
// returns a chunk with key, cipher, data and it's length
chunk_t *encrypt_chunk(const unsigned char *chunk, size_t chunksize) {
    // hashing this chunk
    char *hashkey = sha256(chunk, chunksize);
    printf("[+] chunk hash: %s\n", (char *) hashkey);

    //
    // compress
    //
    size_t output_length = snappy_max_compressed_length(chunksize);
    char *compressed = (char *) malloc(output_length);
    if(snappy_compress((char *) chunk, chunksize, compressed, &output_length) != SNAPPY_OK) {
        fprintf(stderr, "[-] snappy compression error\n");
        exit(EXIT_FAILURE);
    }

    // printf("Compressed size: %lu\n", output_length);

    //
    // encrypt
    //
    size_t encrypt_length;
    unsigned char *encrypt_data = xxtea_encrypt(compressed, output_length, hashkey, &encrypt_length);

    char *hashcrypt = sha256(encrypt_data, encrypt_length);
    printf("[+] encrypted hash: %s\n", (char *) hashcrypt);

    //
    // data checksum
    //
    unsigned long ucrc = crc32(0L, Z_NULL, 0);
    ucrc = crc32(ucrc, encrypt_data, encrypt_length);
    // printf("CRC: %02lx\n", ucrc);

    //
    // final encapsulation (header, crc)
    //
    size_t final_length = encrypt_length + 8 + 8;
    unsigned char *final = malloc(sizeof(char) * final_length); // data + crc + prefix
    sprintf((char *) final, "10000000%02lx", ucrc);
    memcpy(final + 16, encrypt_data, encrypt_length);

    // cleaning
    free(compressed);
    free(encrypt_data);

    return chunk_new(hashcrypt, hashkey, final, final_length);
}

// uncrypt a chunk
// it takes a chunk as parameter
// returns a chunk (without key and cipher) with payload data and length
chunk_t *decrypt_chunk(chunk_t *chunk) {
    //
    // uncrypt payload
    //
    size_t plain_length;
    // printf("[+] cipher: %s\n", chunk->cipher);
    char *plain_data = xxtea_decrypt(chunk->data + 16, chunk->length - 16, chunk->cipher, &plain_length);
    if(!plain_data) {
        fprintf(stderr, "[-] cannot decrypt data, invalid key or payload\n");
        return 0;
    }

    //
    // decompress
    //
    size_t uncompressed_length;
    snappy_uncompressed_length(plain_data, plain_length, &uncompressed_length);
    unsigned char *uncompress = (unsigned char *) malloc(uncompressed_length);
    if(snappy_uncompress(plain_data, plain_length, (char *) uncompress, &uncompressed_length) != SNAPPY_OK) {
        fprintf(stderr, "[-] snappy uncompression error\n");
        exit(EXIT_FAILURE);
    }

    chunk_t *output = chunk_new(NULL, NULL, uncompress, uncompressed_length);
    if(!output)
        return 0;

    //
    // testing integrity
    //
    char *integrity = sha256((unsigned char *) uncompress, uncompressed_length);
    // printf("[+] integrity: %s\n", integrity);

    if(strcmp(integrity, chunk->cipher)) {
        fprintf(stderr, "[-] integrity check failed: hash mismatch\n");
        fprintf(stderr, "[-] %s <> %s\n", integrity, chunk->cipher);
        // FIXME: free
        return 0;
    }

    free(integrity);

    // cleaning
    free(plain_data);

    return output;
}

//
// deprecated
//
chunk_t *upload_chunk(remote_t *remote, chunk_t *chunk) {
    (void) remote;

    printf("[-] upload: deprecated, not implemented anymore\n");
    return chunk;
}

chunk_t *download_chunk(remote_t *remote, chunk_t *chunk) {
    (void) remote;

    printf("[-] upload: deprecated, not implemented anymore\n");
    return chunk;
}

chunk_t *upload(remote_t *remote, buffer_t *buffer) {
    (void) remote;
    (void) buffer;

    printf("[-] upload: deprecated, not implemented anymore\n");
    return NULL;
}

size_t download(remote_t *remote, chunk_t *chunk, buffer_t *buffer) {
    (void) remote;
    (void) chunk;
    (void) buffer;

    printf("[-] download: deprecated, not implemented anymore\n");
    return 0;
}
