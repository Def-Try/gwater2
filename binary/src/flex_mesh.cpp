#include "flex_mesh.h"

#define CM_2_INCH 39.3701f

float rad(float degree) {
	return (degree * (M_PI / 180));
}

// provided by PotatoOS
inline Vector4D unfuckQuat(Vector4D q) {
	return Vector4D(q.y, q.z, q.w, q.x);
}

// Angle to Quat conversion provided by Wikipidia (https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles)
Vector4D angle_to_quat(QAngle ang) {
	float p = rad(ang.x) / 2.0;
	float y = rad(ang.y) / 2.0;
	float r = rad(ang.z) / 2.0;
	float cr = cos(r);
	float sr = sin(r);
	float cp = cos(p);
	float sp = sin(p);
	float cy = cos(y);
	float sy = sin(y);

	return unfuckQuat(Vector4D(
		cr * cp * cy + sr * sp * sy,
		sr * cp * cy - cr * sp * sy,
		cr * sp * cy + sr * cp * sy,
		cr * cp * sy - sr * sp * cy
	));
}

void FlexMesh::destroy(NvFlexLibrary* lib) {
	if (vertices != nullptr) {
		NvFlexFreeBuffer(vertices);
		if (indices == nullptr) {
			NvFlexDestroyConvexMesh(lib, id);
		} else {	// Convex meshes dont have an indices buffer, so the mesh must be concave
			NvFlexFreeBuffer(indices);
			NvFlexDestroyTriangleMesh(lib, id);
		}
	}
}


bool FlexMesh::init_convex(NvFlexLibrary* lib, std::vector<Vector> verts, bool dynamic) {
	destroy(lib);

	// Is Mesh invalid?
	if (verts.size() == 0 || verts.size() % 3 != 0) {
		return false;
	}

	// Allocate buffers
	vertices = NvFlexAllocBuffer(lib, verts.size() / 3, sizeof(Vector4D), eNvFlexBufferHost);
	Vector4D* host_verts = (Vector4D*)NvFlexMap(vertices, eNvFlexMapWait);

	// Find OBB Bounding box automatically during parsing
	Vector min = verts[0];
	Vector max = verts[0];
	for (int i = 0; i < verts.size(); i += 3) {
		Vector tri[3] = { verts[i], verts[i + 1], verts[i + 2] };

		// Turn triangle into normalized plane & add to vertex buffer
		Vector plane_dir = (tri[1] - tri[0]).Cross(tri[0] - tri[2]).Normalized();
		float plane_height = -plane_dir.Dot(tri[0]);	// Negative because our triangle winding is reversed
		host_verts[i / 3] = Vector4D(plane_dir.x, plane_dir.y, plane_dir.z, plane_height);

		min = min.Min(tri[0]);
		min = min.Min(tri[1]);
		min = min.Min(tri[2]);

		max = max.Max(tri[0]);
		max = max.Max(tri[1]);
		max = max.Max(tri[2]);
	}

	NvFlexUnmap(vertices);

	float lower[3] = { min.x, min.y, min.z };
	float upper[3] = { max.x, max.y, max.z };

	id = NvFlexCreateConvexMesh(lib);
	flags = NvFlexMakeShapeFlags(eNvFlexShapeConvexMesh, dynamic);
	NvFlexUpdateConvexMesh(lib, id, vertices, verts.size() / 3, lower, upper);

	return true;
}

