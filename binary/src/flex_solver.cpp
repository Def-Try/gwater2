#pragma once
#include "flex_solver.h"

#define MAX_COLLIDERS 8192	// source can't go over this number of props so.. might as well just have it as the limit

#define CM_2_INCH 39.3701f
// todo: add smartcomments

// Struct that holds FleX solver data
void FlexSolver::add_buffer(std::string name, int type, int count) {
	NvFlexBuffer* buffer = NvFlexAllocBuffer(library, count, type, eNvFlexBufferHost);
	buffers[name] = buffer;

	// Initialize CPU buffer memory
	// this memory is automatically updated when 'NvFlexGet' is called
	hosts[name] = NvFlexMap(buffer, eNvFlexMapWait);
	memset(hosts[name], 0, type * count);
	NvFlexUnmap(buffer);
};

NvFlexBuffer* FlexSolver::get_buffer(std::string name) {
	return buffers[name];
}

//int diff = Max(n - copy_description->elementCount, 0);
//particles.erase(particles.begin() + diff, particles.end());
//copy_description->elementCount = Min(copy_description->elementCount, n);
void FlexSolver::set_active_particles(int n) {
	copy_description->elementCount = n;
}

void FlexSolver::set_active_diffuse(int n) {
	NvFlexSetDiffuseParticles(solver, NULL, NULL, n);
}

int FlexSolver::get_active_particles() {
	return copy_description->elementCount;
}

int FlexSolver::get_active_diffuse() {
	return ((int*)hosts["diffuse_count"])[0];
}

int FlexSolver::get_max_particles() {
	return solver_description.maxParticles;
}

int FlexSolver::get_max_diffuse_particles() {
	return solver_description.maxDiffuseParticles;
}

int FlexSolver::get_max_contacts() {
	return solver_description.maxContactsPerParticle;
}

void* FlexSolver::get_host(std::string name) {
	return hosts[name];
}

std::vector<FlexMesh>* FlexSolver::get_meshes() {
	return &meshes;
}

void FlexSolver::add_particle(Vector4D pos, Vector vel) {
	if (solver == nullptr) return;
	if (get_active_particles() + particles.size() >= get_max_particles()) return;

	Particle p;
	p.pos = pos / CM_2_INCH;
	p.vel = vel / CM_2_INCH;
	particles.push_back(p);
}


// Handles geometry and particle queue update
bool FlexSolver::pretick(NvFlexMapFlags wait) {
	if (solver == nullptr) return false;

	NvFlexCollisionGeometry* geo = (NvFlexCollisionGeometry*)NvFlexMap(get_buffer("geometry"), wait);
	if (!geo) return false;

	Vector4D* pos = (Vector4D*)NvFlexMap(get_buffer("geometry_pos"), eNvFlexMapWait);
	Vector4D* ppos = (Vector4D*)NvFlexMap(get_buffer("geometry_prevpos"), eNvFlexMapWait);
	Vector4D* ang = (Vector4D*)NvFlexMap(get_buffer("geometry_quat"), eNvFlexMapWait);
	Vector4D* pang = (Vector4D*)NvFlexMap(get_buffer("geometry_prevquat"), eNvFlexMapWait);
	int* flag = (int*)NvFlexMap(get_buffer("geometry_flags"), eNvFlexMapWait);

	// Update collider positions
	for (int i = 0; i < meshes.size(); i++) {
		FlexMesh mesh = meshes[i];

		flag[i] = mesh.get_flags();
		geo[i].triMesh.mesh = mesh.get_id();
		geo[i].triMesh.scale[0] = 1;
		geo[i].triMesh.scale[1] = 1;
		geo[i].triMesh.scale[2] = 1;

		geo[i].convexMesh.mesh = mesh.get_id();
		geo[i].convexMesh.scale[0] = 1;
		geo[i].convexMesh.scale[1] = 1;
		geo[i].convexMesh.scale[2] = 1;

		ppos[i] = mesh.get_ppos();
		pos[i] = mesh.get_pos();

		pang[i] = mesh.get_pang();
		ang[i] = mesh.get_ang();

		meshes[i].update();
	}

	NvFlexUnmap(get_buffer("geometry"));
	NvFlexUnmap(get_buffer("geometry_pos"));
	NvFlexUnmap(get_buffer("geometry_prevpos"));
	NvFlexUnmap(get_buffer("geometry_quat"));
	NvFlexUnmap(get_buffer("geometry_prevquat"));
	NvFlexUnmap(get_buffer("geometry_flags"));

	return true;
}

