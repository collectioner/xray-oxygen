#include "stdafx.h"
#include "xrServer.h"
#include "xrserver_objects.h"

#ifdef DEBUG
#	include "xrserver_objects_alife_items.h"
#endif

CSE_Abstract* xrServer::Process_spawn(NET_Packet& P, ClientID sender, BOOL bSpawnWithClientsMainEntityAsParent, CSE_Abstract* tpExistedEntity)
{
	// create server entity
	xrClientData* CL	= ID_to_client	(sender);
	CSE_Abstract*	E	= tpExistedEntity;
	if (!E){
		// read spawn information
		string64			s_name;
		P.r_stringZ			(s_name);
		// create entity
		E = entity_Create	(s_name); R_ASSERT3(E,"Can't create entity.",s_name);
		E->Spawn_Read		(P);
		if(!E->m_gameType.MatchType((u16)game->Type() || !E->match_configuration() || !game->OnPreCreate(E)))
		{
			F_entity_Destroy(E);
			return			NULL;
		}
	}
	else VERIFY(E->m_bALifeControl);
	

	CSE_Abstract			*e_parent = 0;
	if (E->ID_Parent != 0xffff)
	{
		e_parent			= game->get_entity_from_eid(E->ID_Parent);
		if (!e_parent) {
			R_ASSERT		(!tpExistedEntity);
			F_entity_Destroy(E);
			return			NULL;
		}
	}

	// check if we can assign entity to some client
	if (!CL)
	{
		CL	= SelectBestClientToMigrateTo	(E);
	}

	// check for respawn-capability and create phantom as needed
	if (E->RespawnTime && (0xffff==E->ID_Phantom))
	{
		// Create phantom
		CSE_Abstract* Phantom	=	entity_Create	(*E->s_name); R_ASSERT(Phantom);
		Phantom->Spawn_Read		(P);
		Phantom->ID				=	PerformIDgen	(0xffff);
		Phantom->ID_Phantom		=	Phantom->ID;						// Self-linked to avoid phantom-breeding
		Phantom->owner			=	NULL;
		entities.insert			(std::make_pair(Phantom->ID,Phantom));

		Phantom->s_flags.set	(M_SPAWN_OBJECT_PHANTOM,TRUE);

		// Spawn entity
		E->ID					=	PerformIDgen(E->ID);
		E->ID_Phantom			=	Phantom->ID;
		E->owner				=	CL;
		entities.insert			(std::make_pair(E->ID,E));
	} else {
		if (E->s_flags.is(M_SPAWN_OBJECT_PHANTOM))
		{
			// Clone from Phantom
			E->ID					=	PerformIDgen(0xffff);
			E->owner				=	CL;//		= SelectBestClientToMigrateTo	(E);
			E->s_flags.set			(M_SPAWN_OBJECT_PHANTOM,FALSE);
			entities.insert			(std::make_pair(E->ID,E));
		} else {
			// Simple spawn
			if (bSpawnWithClientsMainEntityAsParent)
			{
				R_ASSERT				(CL);
				CSE_Abstract* P		= CL->owner;
				R_ASSERT				(P);
				E->ID_Parent			= P->ID;
			}
			E->ID					=	PerformIDgen(E->ID);
			E->owner				=	CL;
			entities.insert			(std::make_pair(E->ID,E));
		}
	}

	// PROCESS NAME; Name this entity
	if (CL && (E->s_flags.is(M_SPAWN_OBJECT_ASPLAYER)))
	{
		CL->owner		= E;
//		E->set_name_replace	(CL->Name);
	}

	// PROCESS RP;	 3D position/orientation
	PerformRP				(E);
	E->s_RP					= 0xFE;	// Use supplied

	// Parent-Connect
	if (!tpExistedEntity) {
		game->OnCreate		(E->ID);
		
		if (0xffff != E->ID_Parent) {
			R_ASSERT					(e_parent);
			
			game->OnTouch			(E->ID_Parent,E->ID);

			e_parent->children.push_back(E->ID);
		}
	}

	// create packet and broadcast packet to everybody
	NET_Packet				Packet;
	if (CL) 
	{
		// For local ONLY
		E->Spawn_Write		(Packet,TRUE	);
		if (E->s_flags.is(M_SPAWN_UPDATE))
			E->UPDATE_Write	(Packet);
		SendTo				(CL->ID,Packet,net_flags(TRUE,TRUE));

		// For everybody, except client, which contains authorative copy
		E->Spawn_Write		(Packet,FALSE	);
		if (E->s_flags.is(M_SPAWN_UPDATE))
			E->UPDATE_Write	(Packet);
		SendBroadcast		(CL->ID,Packet,net_flags(TRUE,TRUE));
	} else {
		E->Spawn_Write		(Packet,FALSE	);
		if (E->s_flags.is(M_SPAWN_UPDATE))
			E->UPDATE_Write	(Packet);
		ClientID clientID;clientID.set(0);
		SendBroadcast		(clientID, Packet, net_flags(TRUE,TRUE));
	}
	if (!tpExistedEntity)
	{
		game->OnPostCreate(E->ID);
	};

	// log
	//Msg		("- SERVER: Spawning '%s'(%d,%d,%d) as #%d, on '%s'", E->s_name_replace, E->g_team(), E->g_squad(), E->g_group(), E->ID, CL?CL->Name:"*SERVER*");
	return E;
}
