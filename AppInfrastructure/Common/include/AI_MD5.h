/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

#ifndef _AI_MD5_H_
#define _AI_MD5_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Any 32-bit or wider unsigned integer data type will do */
typedef unsigned int AI_MD5_u32plus;

#define AI_MD5_DIGEST_LENGTH    16

typedef struct {
	AI_MD5_u32plus lo, hi;
	AI_MD5_u32plus a, b, c, d;
	unsigned char buffer[64];
	AI_MD5_u32plus block[16];
} AI_MD5_CTX;

extern void AI_MD5_Init(AI_MD5_CTX *ctx);
extern void AI_MD5_Update(AI_MD5_CTX *ctx, const void *data, unsigned long size);
extern void AI_MD5_Final(unsigned char *result, AI_MD5_CTX *ctx);

#ifdef __cplusplus
}
#endif

#endif
