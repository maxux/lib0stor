#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <snappy-c.h>
#include <zlib.h>
#include <math.h>
#include <time.h>
#include "openssl/sha.h"
#include "xxtea.h"
#include "libg8stor.h"

#define CHUNK_SIZE    1024 * 512    // 512 KB

//
// internal system
//
void diep(char *str) {
    perror(str);
    exit(EXIT_FAILURE);
}

//
// remote manager (redis)
//
remote_t *remote_connect(const char *host, int port) {
    struct timeval timeout = {5, 0};
    remote_t *remote;
    redisReply *reply;

    if(!(remote = malloc(sizeof(remote_t))))
        diep("malloc");

    remote->redis = redisConnectWithTimeout(host, port, timeout);
    if(remote->redis == NULL || remote->redis->err) {
        printf("[-] redis: %s\n", (remote->redis->err) ? remote->redis->errstr : "memory error.");
        return NULL;
    }

    // ping redis to ensure connection
    reply = redisCommand(remote->redis, "PING");
    if(strcmp(reply->str, "PONG"))
        fprintf(stderr, "[-] warning, invalid redis PING response: %s\n", reply->str);

    freeReplyObject(reply);

    return remote;
}

void remote_free(remote_t *remote) {
    redisFree(remote->redis);
    free(remote);
}

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

static size_t file_load(char *filename, buffer_t *buffer) {
    if(!(buffer->fp = fopen(filename, "r")))
        diep("fopen");

    buffer->length = file_length(buffer->fp);
    printf("[+] filesize: %lu bytes\n", buffer->length);

    if(buffer->length == 0)
        return 0;

    if(!(buffer->data = malloc(sizeof(char) * buffer->chunksize)))
        diep("malloc");

    return buffer->length;
}

buffer_t *bufferize(char *filename) {
    buffer_t *buffer;

    if(!(buffer = calloc(1, sizeof(buffer_t))))
        diep("malloc");

    buffer->chunksize = CHUNK_SIZE;
    file_load(filename, buffer);

    // file empty, nothing to do.
    if(buffer->length == 0) {
        fprintf(stderr, "[-] file is empty, nothing to do.\n");
        free(buffer);
        return NULL;
    }

    size_t hchunksize = buffer->chunksize / 2;
    buffer->chunks = (int)((buffer->length + hchunksize) / buffer->chunksize);

    // if the file is smaller than a chunks, hardcoding 1 chunk.
    if(buffer->length < buffer->chunksize)
        buffer->chunks = 1;

    return buffer;
}

const unsigned char *buffer_next(buffer_t *buffer) {
    // resize chunksize if it's smaller than the remaining
    // amount of data
    if(buffer->current + buffer->chunksize > buffer->length)
        buffer->chunksize = buffer->length - buffer->current;

    // loading this chunk in memory
    if(fread(buffer->data, buffer->chunksize, 1, buffer->fp) != 1)
        diep("fread");

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
static unsigned char *sha256hex(unsigned char *hash) {
    unsigned char *buffer = calloc((SHA256_DIGEST_LENGTH * 2) + 1, sizeof(char));

    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf((char *) buffer + (i * 2), "%02x", hash[i]);

    return buffer;
}

static unsigned char *sha256(const unsigned char *buffer, size_t length) {
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
chunk_t *chunk_new(unsigned char *hash, unsigned char *key) {
    chunk_t *chunk;

    if(!(chunk = malloc(sizeof(chunk_t))))
        diep("malloc");

    chunk->hash = hash;
    chunk->key = key;

    return chunk;
}

void chunk_free(chunk_t *chunk) {
    free(chunk->hash);
    free(chunk->key);
    free(chunk);
}

//
// workflow
//
chunk_t *upload(remote_t *remote, buffer_t *buffer) {
    redisReply *reply;
    const unsigned char *data = buffer_next(buffer);

    // hashing this chunk
    unsigned char *hashkey = sha256(data, buffer->chunksize);
    printf("[+] chunk hash: %s\n", (char *) hashkey);

    //
    // compress
    //
    size_t output_length = snappy_max_compressed_length(buffer->chunksize);
    char *compressed = (char *) malloc(output_length);
    if(snappy_compress((char *) data, buffer->chunksize, compressed, &output_length) != SNAPPY_OK) {
        fprintf(stderr, "[-] snappy error\n");
        exit(EXIT_FAILURE);
    }

    // printf("Compressed size: %lu\n", output_length);

    //
    // encrypt
    //
    size_t encrypt_length;
    unsigned char *encrypt_data = xxtea_encrypt(compressed, output_length, hashkey, &encrypt_length);

    unsigned char *hashcrypt = sha256(encrypt_data, encrypt_length);
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
    char *final = malloc(sizeof(char) * final_length); // data + crc + prefix
    sprintf(final, "10000000%02lx", ucrc);
    memcpy(final + 16, encrypt_data, encrypt_length);

    //
    // add to backend
    //
    size_t shalen = (size_t) SHA256_DIGEST_LENGTH * 2;

    reply = redisCommand(remote->redis, "SET %b %b", hashkey, shalen, final, final_length);
    printf("[+] uploading: %s: %s\n", hashkey, reply->str);
    freeReplyObject(reply);

    //
    // stats
    //
    buffer->finalsize += output_length;

    // cleaning
    free(compressed);
    free(encrypt_data);
    free(final);

    return chunk_new(hashkey, hashcrypt);
}