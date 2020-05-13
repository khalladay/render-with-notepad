#include <Windows.h>
#include "notepad_frontend.h"
#include <glm.hpp>
#include "gtx/transform.hpp"

const int SCREEN_CHARS_WIDE = 131;
const int SCREEN_CHARS_TALL = 30;
const int SCREEN_NUM_CHARS = SCREEN_CHARS_WIDE * SCREEN_CHARS_TALL;
const int WINDOW_WIDTH_PIXELS = 1365;
const int WINDOW_HEIGHT_PIXELS = 768;
const float PI = 3.14159f;
const float DEG2RAD = 0.0174532925f;


#define USE_LIGHTING 1

using namespace glm;

typedef struct
{
	float zNear = 0.01f;
	float zFar = 10000.0f;
	float aspect = (0.5f * SCREEN_CHARS_WIDE) / float(SCREEN_CHARS_TALL);
	float fov = 60.0f;
}Camera;

typedef struct
{
	vec3 direction;
	float intensity;
}Light;

typedef struct
{
	vec3 bounds[2]; //object space
	mat4 transform;
}Cube;

typedef struct
{
	Cube cube;
	Cube floor;
	Light light;
}Scene;

typedef struct
{
	vec3 origin;
	vec3 direction;
	float tmin;
	float tmax;
}Ray;

typedef struct
{
	vec3 normal;
	vec3 pos;
}RaycastHit;

char floatToChar(float val)
{
	//slightly modified 10 level ascii color ramp from http://paulbourke.net/dataformats/asciiart/
	if (val > 0.9f) return '.';
	if (val > 0.8f) return ':';
	if (val > 0.7f) return '-';
	if (val > 0.6f) return '=';
	if (val > 0.5f) return '+';
	if (val > 0.4f) return '*';
	if (val > 0.3f) return '#';
	if (val > 0.2f) return '%';
	if (val > 0.1f) return '@';
	return '@';
}

//for a given point on a cube, what's the normal? 
vec3 cubeSurfaceNormal(Cube& c, vec3 point)
{
	vec3 cubeCenter = (c.bounds[0] + c.bounds[1]) / 2.0f;
	vec3 centerToPoint = vec3(glm::inverse(c.transform) * vec4(point, 1.0f)) - cubeCenter;

	vec3 absToPoint = glm::abs(centerToPoint);
	vec4 n = vec4(0, 0, 0, 0);

	if (absToPoint.x > absToPoint.y && absToPoint.x > absToPoint.z)
	{
		n.x = glm::sign(centerToPoint.x) * 1.0f;
	}
	else if (absToPoint.y > absToPoint.x && absToPoint.y > absToPoint.z)
	{
		n.y = glm::sign(centerToPoint.y) * 1.0f;
	}
	else if (absToPoint.z > absToPoint.x && absToPoint.z > absToPoint.y)
	{
		n.z = glm::sign(centerToPoint.z) * 1.0f;
	}
	else
	{
		n.z = 0.0f;
	}

#if USE_LIGHTING
	return normalize(vec3(c.transform * n));
#else
	//if lighting disabled, the normal is just to identify which face of the cube is hit
	return n;
#endif
}

bool traceCube(Ray& r, Cube& c, RaycastHit* outHit)
{
	// Slabs method from Real Time Rendering (p.743 in 3rd edition)
	float tMin = 0.0f;
	float tMax = 100000.0f;
	const float EPSILON = 0.001f;

	const glm::vec3 cubeWorldSpacePos(c.transform[3].x, c.transform[3].y, c.transform[3].z); //ac
	const glm::vec3 p = cubeWorldSpacePos - r.origin; //in RTR this is ac - o

	//for each dimension (x,y,z)
	for (int i = 0; i < 3; ++i)
	{
		const vec3 axis(c.transform[i].x, c.transform[i].y, c.transform[i].z); //in RTR, this is ai
		const float e = dot(axis, p); //ai dot p
		const float f = dot(r.direction, axis); // ai dot d

		if (glm::abs(f) > EPSILON)
		{
			float t1 = (e + c.bounds[0][i]) / f; //cube bounds is hi in RTR (cube half lengths)
			float t2 = (e + c.bounds[1][i]) / f; //RTR has hi as the same constant, but since we have both points, let's use them

			if (t1 > t2)
			{
				float tmp = t1;
				t1 = t2;
				t2 = tmp;
			}

			tMin = t1 > tMin ? t1 : tMin;
			tMax = t2 < tMax ? t2 : tMax;

			if (tMin > tMax) return false;
			if (tMax < 0) return false;
		}
		else if (-e + c.bounds[0][i] > 0 || -e + c.bounds[1][i] < 0)
		{
			return false;
		}
	}

	//hitDist is tMin
	if (outHit)
	{
		outHit->pos = r.origin + r.direction * tMin;
		outHit->normal = cubeSurfaceNormal(c, outHit->pos);
	}

	return true;
}

