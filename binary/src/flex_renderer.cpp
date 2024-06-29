#include "flex_renderer.h"
#include "cdll_client_int.h"

extern IVEngineClient* engine = NULL;

#define CM_2_INCH 39.3701f

//extern IMaterialSystem* materials = NULL;	// stops main branch compile from bitching

// Not sure why this isn't defined in the standard math library
#define min(a, b) a < b ? a : b

float u[3] = { 0.5 - SQRT3 / 2, 0.5, 0.5 + SQRT3 / 2 };
float v[3] = { 1, -0.5, 1 };

// Builds meshes of water particles with anisotropy
IMesh* _build_water_anisotropy(int id, FlexRendererThreadData data) {
	int start = id * MAX_PRIMATIVES;
	int end = min((id + 1) * MAX_PRIMATIVES, data.max_particles);

	// We need to figure out how many and which particles are going to be rendered
	int particles_to_render = 0;
	for (int particle_index = start; particle_index < end; ++particle_index) {
		Vector particle_pos = data.particle_positions[particle_index].AsVector3D();

		// Frustrum culling
		Vector4D dst;
		Vector4DMultiply(data.view_projection_matrix, Vector4D(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH, 1), dst);
		if (dst.z < data.radius || -dst.x - dst.w > data.radius * CM_2_INCH || dst.x - dst.w > data.radius * CM_2_INCH || -dst.y - dst.w > data.radius * CM_2_INCH || dst.y - dst.w > data.radius * CM_2_INCH) continue;

		// PVS Culling
		if (!engine->IsBoxVisible(Vector(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH), Vector(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH))) continue;
		
		// Add to our buffer
		data.render_buffer[start + particles_to_render] = particle_index;
		particles_to_render++;
	}

	// TODO: Try probe a bunch of points in a grid for lighting and blend smoothly between them
	// Vector colour = engine->GetLightForPoint(particle_pos, false);

	// Don't even bother
	if (particles_to_render == 0) return nullptr;

	float scale_mult = data.anisotropy_downscale / data.radius;	// no fucking clue why this works
	float inv_scale_mult = data.radius / data.anisotropy_downscale;	// microoptimization

	IMesh* mesh = materials->GetRenderContext()->CreateStaticMesh(VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TEXCOORD0_2D, "");
	CMeshBuilder mesh_builder;
	mesh_builder.Begin(mesh, MATERIAL_TRIANGLES, particles_to_render);
	for (int i = start; i < start + particles_to_render; ++i) {
		int particle_index = data.render_buffer[i];
		Vector particle_pos = data.particle_positions[particle_index].AsVector3D();

		// calculate triangle rotation
		//Vector forward = (eye_pos - particle_pos).Normalized();
		Vector forward = ((particle_pos * CM_2_INCH) - data.eye_pos).Normalized();
		Vector right = forward.Cross(Vector(0, 0, 1)).Normalized();
		Vector up = right.Cross(forward);
		Vector local_pos[3] = { (-up - right * SQRT3), up * 2.0, (-up + right * SQRT3) };	// equalateral triangle

		Vector4D ani0 = data.particle_ani0[particle_index]; ani0.w *= scale_mult;
		Vector4D ani1 = data.particle_ani1[particle_index]; ani1.w *= scale_mult;
		Vector4D ani2 = data.particle_ani2[particle_index]; ani2.w *= scale_mult;

		for (int i = 0; i < 3; i++) {
			// Anisotropy warping (code provided by Spanky)
			Vector pos_ani = local_pos[i] * inv_scale_mult;
			float dot0 = pos_ani.Dot(ani0.AsVector3D()) * ani0.w;
			float dot1 = pos_ani.Dot(ani1.AsVector3D()) * ani1.w;
			float dot2 = pos_ani.Dot(ani2.AsVector3D()) * ani2.w;

			pos_ani += ani0.AsVector3D() * dot0 + ani1.AsVector3D() * dot1 + ani2.AsVector3D() * dot2;

			Vector world_pos = particle_pos + pos_ani;
			mesh_builder.TexCoord2f(0, u[i], v[i]);
			mesh_builder.Position3f(world_pos.x * CM_2_INCH, world_pos.y * CM_2_INCH, world_pos.z * CM_2_INCH);
			mesh_builder.Normal3f(-forward.x, -forward.y, -forward.z);
			mesh_builder.AdvanceVertex();
		}
	}
	mesh_builder.End();

	return mesh;
}

