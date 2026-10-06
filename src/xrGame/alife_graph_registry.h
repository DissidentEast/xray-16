////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_graph_registry.h
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife graph registry
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "xrServer_Objects_ALife_All.h"
#include "alife_level_registry.h"

class CSE_ALifeCreatureActor;
class CSE_ALifeInventoryItem;

class CALifeGraphRegistry
{
public:
    typedef CSafeMapIterator<ALife::_OBJECT_ID, CSE_ALifeDynamicObject, std::less<ALife::_OBJECT_ID>, false>
        OBJECT_REGISTRY;

public:
    class CGraphPointInfo
    {
    protected:
        OBJECT_REGISTRY m_objects;

    public:
        IC OBJECT_REGISTRY& objects() { return (m_objects); }
        IC const OBJECT_REGISTRY& objects() const { return (m_objects); }
    };

public:
    typedef xr_vector<CGraphPointInfo> GRAPH_REGISTRY;
    typedef xr_vector<GameGraph::_GRAPH_ID> TERRAIN_REGISTRY;

protected:
    GRAPH_REGISTRY m_objects;
    TERRAIN_REGISTRY m_terrain[GameGraph::LOCATION_TYPE_COUNT][GameGraph::LOCATION_COUNT];
    CALifeLevelRegistry* m_level;
    CSE_ALifeCreatureActor* m_actor;
    float m_process_time;
    xr_vector<CSE_ALifeDynamicObject*> m_temp;
    // ALife::_OBJECT_ID -> index of the graph point which really holds the object, or u32(-1)
    // when the object is not registered in m_objects at all. object->m_tGraphID cannot be used
    // as the registry key: it is written by code outside of this class (group switch_offline,
    // spawn, save/load, network spawn), so it may disagree with the registry.
    xr_vector<u32> m_object_vertex;
#ifndef MASTER_GOLD
    // diagnostics: one bit per object id, so a whole squad going missing is reported once
    // instead of once per member per frame
    enum
    {
        report_unregistered = 1,
        report_mismatch = 2
    };
    xr_vector<u8> m_reported;
    u32 m_unregistered_removals;
    u32 m_reported_removals;
#endif

protected:
    void setup_current_level();
    template <typename F, typename C>
    IC void iterate(C& c, const F& f);
    // Removes the object from the graph point it is really registered at (O(1), index driven)
    // and from the pre-level temporary queue. Does nothing if the object is not registered.
    void unregister_object(CSE_ALifeDynamicObject* object);
    IC u32 registered_vertex(const ALife::_OBJECT_ID& id) const;
    IC void set_registered_vertex(const ALife::_OBJECT_ID& id, u32 vertex_id);
#ifndef MASTER_GOLD
    IC u8& report_flags(const ALife::_OBJECT_ID& id);
#endif

public:
    CALifeGraphRegistry();
    virtual ~CALifeGraphRegistry();
    void on_load();
    void update(CSE_ALifeDynamicObject* object);
    void attach(CSE_Abstract& object, CSE_ALifeInventoryItem* item, GameGraph::_GRAPH_ID game_vertex_id,
        bool alife_query = true, bool add_children = true);
    void detach(CSE_Abstract& object, CSE_ALifeInventoryItem* item, GameGraph::_GRAPH_ID game_vertex_id,
        bool alife_query = true, bool remove_children = true);
    IC void assign(CSE_ALifeMonsterAbstract* object);
    void add(CSE_ALifeDynamicObject* object, GameGraph::_GRAPH_ID game_vertex_id, bool bUpdateSwitchObjects = true);
    void remove(CSE_ALifeDynamicObject* object, GameGraph::_GRAPH_ID game_vertex_id, bool bUpdateSwitchObjects = true);
    IC void change(
        CSE_ALifeDynamicObject* object, GameGraph::_GRAPH_ID game_vertex_id, GameGraph::_GRAPH_ID next_game_vertex_id);
    IC CALifeLevelRegistry& level() const;
    IC void set_process_time(const float& process_time);
    IC CSE_ALifeCreatureActor* actor() const;
    IC const GRAPH_REGISTRY& objects() const;
    template <typename F>
    IC void iterate_objects(GameGraph::_GRAPH_ID game_vertex_id, const F& f);
};

#include "alife_graph_registry_inline.h"
