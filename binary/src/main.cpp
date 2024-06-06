// bsp parser requirements
// this MUST BE included before everything else because of conflicting source defines
#include "GMFS.h"
#include "BSPParser.h"

#include "GarrysMod/Lua/Interface.h"

#include "flex_solver.h"
#include "flex_renderer.h"
#include "shader_inject.h"

#include "sighack.h"	// for reaction forces

using namespace GarrysMod::Lua;

NvFlexLibrary* FLEX_LIBRARY;	// "The heart of all that is FleX" - AE
ILuaBase* GLOBAL_LUA;			// used for flex error handling
int FLEXSOLVER_METATABLE = 0;
int FLEXRENDERER_METATABLE = 0;

typedef void* (__cdecl* UTIL_EntityByIndexFN)(int);
UTIL_EntityByIndexFN UTIL_EntityByIndex = nullptr;

float CM_2_INCH = 2.54 * 2.54;	// FleX is in centimeters, source is in inches. We need to convert units

//#define GET_FLEX(type, stack_pos) LUA->GetUserType<type>(stack_pos, type == FlexSolver ? FLEXSOLVER_METATABLE : FLEXRENDERER_METATABLE)

/************************** Flex Solver LUA Interface *******************************/

#define GET_FLEXSOLVER(stack_pos) LUA->GetUserType<FlexSolver>(stack_pos, FLEXSOLVER_METATABLE)

// Frees the flex solver instance from memory
LUA_FUNCTION(FLEXSOLVER_GarbageCollect) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);

	FlexSolver* flex = GET_FLEXSOLVER(1);

	LUA->PushNil();
	LUA->SetMetaTable(-2);

	delete flex;
	return 0;
}

LUA_FUNCTION(FLEXSOLVER_AddParticle) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckType(2, Type::Vector);	// position
	LUA->CheckType(3, Type::Vector);	// velocity
	LUA->CheckNumber(4);				// mass

	FlexSolver* flex = GET_FLEXSOLVER(1);
	Vector pos = LUA->GetVector(2);
	Vector vel = LUA->GetVector(3);
	float inv_mass = 1.f / (float)LUA->GetNumber(4);	// FleX uses inverse mass for their calculations
	
	flex->add_particle(Vector4D(pos.x, pos.y, pos.z, inv_mass), vel);
	flex->map_particles();

	return 0;
}

LUA_FUNCTION(FLEXSOLVER_Tick) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(2);	// Delta Time

	FlexSolver* flex = GET_FLEXSOLVER(1);
	
	// Avoid ticking if the deltatime ends up being zero, as it invalidates the simulation
	float dt = (float)LUA->GetNumber(2) * 10;
	if (flex->get_parameter("timescale") == 0 || dt == 0 || flex->get_active_particles() == 0) {
		LUA->PushBool(true);
		return 1;
	}

	bool succ = flex->pretick((NvFlexMapFlags)LUA->GetNumber(3));
	if (succ) flex->tick(dt);

	LUA->PushBool(succ);
	return 1;
}

// Adds a triangle collision mesh to a FlexSolver
LUA_FUNCTION(FLEXSOLVER_AddConcaveMesh) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(2);				// Entity ID
	LUA->CheckType(3, Type::Table);		// Mesh data
	LUA->CheckType(4, Type::Vector);	// Initial Pos
	LUA->CheckType(5, Type::Angle);		// Initial Angle

	Vector pos = LUA->GetVector(4);
	QAngle ang = LUA->GetAngle(5);

	FlexSolver* flex = GET_FLEXSOLVER(1);
	std::vector<Vector> verts;	// mnnmm yess... vector vector
	for (int i = 1; i <= LUA->ObjLen(3); i++) {	// dont forget lua is 1 indexed!
		LUA->PushNumber(i);
		LUA->GetTable(3);
		LUA->GetField(-1, "pos");

		verts.push_back(LUA->GetType(-2) == Type::Vector ? LUA->GetVector(-2) : LUA->GetVector());
		LUA->Pop(2); //pop table & position
	}

	FlexMesh mesh = FlexMesh((int)LUA->GetNumber(2));
	if (!mesh.init_concave(FLEX_LIBRARY, verts, true)) {
		LUA->ThrowError("Tried to add concave mesh with invalid data (NumVertices is not a multiple of 3!)");
		return 0;
	}

	mesh.set_pos(pos);
	mesh.set_ang(ang);
	mesh.update();

	flex->add_mesh(mesh);

	return 0;
}

