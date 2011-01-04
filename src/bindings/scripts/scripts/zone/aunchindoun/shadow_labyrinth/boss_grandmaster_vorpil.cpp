/* Copyright (C) 2006 - 2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* ScriptData
SDName: Boss_Grandmaster_Vorpil
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

#include "precompiled.h"
#include "def_shadow_labyrinth.h"

#define SAY_INTRO                       -1555028
#define SAY_AGGRO1                      -1555029
#define SAY_AGGRO2                      -1555030
#define SAY_AGGRO3                      -1555031
#define SAY_HELP                        -1555032
#define SAY_SLAY1                       -1555033
#define SAY_SLAY2                       -1555034
#define SAY_DEATH                       -1555035

#define SPELL_RAIN_OF_FIRE          33617
#define H_SPELL_RAIN_OF_FIRE        39363

#define SPELL_DRAW_SHADOWS          33563
#define SPELL_SHADOWBOLT_VOLLEY     33841
#define SPELL_BANISH                38791

#define MOB_VOID_TRAVELER           19226
#define SPELL_SACRIFICE             33587
#define SPELL_SHADOW_NOVA           33846
#define SPELL_EMPOWERING_SHADOWS    33783
#define H_SPELL_EMPOWERING_SHADOWS  39364

#define MOB_VOID_PORTAL             19224
#define SPELL_VOID_PORTAL_VISUAL    33569

float VorpilPosition[3] = {-252.8820,-264.3030,17.1};

float VoidPortalCoords[5][3] =
{
    {-283.5894, -239.5718, 12.7},
    {-306.5853, -258.4539, 12.7},
    {-295.8789, -269.0899, 12.7},
    {-209.3401, -262.7564, 17.1},
    {-261.4533, -297.3298, 17.1}
};

class EmpoweringShadowsAura: public Aura
{
    public:
        EmpoweringShadowsAura(SpellEntry *spell, uint32 eff, int32 *bp, Unit *target, Unit *caster) : Aura(spell, eff, bp, target, caster, NULL) {}
};

struct TRINITY_DLL_DECL mob_voidtravelerAI : public ScriptedAI
{
    mob_voidtravelerAI(Creature *c) : ScriptedAI(c)
    {
        HeroicMode = m_creature->GetMap()->IsHeroic();
    }

    bool HeroicMode;
    uint64 VorpilGUID;
    uint32 move;
    bool sacrificed;

    void Reset()
    {
        VorpilGUID = NULL;
        move = 0;
        sacrificed = false;
        m_creature->setActive(true);
    }

    void EnterCombat(Unit *who){}

    void UpdateAI(const uint32 diff)
    {
        /*
        if(Unit *Vorpil = Unit::GetUnit(*m_creature, VorpilGUID))
        {
            m_creature->DealDamage(m_creature, m_creature->GetMaxHealth(), DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            return;
        }
        */

        if(move < diff)
        {
            Unit *Vorpil = Unit::GetUnit(*m_creature, VorpilGUID);
            if(!Vorpil)
                return;

            if(sacrificed)
            {
                Vorpil->CastSpell(Vorpil, HeroicMode?H_SPELL_EMPOWERING_SHADOWS:SPELL_EMPOWERING_SHADOWS, true);
                DoCast(m_creature, SPELL_SHADOW_NOVA, true);
                //m_creature->DealDamage(m_creature, m_creature->GetMaxHealth(), DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                m_creature->setDeathState(CORPSE);
                m_creature->RemoveCorpse();
                return;
            }

            m_creature->GetMotionMaster()->MoveFollow(Vorpil,0,0);
            m_creature->SetSpeed(MOVE_RUN, 0.3f, true);

            if(m_creature->IsWithinDistInMap(Vorpil, 3))
            {
                DoCast(m_creature, SPELL_SACRIFICE, false);
                sacrificed = true;
                move = 500;
                return;
            }

            if(!Vorpil->isInCombat() || Vorpil->isDead())
            {
                m_creature->DealDamage(m_creature, m_creature->GetMaxHealth(), DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                return;
            }
            move = 1000;
        }
        else
            move -= diff;
    }
};
CreatureAI* GetAI_mob_voidtraveler(Creature *_Creature)
{
    return new mob_voidtravelerAI (_Creature);
}

