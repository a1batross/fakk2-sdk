// -----------------------------------------------------------------------------
//
//  $Logfile:: /fakk2_code/fakk2_new/fgame/ammo.h                             $
// $Revision:: 12                                                             $
//   $Author:: Markd                                                          $
//     $Date:: 7/14/00 9:46a                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// $Log:: /fakk2_code/fakk2_new/fgame/ammo.h                                  $
//
// 12    7/14/00 9:46a Markd
// added comment about member variable not being in archive
//
// 11    6/14/00 2:17p Markd
// fixed compiler warnings for Intel Compiler
//
// 10    5/30/00 7:06p Markd
// saved games 4th pass
//
// 9     5/25/00 7:52p Markd
// 2nd pass save game stuff
//
// 8     5/24/00 3:13p Markd
// first phase of save/load games
//
// 7     5/22/00 4:40p Steven
// Fixed ammo ItemPickup (had different number of parms from item one).
//
// 6     5/20/00 5:15p Markd
// Added ammo archive member functions
//
// 5     3/02/00 4:43p Aldie
// Added some ammo functionality for the HUD
//
// 4     12/03/99 7:02p Aldie
// More ammo joy
//
// 3     11/29/99 6:32p Aldie
// Lots of changes for ammo system
//
// 2     11/22/99 6:46p Aldie
// Started working on ammo changes - will finish after Thanksgiving break
//
// 1     9/10/99 10:53a Jimdose
//
// 1     9/08/99 3:15p Aldie
//
// DESCRIPTION:
// Base class for all ammunition for entities derived from the Weapon class.
//

#ifndef __AMMO_H__
#define __AMMO_H__

#include "g_local.h"
#include "item.h"

class AmmoEntity : public Item
{
public:
	CLASS_PROTOTYPE( AmmoEntity );

	AmmoEntity();
	virtual Item *ItemPickup( Entity *other, qboolean add_to_inventory );
};

class Ammo : public Class
{
	int amount;
	int maxamount;
	str name;
	int name_index;

public:
	CLASS_PROTOTYPE( Ammo );

	Ammo();
	Ammo( str name, int amount, int name_index );

	void setAmount( int a );
	int getAmount( void );
	void setMaxAmount( int a );
	int getMaxAmount( void );
	void setName( str name );
	str getName( void );
	int getIndex( void );
	virtual void Archive( Archiver &arc );
};

inline void Ammo::Archive( Archiver &arc )
{
	Class::Archive( arc );

	arc.ArchiveInteger( &amount );
	arc.ArchiveInteger( &maxamount );
	arc.ArchiveString( &name );
	//
	// name_index not archived, because it is auto-generated by gi.itemindex
	//
	if( arc.Loading())
	{
		setName( name );
	}
}

#endif /* ammo.h */
