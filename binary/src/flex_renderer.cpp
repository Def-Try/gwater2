#include "flex_renderer.h"

#define CM_2_INCH 39.3701f

//extern IMaterialSystem* materials = NULL;	// stops main branch compile from bitching

// lord have mercy brothers
void FlexRenderer::build_water(FlexSolver* solver, float radius) {

	if (solver == nullptr) return;

	// Clear previous imeshes since they are being rebuilt
	IMatRenderContext* render_context = materials->GetRenderContext();
	for (IMesh* mesh : water) {
		render_context->DestroyStaticMesh(mesh);
	}
	water.clear();
	
	int max_particles = solver->get_active_particles();
	if (max_particles == 0) return;

	// View matrix, used in frustrum culling
	VMatrix view_matrix, projection_matrix, view_projection_matrix;
	render_context->GetMatrix(MATERIAL_VIEW, &view_matrix);
	render_context->GetMatrix(MATERIAL_PROJECTION, &projection_matrix);
	MatrixMultiply(projection_matrix, view_matrix, view_projection_matrix);
	
	// Get eye position for sprite calculations
	Vector eye_pos; render_context->GetWorldSpaceCameraPosition(&eye_pos);

	float u[3] = { 0.5 - SQRT3 / 2, 0.5, 0.5 + SQRT3 / 2};
	float v[3] = { 1, -0.5, 1 };

	Vector4D* particle_positions = solver->get_parameter("smoothing") != 0 ? (Vector4D*)solver->get_host("particle_smooth") : (Vector4D*)solver->get_host("particle_pos");
	Vector4D* particle_ani1 = (Vector4D*)solver->get_host("particle_ani1");
	Vector4D* particle_ani2 = (Vector4D*)solver->get_host("particle_ani2");
	Vector4D* particle_ani3 = (Vector4D*)solver->get_host("particle_ani3");
	bool particle_ani = solver->get_parameter("anisotropy_scale") != 0;

	// Create meshes and iterates through particles. We also need to abide by the source limits of 2^15 max vertices per mesh
	// Does so in this structure:

	// for (particle through particles) {
	//	create_mesh()
	//	for (primative = 0 through maxprimatives) {
	//    particle++
	//    if frustrum {continue}
	//    primative++
	//  }
	// }

	/*Vector forward = Vector(view_matrix[2][0], view_matrix[2][1], view_matrix[2][2]);
	Vector right = Vector(view_matrix[0][0], view_matrix[0][1], view_matrix[0][2]);
	Vector up = Vector(view_matrix[1][0], view_matrix[1][1], view_matrix[1][2]);
	Vector local_pos[3] = { (-up - right * SQRT3), up * 2.0, (-up + right * SQRT3) };*/

	CMeshBuilder mesh_builder;
	for (int particle_index = 0; particle_index < max_particles;) {
		IMesh* imesh = render_context->CreateStaticMesh(VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TEXCOORD0_2D, "");
		mesh_builder.Begin(imesh, MATERIAL_TRIANGLES, MAX_PRIMATIVES);
			for (int primative = 0; primative < MAX_PRIMATIVES && particle_index < max_particles; particle_index++) {
				Vector particle_pos = particle_positions[particle_index].AsVector3D();

				// Frustrum culling
				Vector4D dst;
				Vector4DMultiply(view_projection_matrix, Vector4D(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH, 1), dst);
				if (dst.z < 0 || -dst.x - dst.w > 0 || dst.x - dst.w > 0 || -dst.y - dst.w > 0 || dst.y - dst.w > 0) {
					continue;
				}

				// calculate triangle rotation
				//Vector forward = (eye_pos - particle_pos).Normalized();
				Vector forward = ((particle_pos * CM_2_INCH) - eye_pos).Normalized();
				Vector right = forward.Cross(Vector(0, 0, 1)).Normalized();
				Vector up = right.Cross(forward);
				Vector local_pos[3] = { (-up - right * SQRT3), up * 2.0, (-up + right * SQRT3) };
				
				if (particle_ani) {
					Vector4D ani1 = particle_ani1[particle_index];
					Vector4D ani2 = particle_ani2[particle_index];
					Vector4D ani3 = particle_ani3[particle_index];

					for (int i = 0; i < 3; i++) {
						// Anisotropy warping (code provided by Spanky)
						Vector pos_ani = local_pos[i] * radius;
						pos_ani = pos_ani + ani1.AsVector3D() * (local_pos[i].Dot(ani1.AsVector3D()) * ani1.w);
						pos_ani = pos_ani + ani2.AsVector3D() * (local_pos[i].Dot(ani2.AsVector3D()) * ani2.w);
						pos_ani = pos_ani + ani3.AsVector3D() * (local_pos[i].Dot(ani3.AsVector3D()) * ani3.w);

						Vector world_pos = particle_pos + pos_ani;
						mesh_builder.TexCoord2f(0, u[i], v[i]);
						mesh_builder.Position3f(world_pos.x * CM_2_INCH, world_pos.y * CM_2_INCH, world_pos.z * CM_2_INCH);
						mesh_builder.Normal3f(-forward.x, -forward.y, -forward.z);
						mesh_builder.AdvanceVertex();
					}
				} else {
					for (int i = 0; i < 3; i++) { // Same as above w/o anisotropy warping
						Vector world_pos = particle_pos + local_pos[i] * radius;
						mesh_builder.TexCoord2f(0, u[i], v[i]);
						mesh_builder.Position3f(world_pos.x * CM_2_INCH, world_pos.y * CM_2_INCH, world_pos.z * CM_2_INCH);
						mesh_builder.Normal3f(-forward.x, -forward.y, -forward.z);
						mesh_builder.AdvanceVertex();
					}
				}

				primative += 1;
			}
		mesh_builder.End();
		mesh_builder.Reset();
		water.push_back(imesh);
	}
};

