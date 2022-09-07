/*
 * HAimbot.cpp
 *
 *  Created on: Oct 9, 2016
 *      Author: nullifiedcat
 */

#include <hacks/Aimbot.hpp>
#include <hacks/CatBot.hpp>
#include <hacks/AntiAim.hpp>
#include <hacks/ESP.hpp>
#include <hacks/Backtrack.hpp>
#include <PlayerTools.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include "MiscTemporary.hpp"
#include <targethelper.hpp>
#include "hitrate.hpp"
#include "FollowBot.hpp"
#include "Warp.hpp"
#include "AntiCheatBypass.hpp"


bool CAimbot::ShouldRun(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	// Don't run while freecam is active
	if (G::FreecamActive) { return false; }

	// Don't run if aimbot is disabled
	if (!Vars::Aimbot::Global::Active.Value) { return false; }

	// Don't run in menus
	if (I::EngineVGui->IsGameUIVisible() || I::VGuiSurface->IsCursorVisible()) { return false; }

	// Don't run if we are frozen in place.
	if (G::Frozen) { return false; }

	if (!Vars::Aimbot::Global::DontWaitForShot.Value && (!G::WeaponCanAttack && !G::IsAttacking) && G::CurWeaponType != EWeaponType::MELEE)
	{ 
		return false; 
	}	//	don't run if we can't shoot (should stop unbearable dt lag)

	if (!pLocal->IsAlive()
		|| pLocal->IsTaunting()
		|| pLocal->IsBonked()
		|| pLocal->GetFeignDeathReady()
		|| pLocal->IsCloaked()
		|| pLocal->IsInBumperKart()
		|| pLocal->IsAGhost())
	{
		return false;
	}

	//switch (G::CurItemDefIndex)
	//{
	//case Soldier_m_RocketJumper:
	//case Demoman_s_StickyJumper: return false;
	//default: break;
	//}

	//switch (pWeapon->GetWeaponID())
	//{
	//case TF_WEAPON_PDA:
	//case TF_WEAPON_PDA_ENGINEER_BUILD:
	//case TF_WEAPON_PDA_ENGINEER_DESTROY:
	//case TF_WEAPON_PDA_SPY:
	//case TF_WEAPON_PDA_SPY_BUILD:
	//case TF_WEAPON_BUILDER:
	//case TF_WEAPON_INVIS:
	//case TF_WEAPON_BUFF_ITEM:
	//case TF_WEAPON_GRAPPLINGHOOK:
	//	{
	//		return false;
	//	}
	//default: break;
	//}

	//	weapon data check for null damage
	if (CTFWeaponInfo* sWeaponInfo = pWeapon->GetTFWeaponInfo()){
		WeaponData_t sWeaponData = sWeaponInfo->m_WeaponData[0];
		if (sWeaponData.m_nDamage < 1){
			return false;
		}
	}

	return true;
}

void CAimbot::Run(CUserCmd* pCmd)
{
	G::CurrentTargetIdx = 0;
	G::PredictedPos = Vec3();
	G::HitscanRunning = false;
	G::HitscanSilentActive = false;
	G::ProjectileSilentActive = false;
	G::AimPos = Vec3();

	const auto pLocal = g_EntityCache.GetLocal();
	const auto pWeapon = g_EntityCache.GetWeapon();
	if (!pLocal || !pWeapon) { return; }

	if (!ShouldRun(pLocal, pWeapon)) { return; }

	if (SandvichAimbot::bIsSandvich = SandvichAimbot::IsSandvich())
	{
		G::CurWeaponType = EWeaponType::HITSCAN;
	}

	switch (G::CurWeaponType)
	{
	case EWeaponType::HITSCAN:
		{
			F::AimbotHitscan.Run(pLocal, pWeapon, pCmd);
			break;
		}

	case EWeaponType::PROJECTILE:
		{
			F::AimbotProjectile.Run(pLocal, pWeapon, pCmd);
			break;
		}

	case EWeaponType::MELEE:
		{
			F::AimbotMelee.Run(pLocal, pWeapon, pCmd);
			break;
		}

	default: break;
	}
}

#include "AimbotMelee.h"
#include "../../Vars.h"

#include "../../Backtrack/Backtrack.h"