// Builds meshes of water particles without anisotropy
IMesh* _build_water(int id, FlexRendererThreadData data) {
	int start = id * MAX_PRIMATIVES;
	int end = min((id + 1) * MAX_PRIMATIVES, data.max_particles);

	// We need to figure out how many and which particles are going to be rendered
	int particles_to_render = 0;
	for (int particle_index = start; particle_index < end; ++particle_index) {
		Vector particle_pos = data.particle_positions[particle_index].AsVector3D();

		// Frustrum culling
		Vector4D dst;
		Vector4DMultiply(data.view_projection_matrix, Vector4D(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH, 1), dst);
		if (dst.z < data.radius || -dst.x - dst.w > data.radius * CM_2_INCH || dst.x - dst.w > data.radius * CM_2_INCH || -dst.y - dst.w > data.radius * CM_2_INCH || dst.y - dst.w > data.radius * CM_2_INCH) continue;

		// PVS Culling
		if (!engine->IsBoxVisible(Vector(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH), Vector(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH))) continue;

		// Add to our buffer
		data.render_buffer[start + particles_to_render] = particle_index;
		particles_to_render++;
	}

	// Don't even bother
	if (particles_to_render == 0) return nullptr;

	IMesh* mesh = materials->GetRenderContext()->CreateStaticMesh(VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TEXCOORD0_2D, "");
	CMeshBuilder mesh_builder;
	mesh_builder.Begin(mesh, MATERIAL_TRIANGLES, particles_to_render);
	for (int i = start; i < start + particles_to_render; ++i) {
		int particle_index = data.render_buffer[i];
		Vector particle_pos = data.particle_positions[particle_index].AsVector3D();

		// calculate triangle rotation
		//Vector forward = (eye_pos - particle_pos).Normalized();
		Vector forward = (particle_pos - data.eye_pos).Normalized();
		Vector right = forward.Cross(Vector(0, 0, 1)).Normalized();
		Vector up = right.Cross(forward);
		Vector local_pos[3] = { (-up - right * SQRT3) * 0.5, up, (-up + right * SQRT3) * 0.5 };	

		for (int i = 0; i < 3; i++) { // Same as above w/o anisotropy warping
			Vector world_pos = particle_pos + local_pos[i] * data.radius;
			mesh_builder.TexCoord2f(0, u[i], v[i]);
			mesh_builder.Position3f(world_pos.x * CM_2_INCH, world_pos.y * CM_2_INCH, world_pos.z * CM_2_INCH);
			mesh_builder.Normal3f(-forward.x, -forward.y, -forward.z);
			mesh_builder.AdvanceVertex();
		}
	}
	mesh_builder.End();

	return mesh;
}