void FlexSolver::map_particles() {
	if (particles.empty()) return;

	// Only copy what we add
	NvFlexCopyDesc desc;
	desc.dstOffset = copy_description->elementCount;
	desc.elementCount = particles.size();
	desc.srcOffset = desc.dstOffset;

	// map buffers for reading / writing
	Vector4D* positions = (Vector4D*)NvFlexMap(get_buffer("particle_pos"), eNvFlexMapWait);
	Vector* velocities = (Vector*)NvFlexMap(get_buffer("particle_vel"), eNvFlexMapWait);
	int* phases = (int*)NvFlexMap(get_buffer("particle_phase"), eNvFlexMapWait);
	int* active = (int*)NvFlexMap(get_buffer("particle_active"), eNvFlexMapWait);
	
	//Vector4D* positions = (Vector4D*)get_host("particle_pos");
	//Vector* velocities = (Vector*)get_host("particle_vel");
	//int* phases = (int*)get_host("particle_phase");
	//int* active = (int*)get_host("particle_active");

	// Add particle
	for (Particle particle : particles) {
		int n = copy_description->elementCount++;		// n = particle_count; n++
		positions[n] = particle.pos;
		velocities[n] = particle.vel;
		phases[n] = NvFlexMakePhase(0, eNvFlexPhaseSelfCollide | eNvFlexPhaseFluid);
		active[n] = n;

		// fixes visual flashing
		((Vector4D*)hosts["particle_smooth"])[n] = particle.pos;
		((Vector4D*)hosts["particle_ani0"])[n] = Vector4D(0, 0, 0, 0);
		((Vector4D*)hosts["particle_ani1"])[n] = Vector4D(0, 0, 0, 0);
		((Vector4D*)hosts["particle_ani2"])[n] = Vector4D(0, 0, 0, 0);
	}

	// unmap buffers
	NvFlexUnmap(get_buffer("particle_pos"));
	NvFlexUnmap(get_buffer("particle_vel"));
	NvFlexUnmap(get_buffer("particle_phase"));
	NvFlexUnmap(get_buffer("particle_active"));

	// Update particle information
	NvFlexSetParticles(solver, get_buffer("particle_pos"), &desc);
	NvFlexSetVelocities(solver, get_buffer("particle_vel"), &desc);
	NvFlexSetPhases(solver, get_buffer("particle_phase"), &desc);
	NvFlexSetActive(solver, get_buffer("particle_active"), &desc);
	NvFlexSetActiveCount(solver, copy_description->elementCount);

	particles.clear();
}