// Adds a convex collision mesh to a FlexSolver
LUA_FUNCTION(FLEXSOLVER_AddConvexMesh) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(2);				// Entity ID
	LUA->CheckType(3, Type::Table);		// Mesh data
	LUA->CheckType(4, Type::Vector);	// Initial Pos
	LUA->CheckType(5, Type::Angle);		// Initial Angle

	Vector pos = LUA->GetVector(4);
	QAngle ang = LUA->GetAngle(5);

	FlexSolver* flex = GET_FLEXSOLVER(1);
	std::vector<Vector> verts;
	for (int i = 1; i <= LUA->ObjLen(3); i++) {	// dont forget lua is 1 indexed!
		LUA->PushNumber(i);
		LUA->GetTable(3);
		LUA->GetField(-1, "pos");

		verts.push_back(LUA->GetType(-2) == Type::Vector ? LUA->GetVector(-2) : LUA->GetVector());
		LUA->Pop(2); //pop table & position
	}

	FlexMesh mesh = FlexMesh((int)LUA->GetNumber(2));
	if (!mesh.init_convex(FLEX_LIBRARY, verts, true)) {
		LUA->ThrowError("Tried to add convex mesh with invalid data (NumVertices is not a multiple of 3!)");
		return 0;
	}

	mesh.set_pos(pos);
	mesh.set_ang(ang);
	mesh.update();

	flex->add_mesh(mesh);

	return 0;
}

// Removes all meshes associated with the entity id
LUA_FUNCTION(FLEXSOLVER_RemoveMesh) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(2); // Entity ID

	FlexSolver* flex = GET_FLEXSOLVER(1);
	flex->remove_mesh(LUA->GetNumber(2));

	return 0;
}

// TODO: Implement
/*
LUA_FUNCTION(FLEXSOLVER_RemoveMeshIndex) {

}*/

LUA_FUNCTION(FLEXSOLVER_SetParameter) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckString(2); // Param
	LUA->CheckNumber(3); // Number

	FlexSolver* flex = GET_FLEXSOLVER(1);
	bool succ = flex->set_parameter(LUA->GetString(2), LUA->GetNumber(3));
	if (!succ) LUA->ThrowError(("Attempt to set invalid parameter '" + (std::string)LUA->GetString(2) + "'").c_str());

	return 0;
}

LUA_FUNCTION(FLEXSOLVER_GetParameter) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckString(2); // Param

	FlexSolver* flex = GET_FLEXSOLVER(1);
	float value = flex->get_parameter(LUA->GetString(2));
	if (isnan(value)) LUA->ThrowError(("Attempt to get invalid parameter '" + (std::string)LUA->GetString(2) + "'").c_str());
	LUA->PushNumber(value);

	return 1;
}

// Updates position and angles of a mesh collider
LUA_FUNCTION(FLEXSOLVER_UpdateMesh) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(2);				// Mesh ID
	LUA->CheckType(3, Type::Vector);	// Prop Pos
	LUA->CheckType(4, Type::Angle);		// Prop Angle

	FlexSolver* flex = GET_FLEXSOLVER(1);
	flex->update_mesh(LUA->GetNumber(2), LUA->GetVector(3), LUA->GetAngle(4));

	return 0;
}

// removes all particles in a flex solver
LUA_FUNCTION(FLEXSOLVER_Reset) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	FlexSolver* flex = GET_FLEXSOLVER(1);

	int* contact_count = (int*)NvFlexMap(flex->get_buffer("contact_count"), eNvFlexMapWait);
	memset(contact_count, 0, sizeof(int) * flex->get_active_particles());
	NvFlexUnmap(flex->get_buffer("contact_count"));

	flex->set_active_particles(0);
	flex->set_active_diffuse(0);

	return 0;
}

LUA_FUNCTION(FLEXSOLVER_GetActiveParticles) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	FlexSolver* flex = GET_FLEXSOLVER(1);
	LUA->PushNumber(flex->get_active_particles());

	return 1;
}

LUA_FUNCTION(FLEXSOLVER_GetActiveDiffuse) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	FlexSolver* flex = GET_FLEXSOLVER(1);
	LUA->PushNumber(flex->get_active_diffuse());

	return 1;
}


