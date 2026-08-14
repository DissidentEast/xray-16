////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_online_offline_group_brain.cpp
//	Created 	: 25.10.2005
//  Modified 	: 25.10.2005
//	Author		: Dmitriy Iassenev
//	Description : ALife Online Offline Group brain class
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "alife_online_offline_group_brain.h"
#include "Common/object_broker.h"
#include "xrServer_Objects_ALife_Monsters.h"

#include "alife_monster_movement_manager.h"
#include "alife_monster_detail_path_manager.h"
#include "alife_monster_patrol_path_manager.h"
#include "ai_space.h"
#include "ef_storage.h"
#include "ef_primary.h"
#include "alife_simulator.h"
#include "alife_graph_registry.h"
#include "movement_manager_space.h"
#include "alife_smart_terrain_registry.h"
#include "alife_object_registry.h"
#include "alife_time_manager.h"
#include "date_time.h"
#ifdef DEBUG
#include "Level.h"
#include "map_location.h"
#include "map_manager.h"
#endif

CALifeOnlineOfflineGroupBrain::CALifeOnlineOfflineGroupBrain(object_type* object)
{
    VERIFY(object);
    m_object = object;

    m_movement_manager = xr_new<CALifeMonsterMovementManager>(object);
}

CALifeOnlineOfflineGroupBrain::~CALifeOnlineOfflineGroupBrain()
{
    xr_delete(m_movement_manager);
}

void CALifeOnlineOfflineGroupBrain::on_state_write(NET_Packet& packet) {}
void CALifeOnlineOfflineGroupBrain::on_state_read(NET_Packet& packet) {}

void CALifeOnlineOfflineGroupBrain::on_register() {}
void CALifeOnlineOfflineGroupBrain::on_unregister() {}
void CALifeOnlineOfflineGroupBrain::on_location_change() {}
void CALifeOnlineOfflineGroupBrain::update()
{
    CALifeSmartTerrainTask* const task = object().get_current_task();
    if (!task)
    {
        // Previously a fatal THROW2: a group registered in a smart terrain that
        // currently has no task for it would crash the game. Recover by skipping
        // the update tick instead; the task may reappear on a later update.
        Msg("! [ALife] group [%d][%s] has no smart terrain task, skipping update", object().ID,
            object().name_replace());
        return;
    }
    movement().path_type(MovementManager::ePathTypeGamePath);
    movement().detail().target(*task);
    movement().update();
}

void CALifeOnlineOfflineGroupBrain::update_position() { movement().update_position(); }

void CALifeOnlineOfflineGroupBrain::on_switch_online() { movement().on_switch_online(); }
void CALifeOnlineOfflineGroupBrain::on_switch_offline() { movement().on_switch_offline(); }
