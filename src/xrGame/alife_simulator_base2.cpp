////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_simulator_base2.cpp
//	Created 	: 25.12.2002
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife Simulator base class
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "alife_simulator_base.h"
#include "relation_registry.h"
#include "alife_registry_wrappers.h"
#include "xrServer_Objects_ALife_Items.h"
#include "alife_graph_registry.h"
#include "alife_object_registry.h"
#include "alife_story_registry.h"
#include "alife_schedule_registry.h"
#include "alife_smart_terrain_registry.h"
#include "alife_group_registry.h"

using namespace ALife;

void CALifeSimulatorBase::register_object(CSE_ALifeDynamicObject* object, bool add_object)
{
    object->on_before_register();

    if (add_object)
        objects().add(object);

    graph().update(object);
    scheduled().add(object);
    story_objects().add(object->m_story_id, object);
    smart_terrains().add(object);
    groups().add(object);

    setup_simulator(object);

    CSE_ALifeInventoryItem* item = smart_cast<CSE_ALifeInventoryItem*>(object);
    if (item && item->attached())
    {
        CSE_ALifeDynamicObject* II = objects().object(item->base()->ID_Parent);

#ifdef DEBUG
        if (std::find(II->children.begin(), II->children.end(), item->base()->ID) != II->children.end())
        {
            Msg("[LSS] Specified item [%s][%d] is already attached to the specified object [%s][%d]",
                item->base()->name_replace(), item->base()->ID, II->name_replace(), II->ID);
            FATAL("[LSS] Cannot recover from the previous error!");
        }
#endif

        II->children.push_back(item->base()->ID);
        II->attach(item, true, false);
    }

    if (can_register_objects())
        object->on_register();
}

void CALifeSimulatorBase::unregister_object(CSE_ALifeDynamicObject* object, bool alife_query)
{
    object->on_unregister();

    CSE_ALifeInventoryItem* item = smart_cast<CSE_ALifeInventoryItem*>(object);
    if (item && item->attached())
        graph().detach(*objects().object(item->base()->ID_Parent), item,
            objects().object(item->base()->ID_Parent)->m_tGraphID, alife_query);

    objects().remove(object->ID);
    story_objects().remove(object->m_story_id);
    smart_terrains().remove(object);
    groups().remove(object);

    // Release frees the object, so every registry holding a raw pointer must drop it
    // here, whatever m_bOnline says. Online victims (rifle kill) and destroyed
    // entities (grenade blast) otherwise leave a dangling CSE_ALifeSchedulable in
    // scheduled() - the next update_scheduled() then calls update() on freed
    // memory (0xC0000005 executing near-null). Both removes below are tolerant
    // when the object was never registered (graph index-driven, scheduled
    // no_assert when need_update() is false), so this is safe for all paths.
    graph().remove(object, object->m_tGraphID);
    // release must never assert: the object can already be out of scheduled()
    // (switched online, debug-spawned base coming online, killed offline).
    scheduled().remove(object, true);
    if (object->m_bOnline && object->ID_Parent == 0xffff)
    {
        // graph().remove() above already removed the level entry (tolerantly).
        // This is only a backup for online objects, so never assert here.
        graph().level().remove(object, true);
    }
}

void CALifeSimulatorBase::on_death(CSE_Abstract* killed, CSE_Abstract* killer)
{
    typedef CSE_ALifeOnlineOfflineGroup::MEMBER GROUP_MEMBER;

    CSE_ALifeCreatureAbstract* creature = smart_cast<CSE_ALifeCreatureAbstract*>(killed);
    if (creature)
        creature->on_death(killer);

    GROUP_MEMBER* member = smart_cast<GROUP_MEMBER*>(killed);
    if (!member)
        return;

    if (member->m_group_id == 0xffff)
        return;

    groups().object(member->m_group_id).notify_on_member_death(member);
}
