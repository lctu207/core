/* Copyright ?2006 - 2008 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* ScriptData
SDName: boss_Akilzon
SD%Complete: 75%
SDComment: Missing timer for Call Lightning and Sound ID's
SQLUpdate:
#Temporary fix for Soaring Eagles

EndScriptData */

#include "precompiled.h"
#include "def_zulaman.h"
#include "Weather.h"

#define SPELL_STATIC_DISRUPTION     43622
#define SPELL_CALL_LIGHTNING        43661 //Missing timer
#define SPELL_GUST_OF_WIND          43621
#define SPELL_ELECTRICAL_STORM      43648
#define SPELL_BERSERK               45078
#define SPELL_ELECTRICAL_DAMAGE     43657
#define SPELL_ELECTRICAL_OVERLOAD   43658
#define SPELL_EAGLE_SWOOP           44732

//"Your death gonna be quick, strangers. You shoulda never have come to this place..."
#define SAY_ONAGGRO "I be da predator! You da prey..."
#define SAY_ONDEATH "You can't... kill... me spirit!"
#define SAY_ONSLAY1 "Ya got nothin'!"
#define SAY_ONSLAY2 "Stop your cryin'!"
#define SAY_ONSUMMON "Feed, me bruddahs!"
#define SAY_ONENRAGE "All you be doing is wasting my time!"
#define SOUND_ONAGGRO 12013
#define SOUND_ONDEATH 12019
#define SOUND_ONSLAY1 12017
#define SOUND_ONSLAY2 12018
#define SOUND_ONSUMMON 12014
#define SOUND_ONENRAGE 12016

#define MOB_SOARING_EAGLE 24858
#define SE_LOC_X_MAX 400
#define SE_LOC_X_MIN 335
#define SE_LOC_Y_MAX 1435
#define SE_LOC_Y_MIN 1370

