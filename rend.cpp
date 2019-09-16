#include	"stdafx.h"
#include	"stdio.h"
#include	"math.h"
#include	"Gz.h"
#include	"rend.h"

#include <algorithm>
#include <vector>
#include <cmath>


#ifndef GZ_VERTEX
typedef	struct {
	float    x;
	float    y;
	float    z;
} GzVertex;
#define GZ_VERTEX
#endif;

#ifndef GZ_EDGE
typedef	struct {
	GzVertex    start;
	GzVertex    end;
	GzVertex    current;
	float		slopeX;
	float		slopeZ;
} GzEdge;
#define GZ_EDGE
#endif;

#ifndef GZ_SPAN
typedef	struct {
	GzVertex    start;
	GzVertex    end;
	GzVertex    current;
	float		slopeZ;
} GzSpan;
#define GZ_SPAN
#endif;


bool isLeftOf(GzEdge e1, GzEdge e2) {
	float x = e1.start.x + e1.slopeX * (e2.end.y - e1.start.y);
	return x < e2.end.x;
}


GzRender::GzRender(int xRes, int yRes)
{
	/* Create a framebuffer for MS Windows display:
	 * - set display resolution
	 * - allocate memory for framebuffer : 3 bytes(b, g, r) x width x height
	 * - allocate memory for pixel buffer
	*/
	xres = xRes;
	yres = yRes;

	pixelbuffer = new GzPixel[xres * yres];
	framebuffer = new char[3 * xres * yres];
}


GzRender::~GzRender()
{
	/* Clean up, free buffer memory */
	delete []pixelbuffer;
	delete []framebuffer;
}


int GzRender::GzDefault()
{
	/* Set pixel buffer to white (0xfff) - start a new frame */
	for (int y = 0; y < yres; y++) {
		for (int x = 0; x < xres; x++) {
			GzPixel pixel = { 0xfff, 0xfff, 0xfff, 1, INT_MAX };
			pixelbuffer[ARRAY(x, y)] = pixel;
		}
	}
	return GZ_SUCCESS;
}


bool isOutOfBound(int x, int y, unsigned short xres, unsigned short yres) {
	/* Boundary check */
	return (x < 0 || xres - 1 < x || y < 0 || yres - 1 < y);
}


GzIntensity clamp(GzIntensity val) {
	/* Clamp value to [0, 4095] */
	if (val < 0) return 0;
	else if (0xfff < val) return 0xfff;
	else return val;
}


int GzRender::GzPut(int i, int j, GzIntensity r, GzIntensity g, GzIntensity b, GzIntensity a, GzDepth z)
{
	/* Write pixel values into the buffer */
	if (isOutOfBound(i, j, xres, yres)) return GZ_SUCCESS;

	// Intensity value check
	r = clamp(r);
	g = clamp(g);
	b = clamp(b);

	// Assign right shifted value (12bits to 8bits)
	pixelbuffer[ARRAY(i, j)] = { r >> 4, g >> 4, b >> 4, a, z };

	return GZ_SUCCESS;
}


int GzRender::GzGet(int i, int j, GzIntensity *r, GzIntensity *g, GzIntensity *b, GzIntensity *a, GzDepth *z)
{
	/* Retrieve a pixel information from the pixel buffer */
	if (isOutOfBound(i, j, xres, yres)) return GZ_SUCCESS;

	*r = pixelbuffer[ARRAY(i, j)].red;
	*g = pixelbuffer[ARRAY(i, j)].green;
	*b = pixelbuffer[ARRAY(i, j)].blue;
	*a = pixelbuffer[ARRAY(i, j)].alpha;
	*z = pixelbuffer[ARRAY(i, j)].z;

	return GZ_SUCCESS;
}


int GzRender::GzFlushDisplay2File(FILE* outfile)
{
	/* Write image to ppm file -- "P6 %d %d 255\r" */
	fprintf(outfile, "P6 %d %d 255\n", xres, yres);

	GzIntensity r = 0, g = 0, b = 0, a = 0;
	GzDepth z = 0;

	for (int y = 0; y < yres; y++) {
		for (int x = 0; x < xres; x++) {
			GzGet(x, y, &r, &g, &b, &a, &z);
			fprintf(outfile, "%c%c%c", (unsigned char)r, (unsigned char)g, (unsigned char)b);
		}
	}

	return GZ_SUCCESS;
}