void FlexRenderer::build_diffuse(FlexSolver* solver, float radius) {
	if (solver == nullptr) return;

	// Clear previous imeshes since they are being rebuilt
	IMatRenderContext* render_context = materials->GetRenderContext();
	for (IMesh* mesh : diffuse) {
		render_context->DestroyStaticMesh(mesh);
	}	
	diffuse.clear();

	int max_particles = ((int*)solver->get_host("diffuse_count"))[0];
	if (max_particles == 0) return;

	// View matrix, used in frustrum culling
	VMatrix view_matrix, projection_matrix, view_projection_matrix;
	render_context->GetMatrix(MATERIAL_VIEW, &view_matrix);
	render_context->GetMatrix(MATERIAL_PROJECTION, &projection_matrix);
	MatrixMultiply(projection_matrix, view_matrix, view_projection_matrix);
	
	// Get eye position for sprite calculations
	Vector eye_pos; render_context->GetWorldSpaceCameraPosition(&eye_pos);

	float u[3] = { 0.5 - SQRT3 / 2, 0.5, 0.5 + SQRT3 / 2 };
	float v[3] = { 1, -0.5, 1 };
	float inv_max_lifetime = 1.f / solver->get_parameter("diffuse_lifetime");
	float particle_scale = solver->get_parameter("timescale") * 0.0016;

	Vector4D* particle_positions = (Vector4D*)solver->get_host("diffuse_pos");
	Vector4D* particle_velocities = (Vector4D*)solver->get_host("diffuse_vel");

	Vector forward = Vector(view_matrix[2][0], view_matrix[2][1], view_matrix[2][2]);
	Vector right = Vector(view_matrix[0][0], view_matrix[0][1], view_matrix[0][2]);
	Vector up = Vector(view_matrix[1][0], view_matrix[1][1], view_matrix[1][2]);
	Vector local_pos[3] = { (-up - right * SQRT3), up * 2.0, (-up + right * SQRT3) };

	CMeshBuilder mesh_builder;
	for (int particle_index = 0; particle_index < max_particles;) {
		IMesh* imesh = render_context->CreateStaticMesh(VERTEX_POSITION | VERTEX_NORMAL | VERTEX_TEXCOORD0_2D, "");
		mesh_builder.Begin(imesh, MATERIAL_TRIANGLES, MAX_PRIMATIVES);
		for (int primative = 0; primative < MAX_PRIMATIVES && particle_index < max_particles; particle_index++) {
			Vector particle_pos = particle_positions[particle_index].AsVector3D() * CM_2_INCH;

			// Frustrum culling
			Vector4D dst;
			Vector4DMultiply(view_projection_matrix, Vector4D(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH, 1), dst);
			if (dst.z < 0 || -dst.x - dst.w > 0 || dst.x - dst.w > 0 || -dst.y - dst.w > 0 || dst.y - dst.w > 0) {
				continue;
			}

			for (int i = 0; i < 3; i++) {
				Vector pos_ani = local_pos[i];	// Warp based on velocity
				pos_ani = pos_ani + (particle_velocities[particle_index].AsVector3D() * pos_ani.Dot(particle_velocities[particle_index].AsVector3D()) * particle_scale).Min(Vector(3, 3, 3)).Max(Vector(-3, -3, -3));

				float lifetime = particle_positions[particle_index].w * inv_max_lifetime;	// scale bubble size by life left
				Vector world_pos = particle_pos + pos_ani * radius * lifetime;

				// Todo: somehow only apply the rotation stuff to mist
				//float u1 = ((u[i] - 0.5) * cos(lifetime * 8) + (v[i] - 0.5) * sin(lifetime * 8)) + 0.5;
				//float v1 = ((u[i] - 0.5) * sin(lifetime * 8) - (v[i] - 0.5) * cos(lifetime * 8)) + 0.5;
				mesh_builder.TexCoord2f(0, u[i], v[i]);
				mesh_builder.Position3f(world_pos.x * CM_2_INCH, world_pos.y * CM_2_INCH, world_pos.z * CM_2_INCH);
				mesh_builder.Normal3f(-forward.x, -forward.y, -forward.z);
				mesh_builder.AdvanceVertex();
			}

			primative += 1;
		}
		mesh_builder.End();
		mesh_builder.Reset();
		diffuse.push_back(imesh);
	}
};

void FlexRenderer::draw_diffuse() {
	for (IMesh* mesh : diffuse) { 
		mesh->Draw();
	}
};

void FlexRenderer::draw_water() {
	for (IMesh* mesh : water) {
		mesh->Draw();
	}
};

FlexRenderer::~FlexRenderer() {
	IMatRenderContext* render_context = materials->GetRenderContext();
	for (IMesh* mesh : water) {
		render_context->DestroyStaticMesh(mesh);
	}

	for (IMesh* mesh : diffuse) {
		render_context->DestroyStaticMesh(mesh);
	}

	water.clear();
	diffuse.clear();
};