bool isShadowed(Ray r, Scene& scene)
{
	RaycastHit hit;

	return traceCube(r, scene.cube, &hit);
}


char tracePixel(Ray& ray, Scene& scene)
{
	RaycastHit hit;
	if (traceCube(ray, scene.cube, &hit))
	{
#if USE_LIGHTING
		float l = glm::max(0.0f, glm::dot(hit.normal, scene.light.direction * -1.0f)) * scene.light.intensity;
		return floatToChar(l);
#else
		if (hit.normal == vec3(1, 0, 0)) return '|';
		if (hit.normal == vec3(-1, 0, 0)) return ':';
		if (hit.normal == vec3(0, 1, 0)) return '?';
		if (hit.normal == vec3(0, -1, 0)) return '+';
		if (hit.normal == vec3(0, 0, 1)) return '=';
		if (hit.normal == vec3(0, 0, -1)) return '#';
#endif
	}
	else if (traceCube(ray, scene.floor, &hit))
	{
		Ray toLight;
		toLight.origin = hit.pos;
		toLight.direction = normalize((scene.light.direction * -1.0f));
		if (isShadowed(toLight, scene))
		{
			return '@';
		}

#if USE_LIGHTING
		float l = glm::max(0.0f, glm::dot(hit.normal, scene.light.direction * -1.0f)) * scene.light.intensity;
		return floatToChar(l);
#else
		return '-';
#endif
	}
	return ' ';
}

void traceScene(Camera& cam, Scene& scene)
{
	float fovConstant = tan((DEG2RAD * cam.fov) / 2);

	for (int y = 0; y < SCREEN_CHARS_TALL; ++y)
	{
		for (int x = 0; x < SCREEN_CHARS_WIDE; ++x)
		{
			//turn xy coords into NDC points
			vec2 pixel = vec2(x + 0.5f, y + 0.5f);

			//ndc in ray tracing is 0-1
			vec2 ndcPixel = vec2(pixel.x / (float)SCREEN_CHARS_WIDE, pixel.y / (float)SCREEN_CHARS_TALL);

			//now get ndcPixel in -1 -> 1 range. (rays to the left of the camera should go negative x, etc)
			//y is 1.0 - y * 2 so that rays above the x are positive
			vec2 remappedPixel = vec2(ndcPixel.x * 2.0f - 1.0f, 1.0f - ndcPixel.y * 2.0f);

			remappedPixel.x *= cam.aspect;
			remappedPixel *= fovConstant;
			vec3 rayDir = (vec3(remappedPixel.x, remappedPixel.y, -1));

			Ray r;
			r.origin = vec3(0, 0, 0);
			r.direction = normalize(rayDir - r.origin);
			drawChar(x, y, tracePixel(r, scene));
		}
	}
}

bool tick(Scene& scene)
{
	static float cubeAngle = 2.0f * DEG2RAD;
	static mat4 cubeRot = glm::rotate(cubeAngle, normalize(vec3(1, 1, 1)));
	scene.cube.transform = scene.cube.transform * cubeRot;
	return true;
}

void printHelp()
{
	printf("RayTracer\nUsage: RayTracer <path to notepad.exe>\n");
	printf("MemoryScanner\nExample: RayTracer C:\WINDOWS\system32\notepad.exe\n");
}


int main(int argc, const char** argv)
{
	if (argc != 2)
	{
		printHelp();
		return 1;
	}

	initializeNotepadFrontend(argv[1], argv[2], WINDOW_WIDTH_PIXELS, WINDOW_HEIGHT_PIXELS, SCREEN_CHARS_WIDE, SCREEN_NUM_CHARS);

	Camera cam;

	Scene scene;
	scene.light = { normalize(vec3(1.0f, -1.0f, -1.0f)), 1.0f };
	scene.cube = { vec3(-2.5f, -2.5f, -2.5f), vec3(2.5f, 2.5f, 2.5f) };
	scene.floor = { vec3(-20.0f, -20.0f, -20.0f), vec3(20.0f, 20.0f, 20.0f) };
	scene.cube.transform = glm::translate(vec3(0, 1, -10));
	scene.floor.transform = glm::translate(vec3(0, -25.0f, 0));

	LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;

	QueryPerformanceFrequency(&Frequency);

	int lastMS = 0;
	while (1)
	{
		QueryPerformanceCounter(&StartingTime);
		clearScreen();

		if (!tick(scene))
		{
			break;
		}
		traceScene(cam, scene);

		//fps counter
		int fps = glm::min(30, 1000 / max(1, lastMS));
		drawChar(0, 0, fps / 10 + 48);
		drawChar(1, 0, fps % 10 + 48);

		swapBuffersAndRedraw();

		QueryPerformanceCounter(&EndingTime);
		ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
		ElapsedMicroseconds.QuadPart *= 1000000;
		ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

		lastMS = ElapsedMicroseconds.QuadPart / 1000;
		Sleep(max(0, 33 - lastMS));
	}
	shutdownNotepadFrontend();

	return 0;
}