struct TRINITY_DLL_DECL boss_akilzonAI : public ScriptedAI
{
    boss_akilzonAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = ((ScriptedInstance*)c->GetInstanceData());
        m_creature->GetPosition(wLoc);
    }
    ScriptedInstance *pInstance;

    uint64 BirdGUIDs[8];
    uint64 CycloneGUID;
    uint64 CloudGUID;

    uint32 StaticDisruption_Timer;
    uint32 GustOfWind_Timer;
    uint32 CallLighting_Timer;
    uint32 ElectricalStorm_Timer;
    uint32 SummonEagles_Timer;
    uint32 Enrage_Timer;

    bool isRaining;

    uint32 checkTimer;
    WorldLocation wLoc;

    void Reset()
    {
        if(pInstance && pInstance->GetData(DATA_AKILZONEVENT) != DONE)
            pInstance->SetData(DATA_AKILZONEVENT, NOT_STARTED);

        StaticDisruption_Timer = urand(5000, 10000);
        GustOfWind_Timer = urand(8000, 15000);
        CallLighting_Timer = urand(8000, 12000);
        ElectricalStorm_Timer = 60000;
        Enrage_Timer = 480000; //8 minutes to enrage
        SummonEagles_Timer = 99999;

        CloudGUID = 0;
        CycloneGUID = 0;
        DespawnSummons();
        for(uint8 i = 0; i < 8; i++)
            BirdGUIDs[i] = 0;

        isRaining = false;

        SetWeather(WEATHER_STATE_FINE, 0.0f);

        checkTimer = 3000;
    }

    void EnterCombat(Unit *who)
    {
        DoYell(SAY_ONAGGRO, LANG_UNIVERSAL, NULL);
        DoPlaySoundToSet(m_creature, SOUND_ONAGGRO);
        DoZoneInCombat();
        if(pInstance)
            pInstance->SetData(DATA_AKILZONEVENT, IN_PROGRESS);
    }

    void JustDied(Unit* Killer)
    {
        DoYell(SAY_ONDEATH,LANG_UNIVERSAL,NULL);
        DoPlaySoundToSet(m_creature, SOUND_ONDEATH);
        if(pInstance)
            pInstance->SetData(DATA_AKILZONEVENT, DONE);
        DespawnSummons();
    }

    void KilledUnit(Unit* victim)
    {
        switch(rand()%2)
        {
        case 0:
            DoYell(SAY_ONSLAY1, LANG_UNIVERSAL, NULL);
            DoPlaySoundToSet(m_creature, SOUND_ONSLAY1);
            break;
        case 1:
            DoYell(SAY_ONSLAY2, LANG_UNIVERSAL, NULL);
            DoPlaySoundToSet(m_creature, SOUND_ONSLAY2);
            break;
        }
    }

    void DespawnSummons()
    {
        for (uint8 i = 0; i < 8; i++)
        {
            Unit* bird = Unit::GetUnit(*m_creature,BirdGUIDs[i]);
            if(bird && bird->isAlive())
            {
                bird->SetVisibility(VISIBILITY_OFF);
                bird->setDeathState(JUST_DIED);
            }
        }
    }

    void SetWeather(uint32 weather, float grade)
    {
        Map *map = m_creature->GetMap();
        if (!map->IsDungeon()) return;

        WorldPacket data(SMSG_WEATHER, (4+4+4));
        data << uint32(weather) << (float)grade << uint8(0);

        map->SendToPlayers(&data);
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (checkTimer < diff)
        {
            if (!m_creature->IsWithinDistInMap(&wLoc, 110.0f))
                EnterEvadeMode();
            else
            {
                m_creature->SetSpeed(MOVE_RUN, 2.0);
                DoZoneInCombat();
            }
            checkTimer = 1000;
        }
        else
            checkTimer -= diff;

        if (Enrage_Timer < diff)
        {
            DoYell(SAY_ONENRAGE, LANG_UNIVERSAL, NULL);
            DoPlaySoundToSet(m_creature, SOUND_ONENRAGE);
            m_creature->CastSpell(m_creature, SPELL_BERSERK, true);
            Enrage_Timer = 600000;
        }
        else
            Enrage_Timer -= diff;

        if (StaticDisruption_Timer < diff)
        {
            Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 0, GetSpellMaxRange(SPELL_STATIC_DISRUPTION), true, m_creature->getVictimGUID());
            if(!target)
                target = m_creature->getVictim();
            AddSpellToCast(target, SPELL_STATIC_DISRUPTION, false, true);
            StaticDisruption_Timer = urand(7000, 14000);
        }
        else
            StaticDisruption_Timer -= diff;

        if (GustOfWind_Timer < diff)
        {
            //we dont want to start a storm with player in the air
            if(ElectricalStorm_Timer < 9000)
                GustOfWind_Timer += 20000;

            if(Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 0, GetSpellMaxRange(SPELL_GUST_OF_WIND), true, m_creature->getVictimGUID()))
                AddSpellToCast(target, SPELL_GUST_OF_WIND);
            GustOfWind_Timer = urand(8000, 14000);
        }
        else
            GustOfWind_Timer -= diff;

        if (CallLighting_Timer < diff)
        {
            AddSpellToCast(m_creature->getVictim(), SPELL_CALL_LIGHTNING);
            CallLighting_Timer = RAND(urand(10000, 15000), urand(30000, 45000));
        }
        else
            CallLighting_Timer -= diff;

        if (!isRaining && ElectricalStorm_Timer < urand(8000, 12000))
        {
            SetWeather(WEATHER_STATE_HEAVY_RAIN, 0.9999f);
            isRaining = true;
        }

        if (isRaining && ElectricalStorm_Timer > 50000)
        {
            SetWeather(WEATHER_STATE_FINE, 0.0f);
            SummonEagles_Timer = 13000;
            isRaining = false;
        }

        if (ElectricalStorm_Timer < diff)
        {
            Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 0, GetSpellMaxRange(SPELL_ELECTRICAL_STORM), true);

            if(!target)
            {
                EnterEvadeMode();
                return;
            }
            // throw player to air and cast electrical storm on
            float x,y,z;
            target->GetPosition(x,y,z);
            target->SendMonsterMove(x,y,m_creature->GetPositionZ()+15,0);
            m_creature->CastSpell(target, SPELL_ELECTRICAL_STORM, false);

            ElectricalStorm_Timer = 60000;
            StaticDisruption_Timer += 10000;
        }
        else
            ElectricalStorm_Timer -= diff;

        if (SummonEagles_Timer < diff)
        {
            DoYell(SAY_ONSUMMON, LANG_UNIVERSAL, NULL);
            DoPlaySoundToSet(m_creature, SOUND_ONSUMMON);

            float x, y, z;
            m_creature->GetPosition(x, y, z);

            for (uint8 i = 0; i < 8; i++)
            {
                Unit* bird = Unit::GetUnit(*m_creature,BirdGUIDs[i]);
                if(!bird)//they despawned on die
                {
                    if(Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 0))
                    {
                        x = target->GetPositionX() + 10 - rand()%20;
                        y = target->GetPositionY() + 10 - rand()%20;
                        z = target->GetPositionZ() + 6 + rand()%5 + 10;
                        if(z > 95) z = 95 - rand()%5;
                    }
                    Creature *pCreature = m_creature->SummonCreature(MOB_SOARING_EAGLE, x, y, z, 0, TEMPSUMMON_CORPSE_DESPAWN, 0);
                    if (pCreature)
                    {
                        pCreature->AddThreat(m_creature->getVictim(), 1.0f);
                        pCreature->AI()->AttackStart(m_creature->getVictim());
                        BirdGUIDs[i] = pCreature->GetGUID();
                    }
                }
            }
            SummonEagles_Timer = 999999;
        }
        else
            SummonEagles_Timer -= diff;

        DoMeleeAttackIfReady();
        CastNextSpellIfAnyAndReady();
    }
};