// Builds meshes of diffuse particles (scaled by velocity, buffer shoved inside ani0)
IMesh* _build_diffuse(int id, FlexRendererThreadData data) {
	int start = id * MAX_PRIMATIVES;
	int end = min((id + 1) * MAX_PRIMATIVES, data.max_particles);

	// We need to figure out how many and which particles are going to be rendered
	int particles_to_render = 0;
	for (int particle_index = start; particle_index < end; ++particle_index) {
		Vector particle_pos = data.particle_positions[particle_index].AsVector3D();

		// Frustrum culling
		Vector4D dst;
		Vector4DMultiply(data.view_projection_matrix, Vector4D(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH, 1), dst);
		if (dst.z < 0 || -dst.x - dst.w > 0 || dst.x - dst.w > 0 || -dst.y - dst.w > 0 || dst.y - dst.w > 0) continue;

		// PVS Culling
		if (!engine->IsBoxVisible(Vector(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH), Vector(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH))) continue;

		// Add to our buffer
		data.render_buffer[start + particles_to_render] = particle_index;
		particles_to_render++;
	}

	// Don't even bother
	if (particles_to_render == 0) return nullptr;

	float particle_scale = 1 * (0.0016 * CM_2_INCH); // Timescale should go here
	IMesh* mesh = materials->GetRenderContext()->CreateStaticMesh(VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TEXCOORD0_2D, "");
	CMeshBuilder mesh_builder;
	mesh_builder.Begin(mesh, MATERIAL_TRIANGLES, particles_to_render);
	for (int i = start; i < start + particles_to_render; ++i) {
		int particle_index = data.render_buffer[i];
		Vector particle_pos = data.particle_positions[particle_index].AsVector3D();

		// calculate triangle rotation
		//Vector forward = (eye_pos - particle_pos).Normalized();
		Vector forward = (particle_pos - data.eye_pos).Normalized();
		Vector right = forward.Cross(Vector(0, 0, 1)).Normalized();
		Vector up = right.Cross(forward);
		Vector local_pos[3] = { (-up - right * SQRT3), up * 2.0, (-up + right * SQRT3) };

		Vector4D ani0 = data.particle_ani0[particle_index];
		float scalar = data.radius * data.particle_positions[particle_index].w;

		for (int i = 0; i < 3; i++) {
			Vector pos_ani = local_pos[i];	// Warp based on velocity
			pos_ani = pos_ani + (data.particle_ani0[particle_index].AsVector3D() * pos_ani.Dot(data.particle_ani0[particle_index].AsVector3D()) * particle_scale).Min(Vector(3, 3, 3)).Max(Vector(-3, -3, -3));

			Vector world_pos = particle_pos + pos_ani * scalar;
			mesh_builder.TexCoord2f(0, u[i], v[i]);
			mesh_builder.Position3f(world_pos.x * CM_2_INCH, world_pos.y * CM_2_INCH, world_pos.z * CM_2_INCH);
			mesh_builder.Normal3f(-forward.x, -forward.y, -forward.z);
			mesh_builder.AdvanceVertex();
		}
	}
	mesh_builder.End();

	return mesh;
}

// lord have mercy brothers

// Launches 1 thread for each mesh. particles are split into meshes with MAX_PRIMATIVES number of primatives
void FlexRenderer::build_meshes(FlexSolver* flex, float diffuse_radius) {
	// Clear previous imeshes since they are being rebuilt
	destroy_meshes();

	int max_particles = flex->get_active_particles();
	if (max_particles == 0) return;

	IMatRenderContext* render_context = materials->GetRenderContext();

	// View matrix, used in frustrum culling
	VMatrix view_matrix, projection_matrix, view_projection_matrix;
	render_context->GetMatrix(MATERIAL_VIEW, &view_matrix);
	render_context->GetMatrix(MATERIAL_PROJECTION, &projection_matrix);
	MatrixMultiply(projection_matrix, view_matrix, view_projection_matrix);

	// Get eye position for sprite calculations
	Vector eye_pos; render_context->GetWorldSpaceCameraPosition(&eye_pos);

	Vector4D* particle_positions = flex->get_parameter("smoothing") != 0 ? (Vector4D*)flex->get_host("particle_smooth") : (Vector4D*)flex->get_host("particle_pos");
	Vector4D* particle_ani0 = (Vector4D*)flex->get_host("particle_ani0");
	Vector4D* particle_ani1 = (Vector4D*)flex->get_host("particle_ani1");
	Vector4D* particle_ani2 = (Vector4D*)flex->get_host("particle_ani2");
	bool particle_ani = flex->get_parameter("anisotropy_scale") != 0;	// Should we do anisotropy calculations?
	float anisotropy_downscale = flex->get_parameter("anisotropy_downscale");
	float radius = flex->get_parameter("radius");

	// Water particles
	int max_meshes = min(ceil(max_particles / (float)MAX_PRIMATIVES), allocated);
	for (int mesh_index = 0; mesh_index < max_meshes; mesh_index++) {
		// update thread data
		FlexRendererThreadData data;
		data.eye_pos = eye_pos;
		data.view_projection_matrix = view_projection_matrix;
		data.particle_positions = particle_positions;
		data.max_particles = max_particles;
		data.radius = radius;
		data.anisotropy_downscale = anisotropy_downscale;
		data.particle_ani0 = particle_ani0;
		data.particle_ani1 = particle_ani1;
		data.particle_ani2 = particle_ani2;
		data.render_buffer = water_buffer;

		// Launch thread
		if (particle_ani) {
			queue[mesh_index] = threads->enqueue(_build_water_anisotropy, mesh_index, data);
		} else {
			queue[mesh_index] = threads->enqueue(_build_water, mesh_index, data);
		}
	}

	// Diffuse particles
	max_particles = flex->get_active_diffuse();
	if (max_particles == 0) return;

	Vector4D* diffuse_positions = (Vector4D*)flex->get_host("diffuse_pos");
	Vector4D* diffuse_velocities = (Vector4D*)flex->get_host("diffuse_vel");
	float radius_mult = radius * diffuse_radius / flex->get_parameter("diffuse_lifetime");

	max_meshes = min(ceil(max_particles / (float)MAX_PRIMATIVES), allocated);
	for (int mesh_index = 0; mesh_index < max_meshes; mesh_index++) {
		// update thread data
		FlexRendererThreadData data;
		data.eye_pos = eye_pos;
		data.view_projection_matrix = view_projection_matrix;
		data.particle_positions = diffuse_positions;
		data.max_particles = max_particles;
		data.radius = radius_mult;
		data.particle_ani0 = diffuse_velocities;
		data.render_buffer = diffuse_buffer;

		// Launch thread
		queue[mesh_index + allocated] = threads->enqueue(_build_diffuse, mesh_index, data);
	}
};