bool CAimbotMelee::CanMeleeHit(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, const Vec3& vecViewAngles,
                               int nTargetIndex)
{
	static Vec3 vecSwingMins = {-18.0f, -18.0f, -18.0f};
	static Vec3 vecSwingMaxs = {18.0f, 18.0f, 18.0f};

	const float flRange = (pWeapon->GetSwingRange(pLocal));

	if (flRange <= 0.0f)
	{
		return false;
	}

	auto vecForward = Vec3();
	Math::AngleVectors(vecViewAngles, &vecForward);

	Vec3 vecTraceStart = pLocal->GetShootPos();
	const Vec3 vecTraceEnd = (vecTraceStart + (vecForward * flRange));

	CTraceFilterHitscan filter;
	filter.pSkip = pLocal;
	CGameTrace trace;
	Utils::TraceHull(vecTraceStart, vecTraceEnd, vecSwingMins, vecSwingMaxs, MASK_SHOT, &filter, &trace);

	if (!(trace.entity && trace.entity->GetIndex() == nTargetIndex))
	{
		if (!Vars::Aimbot::Melee::PredictSwing.Value || pWeapon->GetWeaponID() == TF_WEAPON_KNIFE || pLocal->IsCharging())
		{
			return false;
		}

		static constexpr float FL_DELAY = 0.2f; //it just works

		if (pLocal->OnSolid())
		{
			vecTraceStart += (pLocal->GetVelocity() * FL_DELAY);
		}

		else
		{
			vecTraceStart += (pLocal->GetVelocity() * FL_DELAY) - (Vec3(0.0f, 0.0f, g_ConVars.sv_gravity->GetFloat()) *
				0.5f * FL_DELAY * FL_DELAY);
		}

		Vec3 vecTraceEnd = vecTraceStart + (vecForward * flRange);
		Utils::TraceHull(vecTraceStart, vecTraceEnd, vecSwingMins, vecSwingMaxs, MASK_SHOT, &filter, &trace);
		return (trace.entity && trace.entity->GetIndex() == nTargetIndex);
	}

	return true;
}

EGroupType CAimbotMelee::GetGroupType(CBaseCombatWeapon* pWeapon)
{
	if (Vars::Aimbot::Melee::WhipTeam.Value && pWeapon->GetItemDefIndex() == Soldier_t_TheDisciplinaryAction)
	{
		return EGroupType::PLAYERS_ALL;
	}

	return EGroupType::PLAYERS_ENEMIES;
}

/* Aim at friendly building with the wrench */
bool CAimbotMelee::AimFriendlyBuilding(CBaseObject* pBuilding)
{
	int maxAmmo = 0;
	int maxRocket = 0;

	if (pBuilding->GetLevel() != 3 || pBuilding->GetSapped() || pBuilding->GetHealth() < pBuilding->GetMaxHealth())
	{
		return true;
	}

	if (pBuilding->IsSentrygun())
	{
		switch (pBuilding->GetLevel())
		{
		case 1:
		{
			maxAmmo = 150;
			break;
		}
		case 2:
		{
			maxAmmo = 200;
			break;
		}
		case 3:
		{
			maxAmmo = 200;
			maxRocket = 20; //Yeah?
			break;
		}
		}
	}

	if (pBuilding->GetAmmo() < maxAmmo || pBuilding->GetRockets() < maxRocket)
	{
		return true;
	}

	return false;
}

