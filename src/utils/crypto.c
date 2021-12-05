/*
 * [meteoserver]
 * crypto.c
 * November 23, 2021.
 *
 * Created by Álvaro Romero <alvromero96@gmail.com>
 *
 * Originally forked from: https://github.com/Zunawe/md5-c
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "meteoserver.h"


/**
 * @brief Initializes a MD5 context
 * @param ctx MD5 struct to be initialized
 */
static void md5Init(MD5Context_t *ctx);

/**
 * @brief Adds input to the MD5 context.
 * If the input fills out a block of 512 bits, apply the algorithm (md5Step)
 * and save the result in the buffer. Also updates the overall size.
 * @param input_buffer Input to be added in the context.
 * @param input_len Lenght of the input buffer.
 */
static void md5Update(MD5Context_t *ctx, uint8_t *input_buffer, size_t input_len);

/**
 * @brief Pad the current input to get to 448 bytes, append the size in bits to the very end,
 * and save the result of the final iteration into digest.
 * @param ctx MD5 struct to be destroyed.
 */
static void md5Finalize(MD5Context_t *ctx);

/**
 * @brief Rotates a 32-bit word left by n bits.
 * @param x Word to be rotated.
 * @param n Number of bits that'll be rotated.
 */
static uint32_t rotateLeft(uint32_t x, uint32_t n);

/**
 * @brief Step on 512 bits of input with the main MD5 algorithm.
 * @param buffer Buffer that'll contain the modified data.
 * @param input Current computed hash.
 */
static void md5Step(uint32_t *buffer, uint32_t *input);

/**
* @brief Function to store the MD5 hash into an allocated string.
* @param p Hash to be stored as a string.
* @return Allocated string with the hash.
*/
char    *hash_to_string(uint8_t *p, char *opt);

/**
 * @brief Wrapper function in charge of turning an input message to a MD5 hash.
 * @param input String that'll be turned to a MD5 hash.
 * @return Allocated MD5 hash.
 */
char    *md5String(char *input);



/* Constant data required for the MD5 algorithm */


#define A 0x67452301
#define B 0xefcdab89
#define C 0x98badcfe
#define D 0x10325476

static char hex[] = "0123456789abcdef";

