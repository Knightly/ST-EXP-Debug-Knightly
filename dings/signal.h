/*
 * Copyright (c) 1997 - 2002 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef dings_signal_h
#define dings_signal_h

#include "roadsign.h"

#include "../simdings.h"


/**
 * Signale f�r die Bahnlinien.
 *
 * @see blockstrecke_t
 * @see blockmanager
 * @author Hj. Malthaner
 */
class signal_t : public roadsign_t
{
public:
	signal_t(karte_t *welt, loadsave_t *file);
	signal_t(karte_t *welt, spieler_t *sp, koord3d pos, ribi_t::ribi dir,const roadsign_besch_t *besch) : roadsign_t(welt,sp,pos,dir,besch) { zustand = rot;}

	/**
	* @return Einen Beschreibungsstring f�r das Objekt, der z.B. in einem
	* Beobachtungsfenster angezeigt wird.
	* @author Hj. Malthaner
	*/
	virtual void info(cbuffer_t & buf) const;

	enum ding_t::typ get_typ() const {return ding_t::signal;}
	const char *get_name() const {return "Signal";}

	/**
	* berechnet aktuelles bild
	*/
	void calc_bild();
};

#endif