// ticks the solver
void FlexSolver::tick(float dt) {
	if (solver == nullptr) return;

	// write to device (async)
	NvFlexSetParams(solver, params);
	NvFlexSetShapes(solver,
		get_buffer("geometry"),
		get_buffer("geometry_pos"),
		get_buffer("geometry_quat"),
		get_buffer("geometry_prevpos"),
		get_buffer("geometry_prevquat"),
		get_buffer("geometry_flags"),
		meshes.size()
	);

	// tick
	NvFlexUpdateSolver(solver, dt * get_parameter("timescale"), (int)get_parameter("substeps"), false);

	// read back (async)
	NvFlexGetParticles(solver, get_buffer("particle_pos"), copy_description);
	NvFlexGetVelocities(solver, get_buffer("particle_vel"), copy_description);
	NvFlexGetPhases(solver, get_buffer("particle_phase"), copy_description);
	NvFlexGetActive(solver, get_buffer("particle_active"), copy_description);
	NvFlexGetDiffuseParticles(solver, get_buffer("diffuse_pos"), get_buffer("diffuse_vel"), get_buffer("diffuse_count"));
	if (get_parameter("coupling") != 0) NvFlexGetContacts(solver, get_buffer("contact_planes"), get_buffer("contact_vel"), get_buffer("contact_indices"), get_buffer("contact_count"));
	if (get_parameter("anisotropy_scale") != 0) NvFlexGetAnisotropy(solver, get_buffer("particle_ani0"), get_buffer("particle_ani1"), get_buffer("particle_ani2"), copy_description);
	if (get_parameter("smoothing") != 0) NvFlexGetSmoothParticles(solver, get_buffer("particle_smooth"), copy_description);
}

void FlexSolver::add_mesh(FlexMesh mesh) {
	if (solver == nullptr) return;

	meshes.push_back(mesh);
}

// TODO(?): Use a linked list instead of a vector
void FlexSolver::remove_mesh(int id) {
	if (solver == nullptr) return;

	// TODO: Optimize
	for (int i = meshes.size() - 1; i >= 0; i--) {
		if (meshes[i].get_entity_id() == id) {
			// Free mesh buffers
			meshes[i].destroy(library);
			meshes.erase(meshes.begin() + i);
		}
	}
}

// sets the position and angles of a mesh object. The inputted angle is Eular
void FlexSolver::update_mesh(int index, Vector new_pos, QAngle new_ang) {
	if (solver == nullptr) return;
	if (index < 0 || index >= meshes.size()) return;	// Invalid

	meshes[index].set_pos(new_pos);
	meshes[index].set_ang(new_ang);
}

bool FlexSolver::set_parameter(std::string param, float number) {
	try {
		*param_map.at(param) = number;
		return true;
	}
	catch (std::exception e) {
		if (param == "iterations") {	// defined as an int instead of a float, so it needs to be seperate
			params->numIterations = (int)number;
			return true;
		}
		return false;
	}
}

// Returns NaN on failure
float FlexSolver::get_parameter(std::string param) {
	try {
		return *param_map.at(param);
	}
	catch (std::exception e) {
		if (param == "iterations") {	// ^
			return (float)params->numIterations;
		}
		return NAN;
	}
}

// Initializes a box around a FleX solver with a mins and maxs
void FlexSolver::enable_bounds(Vector mins, Vector maxs) {

	// Right
	params->planes[0][0] = 1.f;
	params->planes[0][1] = 0.f;
	params->planes[0][2] = 0.f;
	params->planes[0][3] = -mins.x;

	// Left
	params->planes[1][0] = -1.f;
	params->planes[1][1] = 0.f;
	params->planes[1][2] = 0.f;
	params->planes[1][3] = maxs.x;

	// Forward
	params->planes[2][0] = 0.f;
	params->planes[2][1] = 1.f;
	params->planes[2][2] = 0.f;
	params->planes[2][3] = -mins.y;

	// Backward
	params->planes[3][0] = 0.f;
	params->planes[3][1] = -1.f;
	params->planes[3][2] = 0.f;
	params->planes[3][3] = maxs.y;

	// Bottom
	params->planes[4][0] = 0.f;
	params->planes[4][1] = 0.f;
	params->planes[4][2] = 1.f;
	params->planes[4][3] = -mins.z;

	// Top
	params->planes[5][0] = 0.f;
	params->planes[5][1] = 0.f;
	params->planes[5][2] = -1.f;
	params->planes[5][3] = maxs.z;

	params->numPlanes = 6;
}

void FlexSolver::disable_bounds() {
	params->numPlanes = 0;
}

