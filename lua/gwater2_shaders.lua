local function GetRenderTargetGWater(name, mult, depth) 
	mult = mult or 1
	return GetRenderTargetEx(name, ScrW() * mult, ScrH() * mult,
		RT_SIZE_DEFAULT,
		depth or 0,
		2 + 4 + 8 + 256,
		0,
		IMAGE_FORMAT_RGBA16161616F
	)
end

local cache_screen0 = render.GetScreenEffectTexture()
local cache_screen1 = render.GetScreenEffectTexture(1)
local cache_depth = GetRenderTargetGWater("1gwater_cache_depth", 1 / 1)
local cache_absorption = GetRenderTargetGWater("gwater_cache_absorption", 1 / 1, MATERIAL_RT_DEPTH_NONE)
local cache_normals = GetRenderTargetGWater("1gwater_cache_normals", 1 / 1, MATERIAL_RT_DEPTH_SEPARATE)
local cache_bloom = GetRenderTargetGWater("2gwater_cache_bloom", 1 / 2)	-- for blurring
local water_blur = Material("gwater2/smooth")
local water_volumetric = Material("gwater2/volumetric")
local water_normals = Material("gwater2/normals")
local water_bubble = Material("gwater2/bubble")	-- bubbles
local water_mist = Material("gwater2/mist")


local debug_depth = CreateClientConVar("gwater2_debug_depth", "0", false)
local debug_absorption = CreateClientConVar("gwater2_debug_absorption", "0", false)
local debug_normals = CreateClientConVar("gwater2_debug_normals", "0", false)

local blur_passes = CreateClientConVar("gwater2_blur_passes", "3", true)
local blur_scale = CreateClientConVar("gwater2_blur_scale", "1", true)
local antialias = GetConVar("mat_antialias")

