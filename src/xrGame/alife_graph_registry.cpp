////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_graph_registry.cpp
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife graph registry
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "alife_graph_registry.h"
#include "xrServerEntities/xrMessages.h"

using namespace ALife;

CALifeGraphRegistry::CALifeGraphRegistry()
{
    m_level = 0;
    m_process_time = 0;
    m_actor = 0;
#ifndef MASTER_GOLD
    m_unregistered_removals = 0;
    m_reported_removals = 0;
#endif
}

CALifeGraphRegistry::~CALifeGraphRegistry() { xr_delete(m_level); }
void CALifeGraphRegistry::on_load()
{
    for (int i = 0; i < GameGraph::LOCATION_TYPE_COUNT; ++i)
    {
        {
            for (int j = 0; j < GameGraph::LOCATION_COUNT; ++j)
                m_terrain[i][j].clear();
        }
        for (GameGraph::_GRAPH_ID j = 0; j < (GameGraph::_GRAPH_ID)ai().game_graph().header().vertex_count(); ++j)
            m_terrain[i][ai().game_graph().vertex(j)->vertex_type()[i]].push_back(j);
    }

    m_objects.resize(ai().game_graph().header().vertex_count());
    m_object_vertex.clear();

    {
        GRAPH_REGISTRY::iterator I = m_objects.begin();
        GRAPH_REGISTRY::iterator E = m_objects.end();
        for (; I != E; ++I)
            (*I).objects().clear();
    }
}

void CALifeGraphRegistry::update(CSE_ALifeDynamicObject* object)
{
    if (!object->m_bDirectControl)
        return;

    if (object->s_flags.is(M_SPAWN_OBJECT_ASPLAYER))
    {
        m_actor = smart_cast<CSE_ALifeCreatureActor*>(object);
        R_ASSERT2(m_actor, "Invalid flag M_SPAWN_OBJECT_ASPLAYER for non-actor object!");

        if (g_start_game_vertex_id)
        {
            m_actor->m_tGraphID = g_start_game_vertex_id;
            m_actor->o_Position = g_start_position;
        }
    }

    if (m_actor && !m_level)
        setup_current_level();

    CSE_ALifeInventoryItem* item = smart_cast<CSE_ALifeInventoryItem*>(object);
    if (!item || !item->attached())
        add(object, object->m_tGraphID);
}

void CALifeGraphRegistry::setup_current_level()
{
    m_level = xr_new<CALifeLevelRegistry>(ai().game_graph().vertex(actor()->m_tGraphID)->level_id());
    level().set_process_time(m_process_time);
    for (int i = 0, n = ai().game_graph().header().vertex_count(); i < n; ++i)
        if (ai().game_graph().vertex(i)->level_id() == level().level_id())
        {
            D_OBJECT_P_MAP::const_iterator I = m_objects[i].objects().objects().begin();
            D_OBJECT_P_MAP::const_iterator E = m_objects[i].objects().objects().end();
            for (; I != E; ++I)
                level().add((*I).second);
        }

    {
        xr_vector<CSE_ALifeDynamicObject*>::const_iterator I = m_temp.begin();
        xr_vector<CSE_ALifeDynamicObject*>::const_iterator E = m_temp.end();
        for (; I != E; ++I)
            level().add(*I);

        m_temp.clear();
    }
    GameGraph::LEVEL_MAP::const_iterator I =
        ai().game_graph().header().levels().find(ai().game_graph().vertex(actor()->m_tGraphID)->level_id());
    R_ASSERT2(ai().game_graph().header().levels().end() != I, "Graph point level ID not found!");

    [[maybe_unused]] const int id = g_pGamePersistent->Level_ID(I->second.name().c_str(), "1.0", true);
    VERIFY3(id >= 0, "Level is corrupted or doesn't exist", I->second.name().c_str());
    ai().load(I->second.name().c_str());

    g_start_game_vertex_id = 0;
}

void CALifeGraphRegistry::attach(CSE_Abstract& object, CSE_ALifeInventoryItem* item,
    GameGraph::_GRAPH_ID game_vertex_id, bool alife_query, bool add_children)
{
#ifdef DEBUG
    if (psAI_Flags.test(aiALife))
    {
        Msg("[LSS] Attaching item [%s][%d] to [%s][%d]", item->base()->name_replace(), item->base()->ID,
            object.name_replace(), object.ID);
    }
#endif
    if (alife_query)
        remove(smart_cast<CSE_ALifeDynamicObject*>(item), game_vertex_id);
    else
        level().remove(smart_cast<CSE_ALifeDynamicObject*>(item));

    CSE_ALifeDynamicObject* dynamic_object = smart_cast<CSE_ALifeDynamicObject*>(&object);
    R_ASSERT2(!alife_query || dynamic_object, "Cannot attach an item to a non-alife object object");

    dynamic_object->attach(item, alife_query, add_children);
}