// Iterates through all particles and calls a lua function with 1 parameter (position)
LUA_FUNCTION(FLEXSOLVER_RenderParticles) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckType(2, Type::Function);

	FlexSolver* flex = GET_FLEXSOLVER(1);
	Vector4D* host = (Vector4D*)flex->get_host("particle_smooth");
	for (int i = 0; i < flex->get_active_particles(); i++) {
		// render function
		LUA->Push(2);
		LUA->PushVector(host[i].AsVector3D());
		LUA->PushNumber(host[i].w);
		LUA->Call(2, 0);
	}

	return 0;
}

// TODO: rewrite this shit
LUA_FUNCTION(FLEXSOLVER_AddMapMesh) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(2);
	LUA->CheckString(3);	// Map name

	FlexSolver* flex = GET_FLEXSOLVER(1);

	// Get path, check if it exists
	std::string path = "maps/" + (std::string)LUA->GetString(3) + ".bsp";
	if (!FileSystem::Exists(path.c_str(), "GAME")) {
		LUA->ThrowError(("[GWater2 Internal Error]: Map path " + path + " not found! (Is the map subscribed to?)").c_str());
		return 0;
	}

	// Path exists, load data
	FileHandle_t file = FileSystem::Open(path.c_str(), "rb", "GAME");
	uint32_t filesize = FileSystem::Size(file);
	uint8_t* data = (uint8_t*)malloc(filesize);
	if (data == nullptr) {
		LUA->ThrowError("[GWater2 Internal Error]: Map collision data failed to load!");
		return 0;
	}
	FileSystem::Read(data, filesize, file);
	FileSystem::Close(file);

	BSPMap map = BSPMap(data, filesize, false);
	FlexMesh mesh = FlexMesh((int)LUA->GetNumber(2));
	if (!mesh.init_concave(FLEX_LIBRARY, (Vector*)map.GetVertices(), map.GetNumVertices(), false)) {
		free(data);
		LUA->ThrowError("Tried to add map mesh with invalid data (NumVertices is 0 or not a multiple of 3!)");

		return 0;
	}

	free(data);
	flex->add_mesh(mesh);

	return 0;
}