void FlexRenderer::update_meshes() {
	IMatRenderContext* render_context = materials->GetRenderContext();
	for (int mesh = 0; mesh < allocated * 2; mesh++) {
		if (queue[mesh].valid()) {
			if (meshes[mesh] != nullptr) render_context->DestroyStaticMesh(meshes[mesh]);
			meshes[mesh] = queue[mesh].get();
		}
	}
}

void FlexRenderer::draw_water() {
	update_meshes();	// Update status of water meshes (join threads)

	IMatRenderContext* render_context = materials->GetRenderContext();
	for (int mesh = 0; mesh < allocated; mesh++) {
		if (meshes[mesh] == nullptr) continue;

		meshes[mesh]->Draw();
	}
};

void FlexRenderer::draw_diffuse() {
	update_meshes();

	IMatRenderContext* render_context = materials->GetRenderContext();
	for (int mesh = allocated; mesh < allocated * 2; mesh++) {
		if (meshes[mesh] == nullptr) continue;

		meshes[mesh]->Draw();
	}
};

void FlexRenderer::destroy_meshes() {
	IMatRenderContext* render_context = materials->GetRenderContext();
	for (int mesh = 0; mesh < allocated * 2; mesh++) {
		if (meshes[mesh] == nullptr) continue;

		render_context->DestroyStaticMesh(meshes[mesh]);
		meshes[mesh] = nullptr;
	}
}

// Allocate buffers
FlexRenderer::FlexRenderer(int max_meshes) {
	allocated = max_meshes;

	// Water meshes take (0 to allocated), diffuse take (allocated to allocated * 2)
	meshes = (IMesh**)calloc(sizeof(IMesh*), allocated * 2);
	if (!meshes) return;	// TODO: Fix undefined behavior if this statement runs

	threads = new ThreadPool(allocated * 2);

	queue = (std::future<IMesh*>*)calloc(sizeof(std::future<IMesh*>), allocated * 2);	// Needs to be zero initialized
	if (!queue) return;

	water_buffer = (int*)malloc(sizeof(int) * allocated * MAX_PRIMATIVES);
	if (!water_buffer) return;

	diffuse_buffer = (int*)malloc(sizeof(int) * allocated * MAX_PRIMATIVES);
	if (!diffuse_buffer) return;
};

FlexRenderer::~FlexRenderer() {
	if (meshes == nullptr) return;	// Never allocated (out of ram?)

	//if (materials->GetRenderContext() == nullptr) return;	// wtf?
	// Destroy existing meshes
	destroy_meshes();
	
	// Redestroy water that was being built in threads
	update_meshes();
	destroy_meshes();

	delete threads;

	if (water_buffer != nullptr) free(water_buffer);
	if (diffuse_buffer != nullptr) free(diffuse_buffer);
	if (meshes != nullptr) free(meshes);
	if (queue != nullptr) free(queue);
};