void CALifeGraphRegistry::detach(CSE_Abstract& object, CSE_ALifeInventoryItem* item,
    GameGraph::_GRAPH_ID game_vertex_id, bool alife_query, bool remove_children)
{
#ifdef DEBUG
    if (psAI_Flags.test(aiALife))
    {
        Msg("[LSS] Detaching item [%s][%d] from [%s][%d]", item->base()->name_replace(), item->base()->ID,
            object.name_replace(), object.ID);
    }
#endif
    if (alife_query)
        add(smart_cast<CSE_ALifeDynamicObject*>(item), game_vertex_id);
    else
    {
        CSE_ALifeDynamicObject* object = smart_cast<CSE_ALifeDynamicObject*>(item);
        VERIFY(object);
        object->m_tGraphID = game_vertex_id;
        level().add(object);
    }

    CSE_ALifeDynamicObject* dynamic_object = smart_cast<CSE_ALifeDynamicObject*>(&object);
    R_ASSERT2(!alife_query || dynamic_object, "Cannot detach an item from non-alife object");

    VERIFY(alife_query || !smart_cast<CSE_ALifeDynamicObject*>(&object) ||
        (ai().game_graph().vertex(smart_cast<CSE_ALifeDynamicObject*>(&object)->m_tGraphID)->level_id() ==
            level().level_id()));

    if (dynamic_object)
        dynamic_object->detach(item, 0, alife_query, remove_children);
    else
    {
#ifdef DEBUG
        bool value =
            std::find(object.children.begin(), object.children.end(), item->base()->ID) != object.children.end();
        if (!value)
        {
            Msg("! ERROR: can't detach independant object. entity[%s:%d], parent[%s:%d], section[%s]",
                item->base()->name_replace(), item->base()->ID, object.name_replace(), object.ID,
                item->base()->s_name.c_str());
        }
#endif // DEBUG
        //		R_ASSERT2				(value,"Can't detach an item which is not on my own");
    }
}

void CALifeGraphRegistry::unregister_object(CSE_ALifeDynamicObject* object)
{
    const u32 vertex_id = registered_vertex(object->ID);

    if (vertex_id != u32(-1))
    {
        if (vertex_id < m_objects.size())
        {
            OBJECT_REGISTRY& registry = m_objects[vertex_id].objects();
            const auto& objects = registry.objects();
            if (objects.find(object->ID) != objects.end())
                registry.remove(object->ID);
        }
        set_registered_vertex(object->ID, u32(-1));
    }

    // an object can also sit in the pre-level queue, where nothing else removes it. Leaving a
    // released object there would mean a dangling pointer in setup_current_level()
    for (u32 i = 0; i < m_temp.size();)
    {
        if (m_temp[i] == object)
            m_temp.erase(m_temp.begin() + i);
        else
            ++i;
    }
}

