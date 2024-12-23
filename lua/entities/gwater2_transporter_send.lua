AddCSLuaFile()

ENT.Type = "anim"
ENT.Base = "base_anim"

ENT.Category     = "GWater2"
ENT.PrintName    = "Transporter Sender"
ENT.Author       = "googer_"
ENT.Purpose      = ""
ENT.Instructions = ""
ENT.Spawnable    = false
ENT.Editable	 = true

function ENT:SetupDataTables()
    self:NetworkVar("Float", 0, "Radius", {KeyName = "Radius", Edit = {type = "Float", order = 0, min = 0, max = 100}})
	self:NetworkVar("Float", 1, "Strength", {KeyName = "Strength", Edit = {type = "Float", order = 1, min = 0, max = 200}})

	if SERVER then return end

	self.PARTICLE_EMITTER = ParticleEmitter(self:GetPos(), false)
	hook.Add("gwater2_tick_drains", self, function()
		gwater2.solver:AddForceField(self:GetPos(), self:GetRadius(), -self:GetStrength(), 0, true)
		self.GWATER2_particles_drained = math.max(0, 
			(self.GWATER2_particles_drained or 0) +
			gwater2.solver:RemoveSphere(gwater2.quick_matrix(self:GetPos(), nil, self:GetRadius()))
		)
	end)
end

function ENT:Initialize()
	if CLIENT then return end
	self:SetModel("models/xqm/button3.mdl")
	self:SetMaterial("phoenix_storms/dome")
	
	self:PhysicsInit(SOLID_VPHYSICS)
	self:SetMoveType(MOVETYPE_VPHYSICS)
	self:SetSolid(SOLID_VPHYSICS)
end