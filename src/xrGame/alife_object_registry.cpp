////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_object_registry.cpp
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife object registry
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "alife_object_registry.h"
#include "ai_debug.h"
#include "xrServerEntities/xrMessages.h"

CALifeObjectRegistry::CALifeObjectRegistry(LPCSTR section) {}
CALifeObjectRegistry::~CALifeObjectRegistry()
{
    OBJECT_REGISTRY::iterator const B = m_objects.begin();
    OBJECT_REGISTRY::iterator I = B;
    OBJECT_REGISTRY::iterator const E = m_objects.end();
    for (; I != E; ++I)
        (*I).second->on_unregister();

    for (I = B; I != E; ++I)
        xr_delete((*I).second);
}

void CALifeObjectRegistry::save(IWriter& memory_stream, CSE_ALifeDynamicObject* object, u32& object_count,
    xr_set<ALife::_OBJECT_ID>& saved)
{
    // Cycle / duplicate guard: a corrupt or mutated object graph (an object
    // listed as a child of more than one parent, or a child cycle) must not
    // cause an object to be saved twice or recurse forever.
    if (!saved.insert(object->ID).second)
        return;

    ++object_count;

    NET_Packet tNetPacket;
    // Spawn
    object->Spawn_Write(tNetPacket, TRUE);
    memory_stream.w_u16(u16(tNetPacket.B.count));
    memory_stream.w(tNetPacket.B.data, tNetPacket.B.count);

    // Update
    tNetPacket.w_begin(M_UPDATE);
    object->UPDATE_Write(tNetPacket);

    memory_stream.w_u16(u16(tNetPacket.B.count));
    memory_stream.w(tNetPacket.B.data, tNetPacket.B.count);

    ALife::OBJECT_VECTOR::const_iterator I = object->children.begin();
    ALife::OBJECT_VECTOR::const_iterator E = object->children.end();
    for (; I != E; ++I)
    {
        CSE_ALifeDynamicObject* child = this->object(*I, true);
        if (!child)
            continue;

        if (!child->can_save())
            continue;

        save(memory_stream, child, object_count, saved);
    }
}

void CALifeObjectRegistry::save(IWriter& memory_stream)
{
    Msg("* Saving objects...");
    memory_stream.open_chunk(OBJECT_CHUNK_DATA);

    u32 position = memory_stream.tell();
    memory_stream.w_u32(u32(-1));

    xr_set<ALife::_OBJECT_ID> saved;
    u32 object_count = 0;
    OBJECT_REGISTRY::iterator I = m_objects.begin();
    OBJECT_REGISTRY::iterator E = m_objects.end();
    for (; I != E; ++I)
    {
        if (!(*I).second->can_save())
            continue;

        if ((*I).second->redundant())
            continue;

        if ((*I).second->ID_Parent != 0xffff)
        {
            // Not a root object: it is normally saved through its parent's
            // recursion. If the parent is missing / not savable, save it as a
            // standalone root instead of silently losing it from the save.
            CSE_ALifeDynamicObject* parent = object((*I).second->ID_Parent, true);
            if (parent && parent->can_save() && !parent->redundant())
                continue;

            ALife::_OBJECT_ID parent_id = (*I).second->ID_Parent;
            (*I).second->ID_Parent = 0xffff;
            save(memory_stream, (*I).second, object_count, saved);
            (*I).second->ID_Parent = parent_id;
            continue;
        }

        save(memory_stream, (*I).second, object_count, saved);
    }

    u32 last_position = memory_stream.tell();
    memory_stream.seek(position);
    memory_stream.w_u32(object_count);
    memory_stream.seek(last_position);

    memory_stream.close_chunk();

    Msg("* %d objects are successfully saved", object_count);
}

