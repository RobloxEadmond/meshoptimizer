/**
 * meshoptimizer - version 1.0
 *
 * Copyright (C) 2016-2017, by Arseny Kapoulkine (arseny.kapoulkine@gmail.com)
 * Report bugs and download new versions at https://github.com/zeux/meshoptimizer
 *
 * This library is distributed under the MIT License. See notice at the end of this file.
 */
#pragma once

#include <cstddef>
#include <vector>

// Version macro; major * 100 + minor * 10 + patch
#define MESHOPTIMIZER_VERSION 100

// If no API is defined, assume default
#ifndef MESHOPTIMIZER_API
#	define MESHOPTIMIZER_API
#endif

// Index buffer generator
// Generates index buffer from the unindexed vertex stream and returns number of unique vertices (max index + 1)
// destination must contain enough space for the resulting index buffer (vertex_count elements)
MESHOPTIMIZER_API size_t generateIndexBuffer(unsigned int* destination, const void* vertices, size_t vertex_count, size_t vertex_size);

// Vertex buffer generator
// Generates vertex buffer from the unindexed vertex stream and index buffer generated by generateIndexBuffer
// destination must contain enough space for the resulting vertex buffer (max index + 1 elements)
MESHOPTIMIZER_API void generateVertexBuffer(void* destination, const unsigned int* indices, const void* vertices, size_t vertex_count, size_t vertex_size);

// Vertex transform cache optimizer using the Tipsify algorithm by Sander et al
//   http://gfx.cs.princeton.edu/pubs/Sander_2007_%3ETR/tipsy.pdf
// Reorders indices to reduce the number of GPU vertex shader invocations
//
// destination must contain enough space for the resulting index buffer (index_count elements)
// cache_size should be less than the actual GPU cache size to avoid cache thrashing
// clusters is an optional output for the overdraw optimizer (below)
MESHOPTIMIZER_API void optimizePostTransform(unsigned short* destination, const unsigned short* indices, size_t index_count, size_t vertex_count, unsigned int cache_size = 16, std::vector<unsigned int>* clusters = 0);
MESHOPTIMIZER_API void optimizePostTransform(unsigned int* destination, const unsigned int* indices, size_t index_count, size_t vertex_count, unsigned int cache_size = 16, std::vector<unsigned int>* clusters = 0);

// Overdraw optimizer using the Tipsify algorithm
// Reorders indices to reduce the number of GPU vertex shader invocations and the pixel overdraw
//
// destination must contain enough space for the resulting index buffer (index_count elements)
// indices must contain index data that is the result of optimizePostTransform (*not* the original mesh indices!)
// clusters must contain cluster data that is the result of optimizePostTransform
// vertex_positions should have float3 position in the first 12 bytes of each vertex - similar to glVertexPointer
// cache_size should be less than the actual GPU cache size to avoid cache thrashing
// threshold indicates how much the overdraw optimizer can degrade vertex cache efficiency (1.05 = up to 5%) to reduce overdraw more efficiently
MESHOPTIMIZER_API void optimizeOverdraw(unsigned short* destination, const unsigned short* indices, size_t index_count, const float* vertex_positions, size_t vertex_positions_stride, size_t vertex_count, const std::vector<unsigned int>& clusters, unsigned int cache_size = 16, float threshold = 1);
MESHOPTIMIZER_API void optimizeOverdraw(unsigned int* destination, const unsigned int* indices, size_t index_count, const float* vertex_positions, size_t vertex_positions_stride, size_t vertex_count, const std::vector<unsigned int>& clusters, unsigned int cache_size = 16, float threshold = 1);

// Vertex fetch cache optimizer
// Reorders vertices and changes indices to reduce the amount of GPU memory fetches during vertex processing
//
// desination must contain enough space for the resulting vertex buffer (vertex_count elements)
// indices is used both as an input and as an output index buffer
MESHOPTIMIZER_API void optimizePreTransform(void* destination, const void* vertices, unsigned short* indices, size_t index_count, size_t vertex_count, size_t vertex_size);
MESHOPTIMIZER_API void optimizePreTransform(void* destination, const void* vertices, unsigned int* indices, size_t index_count, size_t vertex_count, size_t vertex_size);

struct PostTransformCacheStatistics
{
	unsigned int vertices_transformed;
	float acmr; // transformed vertices / triangle count; best case 0.5, worst case 3.0, optimum depends on topology
	float atvr; // transformed vertices / vertex count; best case 1.0, worse case 6.0, optimum is 1.0 (each vertex is transformed once)
};