// Initializes a solver in a FleX library
FlexSolver::FlexSolver(NvFlexLibrary* library, int particles) {
	if (library == nullptr) return;		// Panic

	NvFlexSetSolverDescDefaults(&solver_description);
	solver_description.maxParticles = particles;
	solver_description.maxDiffuseParticles = particles;

	this->library = library;
	solver = NvFlexCreateSolver(library, &solver_description);

	params = new NvFlexParams();
	params->gravity[0] = 0.0f;
	params->gravity[1] = 0.0f;
	params->gravity[2] = -15.24f;	// Earth gravity * Source Units

	params->wind[0] = 0.0f;
	params->wind[1] = 0.0f;
	params->wind[2] = 0.0f;

	params->radius = 0.20f;
	params->viscosity = 0.0f;
	params->dynamicFriction = 0.5f;
	params->staticFriction = 0.0f;
	params->particleFriction = 0.0f;
	params->freeSurfaceDrag = 0.0f;
	params->drag = 0.0f;
	params->lift = 0.0f;
	params->numIterations = 3;
	params->fluidRestDistance = params->radius * 0.65f;
	params->solidRestDistance = params->radius * 0.65f;

	params->anisotropyScale = 1.f;
	params->anisotropyMin = 0.1f;
	params->anisotropyMax = 2.f;
	params->smoothing = 1.0f;

	params->dissipation = 0.f;
	params->damping = 0.0f;
	params->particleCollisionMargin = 0.f;
	params->shapeCollisionMargin = 0.f;	// Increase if lots of water pressure is expected. Higher values cause more collision clipping
	params->collisionDistance = params->radius * 0.5f; // Needed for tri-particle intersection
	params->sleepThreshold = 0.1f;
	params->shockPropagation = 0.0f;
	params->restitution = 0.0f;

	params->maxSpeed = FLT_MAX;
	params->maxAcceleration = 100.0f;
	params->relaxationMode = eNvFlexRelaxationLocal;
	params->relaxationFactor = 1.0f;
	params->solidPressure = 1.0f;
	params->adhesion = 0.0f;
	params->cohesion = 0.05f;
	params->surfaceTension = 0.1f;
	params->vorticityConfinement = 50.0f;
	params->buoyancy = 1.0f;

	params->diffuseThreshold = 100000.0f;
	params->diffuseBuoyancy = 1.f;
	params->diffuseDrag = 0.8f;
	params->diffuseBallistic = 16;
	params->diffuseLifetime = 1.f;	// yes actually in seconds

	params->numPlanes = 5;

	param_map["gravity"] = &(params->gravity[2]);
	param_map["radius"] = &params->radius;
	param_map["viscosity"] = &params->viscosity;
	param_map["dynamic_friction"] = &params->dynamicFriction;
	param_map["static_friction"] = &params->staticFriction;
	param_map["particle_friction"] = &params->particleFriction;
	param_map["free_surface_drag"] = &params->freeSurfaceDrag;
	param_map["drag"] = &params->drag;
	param_map["lift"] = &params->lift;
	//param_map["iterations"] = &params->numIterations;				// integer, cant map
	param_map["fluid_rest_distance"] = &params->fluidRestDistance;
	param_map["solid_rest_distance"] = &params->solidRestDistance;
	param_map["anisotropy_scale"] = &params->anisotropyScale;
	param_map["anisotropy_min"] = &params->anisotropyMin;
	param_map["anisotropy_max"] = &params->anisotropyMax;
	param_map["smoothing"] = &params->smoothing;
	param_map["dissipation"] = &params->dissipation;
	param_map["damping"] = &params->damping;
	param_map["particle_collision_margin"] = &params->particleCollisionMargin;
	param_map["shape_collision_margin"] = &params->shapeCollisionMargin;
	param_map["collision_distance"] = &params->collisionDistance;
	param_map["sleep_threshold"] = &params->sleepThreshold;
	param_map["shock_propagation"] = &params->shockPropagation;
	param_map["restitution"] = &params->restitution;
	param_map["max_speed"] = &params->maxSpeed;
	param_map["max_acceleration"] = &params->maxAcceleration;
	//param_map["relaxation_mode"] = &params->relaxationMode;		// ^
	param_map["relaxation_factor"] = &params->relaxationFactor;
	param_map["solid_pressure"] = &params->solidPressure;
	param_map["adhesion"] = &params->adhesion;
	param_map["cohesion"] = &params->cohesion;
	param_map["surface_tension"] = &params->surfaceTension;
	param_map["vorticity_confinement"] = &params->vorticityConfinement;
	param_map["buoyancy"] = &params->buoyancy;
	param_map["diffuse_threshold"] = &params->diffuseThreshold;
	param_map["diffuse_buoyancy"] = &params->diffuseBuoyancy;
	param_map["diffuse_drag"] = &params->diffuseDrag;
	//param_map["diffuse_ballistic"] = &params->diffuseBallistic;	// ^
	param_map["diffuse_lifetime"] = &params->diffuseLifetime;
	// Extra values we store which are not stored in flexes default parameters
	param_map["substeps"] = new float(3);
	param_map["timescale"] = new float(1);
	param_map["coupling"] = new float(0);
	param_map["anisotropy_downscale"] = new float(10);

	// FleX GPU Buffers
	add_buffer("particle_pos", sizeof(Vector4D), particles);
	add_buffer("particle_vel", sizeof(Vector), particles);
	add_buffer("particle_phase", sizeof(int), particles);
	add_buffer("particle_active", sizeof(int), particles);
	add_buffer("particle_smooth", sizeof(Vector4D), particles);

	add_buffer("geometry", sizeof(NvFlexCollisionGeometry), MAX_COLLIDERS);
	add_buffer("geometry_pos", sizeof(Vector4D), MAX_COLLIDERS);
	add_buffer("geometry_prevpos", sizeof(Vector4D), MAX_COLLIDERS);
	add_buffer("geometry_quat", sizeof(Vector4D), MAX_COLLIDERS);
	add_buffer("geometry_prevquat", sizeof(Vector4D), MAX_COLLIDERS);
	add_buffer("geometry_flags", sizeof(int), MAX_COLLIDERS);

	add_buffer("contact_planes", sizeof(Vector4D), particles * get_max_contacts());
	add_buffer("contact_vel", sizeof(Vector4D), particles * get_max_contacts());
	add_buffer("contact_count", sizeof(int), particles);
	add_buffer("contact_indices", sizeof(int), particles);

	add_buffer("particle_ani0", sizeof(Vector4D), particles);
	add_buffer("particle_ani1", sizeof(Vector4D), particles);
	add_buffer("particle_ani2", sizeof(Vector4D), particles);

	add_buffer("diffuse_pos", sizeof(Vector4D), solver_description.maxDiffuseParticles);
	add_buffer("diffuse_vel", sizeof(Vector4D), solver_description.maxDiffuseParticles);
	add_buffer("diffuse_count", sizeof(int), 1);	// "this may be updated by the GPU which is why it is passed back in a buffer"
};

// Free memory
FlexSolver::~FlexSolver() {
	if (solver == nullptr) return;

	// Free props
	for (FlexMesh mesh : meshes)
		mesh.destroy(library);
	meshes.clear();

	delete param_map["substeps"];					// Seperate since its externally stored & not a default parameter
	delete param_map["timescale"];					// ^
	delete param_map["coupling"];					// ^
	delete param_map["anisotropy_downscale"];		// ^
	delete params;
	delete copy_description;

	// Free buffers / hosts
	for (std::pair<std::string, NvFlexBuffer*> buffer : buffers) 
		NvFlexFreeBuffer(buffer.second);

	NvFlexDestroySolver(solver);	// bye bye solver
	solver = nullptr;
}