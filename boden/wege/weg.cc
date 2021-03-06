/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 *
 * Basisklasse f�r Wege in Simutrans.
 *
 * 14.06.00 getrennt von simgrund.cc
 * �berarbeitet Januar 2001
 *
 * derived from simdings.h in 2007
 *
 * von Hj. Malthaner
 */

#include <stdio.h>

#include "weg.h"

#include "schiene.h"
#include "strasse.h"
#include "monorail.h"
#include "maglev.h"
#include "narrowgauge.h"
#include "kanal.h"
#include "runway.h"


#include "../grund.h"
#include "../../simworld.h"
#include "../../simimg.h"
#include "../../simhalt.h"
#include "../../simdings.h"
#include "../../player/simplay.h"
#include "../../dings/roadsign.h"
#include "../../dings/signal.h"
#include "../../dings/crossing.h"
#include "../../utils/cbuffer_t.h"
#include "../../dataobj/translator.h"
#include "../../dataobj/loadsave.h"
#include "../../besch/weg_besch.h"
#include "../../besch/roadsign_besch.h"

#include "../../tpl/slist_tpl.h"

/**
 * Alle instantiierten Wege
 * @author Hj. Malthaner
 */
slist_tpl <weg_t *> alle_wege;



/**
 * Get list of all ways
 * @author Hj. Malthaner
 */
const slist_tpl <weg_t *> & weg_t::get_alle_wege()
{
	return alle_wege;
}



// returns a way with matchin waytype
weg_t*
weg_t::alloc(waytype_t wt)
{
	weg_t *weg = NULL;
	switch(wt) {
		case tram_wt:
		case track_wt:
			weg = new schiene_t(welt);
			break;
		case monorail_wt:
			weg = new monorail_t(welt);
			break;
		case maglev_wt:
			weg = new maglev_t(welt);
			break;
		case narrowgauge_wt:
			weg = new narrowgauge_t(welt);
			break;
		case road_wt:
			weg = new strasse_t(welt);
			break;
		case water_wt:
			weg = new kanal_t(welt);
			break;
		case air_wt:
			weg = new runway_t(welt);
			break;
		default:
			// keep compiler happy; should never reach here anyway
			assert(0);
			break;
	}
	return weg;
}




// returns a way with matchin waytype
const char* weg_t::waytype_to_string(waytype_t wt)
{
	switch(wt) {
		case tram_wt:	return "tram_track";
		case track_wt:	return "track";
		case monorail_wt: return "monorail_track";
		case maglev_wt: return "maglev_track";
		case narrowgauge_wt: return "narrowgauge_track";
		case road_wt:	return "road";
		case water_wt:	return "water";
		case air_wt:	return "air";
		default:
			// keep compiler happy; should never reach here anyway
			assert(0);
			break;
	}
	return "invalid waytype";
}




/**
 * Setzt die erlaubte H�chstgeschwindigkeit
 * @author Hj. Malthaner
 */
void weg_t::set_max_speed(unsigned int s)
{
	max_speed = s;
}

void weg_t::set_max_weight(uint32 w)
{
	max_weight = w;
}

void weg_t::add_way_constraints(const uint8 permissive, const uint8 prohibitive)
{
	way_constraints_permissive |= permissive;
	way_constraints_prohibitive |= prohibitive;
}

void weg_t::reset_way_constraints()
{
	way_constraints_permissive = besch->get_way_constraints_permissive();
	way_constraints_prohibitive = besch->get_way_constraints_prohibitive();
}


/**
 * Setzt neue Beschreibung. Ersetzt alte H�chstgeschwindigkeit
 * mit wert aus Beschreibung.
 *
 * "Sets new description. Replaced old with maximum speed value of description." (Google)
 * @author Hj. Malthaner
 */
void weg_t::set_besch(const weg_besch_t *b)
{
	besch = b;
	if (hat_gehweg() && besch->get_wtyp() == road_wt && besch->get_topspeed() > 50) {
		//Limit speeds for city roads.
		max_speed = 50;
	}
	else {
		max_speed = besch->get_topspeed();
	}

	max_weight = besch->get_max_weight();
	way_constraints_permissive = besch->get_way_constraints_permissive();
	way_constraints_prohibitive = besch->get_way_constraints_prohibitive();
}


/**
 * initializes statistic array
 * @author hsiegeln
 */
void weg_t::init_statistics()
{
	for (int type=0; type<MAX_WAY_STATISTICS; type++) {
		for (int month=0; month<MAX_WAY_STAT_MONTHS; month++) {
			statistics[month][type] = 0;
		}
	}
}


/**
 * Initializes all member variables
 * @author Hj. Malthaner
 */
void weg_t::init()
{
	set_flag(ding_t::is_wayding);
	ribi = ribi_maske = ribi_t::keine;
	max_speed = 450;
	max_weight = 999;
	besch = 0;
	init_statistics();
	alle_wege.insert(this);
	flags = 0;
}