static uint32_t S[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static uint32_t K[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                       0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                       0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                       0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                       0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                       0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                       0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                       0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                       0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                       0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                       0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                       0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                       0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                       0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                       0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                       0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};


// Bit-manipulation functions defined by the MD5 algorithm
#define F(X, Y, Z) ((X & Y) | (~X & Z))
#define G(X, Y, Z) ((X & Z) | (Y & ~Z))
#define H(X, Y, Z) (X ^ Y ^ Z)
#define I(X, Y, Z) (Y ^ (X | ~Z))


// Padding used to make the size (in bits) of the input congruent to 448 mod 512
static uint8_t PADDING[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};



/* Definitions */


// Initialize a context
static void md5Init(MD5Context_t *ctx)
{
	ctx->size = (uint64_t)0;

	ctx->buffer[0] = (uint32_t)A;
	ctx->buffer[1] = (uint32_t)B;
	ctx->buffer[2] = (uint32_t)C;
	ctx->buffer[3] = (uint32_t)D;
}

// Rotates a 32-bit word left by n bits
static uint32_t rotateLeft(uint32_t x, uint32_t n)
{
	return (x << n) | (x >> (32 - n));
}

// Add some amount of input to the context
static void md5Update(MD5Context_t *ctx, uint8_t *input_buffer, size_t input_len)
{
	uint32_t input[16];
	unsigned int offset = ctx->size % 64;
	ctx->size += (uint64_t)input_len;

	// Copy each byte in input_buffer into the next space in our context input
	for (unsigned int i = 0; i < input_len; ++i)
    {
		ctx->input[offset++] = (uint8_t)*(input_buffer + i);

		// If we've filled our context input, copy it into our local array input
		// then reset the offset to 0 and fill in a new buffer.
		// Every time we fill out a chunk, we run it through the algorithm
		// to enable some back and forth between cpu and i/o
		if (offset % 64 == 0)
        {
			for (unsigned int j = 0; j < 16; ++j)
            {
				// Convert to little-endian
				// The local variable `input` our 512-bit chunk separated into 32-bit words
				// we can use in calculations
				input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
						   (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
						   (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
						   (uint32_t)(ctx->input[(j * 4)]);
			}
			md5Step(ctx->buffer, input);
			offset = 0;
		}
	}
}

// Pad the current input to get to 448 bytes
static void md5Finalize(MD5Context_t *ctx)
{
	uint32_t input[16];
	unsigned int offset = ctx->size % 64;
	unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

	// Fill in the padding andndo the changes to size that resulted from the update
	md5Update(ctx, PADDING, padding_length);
	ctx->size -= (uint64_t)padding_length;

	// Do a final update (internal to this function)
	// Last two 32-bit words are the two halves of the size (converted from bytes to bits)
	for (unsigned int j = 0; j < 14; ++j)
    {
		input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
		           (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
		           (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
		           (uint32_t)(ctx->input[(j * 4)]);
	}

	input[14] = (uint32_t)(ctx->size * 8);
	input[15] = (uint32_t)((ctx->size * 8) >> 32);

	md5Step(ctx->buffer, input);

	// Move the result into digest (convert from little-endian)
	for (unsigned int i = 0; i < 4; ++i)
    {
		ctx->digest[(i * 4) + 0] = (uint8_t)((ctx->buffer[i] & 0x000000FF));
		ctx->digest[(i * 4) + 1] = (uint8_t)((ctx->buffer[i] & 0x0000FF00) >>  8);
		ctx->digest[(i * 4) + 2] = (uint8_t)((ctx->buffer[i] & 0x00FF0000) >> 16);
		ctx->digest[(i * 4) + 3] = (uint8_t)((ctx->buffer[i] & 0xFF000000) >> 24);
	}
}

// Step on 512 bits of input with the main MD5 algorithm
static void md5Step(uint32_t *buffer, uint32_t *input)
{
	uint32_t AA = buffer[0];
	uint32_t BB = buffer[1];
	uint32_t CC = buffer[2];
	uint32_t DD = buffer[3];

	uint32_t        E;
	unsigned int    j;

	for (unsigned int i = 0; i < 64; ++i)
    {
		switch (i / 16)
        {
			case 0:
				E = F(BB, CC, DD);
				j = i;
				break;
			case 1:
				E = G(BB, CC, DD);
				j = ((i * 5) + 1) % 16;
				break;
			case 2:
				E = H(BB, CC, DD);
				j = ((i * 3) + 5) % 16;
				break;
			default:
				E = I(BB, CC, DD);
				j = (i * 7) % 16;
				break;
		}

		uint32_t temp = DD;
		DD = CC;
		CC = BB;
		BB = BB + rotateLeft(AA + E + K[i] + input[j], S[i]);
		AA = temp;
	}

	buffer[0] += AA;
	buffer[1] += BB;
	buffer[2] += CC;
	buffer[3] += DD;
}

// Wrapper function in charge of turning an input message to a MD5 hash
char    *md5String(char *input)
{
	MD5Context_t ctx;
	md5Init(&ctx);
	md5Update(&ctx, (uint8_t *)input, strlen(input));
	md5Finalize(&ctx);

    char *result = calloc(33, sizeof(char));

    // Turns the hash into an human-readable C string
    for (size_t i = 0; i < 16; i++)
	{
		result[(i * 2) + 0] = hex[((ctx.digest[i] & 0xF0) >> 4)];
		result[(i * 2) + 1] = hex[((ctx.digest[i] & 0x0F) >> 0)];
	}

    result[32] = '\0';

	return result;
}