struct TRINITY_DLL_DECL boss_grandmaster_vorpilAI : public ScriptedAI
{
    boss_grandmaster_vorpilAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = ((ScriptedInstance*)c->GetInstanceData());
        HeroicMode = m_creature->GetMap()->IsHeroic();
        Intro = false;
    }

    ScriptedInstance *pInstance;
    bool Intro, HelpYell;
    bool sumportals;
    bool HeroicMode;

    uint32 ShadowBoltVolley_Timer;
    uint32 DrawShadows_Timer;
    uint32 summonTraveler_Timer;
    uint32 banish_Timer;
    uint64 PortalsGuid[5];

    void Reset()
    {
        ShadowBoltVolley_Timer = 15000;
        DrawShadows_Timer = 45000;
        summonTraveler_Timer = 90000;
        banish_Timer = 17000;
        HelpYell = false;
        destroyPortals();
        m_creature->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_HASTE_SPELLS, true);

        if(pInstance)
            pInstance->SetData(DATA_GRANDMASTERVORPILEVENT, NOT_STARTED);
    }

    void summonPortals()
    {
        if(!sumportals)
        {
            for (int i = 0;i<5;i++)
            {
                Creature *Portal = NULL;
                Portal = m_creature->SummonCreature(MOB_VOID_PORTAL,VoidPortalCoords[i][0],VoidPortalCoords[i][1],VoidPortalCoords[i][2],0,TEMPSUMMON_CORPSE_DESPAWN,3000000);
                if(Portal)
                {
                    PortalsGuid[i] = Portal->GetGUID();
                    Portal->CastSpell(Portal,SPELL_VOID_PORTAL_VISUAL,false);
                }
            }
            sumportals = true;
            summonTraveler_Timer = 5000;
        }
    }

    void destroyPortals()
    {
        if(sumportals)
        {
            for (int i = 0;i < 5; i ++)
            {
                Unit *Portal = Unit::GetUnit((*m_creature), PortalsGuid[i]);
                if (Portal && Portal->isAlive())
                    Portal->DealDamage(Portal, Portal->GetHealth(), DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                PortalsGuid[i] = 0;
            }
            sumportals = false;
        }
    }

    void spawnVoidTraveler()
    {
        int pos = rand()%5;
        m_creature->SummonCreature(MOB_VOID_TRAVELER,VoidPortalCoords[pos][0],VoidPortalCoords[pos][1],VoidPortalCoords[pos][2],0,TEMPSUMMON_CORPSE_TIMED_DESPAWN,5000);
        if(!HelpYell)
        {
            DoScriptText(SAY_HELP, m_creature);
            HelpYell = true;
        }
    }

    void JustSummoned(Creature *summoned)
    {
        if (summoned && summoned->GetEntry() == MOB_VOID_TRAVELER)
            ((mob_voidtravelerAI*)summoned->AI())->VorpilGUID = m_creature->GetGUID();
    }

    void KilledUnit(Unit *victim)
    {
        DoScriptText(RAND(SAY_SLAY1, SAY_SLAY2), m_creature);
    }

    void JustDied(Unit *victim)
    {
        DoScriptText(SAY_DEATH, m_creature);
        destroyPortals();

        if(pInstance)
            pInstance->SetData(DATA_GRANDMASTERVORPILEVENT, DONE);
    }

    void EnterCombat(Unit *who)
    {
        DoScriptText(RAND(SAY_AGGRO1, SAY_AGGRO2, SAY_AGGRO3), m_creature);

        summonPortals();

        if(pInstance)
            pInstance->SetData(DATA_GRANDMASTERVORPILEVENT, IN_PROGRESS);
    }

    void MoveInLineOfSight(Unit *who)
    {
        if(who && !m_creature->getVictim() && m_creature->canStartAttack(who))
            AttackStart(who);
        if (!Intro && who && m_creature->IsWithinLOSInMap(who)&& m_creature->IsWithinDistInMap(who, 100) && m_creature->IsHostileTo(who))
        {
            DoScriptText(SAY_INTRO, m_creature);
            Intro = true;
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (ShadowBoltVolley_Timer < diff)
        {
            DoCast(m_creature,SPELL_SHADOWBOLT_VOLLEY);
            ShadowBoltVolley_Timer = 15000;
        }
        else
            ShadowBoltVolley_Timer -= diff;

        if (HeroicMode && banish_Timer < diff)
        {
            Unit *target = SelectUnit(SELECT_TARGET_RANDOM,0,30,false);
            if (target)
            {
                DoCast(target,SPELL_BANISH);
                banish_Timer = 16000;
            }
        }
        else
            banish_Timer -= diff;

        if ( DrawShadows_Timer < diff)
        {
            Map *map = m_creature->GetMap();
            Map::PlayerList const &PlayerList = map->GetPlayers();
            for(Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                if (Player* i_pl = i->getSource())
                    if (i_pl->isAlive() && !i_pl->HasAura(SPELL_BANISH,0) && !i_pl->isGameMaster())
                        i_pl->TeleportTo(m_creature->GetMapId(), VorpilPosition[0],VorpilPosition[1],VorpilPosition[2], 0, TELE_TO_NOT_LEAVE_COMBAT);

            m_creature->Relocate(VorpilPosition[0],VorpilPosition[1],VorpilPosition[2]);
            DoCast(m_creature,SPELL_DRAW_SHADOWS,true);

            DoCast(m_creature,HeroicMode?H_SPELL_RAIN_OF_FIRE:SPELL_RAIN_OF_FIRE);

            ShadowBoltVolley_Timer = 6000;
            DrawShadows_Timer = 30000;
        }
        else
            DrawShadows_Timer -= diff;

        if ( summonTraveler_Timer < diff)
        {
            spawnVoidTraveler();
            summonTraveler_Timer = 10000;
            //enrage at 20%
            if((m_creature->GetHealth()*5) < m_creature->GetMaxHealth())
                summonTraveler_Timer = 5000;
        }
        else
            summonTraveler_Timer -=diff;

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_grandmaster_vorpil(Creature *_Creature)
{
    return new boss_grandmaster_vorpilAI (_Creature);
}

void AddSC_boss_grandmaster_vorpil()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="boss_grandmaster_vorpil";
    newscript->GetAI = &GetAI_boss_grandmaster_vorpil;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="mob_voidtraveler";
    newscript->GetAI = &GetAI_mob_voidtraveler;
    newscript->RegisterSelf();
}