weg_t::~weg_t()
{
	alle_wege.remove(this);
	spieler_t *sp=get_besitzer();
	if(sp) {
		spieler_t::add_maintenance( sp,  -besch->get_wartung() );
	}
}



void weg_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "weg_t" );

	// save owner
	if(file->get_version()>=99006) {
		sint8 spnum=get_player_nr();
		file->rdwr_byte(spnum,"");
		set_player_nr(spnum);
	}

	// all connected directions
	uint8 dummy8 = ribi;
	file->rdwr_byte(dummy8, "\n");
	if(file->is_loading()) {
		ribi = dummy8 & 15;	// before: high bits was maske
		ribi_maske = 0;	// maske will be restored by signal/roadsing
	}

	uint16 dummy16=max_speed;
	file->rdwr_short(dummy16, "\n");
	max_speed=dummy16;

	if(file->get_version()>=89000) {
		dummy8 = flags;
		file->rdwr_byte(dummy8,"f");
		if(file->is_loading()) {
			// all other flags are restored afterwards
			flags = dummy8 & HAS_SIDEWALK;
		}
	}

	for (int type=0; type<MAX_WAY_STATISTICS; type++) {
		for (int month=0; month<MAX_WAY_STAT_MONTHS; month++) {
			sint32 w=statistics[month][type];
			file->rdwr_long(w, "\n");
			statistics[month][type] = (sint16)w;
			// DBG_DEBUG("weg_t::rdwr()", "statistics[%d][%d]=%d", month, type, statistics[month][type]);
		}
	}

	if(file->get_experimental_version() >= 1)
	{
		uint16 wdummy16 = max_weight;
		file->rdwr_short(wdummy16, "\n");
		max_weight = wdummy16;
	}
}


/**
 * Info-text f�r diesen Weg
 * @author Hj. Malthaner
 */
void weg_t::info(cbuffer_t & buf) const
{
	buf.append("\n");
	buf.append(translator::translate("Max. speed:"));
	buf.append(" ");
	buf.append(max_speed);
	buf.append(translator::translate("km/h\n"));

	buf.append(translator::translate("\nMax. weight:"));
	buf.append(" ");
	buf.append(max_weight);
	buf.append("t ");
	buf.append("\n");
	for(sint8 i = -8; i < 8; i ++)
	{
		if(permissive_way_constraint_set(i + 8))
		{
			buf.append(translator::translate("Way constraint permissive "));
			buf.append(i);
			buf.append("\n");
		}
		if(prohibitive_way_constraint_set(i))
		{
			buf.append(translator::translate("Way constraint prohibitive %n\n"));
			buf.append(i);
			buf.append("\n");
		}
	}
#ifdef DEBUG
	buf.append(translator::translate("\nRibi (unmasked)"));
	buf.append(get_ribi_unmasked());

	buf.append(translator::translate("\nRibi (masked)"));
	buf.append(get_ribi());
	buf.append("\n");
#endif
	if(has_sign()) {
		buf.append(translator::translate("\nwith sign/signal\n"));
	}

	if(is_electrified()) {
		buf.append(translator::translate("\nelektrified"));
	}
	else {
		buf.append(translator::translate("\nnot elektrified"));
	}

#if 1
	char buffer[256];
	sprintf(buffer,translator::translate("convoi passed last\nmonth %i\n"), statistics[1][1]);
	buf.append(buffer);
#else
	// Debug - output stats
	buf.append("\n");
	for (int type=0; type<MAX_WAY_STATISTICS; type++) {
		for (int month=0; month<MAX_WAY_STAT_MONTHS; month++) {
			buf.append(statistics[month][type]);
			buf.append(" ");
		}
	buf.append("\n");
	}
#endif
	buf.append("\n");
}


/**
 * called during map rotation
 * @author priss
 */
void weg_t::rotate90()
{
	ding_t::rotate90();
	ribi = ribi_t::rotate90( ribi );
	ribi_maske = ribi_t::rotate90( ribi_maske );
}


/**
 * counts signals on this tile;
 * It would be enough for the signals to register and unreigister themselves, but this is more secure ...
 * @author prissi
 */
void
weg_t::count_sign()
{
	// Either only sign or signal please ...
	flags &= ~(HAS_SIGN|HAS_SIGNAL|HAS_CROSSING);
	const grund_t *gr=welt->lookup(get_pos());
	if(gr) {
		uint8 i = 1;
		// if there is a crossing, the start index is at least three ...
		if(gr->ist_uebergang()) {
			flags |= HAS_CROSSING;
			i = 3;
			const crossing_t* cr = gr->find<crossing_t>();
			uint32 top_speed = cr->get_besch()->get_maxspeed( cr->get_besch()->get_waytype(0)==get_waytype() ? 0 : 1);
			if(top_speed<max_speed) {
				max_speed = top_speed;
			}
		}
		// since way 0 is at least present here ...
		for( ;  i<gr->get_top();  i++  ) {
			ding_t *d=gr->obj_bei(i);
			// sign for us?
			if(d->get_typ()==ding_t::roadsign  &&  ((roadsign_t*)d)->get_besch()->get_wtyp()==get_besch()->get_wtyp()) {
				// here is a sign ...
				flags |= HAS_SIGN;
				return;
			}
			if(d->get_typ()==ding_t::signal  &&  ((signal_t*)d)->get_besch()->get_wtyp()==get_besch()->get_wtyp()) {
				// here is a signal ...
				flags |= HAS_SIGNAL;
				return;
			}
		}
	}
}