// Applies reaction forces on serverside objects
#include "vphysics_interface.h"
LUA_FUNCTION(FLEXSOLVER_ApplyContacts) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(2);	// radius
	LUA->CheckNumber(3);	// dampening
	LUA->CheckNumber(4);	// buoyancy 

	if (UTIL_EntityByIndex == nullptr) return 0;	// not hosting server

	FlexSolver* flex = GET_FLEXSOLVER(1);
	if (!flex->get_parameter("coupling")) return 0;	// Coupling planes arent being generated.. bail

	Vector4D* particle_pos = (Vector4D*)flex->get_host("particle_pos");
	Vector* particle_vel = (Vector*)flex->get_host("particle_vel");
	/*
	Vector4D* contact_vel = (Vector4D*)flex->get_host("contact_vel");
	Vector4D* contact_planes = (Vector4D*)flex->get_host("contact_planes");

	int* contact_count = (int*)flex->get_host("contact_count");
	int* contact_indices = (int*)flex->get_host("contact_indices");*/

	// mapping planes stops random spazzing, but eats perf
	Vector4D* contact_vel = (Vector4D*)NvFlexMap(flex->get_buffer("contact_vel"), eNvFlexMapWait);
	Vector4D* contact_planes = (Vector4D*)NvFlexMap(flex->get_buffer("contact_planes"), eNvFlexMapWait);

	int* contact_count = (int*)NvFlexMap(flex->get_buffer("contact_count"), eNvFlexMapWait);
	int* contact_indices = (int*)NvFlexMap(flex->get_buffer("contact_indices"), eNvFlexMapWait);

	int max_contacts = flex->get_max_contacts();
	float radius = flex->get_parameter("radius");
	float volume_mul = LUA->GetNumber(2) * 4.f * M_PI * (radius * radius);	// Surface area of sphere equation
	float dampening_mul = LUA->GetNumber(3);
	float buoyancy_mul = LUA->GetNumber(4);

	std::vector<FlexMesh> meshes = *flex->get_meshes();

	// Get all props and average them
	for (int i = 0; i < flex->get_active_particles(); i++) {
		int plane_id = contact_indices[i];
		for (int contact = 0; contact < contact_count[plane_id]; contact++) {
			int plane_index = plane_id * max_contacts + contact;
			//int vel_index = i * max_contacts + contact;

			float prop_id = (int)contact_vel[plane_index].w;
			if (prop_id < 0) break;	//	planes defined by FleX will return -1

			FlexMesh prop = FlexMesh(0);
			try {
				prop = meshes.at(prop_id);
			} catch (std::exception e) {
				Warning("[GWater2 Internal Error]: Prevented Crash! Tried to access invalid entity %i!\n", prop_id);
				continue;
			}

			void* ent = UTIL_EntityByIndex(prop.get_entity_id());

			if (ent == nullptr) {
				//Warning("Couldn't find entity!\n");
				continue;
			}
#ifdef WIN64
			IPhysicsObject* phys = *(IPhysicsObject**)((uint64_t)ent + 0x288);
#else
			IPhysicsObject* phys = *(IPhysicsObject**)((uint64_t)ent + 0x1e0);
#endif
			if (phys == nullptr) {
				//Warning("Couldn't find entity's physics object!\n");
				continue;
			}

			Vector plane = contact_planes[plane_index].AsVector3D();
			Vector contact_pos = particle_pos[i].AsVector3D() - plane * radius * 0.5;	// Particle position is not directly *on* plane
			Vector prop_vel; phys->GetVelocityAtPoint(contact_pos, &prop_vel);
			Vector local_vel = (particle_vel[i] * CM_2_INCH - prop_vel) * flex->get_parameter("timescale");
			Vector impact_vel = (plane * fmin(local_vel.Dot(plane), 0) - contact_vel[plane_index].AsVector3D() * dampening_mul) * volume_mul;

			// Buoyancy (completely faked. not at all accurate)
			Vector prop_pos;
			phys->GetPosition(&prop_pos, NULL);
			if (plane.z < 0 && contact_pos.z < prop_pos.z + phys->GetMassCenterLocalSpace().z) {
				impact_vel += Vector(0, 0, volume_mul * buoyancy_mul);
			}

			// Dampening (completely faked. not at all accurate)
			//Vector prop_vel;
			//phys->GetVelocityAtPoint(contact_pos, &prop_vel);
			//impact_vel -= prop_vel * volume_mul * dampening_mul;

			// Cap amount of force (vphysics crashes can occur without it)
			float limit = 100 * phys->GetMass();
			if (impact_vel.Dot(impact_vel) > limit * limit) {
				impact_vel = impact_vel.Normalized() * limit;
			}

			phys->ApplyForceOffset(impact_vel, contact_pos);
			/*
			try {
				Mesh* prop = props.at(prop_id);
				prop->pos = prop->pos + float4(prop_pos.x, prop_pos.y, prop_pos.z, 1);
				prop->ang = prop->ang + float4(impact_vel.x, impact_vel.y, impact_vel.z, 0);
			}
			catch (std::exception e) {
				props[prop_id] = new Mesh(flexLibrary);
				props[prop_id]->pos = float4(prop_pos.x, prop_pos.y, prop_pos.z, 1);
				props[prop_id]->ang = float4(impact_vel.x, impact_vel.y, impact_vel.z, 0);
			}*/
		}
	}

	//NvFlexUnmap(flex->get_buffer("particle_pos"));
	//NvFlexUnmap(flex->get_buffer("particle_vel"));
	NvFlexUnmap(flex->get_buffer("contact_vel"));
	NvFlexUnmap(flex->get_buffer("contact_planes"));
	NvFlexUnmap(flex->get_buffer("contact_count"));
	NvFlexUnmap(flex->get_buffer("contact_indices"));

	return 0;
}

// Original function written by andreweathan
LUA_FUNCTION(FLEXSOLVER_AddCube) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckType(2, Type::Vector); // pos
	LUA->CheckType(3, Type::Vector); // vel
	LUA->CheckType(4, Type::Vector); // cube size
	LUA->CheckType(5, Type::Number); // size apart (usually radius)

	//gmod Vector and fleX float4
	FlexSolver* flex = GET_FLEXSOLVER(1);
	Vector gmodPos = LUA->GetVector(2);		//pos
	Vector gmodVel = LUA->GetVector(3);		//vel
	Vector gmodSize = LUA->GetVector(4);	//size
	float size = LUA->GetNumber(5);			//size apart

	gmodSize = gmodSize / 2.f;
	gmodPos = gmodPos + Vector(size, size, size) / 2.0;

	for (float z = -gmodSize.z; z < gmodSize.z; z++) {
		for (float y = -gmodSize.y; y < gmodSize.y; y++) {
			for (float x = -gmodSize.x; x < gmodSize.x; x++) {
				Vector newPos = Vector(x, y, z) * size + gmodPos;

				flex->add_particle(Vector4D(newPos.x, newPos.y, newPos.z, 1), gmodVel);
			}
		}
	}

	flex->map_particles();

	return 0;
}