local lightmodel = ClientsideModel( "models/kleiner_animations.mdl", RENDERGROUP_OTHER );
local lightpos = EyePos()
-- rebuild meshes every frame (unused atm since PostDrawOpaque is being a bitch)
--[[[
hook.Add("RenderScene", "gwater2_render", function(eye_pos, eye_angles, fov)
	cam.Start3D(eye_pos, eye_angles, fov) -- BuildIMeshes requires a 3d cam context (for frustrum culling)
		local radius = gwater2.solver:GetParameter("radius")
		gwater2.renderer:BuildMeshes(gwater2.solver, radius * 0.5, radius * 0.15)
	cam.End3D()
end)]]

-- gwater2 shader pipeline
hook.Add("PreDrawViewModels", "gwater2_render", function(depth, sky, sky3d)	--PreDrawViewModels
	if gwater2.solver:GetActiveParticles() < 1 then return end

	--if EyePos():DistToSqr(LocalPlayer():EyePos()) > 1 then return end	-- bail if skybox is rendering (used in postdrawopaque)

	-- diffuse particles
	--[[
	render.SetMaterial(Material("models/wireframe"))//Material("models/wireframe"))
	gwater2.solver:RenderParticles(function(pos, size)
		render.DrawSprite(pos, 5, 5, color_white)
	end)]]

	-- Clear render targets
	render.ClearRenderTarget(cache_normals, Color(0, 0, 0, 0))
	render.ClearRenderTarget(cache_depth, Color(0, 0, 0, 0))
	render.ClearRenderTarget(cache_absorption, Color(0, 0, 0, 0))
	render.ClearRenderTarget(cache_bloom, Color(0, 0, 0, 0))
	render.OverrideAlphaWriteEnable(true, true)	-- Required for GWater shaders as they use the alpha component

	-- cached variables
	local scrw = ScrW()
	local scrh = ScrH()
	local water = gwater2.material
	local radius = gwater2.solver:GetParameter("radius")

	-- Build imeshes for multiple passes 
	local forward = EyeAngles():Forward()
	-- render.SetLightingOrigin(EyePos() + (EyeAngles():Forward() * 128))

	-- HACK HACK! hack to make lighting work properly
	render.PushRenderTarget(cache_screen0)
	render.DepthRange(1, 1)
	local tr = util.QuickTrace( EyePos(), LocalPlayer():EyeAngles():Forward() * 800, LocalPlayer())
	local dist = math.min(230, (tr.HitPos - tr.StartPos):Length() / 1.25)
	lightpos = LerpVector(0.8 * FrameTime(), lightpos, EyePos() + (LocalPlayer():EyeAngles():Forward() * dist))
	-- print(dist);
	-- This one sets the cubemap
	render.Model({model="models/shadertest/envballs.mdl",pos=EyePos(),angle=LocalPlayer():GetRenderAngles()})
	-- This one takes care of lights
	render.Model({model="models/shadertest/vertexlit.mdl",pos=lightpos,angle=LocalPlayer():GetRenderAngles()}, lightmodel)
	render.DepthRange(0, 1)
	render.PopRenderTarget()
	
	gwater2.renderer:BuildMeshes(gwater2.solver, radius * 0.5, radius * 0.15)
	--render.SetMaterial(Material("models/props_combine/combine_interface_disp"))

	render.UpdateScreenEffectTexture()	-- _rt_framebuffer is used in refraction shader
	
	-- Depth absorption (disabled when opaque liquids are enabled)
	local _, _, _, a = water:GetVector4D("$color2")
	if water_volumetric:GetFloat("$alpha") != 0 and a > 0 and a < 255 then
		-- ANTIALIAS FIX! (courtesy of Xenthio)
			-- how it works: 
			-- Clear the main rendertarget, keeping depth
			-- Render to main buffer (still has depth), and copy the contents to another rendertarget
			-- Restore the main buffer
		render.SetMaterial(water_volumetric)
		render.Clear(0, 0, 0, 0)
		gwater2.renderer:DrawWater()
		render.CopyTexture(render.GetRenderTarget(), cache_absorption)
		render.DrawTextureToScreen(cache_screen0)
	end
	
	-- Bubble particles inside water
	-- Make sure the water screen texture has bubbles but the normal framebuffer does not
	render.SetMaterial(water_bubble)
	render.UpdateScreenEffectTexture(1)
	gwater2.renderer:DrawDiffuse()
	render.CopyTexture(render.GetRenderTarget(), cache_screen0)
	render.DrawTextureToScreen(cache_screen1)

	-- grab normals
	water_normals:SetFloat("$radius", radius * (39.3701 / 2))
	render.SetMaterial(water_normals)
	render.PushRenderTarget(cache_normals)
	render.SetRenderTargetEx(1, cache_depth)
	render.ClearDepth()
	gwater2.renderer:DrawWater()
	render.PopRenderTarget()
	render.SetRenderTargetEx(1, nil)
	
	-- Blur normals
	water_blur:SetFloat("$radius", radius * 39.3701)
	water_blur:SetTexture("$depthtexture", cache_depth)
	render.SetMaterial(water_blur)
	for i = 1, blur_passes:GetInt() do
		-- Blur X
		--local scale = (5 - i) * 0.05
		local scale = (0.25 / i) * blur_scale:GetFloat()
		water_blur:SetTexture("$normaltexture", cache_normals)
		water_blur:SetVector("$scrs", Vector(scale / scrw, 0))
		render.PushRenderTarget(cache_bloom)	-- Bloom texture resolution is significantly lower than screen res, enabling for a faster blur
		render.DrawScreenQuad()
		render.PopRenderTarget()
		
		-- Blur Y
		water_blur:SetTexture("$normaltexture", cache_bloom)
		water_blur:SetVector("$scrs", Vector(0, scale / scrh))
		render.PushRenderTarget(cache_normals)
		render.DrawScreenQuad()
		render.PopRenderTarget()
	end

	-- Setup water material parameters
	water:SetFloat("$radius", radius * 39.3701)
	water:SetTexture("$normaltexture", cache_normals)
	water:SetTexture("$depthtexture", cache_absorption)
	render.SetMaterial(water)
	gwater2.renderer:DrawWater()
	render.RenderFlashlights( function() gwater2.renderer:DrawWater() end )

	render.OverrideAlphaWriteEnable(false, false)

	render.SetMaterial(water_mist)
	gwater2.renderer:DrawDiffuse()

	-- Debug Draw
	local dbg = 0
	if debug_absorption:GetBool() then render.DrawTextureToScreenRect(cache_absorption, ScrW() * 0.75, (ScrH() / 4) * dbg, ScrW() / 4, ScrH() / 4); dbg = dbg + 1 end
	if debug_normals:GetBool() then render.DrawTextureToScreenRect(cache_normals, ScrW() * 0.75, (ScrH() / 4) * dbg, ScrW() / 4, ScrH() / 4); dbg = dbg + 1 end
	if debug_depth:GetBool() then render.DrawTextureToScreenRect(cache_depth, ScrW() * 0.75, (ScrH() / 4) * dbg, ScrW() / 4, ScrH() / 4); dbg = dbg + 1 end
end)

--hook.Add("NeedsDepthPass", "gwater2_depth", function()
--	DOFModeHack(true)	-- fixes npcs and stuff dissapearing
--	return true
--end)