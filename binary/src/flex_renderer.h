#pragma once
#include <materialsystem/imesh.h>
#include <istudiorender.h>
#include "meshutils.h"		// Fixes linker errors
#include "flex_solver.h"
#include <vector>
#include <thread>
#include <mutex>
#include "ThreadPool.h"

#define MAX_PRIMATIVES 21845
#define MAX_THREADS 8
#define SQRT3 1.73205081

struct FlexRendererThreadData {
	//IMesh*& water;
	Vector eye_pos;
	VMatrix view_projection_matrix;
	Vector4D* particle_positions;
	Vector4D* particle_ani0;
	Vector4D* particle_ani1;
	Vector4D* particle_ani2;
	int* render_buffer;
	int max_particles;
	float radius;
};

class FlexRenderer {
private:
	int allocated = 0;
	ThreadPool* threads = nullptr;
	IMesh** meshes = nullptr;	// water meshes 
	int* water_buffer = nullptr;	// which particles should be rendered?
	int* diffuse_buffer = nullptr;	// ^
	std::future<IMesh*>* queue;
	
	void destroy_meshes();
	void update_meshes();
public:
	void draw_water();
	void draw_diffuse();

	void build_meshes(FlexSolver* flex, float radius, float radius2);

	FlexRenderer(int max_meshes);
	~FlexRenderer();
};