int GzRender::GzFlushDisplay2FrameBuffer()
{
	/* Write pixels to framebuffer: 
	 * - put the pixels into the frame buffer
	 * - The order is blue, green and red, when storing the pixels into the frame buffer
	*/
	GzIntensity r = 0, g = 0, b = 0, a = 0;
	GzDepth z = 0;

	int i = 0;
	for (int y = 0; y < yres; y++) {
		for (int x = 0; x < xres; x++) {
			GzGet(x, y, &r, &g, &b, &a, &z);
			framebuffer[i++] = (unsigned char)b;
			framebuffer[i++] = (unsigned char)g;
			framebuffer[i++] = (unsigned char)r;
		}
	}

	return GZ_SUCCESS;
}


/***********************************************/
/* HW2 methods: implement from here */

int GzRender::GzPutAttribute(int numAttributes, GzToken	*nameList, GzPointer *valueList) 
{
	/* HW 2.1
	 * - Set renderer attribute states (e.g.: GZ_RGB_COLOR default color)
	 * - In later homeworks set shaders, interpolaters, texture maps, and lights
	*/
	for (int i = 0; i < numAttributes; i++) {
		if (nameList[i] == GZ_RGB_COLOR) {
			flatcolor[0] = ctoi((*(GzColor*)valueList[i])[0]);
			flatcolor[1] = ctoi((*(GzColor*)valueList[i])[1]);
			flatcolor[2] = ctoi((*(GzColor*)valueList[i])[2]);
		}
	}
	return GZ_SUCCESS;
}


GzSpan setSpan(GzVertex startL, GzVertex endR) {
	float slopeZ = (endR.z - startL.z) / (endR.x - startL.x);
	GzVertex startPix = { ceil(startL.x), startL.y, startL.z + slopeZ * (ceil(startL.x) - startL.x) };
	return { startPix, endR, startPix, slopeZ };
}