struct TRINITY_DLL_DECL mob_soaring_eagleAI : public ScriptedAI
{
    mob_soaring_eagleAI(Creature *c) : ScriptedAI(c) {}

    uint32 EagleSwoop_Timer;
    bool arrived;
    uint32 TargetGUID;

    void Reset()
    {
        EagleSwoop_Timer = 5000 + rand()%5000;
        arrived = true;
        TargetGUID = 0;
        m_creature->SetUnitMovementFlags(MOVEMENTFLAG_LEVITATING);
    }

    void EnterCombat(Unit *who) {DoZoneInCombat();}

    void MoveInLineOfSight(Unit *) {}

    void MovementInform(uint32, uint32)
    {
        arrived = true;
        if(TargetGUID)
        {
            if(Unit* target = Unit::GetUnit(*m_creature, TargetGUID))
                m_creature->CastSpell(target, SPELL_EAGLE_SWOOP, true);
            TargetGUID = 0;
            m_creature->SetSpeed(MOVE_RUN, 1.2f);
            EagleSwoop_Timer = 5000 + rand()%5000;
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if(EagleSwoop_Timer < diff) EagleSwoop_Timer = 0;
        else EagleSwoop_Timer -= diff;

        if(arrived)
        {
            if(Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 0))
            {
                float x, y, z;
                if(EagleSwoop_Timer)
                {
                    x = target->GetPositionX() + 10 - rand()%20;
                    y = target->GetPositionY() + 10 - rand()%20;
                    z = target->GetPositionZ() + 10 + rand()%5;
                    if(z > 95) z = 95 - rand()%5;
                }
                else
                {
                    target->GetContactPoint(m_creature, x, y, z);
                    z += 2;
                    m_creature->SetSpeed(MOVE_RUN, 5.0f);
                    TargetGUID = target->GetGUID();
                }
                m_creature->AddUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT);
                m_creature->GetMotionMaster()->MovePoint(0, x, y, z);
                m_creature->RemoveUnitMovementFlag(MOVEMENTFLAG_ONTRANSPORT);
                arrived = false;
            }
        }
    }
};

//Soaring Eagle
CreatureAI* GetAI_mob_soaring_eagle(Creature *_Creature)
{
    return new mob_soaring_eagleAI(_Creature);
}

CreatureAI* GetAI_boss_akilzon(Creature *_Creature)
{
    return new boss_akilzonAI(_Creature);
}

void AddSC_boss_akilzon()
{
    Script *newscript = NULL;

    newscript = new Script;
    newscript->Name="boss_akilzon";
    newscript->GetAI = &GetAI_boss_akilzon;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="mob_akilzon_eagle";
    newscript->GetAI = &GetAI_mob_soaring_eagle;
    newscript->RegisterSelf();
}

