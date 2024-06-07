SWEP.PrintName = "Water Gun"
    
SWEP.Author = "Mee / Neatro" 
SWEP.Purpose = "shoot water man"
SWEP.Instructions = "just left click"
SWEP.Category = "gwater2" 
SWEP.DrawAmmo       = false
SWEP.DrawCrosshair	= true
SWEP.DrawWeaponInfoBox = false

SWEP.Spawnable = true --Must be true
SWEP.AdminOnly = false

SWEP.AutoSwitchTo = false
SWEP.AutoSwitchFrom = false
SWEP.Weight = 1

SWEP.Primary.ClipSize      = -1
SWEP.Primary.DefaultClip   = -1
SWEP.Primary.Automatic     = true
SWEP.Primary.Ammo          = "none"
SWEP.Primary.Delay = 0

SWEP.Base = "weapon_base"

SWEP.Secondary.ClipSize		= -1
SWEP.Secondary.DefaultClip	= -1
SWEP.Secondary.Automatic	= false
SWEP.Secondary.Ammo		= "none"
SWEP.Secondary.Delay = 0

SWEP.ViewModelFlip		= false
SWEP.ViewModelFOV		= 70
SWEP.ViewModel			= "models/weapons/c_pistol.mdl" 
SWEP.WorldModel			= "models/weapons/w_pistol.mdl"
SWEP.UseHands           = true

function SWEP:Initialize()
	self:SetMaterial("hunter/myplastic")
	self:SetModelScale(0.5)
end 

local function fuckgarry(w, s)
	if SERVER then 
		if game.SinglePlayer() then 
			w:CallOnClient(s) 
		end
		return true
	end
	return IsFirstTimePredicted()
end

function SWEP:PrimaryAttack()
	--self:SetNextPrimaryFire(CurTime() + 1/60)
	if fuckgarry(self, "PrimaryAttack") then return end

	local owner = self:GetOwner()
	local forward = owner:EyeAngles():Forward()
	local sprite_size = gwater2.solver:GetParameter("radius") * math.Rand(1, 1.01)
	gwater2.solver:AddCube(owner:EyePos() + forward * 20 * sprite_size * 64, forward * 500, Vector(2, 2, 2), sprite_size * 2)
end

function SWEP:SecondaryAttack()
	if fuckgarry(self, "SecondaryAttack") then return end

	local owner = self:GetOwner()
	local forward = owner:EyeAngles():Forward()
	local sprite_size = gwater2.solver:GetParameter("fluid_rest_distance")
	gwater2.solver:AddCube(owner:EyePos() + forward * 40 * sprite_size * 64, forward * 500, Vector(16, 16, 16), sprite_size * 2)
end

function SWEP:Reload()
	if fuckgarry(self, "Reload") then return end
	
	gwater2.solver:Reset()
end

if SERVER then return end

function SWEP:DrawHUD()
	draw.DrawText("Left-Click to Spawn Particles", "CloseCaption_Normal", ScrW() * 0.99, ScrH() * 0.75, color_white, TEXT_ALIGN_RIGHT)
	draw.DrawText("Right-Click to Spawn a Cube", "CloseCaption_Normal", ScrW() * 0.99, ScrH() * 0.78, color_white, TEXT_ALIGN_RIGHT)
	draw.DrawText("Reload to Remove All", "CloseCaption_Normal", ScrW() * 0.99, ScrH() * 0.81, color_white, TEXT_ALIGN_RIGHT)
end

local function format_int(i)
	return tostring(i):reverse():gsub("%d%d%d", "%1,"):reverse():gsub("^,", "")
end

-- visual counter on gun
function SWEP:PostDrawViewModel(vm, weapon, ply)
	local pos, ang = vm:GetBonePosition(39)--self:GetOwner():GetViewModel():GetBonePosition(0)
	if !pos or !ang then return end
	
	ang = ang + Angle(180, 0, -ang[3] * 2)
	pos = pos - ang:Right() * 1.5
	cam.Start3D2D(pos, ang, 0.03)
		local text = "Water Particles: " .. format_int(gwater2.solver:GetActiveParticles()) .. "/" .. format_int(gwater2.solver:GetMaxParticles())
		local text2 = "Foam Particles: " .. format_int(gwater2.solver:GetActiveDiffuse()) .. "/" .. format_int(gwater2.solver:GetMaxDiffuseParticles())
		draw.DrawText(text, "CloseCaption_Normal", 4, -24, Color(0, 0, 0, 255), TEXT_ALIGN_CENTER)
		draw.DrawText(text, "CloseCaption_Normal", 2, -26, color_white, TEXT_ALIGN_CENTER)

		draw.DrawText(text2, "CloseCaption_Normal", 2, 2, Color(0, 0, 0, 255), TEXT_ALIGN_CENTER)
		draw.DrawText(text2, "CloseCaption_Normal", 0, 0, color_white, TEXT_ALIGN_CENTER)
	cam.End3D2D()
end

-- PostDrawViewModel^ doesn't write to depth buffer... add our own rendering hook
-- TODO: This code sucks. Rewrite this
local wireframe = Material("models/wireframe")
hook.Add("PostDrawTranslucentRenderables", "gwater2_fuckthisshitman", function()
	local owner = LocalPlayer()
	local wep = owner:GetActiveWeapon()
	if IsValid(wep) and wep:GetClass() == "weapon_gw2_watergun" then
		local forward = owner:EyeAngles():Forward()

		-- tiny cube
		local sprite_size = gwater2.solver:GetParameter("radius") * 64
		local pos = owner:EyePos() + forward * 20 * sprite_size 
		local size = Vector(2, 2, 2) * sprite_size

		-- big cube
		local sprite_size = gwater2.solver:GetParameter("fluid_rest_distance") * 64
		local pos2 = owner:EyePos() + forward * 40 * sprite_size
		local size2 = Vector(16.5, 16.5, 16.5) * sprite_size

		render.DrawWireframeBox(pos, Angle(), -size, size, Color(255, 255, 255, 255), true)
		
		--render.DrawWireframeBox(pos2, Angle(), -size2, size2, Color(255, 255, 255, 255), true)
	end
end)

--[[ -- Benchmarking stuff (ignore)
local avg = 0
local line = 0
local lines = {}
surface.SetDrawColor(0, 255, 0, 255)
avg = avg + (RealFrameTime() - avg) * 0.01

lines[line] = -(6 / avg) + 1080
for i = 0, 1920 do

	if !lines[i - 1] or !lines[i] then continue end
	surface.DrawLine(i * 1, lines[i - 1], i * 1 + 1, lines[i])
end


line = (line + 1) % 1920]]