int GzRender::GzPutTriangle(int	numParts, GzToken *nameList, GzPointer *valueList)
{
	/* HW 2.2
	 * - Pass in a triangle description with tokens and values corresponding to
			GZ_NULL_TOKEN: do nothing - no values
			GZ_POSITION:   3 vert positions in model space
	 * - Invoke the rastrizer/scanline framework
	 * - Return error code
	*/
	
	// Construct vertices
	GzVertex vertices[3] = { 0 };
	for (int i = 0; i < numParts; i++) {
		if (nameList[i] == GZ_POSITION) {
			// Add v1, v2 and v3
			vertices[0] = { ((GzCoord*)valueList[i])[0][0], ((GzCoord*)valueList[i])[0][1], ((GzCoord*)valueList[i])[0][2] };
			vertices[1] = { ((GzCoord*)valueList[i])[1][0], ((GzCoord*)valueList[i])[1][1], ((GzCoord*)valueList[i])[1][2] };
			vertices[2] = { ((GzCoord*)valueList[i])[2][0], ((GzCoord*)valueList[i])[2][1], ((GzCoord*)valueList[i])[2][2] };

			// Sort vertices by y
			std::sort(vertices, vertices + 3, [](GzVertex v1, GzVertex v2) { return v1.y < v2.y; });
		}
	}
	
	// Set up edge DDAs
	GzEdge e01 = { vertices[0], vertices[1], vertices[0],
				  (vertices[1].x - vertices[0].x) / (vertices[1].y - vertices[0].y),
				  (vertices[1].z - vertices[0].z) / (vertices[1].y - vertices[0].y) };
	GzEdge e12 = { vertices[1], vertices[2], vertices[1],
				  (vertices[2].x - vertices[1].x) / (vertices[2].y - vertices[1].y),
				  (vertices[2].z - vertices[1].z) / (vertices[2].y - vertices[1].y) };
	GzEdge e02 = { vertices[0], vertices[2], vertices[0],
				  (vertices[2].x - vertices[0].x) / (vertices[2].y - vertices[0].y),
				  (vertices[2].z - vertices[0].z) / (vertices[2].y - vertices[0].y) };
	
	// Categorize edges if R or L
	std::vector<GzEdge> LEdges, REdges;
	if (isLeftOf(e01, e02)) { // L(0-1-2) R(0-2)
		LEdges.push_back(e01);
		LEdges.push_back(e12);
		REdges.push_back(e02);
	} else { // L(0-2) R(0-1-2)
		LEdges.push_back(e02);
		REdges.push_back(e01);
		REdges.push_back(e12);
	}

	GzIntensity crtR = 0, crtG = 0, crtB = 0, crtA = 0;
	GzDepth crtZ = 0;
	GzSpan span;
	float dy, slopeZ;
	GzVertex startL, endR;

	// Note: This algorithm picks a pixel on top and left, not bottom and right

	if (LEdges.size() == 2) { // Configuration of L(1-2-3) R(1-3)
		// Upper half
		dy = ceil(LEdges[0].start.y) - LEdges[0].start.y;
		startL = { LEdges[0].start.x + LEdges[0].slopeX * dy, ceil(LEdges[0].start.y), LEdges[0].start.z + LEdges[0].slopeZ * dy };
		endR = { REdges[0].start.x + REdges[0].slopeX * dy, ceil(REdges[0].start.y), REdges[0].start.z + REdges[0].slopeZ * dy };
		span = setSpan(startL, endR);

		for (int y = ceil(LEdges[0].start.y); y < LEdges[0].end.y; y++) {
			for (int x = span.start.x; x < span.end.x; x++) {
				GzGet(x, y, &crtR, &crtG, &crtB, &crtA, &crtZ);
				if (span.current.z < crtZ) GzPut(x, y, flatcolor[0], flatcolor[1], flatcolor[2], 1, span.current.z);
				span.current = { (float)x + 1, (float)y, span.current.z + span.slopeZ };
			}
			// Set span to y + 1
			startL = { startL.x + LEdges[0].slopeX, startL.y + 1, startL.z + LEdges[0].slopeZ };
			endR = { endR.x + REdges[0].slopeX, endR.y + 1, endR.z + REdges[0].slopeZ };
			span = setSpan(startL, endR);
		}

		// Bottom half
		dy = ceil(LEdges[1].start.y) - LEdges[1].start.y;
		startL = { LEdges[1].start.x + LEdges[1].slopeX * dy, ceil(LEdges[1].start.y), LEdges[1].start.z + LEdges[1].slopeZ * dy };
		span = setSpan(startL, endR);

		for (int y = ceil(LEdges[1].start.y); y < LEdges[1].end.y; y++) {
			for (int x = span.start.x; x < span.end.x; x++) {
				GzGet(x, y, &crtR, &crtG, &crtB, &crtA, &crtZ);
				if (span.current.z < crtZ) GzPut(x, y, flatcolor[0], flatcolor[1], flatcolor[2], 1, span.current.z);
				span.current = { (float)x + 1, (float)y, span.current.z + span.slopeZ };
			}

			// Set span to y + 1
			startL = { startL.x + LEdges[1].slopeX, startL.y + 1, startL.z + LEdges[1].slopeZ };
			endR = { endR.x + REdges[0].slopeX, endR.y + 1, endR.z + REdges[0].slopeZ };
			span = setSpan(startL, endR);
		}
	}
	else { // Configuration of L(1-3) R(1-2-3)
		// Upper half
		dy = ceil(LEdges[0].start.y) - LEdges[0].start.y;
		startL = { LEdges[0].start.x + LEdges[0].slopeX * dy, ceil(LEdges[0].start.y), LEdges[0].start.z + LEdges[0].slopeZ * dy };
		endR = { REdges[0].start.x + REdges[0].slopeX * dy, ceil(REdges[0].start.y), REdges[0].start.z + REdges[0].slopeZ * dy };
		span = setSpan(startL, endR);

		for (int y = ceil(REdges[0].start.y); y < REdges[0].end.y; y++) {
			for (int x = span.start.x; x < span.end.x; x++) {
				GzGet(x, y, &crtR, &crtG, &crtB, &crtA, &crtZ);
				if (span.current.z < crtZ) GzPut(x, y, flatcolor[0], flatcolor[1], flatcolor[2], 1, span.current.z);
				span.current = { (float)x + 1, (float)y, span.current.z + span.slopeZ };
			}
			// Set span to y + 1
			startL = { startL.x + LEdges[0].slopeX, startL.y + 1, startL.z + LEdges[0].slopeZ };
			endR = { endR.x + REdges[0].slopeX, endR.y + 1, endR.z + REdges[0].slopeZ };
			span = setSpan(startL, endR);
		}

		// Bottom half
		dy = ceil(REdges[1].start.y) - REdges[1].start.y;
		endR = { REdges[1].start.x + REdges[1].slopeX * dy, ceil(REdges[1].start.y), REdges[1].start.z + REdges[1].slopeZ * dy };
		span = setSpan(startL, endR);

		for (int y = ceil(REdges[1].start.y); y < REdges[1].end.y; y++) {
			for (int x = span.start.x; x < span.end.x; x++) {
				GzGet(x, y, &crtR, &crtG, &crtB, &crtA, &crtZ);
				if (span.current.z < crtZ) GzPut(x, y, flatcolor[0], flatcolor[1], flatcolor[2], 1, span.current.z);
				span.current = { (float)x + 1, (float)y, span.current.z + span.slopeZ };
			}

			// Set span to y + 1
			startL = { startL.x + LEdges[0].slopeX, startL.y + 1, startL.z + LEdges[0].slopeZ };
			endR = { endR.x + REdges[1].slopeX, endR.y + 1, endR.z + REdges[1].slopeZ };
			span = setSpan(startL, endR);
		}
	}

	return GZ_SUCCESS;
}