void
weg_t::calc_bild()
{
	// V.Meyer: weg_position_t changed to grund_t::get_neighbour()
	grund_t *from = welt->lookup(get_pos());
	grund_t *to;

	if(from==NULL  ||  besch==NULL  ||  from->ist_tunnel()) {
		// no ground, in tunnel
		set_bild(IMG_LEER);
		return;
	}
	if(from->ist_bruecke()  &&  from->obj_bei(0)==this) {
		// first way on a bridge (bruecke_t will set the image)
		return;
	}

	// use snow image if above snowline and above ground
	bool snow = (get_pos().z >= welt->get_snowline());

	hang_t::typ hang = from->get_weg_hang();
	if(hang != hang_t::flach) {
		set_bild(besch->get_hang_bild_nr(hang, snow));
		return;
	}

	const ribi_t::ribi ribi = get_ribi_unmasked();

	if(ribi_t::ist_kurve(ribi)  &&  besch->has_diagonal_bild()) {
		ribi_t::ribi r1 = ribi_t::keine, r2 = ribi_t::keine;

		bool diagonal = false;
		switch(ribi) {
			case ribi_t::nordost:
				if(from->get_neighbour(to, get_waytype(), koord::ost))
					r1 = to->get_weg_ribi_unmasked(get_waytype());
				if(from->get_neighbour(to, get_waytype(), koord::nord))
					r2 = to->get_weg_ribi_unmasked(get_waytype());
				diagonal =
					(r1 == ribi_t::suedwest || r2 == ribi_t::suedwest) &&
					r1 != ribi_t::nordwest &&
					r2 != ribi_t::suedost;
			break;

			case ribi_t::suedost:
				if(from->get_neighbour(to, get_waytype(), koord::ost))
					r1 = to->get_weg_ribi_unmasked(get_waytype());
				if(from->get_neighbour(to, get_waytype(), koord::sued))
					r2 = to->get_weg_ribi_unmasked(get_waytype());
				diagonal =
					(r1 == ribi_t::nordwest || r2 == ribi_t::nordwest) &&
					r1 != ribi_t::suedwest &&
					r2 != ribi_t::nordost;
			break;

			case ribi_t::nordwest:
				if(from->get_neighbour(to, get_waytype(), koord::west))
					r1 = to->get_weg_ribi_unmasked(get_waytype());
				if(from->get_neighbour(to, get_waytype(), koord::nord))
					r2 = to->get_weg_ribi_unmasked(get_waytype());
				diagonal =
					(r1 == ribi_t::suedost || r2 == ribi_t::suedost) &&
					r1 != ribi_t::nordost &&
					r2 != ribi_t::suedwest;
			break;

			case ribi_t::suedwest:
				if(from->get_neighbour(to, get_waytype(), koord::west))
					r1 = to->get_weg_ribi_unmasked(get_waytype());
				if(from->get_neighbour(to, get_waytype(), koord::sued))
					r2 = to->get_weg_ribi_unmasked(get_waytype());
				diagonal =
					(r1 == ribi_t::nordost || r2 == ribi_t::nordost) &&
					r1 != ribi_t::suedost &&
					r2 != ribi_t::nordwest;
				break;
		}

		if(diagonal) {
			static int rekursion = 0;

			if(rekursion == 0) {
				rekursion++;
				for(int r = 0; r < 4; r++) {
					if(from->get_neighbour(to, get_waytype(), koord::nsow[r])) {
						to->get_weg(get_waytype())->calc_bild();
					}
				}
				rekursion--;
			}

			image_id bild = besch->get_diagonal_bild_nr(ribi, snow);
			if(bild != IMG_LEER) {
				set_bild(bild);
				return;
			}
		}
	}

	set_bild(besch->get_bild_nr(ribi, snow));
}



/**
 * new month
 * @author hsiegeln
 */
void weg_t::neuer_monat()
{
	for (int type=0; type<MAX_WAY_STATISTICS; type++) {
		for (int month=MAX_WAY_STAT_MONTHS-1; month>0; month--) {
			statistics[month][type] = statistics[month-1][type];
		}
		statistics[0][type] = 0;
	}
}



// correct speed and maitainace
void weg_t::laden_abschliessen()
{
	spieler_t *sp=get_besitzer();
	if(sp  &&  besch) {
		spieler_t::add_maintenance( sp,  besch->get_wartung() );
	}
}