// Vertex transform cache analyzer
// Returns cache hit statistics using a simplified FIFO model
// Results will not match actual GPU performance
MESHOPTIMIZER_API PostTransformCacheStatistics analyzePostTransform(const unsigned short* indices, size_t index_count, size_t vertex_count, unsigned int cache_size = 32);
MESHOPTIMIZER_API PostTransformCacheStatistics analyzePostTransform(const unsigned int* indices, size_t index_count, size_t vertex_count, unsigned int cache_size = 32);

struct OverdrawStatistics
{
	unsigned int pixels_covered;
	unsigned int pixels_shaded;
	float overdraw; // shaded pixels / covered pixels; best case 1.0
};

// Overdraw analyzer
// Returns overdraw statistics using a software rasterizer
// Results will not match actual GPU performance
//
// vertex_positions should have float3 position in the first 12 bytes of each vertex - similar to glVertexPointer
MESHOPTIMIZER_API OverdrawStatistics analyzeOverdraw(const unsigned short* indices, size_t index_count, const float* vertex_positions, size_t vertex_positions_stride, size_t vertex_count);
MESHOPTIMIZER_API OverdrawStatistics analyzeOverdraw(const unsigned int* indices, size_t index_count, const float* vertex_positions, size_t vertex_positions_stride, size_t vertex_count);

struct PreTransformCacheStatistics
{
	unsigned int bytes_fetched;
	float overfetch; // fetched bytes / vertex buffer size; best case 1.0 (each byte is fetched once)
};

// Vertex fetch cache analyzer
// Returns cache hit statistics using a simplified direct mapped model
// Results will not match actual GPU performance
MESHOPTIMIZER_API PreTransformCacheStatistics analyzePreTransform(const unsigned short* indices, size_t index_count, size_t vertex_count, size_t vertex_size);
MESHOPTIMIZER_API PreTransformCacheStatistics analyzePreTransform(const unsigned int* indices, size_t index_count, size_t vertex_count, size_t vertex_size);

// Quantization into commonly supported data formats

// Quantize a float in [0..1] range into an N-bit fixed point unorm value
// Assumes reconstruction function (q / (2^bits-1)), which is the case for fixed-function normalized fixed point conversion
// Maximum reconstruction error: 1/2^(bits+1)
inline int quantizeUnorm(float v, int bits)
{
	const float scale = float((1 << bits) - 1);

	v = (v >= 0) ? v : 0;
	v = (v <= 1) ? v : 1;

	return int(v * scale + 0.5f);
}

// Quantize a float in [-1..1] range into an N-bit fixed point snorm value
// Assumes reconstruction function (q / (2^(bits-1)-1)), which is the case for fixed-function normalized fixed point conversion (except OpenGL)
// Maximum reconstruction error: 1/2^bits
// Warning: OpenGL fixed function reconstruction function can't represent 0 exactly; when using OpenGL, use this function and have the shader reconstruct by dividing by 2^(bits-1)-1.
inline int quantizeSnorm(float v, int bits)
{
	const float scale = float((1 << (bits - 1)) - 1);

	float round = (v >= 0 ? 0.5f : -0.5f);

	v = (v >= -1) ? v : -1;
	v = (v <= +1) ? v : +1;

	return int(v * scale + round);
}

// Quantize a float into half-precision floating point value
// Generates +-inf for overflow, preserves NaN, flushes denormals to zero, rounds to nearest
// Representable magnitude range: [6e-5; 65504]
// Maximum relative reconstruction error: 5e-4
inline unsigned short quantizeHalf(float v)
{
	union { float f; unsigned int ui; } u = {v};
	unsigned int ui = u.ui;

	int s = (ui >> 16) & 0x8000;
	int em = ui & 0x7fffffff;

	// bias exponent and round to nearest; 112 is relative exponent bias (127-15)
	int h = (em - (112 << 23) + (1 << 12)) >> 13;

	// underflow: flush to zero; 113 encodes exponent -14
	h = (em < (113 << 23)) ? 0 : h;

	// overflow: infinity; 143 encodes exponent 16
	h = (em >= (143 << 23)) ? 0x7c00 : h;

	// NaN; note that we convert all types of NaN to qNaN
	h = (em > (255 << 23)) ? 0x7e00 : h;

	return (unsigned short)(s | h);
}

/**
 * Copyright (c) 2016-2017 Arseny Kapoulkine
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