std::vector<Target_t> CAimbotMelee::GetTargets(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	std::vector<Target_t> validTargets;
	const auto sortMethod = static_cast<ESortMethod>(Vars::Aimbot::Melee::SortMethod.Value);

	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	const bool respectFOV = (sortMethod == ESortMethod::FOV || Vars::Aimbot::Melee::RespectFOV.Value);

	// Players
	if (Vars::Aimbot::Global::AimPlayers.Value)
	{
		const auto groupType = GetGroupType(pWeapon);

		for (const auto& pTarget : g_EntityCache.GetGroup(groupType))
		{
			// Is the target valid and alive?
			if (!pTarget->IsAlive() || pTarget->IsAGhost() || pTarget == pLocal)
			{
				continue;
			}

			if (F::AimbotGlobal.ShouldIgnore(pTarget, true)) { continue; }

			Vec3 vPos = pTarget->GetHitboxPos(HITBOX_PELVIS);
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (respectFOV && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
			{
				continue;
			}
			
			const auto& priority = F::AimbotGlobal.GetPriority(pTarget->GetIndex());
			const float flDistTo = vLocalPos.DistTo(vPos);
			validTargets.push_back({pTarget, ETargetType::PLAYER, vPos, vAngleTo, flFOVTo, flDistTo, -1, false, priority});
		}
	}

	// Buildings
	if (Vars::Aimbot::Global::AimBuildings.Value)
	{
		const bool hasWrench = (pWeapon->GetWeaponID() == TF_WEAPON_WRENCH);
		const bool canDestroySapper = (G::CurItemDefIndex == Pyro_t_Homewrecker || G::CurItemDefIndex == Pyro_t_TheMaul || G::CurItemDefIndex == Pyro_t_NeonAnnihilator || G::CurItemDefIndex ==
			Pyro_t_NeonAnnihilatorG);

		for (const auto& pObject : g_EntityCache.GetGroup(hasWrench || canDestroySapper ? EGroupType::BUILDINGS_ALL : EGroupType::BUILDINGS_ENEMIES))
		{
			const auto& pBuilding = reinterpret_cast<CBaseObject*>(pObject);

			// Is the building valid and alive?
			if (!pObject || !pBuilding || !pObject->IsAlive())
			{
				continue;
			}

			if (hasWrench && (pBuilding->GetTeamNum() == pLocal->GetTeamNum()))
			{
				if (!AimFriendlyBuilding(pBuilding))
				{
					continue;
				}
			}

			if (canDestroySapper && (pBuilding->GetTeamNum() == pLocal->GetTeamNum()))
			{
				if (!pBuilding->GetSapped())
				{
					continue;
				}
			}

			Vec3 vPos = pObject->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

			if (respectFOV && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
			{
				continue;
			}

			const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;
			validTargets.push_back({pObject, ETargetType::BUILDING, vPos, vAngleTo, flFOVTo, flDistTo});
		}
	}

	// NPCs
	if (Vars::Aimbot::Global::AimNPC.Value)
	{
		for (const auto& pNPC : g_EntityCache.GetGroup(EGroupType::WORLD_NPC))
		{
			Vec3 vPos = pNPC->GetWorldSpaceCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);

			const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;

			if (respectFOV && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
			{
				continue;
			}

			validTargets.push_back({pNPC, ETargetType::NPC, vPos, vAngleTo, flFOVTo, flDistTo});
		}
	}

	return validTargets;
}

bool CAimbotMelee::VerifyTarget(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, Target_t& target)
{
	Vec3 hitboxpos;

	// Backtrack the target if required
	if (Vars::Backtrack::Enabled.Value && Vars::Backtrack::LastTick.Value && target.m_TargetType == ETargetType::PLAYER)
	{
		if (const auto& pLastTick = F::Backtrack.GetRecord(target.m_pEntity->GetIndex(), BacktrackMode::Last))
		{
			if (const auto& pHDR = pLastTick->HDR)
			{
				if (const auto& pSet = pHDR->GetHitboxSet(pLastTick->HitboxSet))
				{
					if (const auto& pBox = pSet->hitbox(HITBOX_PELVIS))
					{
						const Vec3 vPos = (pBox->bbmin + pBox->bbmax) * 0.5f;
						Vec3 vOut;
						const matrix3x4& bone = pLastTick->BoneMatrix.BoneMatrix[pBox->bone];
						Math::VectorTransform(vPos, bone, vOut);
						hitboxpos = vOut;
						target.SimTime = pLastTick->SimulationTime;
					}
				}
			}
		}

		// Check if the backtrack pos is visible
		if (Utils::VisPos(pLocal, target.m_pEntity, pLocal->GetShootPos(), hitboxpos))
		{
			target.m_vAngleTo = Math::CalcAngle(pLocal->GetShootPos(), hitboxpos);
			target.m_vPos = hitboxpos;
			target.ShouldBacktrack = true;
			return true;
		}

		// Check if the player is in range for a non-backtrack hit
		if (Vars::Backtrack::Latency.Value > 200)
		{
			return false;
		}
	}

	if (Vars::Aimbot::Melee::RangeCheck.Value && !(Vars::Backtrack::Enabled.Value && Vars::Backtrack::LastTick.Value))
	{
		if (!CanMeleeHit(pLocal, pWeapon, target.m_vAngleTo, target.m_pEntity->GetIndex()))
		{
			return false;
		}
	}
	else
	{
		const float flRange = (pWeapon->GetSwingRange(pLocal));
		if (hitboxpos.DistTo(target.m_vPos) < flRange)
		{
			if (!Utils::VisPos(pLocal, target.m_pEntity, pLocal->GetShootPos(), target.m_vPos))
			{
				return false;
			}
		}
	}

	return true;
}

bool CAimbotMelee::GetTarget(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, Target_t& outTarget)
{
	auto validTargets = GetTargets(pLocal, pWeapon);
	if (validTargets.empty()) { return false; }

	const auto& sortMethod = static_cast<ESortMethod>(Vars::Aimbot::Melee::SortMethod.Value);
	F::AimbotGlobal.SortTargets(&validTargets, sortMethod);

	for (auto& target : validTargets)
	{
		if (VerifyTarget(pLocal, pWeapon, target))
		{
			outTarget = target;
			return true;
		}
	}

	return false;
}

void CAimbotMelee::Aim(CUserCmd* pCmd, Vec3& vAngle)
{
	vAngle -= G::PunchAngles;
	Math::ClampAngles(vAngle);

	switch (Vars::Aimbot::Melee::AimMethod.Value)
	{
	case 0:
		{
			pCmd->viewangles = vAngle;
			I::EngineClient->SetViewAngles(pCmd->viewangles);
			break;
		}

	case 1:
		{
			if (Vars::Aimbot::Melee::SmoothingAmount.Value == 0)
			{
				// plain aim at 0 smoothing factor
				pCmd->viewangles = vAngle;
				I::EngineClient->SetViewAngles(pCmd->viewangles);
				break;
			}

			Vec3 vecDelta = vAngle - pCmd->viewangles;
			Math::ClampAngles(vecDelta);
			pCmd->viewangles += vecDelta / Vars::Aimbot::Melee::SmoothingAmount.Value;
			I::EngineClient->SetViewAngles(pCmd->viewangles);
			break;
		}

	case 2:
		{
			Utils::FixMovement(pCmd, vAngle);
			pCmd->viewangles = vAngle;
			break;
		}

	default: break;
	}
}

bool CAimbotMelee::ShouldSwing(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd, const Target_t& Target)
{
	if (!Vars::Aimbot::Global::AutoShoot.Value)
	{
		return false;
	}

	if (Vars::Backtrack::Enabled.Value && Vars::Backtrack::LastTick.Value)
	{
		const float flRange = pWeapon->GetSwingRange(pLocal);

		if (Target.m_vPos.DistTo(pLocal->GetShootPos()) > flRange * 1.9f) // It just works?
		{
			//I::DebugOverlay->AddLineOverlay(Target.m_vPos, pLocal->GetShootPos(), 255, 0, 0, false, 1.f);
			return false;
		}
		/*I::DebugOverlay->AddLineOverlay(Target.m_vPos, pLocal->GetShootPos(), 0, 255, 0, false, 1.f);*/
		return true;
	}

	//There's a reason for running this even if range check is enabled (it calls this too), trust me :)
	if (!CanMeleeHit(pLocal, pWeapon, Vars::Aimbot::Melee::AimMethod.Value == 2 ? Target.m_vAngleTo : I::EngineClient->GetViewAngles(), Target.m_pEntity->GetIndex()))
	{
		return false;
	}

	return true;
}

bool CAimbotMelee::IsAttacking(const CUserCmd* pCmd, CBaseCombatWeapon* pWeapon)
{
	if (pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
	{
		return ((pCmd->buttons & IN_ATTACK) && G::WeaponCanAttack);
	}

	return fabs(pWeapon->GetSmackTime() - I::GlobalVars->curtime) < I::GlobalVars->interval_per_tick * 2.0f;
}

void CAimbotMelee::Run(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon, CUserCmd* pCmd)
{
	if (!Vars::Aimbot::Global::Active.Value || G::AutoBackstabRunning || pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
	{
		return;
	}

	Target_t target = {};

	const bool bShouldAim = (Vars::Aimbot::Global::AimKey.Value == VK_LBUTTON ? (pCmd->buttons & IN_ATTACK) : F::AimbotGlobal.IsKeyDown());

	if (GetTarget(pLocal, pWeapon, target) && bShouldAim)
	{
		G::CurrentTargetIdx = target.m_pEntity->GetIndex();

		if (Vars::Aimbot::Melee::AimMethod.Value == 2)
		{
			G::AimPos = target.m_vPos;
		}

		// Early swing prediction
		if (ShouldSwing(pLocal, pWeapon, pCmd, target))
		{
			pCmd->buttons |= IN_ATTACK;
		}

		const bool bIsAttacking = IsAttacking(pCmd, pWeapon);

		if (bIsAttacking)
		{
			G::IsAttacking = true;
		}

		// Set the target tickcount (Backtrack)
		if (bIsAttacking && target.m_TargetType == ETargetType::PLAYER)
		{
			const float simTime = target.ShouldBacktrack ? target.SimTime : target.m_pEntity->GetSimulationTime();
			pCmd->tick_count = TIME_TO_TICKS(simTime + G::LerpTime);
		}

		if (Vars::Aimbot::Melee::AimMethod.Value == 2)
		{
			if (bIsAttacking)
			{
				Aim(pCmd, target.m_vAngleTo);
				G::SilentTime = true;
			}
		}
		else
		{
			Aim(pCmd, target.m_vAngleTo);
		}
	}
}

#include "AimbotMelee.h"
#include "../../Vars.h"

#include "../../Backtrack/Backtrack.h"

bool CAimbotMelee::CanMeleeHit(CBaseEntity *pLocal, CBaseCombatWeapon *pWeapon, const Vec3 &vecViewAngles, int nTargetIndex)
{
    static Vec3 vecSwingMins = { -18.0f, -18.0f, -18.0f };
    static Vec3 vecSwingMaxs = { 18.0f, 18.0f, 18.0f };

    const float flRange = (pWeapon->GetSwingRange(pLocal));

    if (flRange <= 0.0f)
    {
        return false;
    }

    auto vecForward = Vec3();
    Math::AngleVectors(vecViewAngles, &vecForward);

    Vec3 vecTraceStart     = pLocal->GetShootPos();
    const Vec3 vecTraceEnd = (vecTraceStart + (vecForward * flRange));

    CTraceFilterHitscan filter;
    filter.pSkip = pLocal;
    CGameTrace trace;
    Utils::TraceHull(vecTraceStart, vecTraceEnd, vecSwingMins, vecSwingMaxs, MASK_SHOT, &filter, &trace);

    if (!(trace.entity && trace.entity->GetIndex() == nTargetIndex))
    {
        if (!Vars::Aimbot::Melee::PredictSwing.Value || pWeapon->GetWeaponID() == TF_WEAPON_KNIFE || pLocal->IsCharging())
        {
            return false;
        }

        static constexpr float FL_DELAY = 0.2f; // it just works

        if (pLocal->OnSolid())
        {
            vecTraceStart += (pLocal->GetVelocity() * FL_DELAY);
        }

        else
        {
            vecTraceStart += (pLocal->GetVelocity() * FL_DELAY) - (Vec3(0.0f, 0.0f, g_ConVars.sv_gravity->GetFloat()) * 0.5f * FL_DELAY * FL_DELAY);
        }

        Vec3 vecTraceEnd = vecTraceStart + (vecForward * flRange);
        Utils::TraceHull(vecTraceStart, vecTraceEnd, vecSwingMins, vecSwingMaxs, MASK_SHOT, &filter, &trace);
        return (trace.entity && trace.entity->GetIndex() == nTargetIndex);
    }

    return true;
}

EGroupType CAimbotMelee::GetGroupType(CBaseCombatWeapon *pWeapon)
{
    if (Vars::Aimbot::Melee::WhipTeam.Value && pWeapon->GetItemDefIndex() == Soldier_t_TheDisciplinaryAction)
    {
        return EGroupType::PLAYERS_ALL;
    }

    return EGroupType::PLAYERS_ENEMIES;
}

/* Aim at friendly building with the wrench */
bool CAimbotMelee::AimFriendlyBuilding(CBaseObject *pBuilding)
{
    int maxAmmo   = 0;
    int maxRocket = 0;

    if (pBuilding->GetLevel() != 3 || pBuilding->GetSapped() || pBuilding->GetHealth() < pBuilding->GetMaxHealth())
    {
        return true;
    }

    if (pBuilding->IsSentrygun())
    {
        switch (pBuilding->GetLevel())
        {
        case 1:
        {
            maxAmmo = 150;
            break;
        }
        case 2:
        {
            maxAmmo = 200;
            break;
        }
        case 3:
        {
            maxAmmo   = 200;
            maxRocket = 20; // Yeah?
            break;
        }
        }
    }

    if (pBuilding->GetAmmo() < maxAmmo || pBuilding->GetRockets() < maxRocket)
    {
        return true;
    }

    return false;
}

std::vector<Target_t> CAimbotMelee::GetTargets(CBaseEntity *pLocal, CBaseCombatWeapon *pWeapon)
{
    std::vector<Target_t> validTargets;
    const auto sortMethod = static_cast<ESortMethod>(Vars::Aimbot::Melee::SortMethod.Value);

    const Vec3 vLocalPos    = pLocal->GetShootPos();
    const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

    const bool respectFOV = (sortMethod == ESortMethod::FOV || Vars::Aimbot::Melee::RespectFOV.Value);

    // Players
    if (Vars::Aimbot::Global::AimPlayers.Value)
    {
        const auto groupType = GetGroupType(pWeapon);

        for (const auto &pTarget : g_EntityCache.GetGroup(groupType))
        {
            // Is the target valid and alive?
            if (!pTarget->IsAlive() || pTarget->IsAGhost() || pTarget == pLocal)
            {
                continue;
            }

            if (F::AimbotGlobal.ShouldIgnore(pTarget, true))
            {
                continue;
            }

            Vec3 vPos           = pTarget->GetHitboxPos(HITBOX_PELVIS);
            Vec3 vAngleTo       = Math::CalcAngle(vLocalPos, vPos);
            const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

            if (respectFOV && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
            {
                continue;
            }

            const auto &priority = F::AimbotGlobal.GetPriority(pTarget->GetIndex());
            const float flDistTo = vLocalPos.DistTo(vPos);
            validTargets.push_back({ pTarget, ETargetType::PLAYER, vPos, vAngleTo, flFOVTo, flDistTo, -1, false, priority });
        }
    }

    // Buildings
    if (Vars::Aimbot::Global::AimBuildings.Value)
    {
        const bool hasWrench        = (pWeapon->GetWeaponID() == TF_WEAPON_WRENCH);
        const bool canDestroySapper = (G::CurItemDefIndex == Pyro_t_Homewrecker || G::CurItemDefIndex == Pyro_t_TheMaul || G::CurItemDefIndex == Pyro_t_NeonAnnihilator || G::CurItemDefIndex == Pyro_t_NeonAnnihilatorG);

        for (const auto &pObject : g_EntityCache.GetGroup(hasWrench || canDestroySapper ? EGroupType::BUILDINGS_ALL : EGroupType::BUILDINGS_ENEMIES))
        {
            const auto &pBuilding = reinterpret_cast<CBaseObject *>(pObject);

            // Is the building valid and alive?
            if (!pObject || !pBuilding || !pObject->IsAlive())
            {
                continue;
            }

            if (hasWrench && (pBuilding->GetTeamNum() == pLocal->GetTeamNum()))
            {
                if (!AimFriendlyBuilding(pBuilding))
                {
                    continue;
                }
            }

            if (canDestroySapper && (pBuilding->GetTeamNum() == pLocal->GetTeamNum()))
            {
                if (!pBuilding->GetSapped())
                {
                    continue;
                }
            }

            Vec3 vPos           = pObject->GetWorldSpaceCenter();
            Vec3 vAngleTo       = Math::CalcAngle(vLocalPos, vPos);
            const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

            if (respectFOV && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
            {
                continue;
            }

            const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;
            validTargets.push_back({ pObject, ETargetType::BUILDING, vPos, vAngleTo, flFOVTo, flDistTo });
        }
    }

    // NPCs
    if (Vars::Aimbot::Global::AimNPC.Value)
    {
        for (const auto &pNPC : g_EntityCache.GetGroup(EGroupType::WORLD_NPC))
        {
            Vec3 vPos     = pNPC->GetWorldSpaceCenter();
            Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);

            const float flFOVTo  = Math::CalcFov(vLocalAngles, vAngleTo);
            const float flDistTo = sortMethod == ESortMethod::DISTANCE ? vLocalPos.DistTo(vPos) : 0.0f;

            if (respectFOV && flFOVTo > Vars::Aimbot::Global::AimFOV.Value)
            {
                continue;
            }

            validTargets.push_back({ pNPC, ETargetType::NPC, vPos, vAngleTo, flFOVTo, flDistTo });
        }
    }

    return validTargets;
}

bool CAimbotMelee::VerifyTarget(CBaseEntity *pLocal, CBaseCombatWeapon *pWeapon, Target_t &target)
{
    Vec3 hitboxpos;

    // Backtrack the target if required
    if (Vars::Backtrack::Enabled.Value && Vars::Backtrack::LastTick.Value && target.m_TargetType == ETargetType::PLAYER)
    {
        if (const auto &pLastTick = F::Backtrack.GetRecord(target.m_pEntity->GetIndex(), BacktrackMode::Last))
        {
            if (const auto &pHDR = pLastTick->HDR)
            {
                if (const auto &pSet = pHDR->GetHitboxSet(pLastTick->HitboxSet))
                {
                    if (const auto &pBox = pSet->hitbox(HITBOX_PELVIS))
                    {
                        const Vec3 vPos = (pBox->bbmin + pBox->bbmax) * 0.5f;
                        Vec3 vOut;
                        const matrix3x4 &bone = pLastTick->BoneMatrix.BoneMatrix[pBox->bone];
                        Math::VectorTransform(vPos, bone, vOut);
                        hitboxpos      = vOut;
                        target.SimTime = pLastTick->SimulationTime;
                    }
                }
            }
        }

        // Check if the backtrack pos is visible
        if (Utils::VisPos(pLocal, target.m_pEntity, pLocal->GetShootPos(), hitboxpos))
        {
            target.m_vAngleTo      = Math::CalcAngle(pLocal->GetShootPos(), hitboxpos);
            target.m_vPos          = hitboxpos;
            target.ShouldBacktrack = true;
            return true;
        }

        // Check if the player is in range for a non-backtrack hit
        if (Vars::Backtrack::Latency.Value > 200)
        {
            return false;
        }
    }

    if (Vars::Aimbot::Melee::RangeCheck.Value && !(Vars::Backtrack::Enabled.Value && Vars::Backtrack::LastTick.Value))
    {
        if (!CanMeleeHit(pLocal, pWeapon, target.m_vAngleTo, target.m_pEntity->GetIndex()))
        {
            return false;
        }
    }
    else
    {
        const float flRange = (pWeapon->GetSwingRange(pLocal));
        if (hitboxpos.DistTo(target.m_vPos) < flRange)
        {
            if (!Utils::VisPos(pLocal, target.m_pEntity, pLocal->GetShootPos(), target.m_vPos))
            {
                return false;
            }
        }
    }

    return true;
}

bool CAimbotMelee::GetTarget(CBaseEntity *pLocal, CBaseCombatWeapon *pWeapon, Target_t &outTarget)
{
    auto validTargets = GetTargets(pLocal, pWeapon);
    if (validTargets.empty())
    {
        return false;
    }

    const auto &sortMethod = static_cast<ESortMethod>(Vars::Aimbot::Melee::SortMethod.Value);
    F::AimbotGlobal.SortTargets(&validTargets, sortMethod);

    for (auto &target : validTargets)
    {
        if (VerifyTarget(pLocal, pWeapon, target))
        {
            outTarget = target;
            return true;
        }
    }

    return false;
}

void CAimbotMelee::Aim(CUserCmd *pCmd, Vec3 &vAngle)
{
    vAngle -= G::PunchAngles;
    Math::ClampAngles(vAngle);

    switch (Vars::Aimbot::Melee::AimMethod.Value)
    {
    case 0:
    {
        pCmd->viewangles = vAngle;
        I::EngineClient->SetViewAngles(pCmd->viewangles);
        break;
    }

    case 1:
    {
        if (Vars::Aimbot::Melee::SmoothingAmount.Value == 0)
        {
            // plain aim at 0 smoothing factor
            pCmd->viewangles = vAngle;
            I::EngineClient->SetViewAngles(pCmd->viewangles);
            break;
        }

        Vec3 vecDelta = vAngle - pCmd->viewangles;
        Math::ClampAngles(vecDelta);
        pCmd->viewangles += vecDelta / Vars::Aimbot::Melee::SmoothingAmount.Value;
        I::EngineClient->SetViewAngles(pCmd->viewangles);
        break;
    }

    case 2:
    {
        Utils::FixMovement(pCmd, vAngle);
        pCmd->viewangles = vAngle;
        break;
    }

    default:
        break;
    }
}

bool CAimbotMelee::ShouldSwing(CBaseEntity *pLocal, CBaseCombatWeapon *pWeapon, CUserCmd *pCmd, const Target_t &Target)
{
    if (!Vars::Aimbot::Global::AutoShoot.Value)
    {
        return false;
    }

    if (Vars::Backtrack::Enabled.Value && Vars::Backtrack::LastTick.Value)
    {
        const float flRange = pWeapon->GetSwingRange(pLocal);

        if (Target.m_vPos.DistTo(pLocal->GetShootPos()) > flRange * 1.9f) // It just works?
        {
            // I::DebugOverlay->AddLineOverlay(Target.m_vPos, pLocal->GetShootPos(), 255, 0, 0, false, 1.f);
            return false;
        }
        /*I::DebugOverlay->AddLineOverlay(Target.m_vPos, pLocal->GetShootPos(), 0, 255, 0, false, 1.f);*/
        return true;
    }

    // There's a reason for running this even if range check is enabled (it calls this too), trust me :)
    if (!CanMeleeHit(pLocal, pWeapon, Vars::Aimbot::Melee::AimMethod.Value == 2 ? Target.m_vAngleTo : I::EngineClient->GetViewAngles(), Target.m_pEntity->GetIndex()))
    {
        return false;
    }

    return true;
}

bool CAimbotMelee::IsAttacking(const CUserCmd *pCmd, CBaseCombatWeapon *pWeapon)
{
    if (pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
    {
        return ((pCmd->buttons & IN_ATTACK) && G::WeaponCanAttack);
    }

    return fabs(pWeapon->GetSmackTime() - I::GlobalVars->curtime) < I::GlobalVars->interval_per_tick * 2.0f;
}

void CAimbotMelee::Run(CBaseEntity *pLocal, CBaseCombatWeapon *pWeapon, CUserCmd *pCmd)
{
    if (!Vars::Aimbot::Global::Active.Value || G::AutoBackstabRunning || pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
    {
        return;
    }

    Target_t target = {};

    const bool bShouldAim = (Vars::Aimbot::Global::AimKey.Value == VK_LBUTTON ? (pCmd->buttons & IN_ATTACK) : F::AimbotGlobal.IsKeyDown());

    if (GetTarget(pLocal, pWeapon, target) && bShouldAim)
    {
        G::CurrentTargetIdx = target.m_pEntity->GetIndex();

        if (Vars::Aimbot::Melee::AimMethod.Value == 2)
        {
            G::AimPos = target.m_vPos;
        }

        // Early swing prediction
        if (ShouldSwing(pLocal, pWeapon, pCmd, target))
        {
            pCmd->buttons |= IN_ATTACK;
        }

        const bool bIsAttacking = IsAttacking(pCmd, pWeapon);

        if (bIsAttacking)
        {
            G::IsAttacking = true;
        }

        // Set the target tickcount (Backtrack)
        if (bIsAttacking && target.m_TargetType == ETargetType::PLAYER)
        {
            const float simTime = target.ShouldBacktrack ? target.SimTime : target.m_pEntity->GetSimulationTime();
            pCmd->tick_count    = TIME_TO_TICKS(simTime + G::LerpTime);
        }

        if (Vars::Aimbot::Melee::AimMethod.Value == 2)
        {
            if (bIsAttacking)
            {
                Aim(pCmd, target.m_vAngleTo);
                G::SilentTime = true;
            }
        }
        else
        {
            Aim(pCmd, target.m_vAngleTo);
        }
    }
}
