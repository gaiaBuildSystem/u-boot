/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Usefuls routines based on the LzmaTest.c file from LZMA SDK 4.65
 *
 * Copyright (C) 2007-2008 Industrie Dial Face S.p.A.
 * Luigi 'Comio' Mantellini (luigi.mantellini@idf-hit.com)
 *
 * Copyright (C) 1999-2005 Igor Pavlov
 */

#ifndef __LZMA_TOOL_H__
#define __LZMA_TOOL_H__

#include <lzma/LzmaTypes.h>

/**
 * lzmaBuffToBuffDecompress() - Decompress LZMA data
 *
 * @outStream: output buffer
 * @uncompressedSize: On entry, the mnaximum uncompressed size of the data;
 *	on exit, the actual uncompressed size after processing
 * @inStream: Compressed bytes to decompress
 * @length: Sizeof @inStream
 * @return 0 if OK, SZ_ERROR_DATA if the data is in a format that cannot be
 *	decompressed; SZ_ERROR_OUTPUT_EOF if *uncompressedSize is too small;
 *	see also other SZ_ERROR... values
 */
int lzmaBuffToBuffDecompress(unsigned char *outStream, SizeT *uncompressedSize,
			     const unsigned char *inStream, SizeT length);

/**
 * lzma_uncompressed_size() - Find the uncompressed size of LZMA data
 *
 * The LZMA header has a field for the uncompressed size, but encoders which
 * stream their input (e.g. the lzma tool reading stdin, as the Linux build
 * does) leave it as 'unknown', in which case this fails.
 *
 * @inStream: Compressed data
 * @length: Size of @inStream
 * @sizep: Returns the uncompressed size
 * Return: 0 if OK, -EINVAL if the data is too short to hold a header,
 * -EOPNOTSUPP if the size is not recorded, -E2BIG if it does not fit in ulong
 */
int lzma_uncompressed_size(const unsigned char *inStream, SizeT length,
			   ulong *sizep);

#endif
