#pragma once
#include <NvFlex.h>
#include "mathlib/vector.h"
#include "mathlib/vector4d.h"
#include <vector>

// Data wrapper for FleX collisions
class FlexMesh {
private:
	int entity_id;	// id associated with the entity its attached to in source, as some physmeshes have multiple colliders (eg. ragdolls)
	NvFlexTriangleMeshId id;
	int flags;
	NvFlexBuffer* vertices = nullptr;
	NvFlexBuffer* indices = nullptr;

	Vector4D pos = Vector4D(0, 0, 0, 0);
	Vector4D ang = Vector4D(0, 0, 0, 1); // Quaternion

	Vector4D ppos = Vector4D(0, 0, 0, 0);	// Previous pos
	Vector4D pang = Vector4D(0, 0, 0, 1);	// Previous ang

public:
	bool init_concave(NvFlexLibrary* lib, std::vector<Vector> verts, bool dynamic);	
	bool init_concave(NvFlexLibrary* lib, Vector* verts, int num_verts, bool dynamic);	// Used only for maps
	bool init_convex(NvFlexLibrary* lib, std::vector<Vector> verts, bool dynamic);
	void destroy(NvFlexLibrary* lib);

	void set_pos(Vector pos);
	void set_pos(Vector4D pos);
	void set_ang(QAngle ang);
	void set_ang(Vector4D ang);

	Vector4D get_pos();	// Returns a Vector4D for convenience
	Vector4D get_ang();

	Vector4D get_ppos();
	Vector4D get_pang();

	NvFlexTriangleMeshId get_id();
	int get_entity_id();
	int get_flags();

	void update();	// sets the previous position/angle to current position/angle (previous_pos = pos; previous_ang = ang)
	
	FlexMesh(int mesh_id);
	FlexMesh();
};