bool FlexMesh::init_concave(NvFlexLibrary* lib, std::vector<Vector> verts, bool dynamic) {
	destroy(lib);

	// Is Mesh invalid?
	if (verts.size() == 0 || verts.size() % 3 != 0) {
		return false;
	}
	
	// Allocate buffers
	vertices = NvFlexAllocBuffer(lib, verts.size(), sizeof(Vector4D), eNvFlexBufferHost);
	indices = NvFlexAllocBuffer(lib, verts.size(), sizeof(int), eNvFlexBufferHost);
	Vector4D* host_verts = (Vector4D*)NvFlexMap(vertices, eNvFlexMapWait);
	int* host_indices = (int*)NvFlexMap(indices, eNvFlexMapWait);

	// Find OBB Bounding box automatically during parsing
	Vector min = verts[0];
	Vector max = verts[0];
	for (int i = 0; i < verts.size(); i++) {
		verts[i].x /= CM_2_INCH;
		verts[i].y /= CM_2_INCH;
		verts[i].z /= CM_2_INCH;
		host_verts[i] = Vector4D(verts[i].x, verts[i].y, verts[i].z, 0);
		// Flip triangle winding (xyz -> yxz)
		switch (i % 3) {
			case 0:
				host_indices[i] = i + 1;
				break;
			case 1:
				host_indices[i] = i - 1;
				break;
			case 2:
				host_indices[i] = i;
				break;
		}

		min = min.Min(verts[i]);
		max = max.Max(verts[i]);
	}
	NvFlexUnmap(vertices);
	NvFlexUnmap(indices);

	float lower[3] = { min.x, min.y, min.z };
	float upper[3] = { max.x, max.y, max.z };

	id = NvFlexCreateTriangleMesh(lib);
	flags = NvFlexMakeShapeFlags(eNvFlexShapeTriangleMesh, dynamic);
	NvFlexUpdateTriangleMesh(lib, id, vertices, indices, verts.size(), verts.size() / 3, lower, upper);

	return true;
}

// this overload is only used for map collision, as the BSP Parser returns an array instead of a vector
bool FlexMesh::init_concave(NvFlexLibrary* lib, Vector* verts, int num_verts, bool dynamic) {
	destroy(lib);

	// Is Mesh invalid?
	if (num_verts == 0 || num_verts % 3 != 0) {
		return false;
	}

	// Allocate buffers
	vertices = NvFlexAllocBuffer(lib, num_verts, sizeof(Vector4D), eNvFlexBufferHost);
	indices = NvFlexAllocBuffer(lib, num_verts, sizeof(int), eNvFlexBufferHost);
	Vector4D* host_verts = (Vector4D*)NvFlexMap(vertices, eNvFlexMapWait);
	int* host_indices = (int*)NvFlexMap(indices, eNvFlexMapWait);

	// Find OBB Bounding box automatically during parsing
	Vector min = verts[0];
	Vector max = verts[0];
	for (int i = 0; i < num_verts; i++) {

		verts[i].x /= CM_2_INCH;
		verts[i].y /= CM_2_INCH;
		verts[i].z /= CM_2_INCH;
		host_verts[i] = Vector4D(verts[i].x, verts[i].y, verts[i].z, 0);
		host_indices[i] = i;

		min = min.Min(verts[i]);
		max = max.Max(verts[i]);
	}
	NvFlexUnmap(vertices);
	NvFlexUnmap(indices);

	float lower[3] = { min.x, min.y, min.z };
	float upper[3] = { max.x, max.y, max.z };

	id = NvFlexCreateTriangleMesh(lib);
	flags = NvFlexMakeShapeFlags(eNvFlexShapeTriangleMesh, dynamic);
	NvFlexUpdateTriangleMesh(lib, id, vertices, indices, num_verts, num_verts / 3, lower, upper);

	return true;
}

// sets the previous position/angle to current position/angle (previous_pos = pos; previous_ang = ang)
void FlexMesh::update() {
	ppos = pos;
	pang = ang;
}

Vector4D FlexMesh::get_pos() {
	return pos;
}
void FlexMesh::set_pos(Vector p) {
	pos = Vector4D(p.x, p.y, p.z, 0);	// unsure what the last number is for. FleX requires it to exist
}

Vector4D FlexMesh::get_ang() {
	return ang;
}

Vector4D FlexMesh::get_ppos() {
	return ppos;
}

Vector4D FlexMesh::get_pang() {
	return pang;
}

void FlexMesh::set_ang(QAngle a) {
	ang = angle_to_quat(a);
}

NvFlexTriangleMeshId FlexMesh::get_id() {
	return id;
}

int FlexMesh::get_entity_id() {
	return entity_id;
}

int FlexMesh::get_flags() {
	return flags;
}

FlexMesh::FlexMesh(int id) {
	entity_id = id;
}