CSE_ALifeDynamicObject* CALifeObjectRegistry::get_object(IReader& file_stream)
{
    NET_Packet tNetPacket;
    u16 u_id;
    // Spawn
    if (file_stream.elapsed() < 2)
    {
        Msg("! [ALife] object registry: truncated spawn packet size in save, aborting object load");
        return (0);
    }
    tNetPacket.B.count = file_stream.r_u16();
    if ((size_t)tNetPacket.B.count > sizeof(tNetPacket.B.data) || file_stream.elapsed() < tNetPacket.B.count)
    {
        Msg("! [ALife] object registry: invalid spawn packet size %u in save, aborting object load",
            tNetPacket.B.count);
        return (0);
    }
    file_stream.r(tNetPacket.B.data, tNetPacket.B.count);
    tNetPacket.r_begin(u_id);
    R_ASSERT2(M_SPAWN == u_id, "Invalid packet ID (!= M_SPAWN)");

    string64 s_name;
    tNetPacket.r_stringZ(s_name);
    // create entity
    CSE_Abstract* tpSE_Abstract = F_entity_Create(s_name);
    R_ASSERT2(tpSE_Abstract, "Can't create entity.");
    CSE_ALifeDynamicObject* tpALifeDynamicObject = smart_cast<CSE_ALifeDynamicObject*>(tpSE_Abstract);
    R_ASSERT2(tpALifeDynamicObject, "Non-ALife object in the saved game!");
    tpALifeDynamicObject->Spawn_Read(tNetPacket);

    // Update
    if (file_stream.elapsed() < 2)
    {
        Msg("! [ALife] object registry: truncated update packet size in save, aborting object load");
        xr_delete(tpALifeDynamicObject);
        return (0);
    }
    tNetPacket.B.count = file_stream.r_u16();
    if ((size_t)tNetPacket.B.count > sizeof(tNetPacket.B.data) || file_stream.elapsed() < tNetPacket.B.count)
    {
        Msg("! [ALife] object registry: invalid update packet size %u in save, aborting object load",
            tNetPacket.B.count);
        xr_delete(tpALifeDynamicObject);
        return (0);
    }
    file_stream.r(tNetPacket.B.data, tNetPacket.B.count);
    tNetPacket.r_begin(u_id);
    R_ASSERT2(M_UPDATE == u_id, "Invalid packet ID (!= M_UPDATE)");
    tpALifeDynamicObject->UPDATE_Read(tNetPacket);

    return (tpALifeDynamicObject);
}

bool CALifeObjectRegistry::load(IReader& file_stream)
{
    Msg("* Loading objects...");
    R_ASSERT2(file_stream.find_chunk(OBJECT_CHUNK_DATA), "Can't find chunk OBJECT_CHUNK_DATA!");

    m_objects.clear();

    u32 count = file_stream.r_u32();
    // Validate the count against the remaining stream size and the hard u16 ID
    // limit before stack-allocating: a corrupt save must not cause a stack
    // overflow or unbounded allocation. Object IDs are u16, so no legitimate
    // save can contain more than 65536 objects.
    const intptr_t remaining = file_stream.elapsed();
    if (count > 0x10000 || remaining < 0 || (intptr_t)count > remaining / 4)
    {
        Msg("! [ALife] object registry: corrupted object count %u (remaining %lld bytes), aborting load", count,
            (long long)remaining);
        return (false);
    }

    CSE_ALifeDynamicObject** objects = (CSE_ALifeDynamicObject**)xr_alloca(count * sizeof(CSE_ALifeDynamicObject*));

    CSE_ALifeDynamicObject** I = objects;
    CSE_ALifeDynamicObject** E = objects + count;
    for (; I != E; ++I)
    {
        *I = get_object(file_stream);
        if (!*I)
        {
            // Corrupt or truncated save: do not leave a half-populated registry
            // that would desync parent/child relationships. Release objects
            // already added the same way the destructor does.
            Msg("! [ALife] object registry: failed to read object, aborting load");
            OBJECT_REGISTRY::iterator J = m_objects.begin();
            OBJECT_REGISTRY::iterator K = m_objects.end();
            for (; J != K; ++J)
            {
                (*J).second->on_unregister();
                xr_delete((*J).second);
            }
            m_objects.clear();
            return (false);
        }

        // Duplicate IDs in a save are a corruption marker; keep the first
        // occurrence and drop the rest instead of silently overwriting.
        if (m_objects.find((*I)->ID) != m_objects.end())
        {
            Msg("! [ALife] object registry: duplicate object id %u in save, skipping", (*I)->ID);
            xr_delete(*I);
            continue;
        }

        add(*I);
    }

    Msg("* %d objects are successfully loaded", count);
    return (true);
}