// Initializes a box (6 planes) with a mins and maxs on a FlexSolver
// Inputting nil disables the bounds.
LUA_FUNCTION(FLEXSOLVER_InitBounds) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	FlexSolver* flex = GET_FLEXSOLVER(1);

	if (LUA->GetType(2) == Type::Vector && LUA->GetType(3) == Type::Vector) {
		flex->enable_bounds(LUA->GetVector(2), LUA->GetVector(3));
	} else {
		flex->disable_bounds();
	}
	
	return 0;
}

LUA_FUNCTION(FLEXSOLVER_GetMaxParticles) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	FlexSolver* flex = GET_FLEXSOLVER(1);

	LUA->PushNumber(flex->get_max_particles());
	return 1;
}

LUA_FUNCTION(FLEXSOLVER_GetMaxDiffuseParticles) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	FlexSolver* flex = GET_FLEXSOLVER(1);

	LUA->PushNumber(flex->get_max_particles());
	return 1;
}

// Runs a lua function with some data on all FlexMeshes stored in a FlexSolver
// This is faster then returning a table of values and using ipairs and also allows removal / additions during function execution
// first parameter is the index of the mesh inside the vector
// second parameter is the entity id associated that was given during AddMesh
// third parameter is the number of reoccurring id's in a row (eg. given id's 0,1,1,1 the parameter would be 2 at the end of execution since 1 was repeated two more times)
// ^the third parameter sounds confusing but its useful for multi-joint entities such as ragdolls/players/npcs
LUA_FUNCTION(FLEXSOLVER_IterateMeshes) {
	LUA->CheckType(1, FLEXSOLVER_METATABLE);
	LUA->CheckType(2, Type::Function);
	FlexSolver* flex = GET_FLEXSOLVER(1);

	int i = 0;
	int repeat = 0;
	int previous_id;
	for (FlexMesh mesh : *flex->get_meshes()) {
		int id = mesh.get_entity_id();

		repeat = (i != 0 && previous_id == id) ? repeat + 1 : 0;	// if (same as last time) {repeat = repeat + 1} else {repeat = 0}
		previous_id = id;

		// func(i, id, repeat)
		LUA->Push(2);
		LUA->PushNumber(i);
		LUA->PushNumber(id);
		LUA->PushNumber(repeat);
		LUA->PCall(3, 0, 0);

		i++;
	}

	return 0;
}


/*********************************** Flex Renderer LUA Interface *******************************************/

#define GET_FLEXRENDERER(stack_pos) LUA->GetUserType<FlexRenderer>(stack_pos, FLEXRENDERER_METATABLE)

// Frees the flex renderer and its allocated meshes from memory
LUA_FUNCTION(FLEXRENDERER_GarbageCollect) {
	LUA->CheckType(1, FLEXRENDERER_METATABLE);
	FlexRenderer* flex = GET_FLEXRENDERER(1);

	LUA->PushNil();
	LUA->SetMetaTable(-2);

	delete flex;
	return 0;
}

