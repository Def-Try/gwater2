#include "flex_renderer.h"

#define CM_2_INCH 39.3701f

//extern IMaterialSystem* materials = NULL;	// stops main branch compile from bitching

bool solveQuadratic(float a, float b, float c, float &minT, float &maxT)
{
	if (a == 0.0 && b == 0.0)
	{
		minT = 0.0;
		maxT = 0.0;
		return false;
	}

	float discriminant = b * b - 4.0 * a * c;

	if (discriminant < 0.0)
	{
		return false;
	}

	float t = -0.5 * (b + Sign(b) * sqrt(discriminant));
	minT = t / a;
	maxT = c / t;

	if (minT > maxT)
	{
		float tmp = minT;
		minT = maxT;
		maxT = tmp;
	}

	return true;
}


float DotInvW(Vector4D a, Vector4D b) { return a.x * b.x + a.y * b.y + a.z * b.z - a.w * b.w; }

/// experimental port of anisotropy vertex shader to c++ dont use this yet
void vertexshadermain(Vector4D q1, Vector4D q2, Vector4D q3, Vector4D worldPos)
{
	// construct quadric matrix
	Vector4D q[4];
	q[0] = Vector4D(q1.x * q1.w, q1.y * q1.w, q1.z * q1.w, 0.0);
	q[1] = Vector4D(q2.x * q2.w, q2.y * q2.w, q2.z * q2.w, 0.0);
	q[2] = Vector4D(q3.x * q3.w, q3.y * q3.w, q3.z * q3.w, 0.0);
	q[3] = Vector4D(worldPos.x, worldPos.y, worldPos.z, 1.0);

	// transforms a normal to parameter space (inverse transpose of (q*modelview)^-T)
	Vector4D invClip[4];// = transpose(gl_ModelViewProjectionMatrix * q);

	// solve for the right hand bounds in homogenous clip space
	float a1 = DotInvW(invClip[3], invClip[3]);
	float b1 = -2.0f * DotInvW(invClip[0], invClip[3]);
	float c1 = DotInvW(invClip[0], invClip[0]);

	float xmin;
	float xmax;
	solveQuadratic(a1, b1, c1, xmin, xmax);

	// solve for the right hand bounds in homogenous clip space
	float a2 = DotInvW(invClip[3], invClip[3]);
	float b2 = -2.0f * DotInvW(invClip[1], invClip[3]);
	float c2 = DotInvW(invClip[1], invClip[1]);

	float ymin;
	float ymax;
	solveQuadratic(a2, b2, c2, ymin, ymax);

	Vector4D gl_Position = Vector4D(worldPos.x, worldPos.y, worldPos.z, 1.0);
	Vector4D gl_TexCoord[4];
	gl_TexCoord[0] = Vector4D(xmin, xmax, ymin, ymax);

	// construct inverse quadric matrix (used for ray-casting in parameter space)
	Vector4D invq[4];
	invq[0] = Vector4D(q1.x / q1.w, q1.y / q1.w, q1.z / q1.w, 0.0);
	invq[1] = Vector4D(q2.x / q2.w, q2.y / q2.w, q2.z / q2.w, 0.0);
	invq[2] = Vector4D(q3.x / q3.w, q3.y / q3.w, q3.z / q3.w, 0.0);
	invq[3] = Vector4D(0.0, 0.0, 0.0, 1.0);

	//invq = transpose(invq);
	//invq[3] = -(invq * //gl_Position);

	// transform a point from view space to parameter space
	//invq = invq * gl_ModelViewMatrixInverse;

	// pass down
	gl_TexCoord[1] = invq[0];
	gl_TexCoord[2] = invq[1];
	gl_TexCoord[3] = invq[2];
	gl_TexCoord[4] = invq[3];

	// compute ndc pos for frustrum culling in GS
	Vector4D ndcPos;// = gl_ModelViewProjectionMatrix * vec4(worldPos.xyz, 1.0);
	gl_TexCoord[5] = ndcPos / ndcPos.w;
}

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

				Vector4D aniscale = Vector4D(1, 1, 1, CM_2_INCH);
				if (particle_ani) {
					Vector4D ani1 = particle_ani1[particle_index] * aniscale;
					Vector4D ani2 = particle_ani2[particle_index] * aniscale;
					Vector4D ani3 = particle_ani3[particle_index] * aniscale;

					for (int i = 0; i < 3; i++) {
						// Old anisotropy warping (code provided by Spanky)
						//Vector pos_ani = local_pos[i];
						//pos_ani = pos_ani + ani1.AsVector3D() * (local_pos[i].Dot(ani1.AsVector3D()) * ani1.w);
						//pos_ani = pos_ani + ani2.AsVector3D() * (local_pos[i].Dot(ani2.AsVector3D()) * ani2.w);
						//pos_ani = pos_ani + ani3.AsVector3D() * (local_pos[i].Dot(ani3.AsVector3D()) * ani3.w);

						// New anisotropy warping (code provided by Xenthio)
						local_pos[i] *= radius;
						float dot1 = local_pos[i].Dot(ani1.AsVector3D());
						float dot2 = local_pos[i].Dot(ani2.AsVector3D());
						float dot3 = local_pos[i].Dot(ani3.AsVector3D());
						Vector pos_ani = local_pos[i] + ani1.AsVector3D() * ani1.w * dot1 + ani2.AsVector3D() * ani2.w * dot2 + ani3.AsVector3D() * ani3.w * dot3;

						Vector world_pos = particle_pos + pos_ani;// *radius;
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
			Vector particle_pos = particle_positions[particle_index].AsVector3D();

			// Frustrum culling
			Vector4D dst;
			Vector4DMultiply(view_projection_matrix, Vector4D(particle_pos.x * CM_2_INCH, particle_pos.y * CM_2_INCH, particle_pos.z * CM_2_INCH, 1), dst);
			if (dst.z < 0 || -dst.x - dst.w > 0 || dst.x - dst.w > 0 || -dst.y - dst.w > 0 || dst.y - dst.w > 0) {
				continue;
			}

			for (int i = 0; i < 3; i++) {
				Vector pos_ani = local_pos[i];	// Warp based on velocity
				Vector vel = particle_velocities[particle_index].AsVector3D() * CM_2_INCH;
				pos_ani = pos_ani + (vel * pos_ani.Dot(vel) * particle_scale).Min(Vector(3, 3, 3)).Max(Vector(-3, -3, -3));

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