void CALifeGraphRegistry::add(CSE_ALifeDynamicObject* object, GameGraph::_GRAPH_ID game_vertex_id, bool update)
{
#ifdef DEBUG
    if (psAI_Flags.test(aiALife))
    {
        Msg("[LSS] adding object [%s][%d] to graph point %d", object->name_replace(), object->ID, game_vertex_id);
    }
#endif
    if (!object->m_bOnline && object->used_ai_locations() /**&& object->interactive()**/)
    {
        if (!ai().game_graph().valid_vertex_id(game_vertex_id))
        {
            // scripts may teleport to a graph point which does not exist
#ifndef MASTER_GOLD
            Msg("! [ALife] graph registry: add [%s][%d] - invalid graph point %u, object left where it is",
                object->name_replace(), object->ID, game_vertex_id);
#endif
            return;
        }

        OBJECT_REGISTRY& target = m_objects[game_vertex_id].objects();
        const auto& target_objects = target.objects();
        const auto registered_at_target = target_objects.find(object->ID);

        // already registered where it is being added - nothing to do, and doing it again would
        // raise "Specified object has been already found in the registry!"
        if (registered_vertex(object->ID) != (u32)game_vertex_id ||
            registered_at_target == target_objects.end() || registered_at_target->second != object)
        {
            // drop the entry in the graph point it is really registered at (if any), so that
            // relocation is atomic and cannot leave the object in two graph points at once
            unregister_object(object);

            const auto stale = target.objects().find(object->ID);
            if (stale != target.objects().end())
                target.remove(object->ID, true);

            target.add(object->ID, object);
            set_registered_vertex(object->ID, (u32)game_vertex_id);
        }
        object->m_tGraphID = game_vertex_id;
    }
    else
    {
        if (!m_level && update)
        {
            bool queued = false;
            for (u32 i = 0; i < m_temp.size(); ++i)
            {
                if (m_temp[i] == object)
                {
                    queued = true;
                    break;
                }
            }
            if (!queued)
                m_temp.push_back(object);
            if (ai().game_graph().valid_vertex_id(game_vertex_id))
                object->m_tGraphID = game_vertex_id;
        }
    }

    if (update && m_level && ai().game_graph().valid_vertex_id(game_vertex_id))
    {
        // the level registry is a cache of the objects living on this level, and the same object
        // legitimately reaches it more than once: it is filled by setup_current_level() and by
        // add(), which is called whenever an object is registered, switches online, joins a
        // squad, loses an item and so on (graph().update() on a squad member which just died,
        // for example). Inserting it twice raises "Specified object has been already found in
        // the registry!", so only replace the entry when it really points at another object.
        const auto registered = level().objects().find(object->ID);
        if (registered == level().objects().end())
            level().add(object);
        else if (registered->second != object)
        {
            level().remove(object, true);
            level().add(object);
        }
    }
}

void CALifeGraphRegistry::remove(CSE_ALifeDynamicObject* object, GameGraph::_GRAPH_ID game_vertex_id, bool update)
{
#ifdef DEBUG
    if (object->used_ai_locations() /**&& object->interactive()**/ && psAI_Flags.test(aiALife))
    {
        Msg("[LSS] removing object [%s][%d] from graph point %d", object->name_replace(), object->ID, game_vertex_id);
    }
#endif

    // game_vertex_id is only a hint: object->m_tGraphID is written by code outside of this class
    // and does not always match the graph point which holds the object (squads overwrite it for
    // their members, spawn and save/load write it directly), so the registry index is the only
    // reliable source. Removing from a graph point the object is not in raises "Specified object
    // hasn't been found in the registry!" and kills the simulation.
#ifndef MASTER_GOLD
    if (object->used_ai_locations())
    {
        const u32 vertex_id = registered_vertex(object->ID);
        if (vertex_id != u32(-1))
        {
            u8& reported = report_flags(object->ID);
            if ((reported & report_mismatch) == 0 &&
                (!ai().game_graph().valid_vertex_id(game_vertex_id) || vertex_id != (u32)game_vertex_id))
            {
                reported |= report_mismatch;
                Msg("! [ALife] graph registry: object [%s][%d] is registered at graph point %u, not %u - fixed",
                    object->name_replace(), object->ID, vertex_id, game_vertex_id);
            }
        }
        else
        {
            // not an error: an object which is online, or which was saved with direct control
            // switched off, was never registered in a graph point in the first place
            ++m_unregistered_removals;
            u8& reported = report_flags(object->ID);
            if ((reported & report_unregistered) == 0 && m_reported_removals < 10)
            {
                reported |= report_unregistered;
                ++m_reported_removals;
                Msg("! [ALife] graph registry: remove [%s][%d] - not in the graph registry (graph point %u), "
                    "this is expected for online objects",
                    object->name_replace(), object->ID, game_vertex_id);
            }
            if (m_unregistered_removals % 100 == 0)
                Msg("! [ALife] graph registry: %u objects removed so far which were not in the graph registry",
                    m_unregistered_removals);
        }
    }
#endif

    unregister_object(object);

    if (update && m_level)
    {
        // the level registry is a cache of the objects living on this level (used to decide who
        // switches online/offline), not a source of truth - add() puts the object back, so a
        // missing entry must not kill the simulation
        bool level_no_assert = !ai().game_graph().valid_vertex_id(game_vertex_id) ||
                               object->used_ai_locations() ||
                               ai().game_graph().vertex(game_vertex_id)->level_id() != level().level_id();
        level().remove(object, level_no_assert);
    }
}