// FlexRenderer imesh related functions
// Builds water particles to IMesh* structs
LUA_FUNCTION(FLEXRENDERER_BuildWater) {
	LUA->CheckType(1, FLEXRENDERER_METATABLE);
	LUA->CheckType(2, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(3);
	GET_FLEXRENDERER(1)->build_water(GET_FLEXSOLVER(2), LUA->GetNumber(3));

	return 0;
}

// Renders water IMesh* structs (built from func. above)
LUA_FUNCTION(FLEXRENDERER_DrawWater) {
	LUA->CheckType(1, FLEXRENDERER_METATABLE);
	GET_FLEXRENDERER(1)->draw_water();

	return 0;
}

// Builds diffuse particles to IMesh* structs
LUA_FUNCTION(FLEXRENDERER_BuildDiffuse) {
	LUA->CheckType(1, FLEXRENDERER_METATABLE);
	LUA->CheckType(2, FLEXSOLVER_METATABLE);
	LUA->CheckNumber(3);
	GET_FLEXRENDERER(1)->build_diffuse(GET_FLEXSOLVER(2), LUA->GetNumber(3));

	return 0;
}

// Renders diffuse IMesh* structs (built from func. above)
LUA_FUNCTION(FLEXRENDERER_DrawDiffuse) {
	LUA->CheckType(1, FLEXRENDERER_METATABLE);
	GET_FLEXRENDERER(1)->draw_diffuse();

	return 0;
}

/************************************* Global LUA Interface **********************************************/

#define ADD_FUNCTION(LUA, funcName, tblName) LUA->PushCFunction(funcName); LUA->SetField(-2, tblName)

// must be freed from memory
LUA_FUNCTION(NewFlexSolver) {
	LUA->CheckNumber(1);
	if (LUA->GetNumber(1) <= 0) LUA->ThrowError("Max Particles must be a positive number!");

	FlexSolver* flex = new FlexSolver(FLEX_LIBRARY, LUA->GetNumber(1));
	LUA->PushUserType(flex, FLEXSOLVER_METATABLE);
	LUA->PushMetaTable(FLEXSOLVER_METATABLE);	// Add our meta functions
	LUA->SetMetaTable(-2);

	/*
	NvFlexSolverCallback callback;
	callback.userData = LUA->GetUserdata();	// "DePriCatEd!!".. maybe add a function with identical or similar functionaliy... :/
	callback.function = [](NvFlexSolverCallbackParams p) {
		// hook.Call("GW2FlexCallback", nil, FlexSolver)
		GLOBAL_LUA->PushSpecial(SPECIAL_GLOB);
		GLOBAL_LUA->GetField(-1, "hook");
		GLOBAL_LUA->GetField(-1, "Call");
		GLOBAL_LUA->PushString("GW2FlexCallback");
		GLOBAL_LUA->PushNil();
		GLOBAL_LUA->PushUserdata(p.userData);	// ^
		GLOBAL_LUA->PushMetaTable(FLEXSOLVER_METATABLE);
		GLOBAL_LUA->SetMetaTable(-2);
		GLOBAL_LUA->PCall(3, 0, 0);
	};
	flex->add_callback(callback, eNvFlexStageUpdateEnd);*/

	return 1;
}

LUA_FUNCTION(NewFlexRenderer) {
	FlexRenderer* flex_renderer = new FlexRenderer();
	LUA->PushUserType(flex_renderer, FLEXRENDERER_METATABLE);
	LUA->PushMetaTable(FLEXRENDERER_METATABLE);
	LUA->SetMetaTable(-2);

	return 1;
}

// WILL LEAK MEMORY! VERTEX AND INDEX BUFFERS ARE NOT FREED
LUA_FUNCTION(NewRenderer) {
	IMatRenderContext* render_context = materials->GetRenderContext();
	IIndexBuffer* index_buffer = render_context->CreateStaticIndexBuffer(MATERIAL_INDEX_FORMAT_32BIT, 3, "");
	IVertexBuffer* vertex_buffer = render_context->CreateStaticVertexBuffer(MATERIAL_VERTEX_FORMAT_MODEL, 3, "");

	CIndexBuilder index_builder;
	index_builder.Begin(index_buffer, 3, 0);
	index_builder.FastTriangle(0);
	index_builder.End();

	CVertexBuilder vertex_builder;
	vertex_builder.Begin(vertex_buffer, 3);
	vertex_builder.Position3f(0, 0, 0);
	vertex_builder.Normal3f(0, 0, 1);
	vertex_builder.Color4f(1, 1, 1, 1);
	vertex_builder.AdvanceVertex();
	vertex_builder.Position3f(100, 0, 0);
	vertex_builder.Normal3f(0, 0, 1);
	vertex_builder.Color4f(1, 1, 1, 1);
	vertex_builder.AdvanceVertex();
	vertex_builder.Position3f(0, 100, 0);
	vertex_builder.Normal3f(0, 0, 1);
	vertex_builder.Color4f(1, 1, 1, 1);
	vertex_builder.AdvanceVertex();
	vertex_builder.End();

	render_context->BindVertexBuffer(0, vertex_buffer, vertex_builder.Offset(), 0, vertex_builder.TotalVertexCount(), MATERIAL_VERTEX_FORMAT_MODEL);
	render_context->BindIndexBuffer(index_buffer, index_builder.Offset());
	render_context->Draw(MATERIAL_TRIANGLES, 0, 3);
	render_context->EndBatch();

	return 0;
}

// `mat_antialias 0` but shit
/*LUA_FUNCTION(SetMSAAEnabled) {
	MaterialSystem_Config_t config = materials->GetCurrentConfigForVideoCard();
	config.m_nAAQuality = 0;
	config.m_nAASamples = 0;
	//config.m_nForceAnisotropicLevel = 1;
	//config.m_bShadowDepthTexture = false;
	//config.SetFlag(MATSYS_VIDCFG_FLAGS_FORCE_TRILINEAR, false);
	MaterialSystem_Config_t config_default = MaterialSystem_Config_t();
	//config.m_Flags = config_default.m_Flags;
	//config.m_DepthBias_ShadowMap = config_default.m_DepthBias_ShadowMap;
	//config.dxSupportLevel = config_default.dxSupportLevel;
	//config.m_SlopeScaleDepthBias_ShadowMap = config_default.m_SlopeScaleDepthBias_ShadowMap;
	materials->OverrideConfig(config, false);
	return 0;
}*/

GMOD_MODULE_OPEN() {
	GLOBAL_LUA = LUA;
	FLEX_LIBRARY = NvFlexInit(
		NV_FLEX_VERSION, 
		[](NvFlexErrorSeverity type, const char* message, const char* file, int line) {
			std::string error = "[GWater2 Internal Error]: " + (std::string)message;
			GLOBAL_LUA->ThrowError(error.c_str());
		}
	);

	if (FLEX_LIBRARY == nullptr) 
		LUA->ThrowError("[GWater2 Internal Error]: Nvidia FleX Failed to load! (Does your GPU meet the minimum requirements to run FleX?)");
	
	if (!Sys_LoadInterface("materialsystem", MATERIAL_SYSTEM_INTERFACE_VERSION, NULL, (void**)&materials))
		LUA->ThrowError("[GWater2 Internal Error]: C++ Materialsystem failed to load!");

	//if (!Sys_LoadInterface("shaderapidx9", SHADER_DEVICE_INTERFACE_VERSION, NULL, (void**)&g_pShaderDevice))
	//	LUA->ThrowError("[GWater2 Internal Error]: C++ Shaderdevice failed to load!");

	//if (!Sys_LoadInterface("shaderapidx9", SHADERAPI_INTERFACE_VERSION, NULL, (void**)&g_pShaderAPI))
	//	LUA->ThrowError("[GWater2 Internal Error]: C++ Shaderapi failed to load!");

	//if (!Sys_LoadInterface("studiorender", STUDIO_RENDER_INTERFACE_VERSION, NULL, (void**)&g_pStudioRender)) 
	//	LUA->ThrowError("[GWater2 Internal Error]: C++ Studiorender failed to load!");

	// Defined in 'shader_inject.h'
	if (!inject_shaders())
		LUA->ThrowError("[GWater2 Internal Error]: C++ Shadersystem failed to load!");

	// GMod filesystem (Used for bsp parser)
	if (FileSystem::LoadFileSystem() != FILESYSTEM_STATUS::OK)
		LUA->ThrowError("[GWater2 Internal Error]: C++ Filesystem failed to load!");

	FLEXSOLVER_METATABLE = LUA->CreateMetaTable("FlexSolver");
	ADD_FUNCTION(LUA, FLEXSOLVER_GarbageCollect, "__gc");	// FlexMetaTable.__gc = FlexGC

	// FlexMetaTable.__index = {func1, func2, ...}
	LUA->CreateTable();
	ADD_FUNCTION(LUA, FLEXSOLVER_GarbageCollect, "Destroy");
	ADD_FUNCTION(LUA, FLEXSOLVER_Tick, "Tick");
	ADD_FUNCTION(LUA, FLEXSOLVER_AddParticle, "AddParticle");
	ADD_FUNCTION(LUA, FLEXSOLVER_AddCube, "AddCube");
	ADD_FUNCTION(LUA, FLEXSOLVER_GetMaxParticles, "GetMaxParticles");
	ADD_FUNCTION(LUA, FLEXSOLVER_GetMaxDiffuseParticles, "GetMaxDiffuseParticles");
	ADD_FUNCTION(LUA, FLEXSOLVER_RenderParticles, "RenderParticles");
	ADD_FUNCTION(LUA, FLEXSOLVER_AddConcaveMesh, "AddConcaveMesh");
	ADD_FUNCTION(LUA, FLEXSOLVER_AddConvexMesh, "AddConvexMesh");
	ADD_FUNCTION(LUA, FLEXSOLVER_RemoveMesh, "RemoveMesh");
	ADD_FUNCTION(LUA, FLEXSOLVER_UpdateMesh, "UpdateMesh");
	ADD_FUNCTION(LUA, FLEXSOLVER_SetParameter, "SetParameter");
	ADD_FUNCTION(LUA, FLEXSOLVER_GetParameter, "GetParameter");
	ADD_FUNCTION(LUA, FLEXSOLVER_GetActiveParticles, "GetActiveParticles");
	ADD_FUNCTION(LUA, FLEXSOLVER_GetActiveDiffuse, "GetActiveDiffuse");
	ADD_FUNCTION(LUA, FLEXSOLVER_AddMapMesh, "AddMapMesh");
	ADD_FUNCTION(LUA, FLEXSOLVER_IterateMeshes, "IterateMeshes");
	ADD_FUNCTION(LUA, FLEXSOLVER_ApplyContacts, "ApplyContacts");
	ADD_FUNCTION(LUA, FLEXSOLVER_InitBounds, "InitBounds");
	ADD_FUNCTION(LUA, FLEXSOLVER_Reset, "Reset");
	LUA->SetField(-2, "__index");

	FLEXRENDERER_METATABLE = LUA->CreateMetaTable("FlexRenderer");
	ADD_FUNCTION(LUA, FLEXRENDERER_GarbageCollect, "__gc");	// FlexMetaTable.__gc = FlexGC

	// FlexMetaTable.__index = {func1, func2, ...}
	LUA->CreateTable();
	ADD_FUNCTION(LUA, FLEXRENDERER_GarbageCollect, "Destroy");
	ADD_FUNCTION(LUA, FLEXRENDERER_BuildWater, "BuildWater");
	ADD_FUNCTION(LUA, FLEXRENDERER_DrawWater, "DrawWater");
	ADD_FUNCTION(LUA, FLEXRENDERER_BuildDiffuse, "BuildDiffuse");
	ADD_FUNCTION(LUA, FLEXRENDERER_DrawDiffuse, "DrawDiffuse");
	LUA->SetField(-2, "__index");

	// _G.FlexSolver = NewFlexSolver
	LUA->PushSpecial(SPECIAL_GLOB);
	ADD_FUNCTION(LUA, NewFlexSolver, "FlexSolver");
	ADD_FUNCTION(LUA, NewFlexRenderer, "FlexRenderer");
	ADD_FUNCTION(LUA, NewRenderer, "NewRenderer");
	LUA->Pop();

	// Get serverside physics objects from client DLL. Since server.dll exists in memory, we can find it and avoid networking.
	// Pretty sure this is what hacked clients do
#ifdef WIN64 
	const char* sig = "48 83 EC ?? 8B D1 85 C9 7E ?? 48 8B ?? ?? ?? ?? ?? 48 8B";
#else
	const char* sig = "55 8B EC 8B ?? ?? 85 D2 ?? ?? 8B 0D ?? ?? ?? ?? 52 8B ?? FF 50 ?? 8B C8 85 C9";
#endif
	void* UTIL_EntityByIndexAddr = Scan("server.dll", sig);
	if (UTIL_EntityByIndexAddr == nullptr) {
		Warning("[GWater2 Internal Error] Couldn't find UTIL_EntityByIndex!\n");
		return 0;
	}
	UTIL_EntityByIndex = (UTIL_EntityByIndexFN)UTIL_EntityByIndexAddr;
	return 0;
}

// Called when the module is unloaded
GMOD_MODULE_CLOSE() {
	if (FLEX_LIBRARY) {
		NvFlexShutdown(FLEX_LIBRARY);
		FLEX_LIBRARY = nullptr;
	}

	// Defined in 'shader_inject.h'
	eject_shaders();

	return 0;
}