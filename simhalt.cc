/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/*
 * Haltestellen fuer Simutrans
 * 03.2000 getrennt von simfab.cc
 *
 * Hj. Malthaner
 */
#include <algorithm>

#include "freight_list_sorter.h"

#include "simcity.h"
#include "simcolor.h"
#include "simconvoi.h"
#include "simdebug.h"
#include "simfab.h"
#include "simgraph.h"
#include "simhalt.h"
#include "simintr.h"
#include "simmem.h"
#include "simmesg.h"
#include "simplan.h"
#include "simtools.h"
#include "player/simplay.h"
#include "simwin.h"
#include "simworld.h"
#include "simware.h"

#include "bauer/hausbauer.h"
#include "bauer/warenbauer.h"

#include "besch/ware_besch.h"

#include "boden/boden.h"
#include "boden/grund.h"
#include "boden/wasser.h"
#include "boden/wege/strasse.h"
#include "boden/wege/weg.h"

#include "dataobj/einstellungen.h"
#include "dataobj/fahrplan.h"
#include "dataobj/loadsave.h"
#include "dataobj/translator.h"
#include "dataobj/umgebung.h"
#ifndef NEW_PATHING
#include "dataobj/warenziel.h"
#endif

#include "dings/gebaeude.h"
#include "dings/label.h"

#include "gui/halt_info.h"
#include "gui/karte.h"

#include "utils/simstring.h"

#include "vehicle/simpeople.h"

#ifdef LAGER_NOT_IN_USE
#include "dings/lagerhaus.h"
#endif

karte_t *haltestelle_t::welt = NULL;

slist_tpl<halthandle_t> haltestelle_t::alle_haltestellen;

stringhashtable_tpl<halthandle_t> haltestelle_t::all_names;



halthandle_t haltestelle_t::get_halt( karte_t *welt, const koord pos, const spieler_t *sp )
{
	const planquadrat_t *plan = welt->lookup(pos);
	if(plan) {
		if(plan->get_halt().is_bound()  &&  spieler_t::check_owner(sp,plan->get_halt()->get_besitzer())  ) {
			return plan->get_halt();
		}
		// no halt? => we do the water check
		if(plan->get_kartenboden()->ist_wasser()) {
			// may catch bus stops close to water ...
			const uint8 cnt = plan->get_haltlist_count();
			// first check for own stop
			for(  uint8 i=0;  i<cnt;  i++  ) {
				if(  plan->get_haltlist()[i]->get_besitzer()==sp  ) {
					return plan->get_haltlist()[i];
				}
			}
			// then for public stop
			for(  uint8 i=0;  i<cnt;  i++  ) {
				if(  plan->get_haltlist()[i]->get_besitzer()==welt->get_spieler(1)  ) {
					return plan->get_haltlist()[i];
				}
			}
			// so: nothing found
		}
	}
	return halthandle_t();
}


halthandle_t
haltestelle_t::get_halt( karte_t *welt, const koord3d pos, const spieler_t *sp )
{
	const grund_t *gr = welt->lookup(pos);
	if(gr) {
		if(gr->get_halt().is_bound()  &&  spieler_t::check_owner(sp,gr->get_halt()->get_besitzer())  ) {
			return gr->get_halt();
		}
		// no halt? => we do the water check
		if(gr->ist_wasser()) {
			// may catch bus stops close to water ...
			const planquadrat_t *plan = welt->lookup(pos.get_2d());
			const uint8 cnt = plan->get_haltlist_count();
			// first check for own stop
			for(  uint8 i=0;  i<cnt;  i++  ) {
				if(  plan->get_haltlist()[i]->get_besitzer()==sp  ) {
					return plan->get_haltlist()[i];
				}
			}
			// then for public stop
			for(  uint8 i=0;  i<cnt;  i++  ) {
				if(  plan->get_haltlist()[i]->get_besitzer()==welt->get_spieler(1)  ) {
					return plan->get_haltlist()[i];
				}
			}
			// so: nothing found
		}
	}
	return halthandle_t();
}


koord haltestelle_t::get_basis_pos() const
{
	if (tiles.empty()) return koord::invalid;
	assert(tiles.front().grund->get_pos().get_2d() == init_pos);
	return tiles.front().grund->get_pos().get_2d();
}


koord3d haltestelle_t::get_basis_pos3d() const
{
	if (tiles.empty()) return koord3d::invalid;
	return tiles.front().grund->get_pos();
}


/**
 * Station factory method. Returns handles instead of pointers.
 * @author Hj. Malthaner
 */
halthandle_t haltestelle_t::create(karte_t *welt, koord pos, spieler_t *sp)
{
	haltestelle_t * p = new haltestelle_t(welt, pos, sp);
	return p->self;
}


/*
 * removes a ground tile from a station
 * @author prissi
 */
bool
haltestelle_t::remove(karte_t *welt, spieler_t *sp, koord3d pos, const char *&msg)
{
	msg = NULL;
	grund_t *bd = welt->lookup(pos);

	// wrong ground?
	if(bd==NULL) {
		dbg->error("haltestelle_t::remove()","illegal ground at %d,%d,%d", pos.x, pos.y, pos.z);
		return false;
	}

	halthandle_t halt = bd->get_halt();
	if(!halt.is_bound()) {
		dbg->error("haltestelle_t::remove()","no halt at %d,%d,%d", pos.x, pos.y, pos.z);
		return false;
	}

DBG_MESSAGE("haltestelle_t::remove()","removing segment from %d,%d,%d", pos.x, pos.y, pos.z);

	// otherwise there will be marked tiles left ...
	halt->mark_unmark_coverage(false);
	halt->rem_grund(bd);

	// remove station building?
	gebaeude_t* gb = bd->find<gebaeude_t>();
	if(gb) {
DBG_MESSAGE("haltestelle_t::remove()",  "removing building" );
		hausbauer_t::remove( welt, sp, gb );
		bd = NULL;	// no need to recalc image
	}

	if(!halt->existiert_in_welt()) {
DBG_DEBUG("haltestelle_t::remove()","remove last");
		// all deleted?
DBG_DEBUG("haltestelle_t::remove()","destroy");
		haltestelle_t::destroy( halt );
	}
	else {
DBG_DEBUG("haltestelle_t::remove()","not last");
		// may have been changed ... (due to post office/dock/railways station deletion)
		halt->recalc_station_type();
	}

	// if building was removed this is false!
	if(bd) {
		bd->calc_bild();
		reliefkarte_t::get_karte()->calc_map_pixel(pos.get_2d());
	}
	return true;
}



/**
 * Station factory method. Returns handles instead of pointers.
 * @author Hj. Malthaner
 */
halthandle_t haltestelle_t::create(karte_t *welt, loadsave_t *file)
{
	haltestelle_t *p = new haltestelle_t(welt, file);
	return p->self;
}


/**
 * Station destruction method.
 * @author Hj. Malthaner
 */
void haltestelle_t::destroy(halthandle_t &halt)
{
	haltestelle_t *p = halt.get_rep();
	delete p;
}


/**
 * Station destruction method.
 * Da destroy() alle_haltestellen modifiziert kann kein Iterator benutzt
 * werden! V. Meyer
 * @author Hj. Malthaner
 */
void haltestelle_t::destroy_all(karte_t *welt)
{
	haltestelle_t::welt = welt;
	while (!alle_haltestellen.empty()) {
		halthandle_t halt = alle_haltestellen.front();
		destroy(halt);
	}
}

haltestelle_t::haltestelle_t(karte_t* wl, loadsave_t* file)
{
	self = halthandle_t(this);

	welt = wl;
	marke = 0;

	pax_happy = 0;
	pax_unhappy = 0;
	pax_no_route = 0;

	const uint8 max_categories = warenbauer_t::get_max_catg_index();

	waren = (vector_tpl<ware_t> **)calloc( max_categories, sizeof(vector_tpl<ware_t> *) );
#ifdef NEW_PATHING
	iterations = 0;
	search_complete = false;
	waiting_times = new koordhashtable_tpl<koord, fixed_list_tpl<uint16, 16> >[max_categories];
	connexions = new quickstone_hashtable_tpl<haltestelle_t, connexion*>[max_categories];
	paths = new quickstone_hashtable_tpl<haltestelle_t, path>[max_categories];
	connexions_timestamp = new uint16[max_categories];
	paths_timestamp = new uint16[max_categories];
	reschedule = new bool[max_categories];
	for(uint8 i = 0; i < max_categories; i ++)
	{
		paths_timestamp[i] = 0;
		connexions_timestamp[i] = 0;
		reschedule[i] = false;
	}
#else
	warenziele = new vector_tpl<halthandle_t>[ warenbauer_t::get_max_catg_index() ];
#endif

	status_color = COL_YELLOW;

	reroute_counter = welt->get_schedule_counter()-1;
	rebuilt_destination_counter = reroute_counter;

	enables = NOT_ENABLED;

	// @author hsiegeln
	sortierung = freight_list_sorter_t::by_name;
	resort_freight_info = true;
	
	rdwr(file);

	alle_haltestellen.insert(self);

	check_waiting = 0;
}


haltestelle_t::haltestelle_t(karte_t* wl, koord k, spieler_t* sp)
{
	self = halthandle_t(this);
	assert( !alle_haltestellen.is_contained(self) );
	alle_haltestellen.insert(self);

	welt = wl;
	marke = 0;

	this->init_pos = k;
	besitzer_p = sp;
#ifdef LAGER_NOT_IN_USE
	lager = NULL;
#endif

	enables = NOT_ENABLED;

	reroute_counter = welt->get_schedule_counter()-1;
	rebuilt_destination_counter = reroute_counter;

	const uint8 max_categories = warenbauer_t::get_max_catg_index();

	waren = (vector_tpl<ware_t> **)calloc( max_categories, sizeof(vector_tpl<ware_t> *) );
#ifdef NEW_PATHING
	iterations = 0;
	search_complete = false;
	waiting_times = new koordhashtable_tpl<koord, fixed_list_tpl<uint16, 16> >[max_categories];
	connexions = new quickstone_hashtable_tpl<haltestelle_t, connexion*>[max_categories];
	paths = new quickstone_hashtable_tpl<haltestelle_t, path>[max_categories];
#else
	warenziele = new vector_tpl<halthandle_t>[ max_categories ];
#endif

	pax_happy = 0;
	pax_unhappy = 0;
	pax_no_route = 0;
	status_color = COL_YELLOW;

	sortierung = freight_list_sorter_t::by_name;
	init_financial_history();

	if(welt->ist_in_kartengrenzen(k)) {
		welt->access(k)->set_halt(self);
	}
#ifdef NEW_PATHING
	connexions_timestamp = new uint16[max_categories];
	paths_timestamp = new uint16[max_categories];
	reschedule = new bool[max_categories];
	for(uint8 i = 0; i < max_categories; i ++)
	{
		paths_timestamp[i] = 0;
		connexions_timestamp[i] = 0;
		reschedule[i] = false;
	}
#endif

	check_waiting = 0;
}


haltestelle_t::~haltestelle_t()
{
	assert(self.is_bound());

	// first: remove halt from all lists
	int i=0;
	while(alle_haltestellen.is_contained(self)) {
		alle_haltestellen.remove(self);
		i++;
	}
	if (i != 1) {
		dbg->error("haltestelle_t::~haltestelle_t()", "handle %i found %i times in haltlist!", self.get_id(), i );
	}

	// do not forget the players list ...
	if(besitzer_p!=NULL) {
		besitzer_p->halt_remove(self);
	}

	// free name
	set_name(NULL);

	// remove from ground and planquadrat haltlists
	koord ul(32767,32767);
	koord lr(0,0);
	while(  !tiles.empty()  ) {
		koord pos = tiles.remove_first().grund->get_pos().get_2d();
		planquadrat_t *pl = welt->access(pos);
		assert(pl);
		pl->set_halt( halthandle_t() );
		for( uint8 i=0;  i<pl->get_boden_count();  i++  ) {
			pl->get_boden_bei(i)->set_halt( halthandle_t() );
		}
		// bounding box for adjustments
		if(ul.x>pos.x ) ul.x = pos.x;
		if(ul.y>pos.y ) ul.y = pos.y;
		if(lr.x<pos.x ) lr.x = pos.x;
		if(lr.y<pos.y ) lr.y = pos.y;
	}

	/* remove probably remaining halthandle at init_pos
	 * (created during loadtime for stops without ground) */
	planquadrat_t* pl = welt->access(init_pos);
	if(pl  &&  pl->get_halt()==self) {
		pl->set_halt( halthandle_t() );
	}

	// remove from all haltlists
	ul.x = max( 0, ul.x-welt->get_einstellungen()->get_station_coverage() );
	ul.y = max( 0, ul.y-welt->get_einstellungen()->get_station_coverage() );
	lr.x = min( welt->get_groesse_x(), lr.x+1+welt->get_einstellungen()->get_station_coverage() );
	lr.y = min( welt->get_groesse_y(), lr.y+1+welt->get_einstellungen()->get_station_coverage() );
	for(  int y=ul.y;  y<lr.y;  y++  ) {
		for(  int x=ul.x;  x<lr.x;  x++  ) {
			planquadrat_t *plan = welt->access(x,y);
			if(plan->get_haltlist_count()>0) {
				plan->remove_from_haltlist( welt, self );
			}
		}
	}

	// finally detach handle
	// before it is needed for clearing up the planqudrat and tiles
	self.detach();

	destroy_win((long)this);

	for(unsigned i=0; i<warenbauer_t::get_max_catg_index(); i++) {
		if(waren[i]) {
			delete waren[i];
			waren[i] = NULL;
		}
	}
	free( waren );
#ifdef NEW_PATHING
	for(uint8 i = 0; i < warenbauer_t::get_max_catg_index(); i ++)
	{
		reset_connexions(i);
	}

	delete[] connexions;
	delete[] paths;
	if(!open_list.empty())
	{
		delete[] path_nodes;
	}
	delete[] waiting_times;
	delete[] paths_timestamp;
	delete[] connexions_timestamp;
	delete[] reschedule;
#else
	delete[] warenziele;
#endif

	// routes may have changed without this station ...
	verbinde_fabriken();
	welt->set_schedule_counter();
}


void
haltestelle_t::rotate90( const sint16 y_size )
{
	init_pos.rotate90( y_size );
	// rotate waren destinations
	// iterate over all different categories
	for(unsigned i=0; i<warenbauer_t::get_max_catg_index(); i++) {
		if(waren[i]) {
			vector_tpl<ware_t> * warray = waren[i];
			for(int j=warray->get_count()-1;  j>=0;  j--  ) {
				ware_t & ware = (*warray)[j];
				if(ware.menge>0) {
					koord k = ware.get_zielpos();
					k.rotate90( y_size );
					// since we need to point at factory (0,0)
					fabrik_t *fab = fabrik_t::get_fab( welt, k );
					ware.set_zielpos( fab ? fab->get_pos().get_2d() : k );
				}
				else {
					// empty => remove
					(*warray).remove_at( j );
				}
			}
		}
	}
	verbinde_fabriken();
}



const char* haltestelle_t::get_name() const
{
	const char *name = "Unknown";
	if (tiles.empty()) {
		name = "Unnamed";
	} else {
		grund_t* bd = welt->lookup(get_basis_pos3d());
		if(bd  &&  bd->get_flag(grund_t::has_text)) {
			name = bd->get_text();
		}
	}
	return name;
}



/**
 * Sets the name. Creates a copy of name.
 * @author Hj. Malthaner
 */
void
haltestelle_t::set_name(const char *new_name)
{
	grund_t *gr = welt->lookup(get_basis_pos3d());
	if(gr) {
		if(gr->get_flag(grund_t::has_text)) {
			halthandle_t h = all_names.remove(gr->get_text());
			if(h!=self) {
				DBG_MESSAGE("haltestelle_t::set_name()","name %s already used!",gr->get_text());
			}
		}
		if(!gr->find<label_t>()) {
			gr->set_text( new_name );
			if(new_name  &&  !all_names.put(gr->get_text(),self)) {
				DBG_MESSAGE("haltestelle_t::set_name()","name %s already used!",new_name);
			}
		}
	}
}



char *haltestelle_t::create_name(const koord k, const char *typ)
{
	stadt_t *stadt = welt->suche_naechste_stadt(k);
	const char *stop = translator::translate(typ);
	char buf[1024];

	// this fails only, if there are no towns at all!
	if(stadt==NULL) {
		// get a default name
		sprintf( buf, translator::translate("land stop %i %s"), get_besitzer()->get_haltcount(), stop );
		return strdup(buf);
	}

	// now we have a city
	const char *city_name = stadt->get_name();
	sint16 li_gr = stadt->get_linksoben().x - 2;
	sint16 re_gr = stadt->get_rechtsunten().x + 2;
	sint16 ob_gr = stadt->get_linksoben().y - 2;
	sint16 un_gr = stadt->get_rechtsunten().y + 2;

	// strings for intown / outside of town
	const bool inside = (li_gr < k.x  &&  re_gr > k.x  &&  ob_gr < k.y  &&  un_gr > k.y);

	if(!welt->get_einstellungen()->get_numbered_stations()) {

		static const koord next_building[24] = {
			koord( 0, -1), // nord
			koord( 1,  0), // ost
			koord( 0,  1), // sued
			koord(-1,  0), // west
			koord( 1, -1), // nordost
			koord( 1,  1), // suedost
			koord(-1,  1), // suedwest
			koord(-1, -1), // nordwest
			koord( 0, -2),	// double nswo
			koord( 2,  0),
			koord( 0,  2),
			koord(-2,  0),
			koord( 1, -2),	// all the remaining 3s
			koord( 2, -1),
			koord( 2,  1),
			koord( 1,  2),
			koord(-1,  2),
			koord(-2,  1),
			koord(-2, -1),
			koord(-1, -2),
			koord( 2, -2),	// and now all buildings with distance 4
			koord( 2,  2),
			koord(-2,  2),
			koord(-2, -2)
		};

		// standard names:
		// order: factory, attraction, direction, normal name
		// prissi: first we try a factory name
		slist_tpl<fabrik_t *>fabs;
		if (self.is_bound()) {
			// first factories (so with same distance, they have priority)
			int this_distance = 999;
			slist_iterator_tpl<fabrik_t*> fab_iter(get_fab_list());
			while (fab_iter.next()) {
				int distance = abs_distance(fab_iter.get_current()->get_pos().get_2d(), k);
				if (distance < this_distance) {
					fabs.insert(fab_iter.get_current());
					distance = this_distance;
				}
				else {
					fabs.append(fab_iter.get_current());
				}
			}
		}
		else {
			// since the distance are presorted, we can just append for a good choice ...
			for(  int test=0;  test<24;  test++  ) {
				fabrik_t *fab = fabrik_t::get_fab(welt,k+next_building[test]);
				if(fab  &&  fabs.is_contained(fab)) {
					fabs.append(fab);
				}
			}
		}

		// are there fabs?
		const char *building_base = translator::translate("%s building %s %s");
		slist_iterator_tpl<fabrik_t*> fab_iter(fabs);
		while (fab_iter.next()) {
			// with factories
			sprintf(buf, building_base, city_name, fab_iter.get_current()->get_name(), stop );
			if(  !all_names.get(buf).is_bound()  ) {
				return strdup(buf);
			}
		}

		// no fabs or all names used up already
		// check for other special building (townhall, monument, tourst attraction)
		for (int i=0; i<24; i++) {
			grund_t *gr = welt->lookup_kartenboden( next_building[i] + k);
			if(gr==NULL  ||  gr->get_typ()!=grund_t::fundament) {
				// no building here
				continue;
			}
			// since closes coordinates are tested first, we do not need to not sort this
			const char *building_name = NULL;
			const gebaeude_t* gb = gr->find<gebaeude_t>();
			if(gb==NULL) {
				// field may have foundations but no building
				continue;
			}
			// now we have a building here
			if (gb->is_monument()) {
				building_name = translator::translate(gb->get_name());
			} else if (gb->ist_rathaus() ||
				gb->get_tile()->get_besch()->get_utyp() == haus_besch_t::attraction_land || // land attraction
				gb->get_tile()->get_besch()->get_utyp() == haus_besch_t::attraction_city) { // town attraction
				building_name = make_single_line_string(translator::translate(gb->get_tile()->get_besch()->get_name()), 2);
			}
			else {
				// normal town house => not suitable for naming
				continue;
			}
			// now we have a name: try it
			sprintf(buf, building_base, city_name, building_name, stop );
			if(  !all_names.get(buf).is_bound()  ) {
				return strdup(buf);
			}
		}

		// still all names taken => then try the normal naming scheme ...
		char numbername[10];
		if(inside) {
			strcpy( numbername, "0center" );
		} else if (li_gr - 6 < k.x  &&  re_gr + 6 > k.x  &&  ob_gr - 6 < k.y  &&  un_gr + 6 > k.y) {
			// close to the city we use a different scheme, with suburbs
			strcpy( numbername, "0suburb" );
		}
		else {
			strcpy( numbername, "0extern" );
		}

		const char *dirname = NULL;
		static const char *diagonal_name[4] = { "nordwest", "nordost", "suedost", "suedwest" };
		static const char *direction_name[4] = { "nord", "ost", "sued", "west" };

		if (k.y < ob_gr  ||  (inside  &&  k.y*3 < (un_gr+ob_gr+ob_gr))  ) {
			if (k.x < li_gr) {
				dirname = diagonal_name[(4-welt->get_einstellungen()->get_rotation())%4];
			}
			else if (k.x > re_gr) {
				dirname = diagonal_name[(1-welt->get_einstellungen()->get_rotation())%4];;
			}
			else {
				dirname = direction_name[(0-welt->get_einstellungen()->get_rotation())%4];;
			}
		} else if (k.y > un_gr  ||  (inside  &&  k.y*3 > (un_gr+un_gr+ob_gr))  ) {
			if (k.x < li_gr) {
				dirname = diagonal_name[(3-welt->get_einstellungen()->get_rotation())%4];;
			}
			else if (k.x > re_gr) {
				dirname = diagonal_name[(2-welt->get_einstellungen()->get_rotation())%4];;
			}
			else {
				dirname = direction_name[(2-welt->get_einstellungen()->get_rotation())%4];;
			}
		} else {
			if (k.x <= stadt->get_pos().x) {
				dirname = direction_name[(3-welt->get_einstellungen()->get_rotation())%4];;
			}
			else {
				dirname = direction_name[(1-welt->get_einstellungen()->get_rotation())%4];;
			}
		}
		dirname = translator::translate(dirname);

		// Try everything to get a unique name
		while(true) {
			// well now try them all from "0..." over "9..." to "A..." to "Z..."
			for(  int i=0;  i<10+26;  i++  ) {
				numbername[0] = i<10 ? '0'+i : 'A'+i-10;
				const char *base_name = translator::translate(numbername);
				if(base_name==numbername) {
					// not translated ... try next
					continue;
				}
				// allow for names without direction
				uint8 count_s = 0;
				for(  uint i=0;  base_name[i]!=0;  i++  ) {
					if(  base_name[i]=='%'  && base_name[i+1]=='s'  ) {
						i++;
						count_s++;
					}
				}
				if(count_s==3) {
					// ok, try this name, if free ...
					sprintf(buf, base_name, city_name, dirname, stop );
				}
				else {
					// ok, try this name, if free ...
					sprintf(buf, base_name, city_name, stop );
				}
				if(  !all_names.get(buf).is_bound()  ) {
					return strdup(buf);
				}
			}
			// here we did not find a suitable name ...
			// ok, no suitable city names, try the suburb ones ...
			if(  strcmp(numbername+1,"center")==0  ) {
				strcpy( numbername, "0suburb" );
			}
			// ok, no suitable suburb names, try the external ones (if not inside city) ...
			else if(  strcmp(numbername+1,"suburb")==0  &&  !inside  ) {
				strcpy( numbername, "0extern" );
			}
			else {
				// no suitable unique name found at all ...
				break;
			}
		}
	}

	/* so far we did not found a matching station name
	 * as a last resort, we will try numbered names
	 * (or the user requested this anyway)
	 */

	// strings for intown / outside of town
	const char *base_name = inside ? translator::translate("%s city %d %s") : translator::translate("%s land %d %s");

	// finally: is there a stop with this name alrady?
	for(  int i=1;  i<32767;  i++  ) {
		sprintf(buf, base_name, city_name, i, stop );
		if(  !all_names.get(buf).is_bound()  ) {
			return strdup(buf);
		}
	}

	// emergency measure: But before we should run out of handles anyway ...
	assert(0);
	return strdup("Unnamed");
}



void
haltestelle_t::step()
{
//	DBG_MESSAGE("haltestelle_t::step()","%s (cnt %i)",get_name(),reroute_counter);
	if(rebuilt_destination_counter != welt->get_schedule_counter()) 
	{
#ifndef NEW_PATHING
		// Old routing system -
		// recalculate straight away
		rebuild_destinations();
#else
		
		// New routing system -
		// recalculate on demand
		// Must recalculate all here, since this is non-specific.
		for(uint8 i = 0; i < warenbauer_t::get_max_catg_index(); i ++)
		{
			reschedule[i] = true;
		}

		// Note: setting reschedule to true here
		// is inefficient, as it means that all halts
		// will need to be recalculated. Where possible,
		// it is best to set reschedule to true
		// individually depending on which halts are
		// affected by rescheduling. 
#endif
	}

	else 
	{
		// all new connection updated => recalc routes
		if(reroute_counter != welt->get_schedule_counter()) 
		{
			reroute_goods();

	//		DBG_MESSAGE("haltestelle_t::step()","rerouting goods at %s",get_name());
		}
	}
	recalc_status();
	
	// Every 256 steps - check whether
	// passengers/goods have been waiting
	// too long.
	if(check_waiting == 0)
	{
		vector_tpl<ware_t> *warray;
		for(uint16 j = 0; j < warenbauer_t::get_max_catg_index(); j ++)
		{
			warray = waren[j];
			if(warray == NULL)
			{
				continue;
			}
			for(uint32 i = 0; i < warray->get_count(); i++) 
			{
				ware_t &tmp = (*warray)[i];

				// skip empty entries
				if(tmp.menge == 0) 
				{
					continue;
				}
						
				// Checks to see whether the freight has been waiting too long.
				// If so, discard it.
				if(tmp.get_besch()->get_speed_bonus() > 0)
				{
					// Only consider for discarding if the goods care about their timings.
					// Goods/passengers' maximum waiting times are proportionate to the length of the journey.
					const uint16 base_max_minutes = (welt->get_einstellungen()->get_passenger_max_wait() / tmp.get_besch()->get_speed_bonus()) * 10;  // Minutes are recorded in tenths
					const uint16 thrice_journey = connexions[tmp.get_besch()->get_catg_index()].get(tmp.get_zwischenziel()) != NULL ? connexions[tmp.get_besch()->get_catg_index()].get(tmp.get_zwischenziel())->journey_time * 3 : base_max_minutes;
					const uint16 max_minutes = base_max_minutes < thrice_journey ? base_max_minutes : thrice_journey;
					const uint16 waiting_minutes = get_waiting_minutes(welt->get_zeit_ms() - tmp.arrival_time);
					if(waiting_minutes > max_minutes)
					{
						// Waiting too long: discard
						if(tmp.is_passenger())
						{
							// Passengers - use unhappy graph.
							add_pax_unhappy(tmp.menge);
						}
						
						// The goods/passengers leave.
						tmp.menge = 0;
					}		
				}
			}
		}
	}
	// Will overflow at 255
	check_waiting ++;
}



/**
 * Called every month
 * @author Hj. Malthaner
 */
void haltestelle_t::neuer_monat()
{
	if(  welt->get_active_player()==besitzer_p  &&  status_color == COL_RED  ) {
		char buf[256];
		sprintf(buf, translator::translate("!0_STATION_CROWDED"), get_name());
		welt->get_message()->add_message(buf, get_basis_pos(),message_t::full, PLAYER_FLAG|besitzer_p->get_player_nr(), IMG_LEER );
		enables &= (PAX|POST|WARE);
	}

	// Hajo: reset passenger statistics
	pax_happy = 0;
	pax_no_route = 0;
	pax_unhappy = 0;

	// hsiegeln: roll financial history
	for (int j = 0; j<MAX_HALT_COST; j++) {
		for (int k = MAX_MONTHS-1; k>0; k--) {
			financial_history[k][j] = financial_history[k-1][j];
		}
		financial_history[0][j] = 0;
	}
}



/**
 * Called every 255 steps
 * will distribute the goods to changed routes (if there are any)
 * @author Hj. Malthaner
 */
void haltestelle_t::reroute_goods()
{
	// reroute only on demand
	reroute_counter = welt->get_schedule_counter();

	for(unsigned i=0; i<warenbauer_t::get_max_catg_index(); i++) 
	{
		if(waren[i])
		{
			vector_tpl<ware_t> * warray = waren[i];
			vector_tpl<ware_t> * new_warray = new vector_tpl<ware_t>(warray->get_count());

			// Hajo:
			// Step 1: re-route goods now and then to adapt to changes in
			// world layout, remove all goods which destination was removed from the map
			// prissi;
			// also the empty entries of the array are cleared
			for(int j = warray->get_count() - 1; j  >= 0; j--) 
			{
				ware_t & ware = (*warray)[j];

				if(ware.menge == 0) 
				{
					continue;
				}

				// since also the factory halt list is added to the ground, we can use just this ...
				if(welt->lookup(ware.get_zielpos())->is_connected(self)) 
				{
					// we are already there!
					if(ware.is_freight()) 
					{
						liefere_an_fabrik(ware);
					}
					continue;
				}

				// check if this good can still reach its destination
				
#ifdef NEW_PATHING
				if(find_route(ware) == 65535)
				{
#else
				if(  suche_route( ware, NULL, false )==NO_ROUTE  ) 
				{
#endif
					// remove invalid destinations
					continue;
				}

				// add to new array
				new_warray->append( ware );
			}	

			INT_CHECK( "simhalt.cc 489" );

			// delete, if nothing connects here
			if (new_warray->empty()) 
			{
#ifndef NEW_PATHING
				if(warenziele[i].empty()) 
#else
				if(connexions[i].empty())
#endif
				{
					// no connections from here => delete
					delete new_warray;
					new_warray = NULL;
				}
			}

			// replace the array
			delete waren[i];
			waren[i] = new_warray;
		}
	}
//#endif
	// likely the display must be updated after this
	resort_freight_info = true;
}



/*
 * connects a factory to a halt
 */
void haltestelle_t::verbinde_fabriken()
{
	// unlink all
	slist_iterator_tpl <fabrik_t *> fab_iter(fab_list);
	while( fab_iter.next() ) {
		fab_iter.get_current()->unlink_halt(self);
	}
	fab_list.clear();

	// then reconnect
	for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		grund_t* gb = i->grund;
		koord p = gb->get_pos().get_2d();

		int cov = welt->get_einstellungen()->get_station_coverage();
		vector_tpl<fabrik_t*>& fablist = fabrik_t::sind_da_welche(welt, p - koord(cov, cov), p + koord(cov, cov));
		for(unsigned i=0; i<fablist.get_count(); i++) {
			fabrik_t* fab = fablist[i];
			if(!fab_list.is_contained(fab)) {
				fab_list.insert(fab);
				fab->link_halt(self);
			}
		}
	}
}



/*
 * removes factory to a halt
 */
void
haltestelle_t::remove_fabriken(fabrik_t *fab)
{
	fab_list.remove(fab);
}


#ifndef NEW_PATHING

void haltestelle_t::hat_gehalten(const ware_besch_t *type, const schedule_t *fpl, const spieler_t *sp )
{
	if(type != warenbauer_t::nichts) {
		for(int i=0; i<fpl->get_count(); i++) {

			// Hajo: Haltestelle selbst wird nicht in Zielliste aufgenommen
			//"Station itself is not in target list" (Google)
			halthandle_t halt = get_halt(welt, fpl->eintrag[i].pos, sp);
			// not existing, or own, or not enabled => ignore
			if(!halt.is_bound()  ||  halt == self  ||  !halt->is_enabled(type)) {
				continue;
			}

			// we need to do this here; otherwise the position of the stop (if in water) may not directly be a halt!
			vector_tpl<halthandle_t> &wz_list = warenziele[ type->get_catg_index() ];
			if(  !wz_list.is_contained(halt)  ) {
				wz_list.append( halt );
				if(  waren[type->get_catg_index()] == NULL  ) {
					// indicates that this can route those goods
					waren[type->get_catg_index()] = new vector_tpl<ware_t>(0);
				}
			}
		}
	}
}

#else

uint16 haltestelle_t::get_average_waiting_time(halthandle_t halt, uint8 category) const
{
	if(&waiting_times[category].get(halt->get_basis_pos()) != NULL)
	{
		fixed_list_tpl<uint16, 16> times = waiting_times[category].get(halt->get_basis_pos());
		const uint16 count = times.get_count();
		if(count > 0 && halt.is_bound())
		{
			uint32 total_times = 0;
			ITERATE(times,i)
			{
				total_times += times.get_element(i);
			}
			total_times /= count;
			// Minimum waiting time of 1 minute (i.e., 10 tenths of a minute)
			// This simulates the overhead time needed to arrive at a stop, and 
			// board, etc. It should help to prevent perverse vias on a single route.
			return total_times >= 10 ? (uint16)total_times : 10;
		}
		return 9;
	}
	return 9;
}

void haltestelle_t::add_connexion(const ware_besch_t *type, const schedule_t *fpl, const convoihandle_t cnv, const linehandle_t line)
{
	
	if(type != warenbauer_t::nichts) 
	{
		ITERATE_PTR(fpl,i)
		{
			// Hajo: Haltestelle selbst wird nicht in Zielliste aufgenommen
			//"Station itself is not in target list" (Google)
			halthandle_t halt = haltestelle_t::get_halt(welt, fpl->eintrag[i].pos, besitzer_p);
			if(!halt.is_bound())
			{
				// Try a public player halt
				spieler_t* sp = welt->get_spieler(0);
				halt = haltestelle_t::get_halt(welt, fpl->eintrag[i].pos, sp);
			}
			// not existing, or own, or not enabled => ignore
			if(!halt.is_bound() || halt == self || !halt->is_enabled(type)) 
			{
				continue;
			}

			// Check the journey times to the connexion
			connexion* new_connexion = new connexion;
			new_connexion->waiting_time = get_average_waiting_time(halt, type->get_catg_index());
			
			// Check the average speed.
			uint16 average_speed = 0;
			if(line.is_bound())
			{
				average_speed = line->get_finance_history(1, LINE_AVERAGE_SPEED) > 0 ? line->get_finance_history(1, LINE_AVERAGE_SPEED) : line->get_finance_history(0, LINE_AVERAGE_SPEED);

				if(average_speed == 0)
				{
					// If the average speed is not initialised, take a guess to prevent perverse outcomes and possible deadlocks.
					average_speed = speed_to_kmh(line->get_convoy(0)->get_min_top_speed()) / 2;
				}
			}
			else if(cnv.is_bound())
			{
				average_speed = cnv->get_finance_history(1, CONVOI_AVERAGE_SPEED) > 0 ? cnv->get_finance_history(1, CONVOI_AVERAGE_SPEED) : cnv->get_finance_history(0, CONVOI_AVERAGE_SPEED);
				if(average_speed == 0)
				{
					// If the average speed is not initialised, take a guess to prevent perverse outcomes and possible deadlocks.
					average_speed = speed_to_kmh(cnv->get_min_top_speed()) / 2;
				}
			}
			else
			{
				// This should never be reached.
				assert(false);
				continue;
			}
			
			//const uint32 journey_distance = accurate_distance(halt->get_basis_pos(), get_basis_pos());
			uint32 journey_distance = 0;
			bool start_counting = false;
			halthandle_t current_halt = self;
			halthandle_t previous_halt;
			bool goal_found = false;
			const uint16 max_steps = fpl->get_count();
			uint16 current_step = 0;
			
			while(!goal_found)
			{
				previous_halt = current_halt;
				current_halt = haltestelle_t::get_halt(welt, fpl->eintrag[current_step].pos, besitzer_p);
				if(!current_halt.is_bound())
				{
					// Try a public player halt
					current_halt = haltestelle_t::get_halt(welt, fpl->eintrag[current_step].pos, welt->get_spieler(0));
				}
				if(!current_halt.is_bound())
				{
					current_halt = previous_halt;
				}
				if(current_halt == self)
				{
					if(!start_counting)
					{
						start_counting = true;
					}
					else
					{
						// This means that the route returns to this halt before it gets
						// to the destination, so the distance will be measured from the
						// *second* instance of it coming here. 
						journey_distance = 0;
					}
				}
				if(start_counting && current_halt.is_bound() && previous_halt.is_bound() && current_halt != self)
				{
					journey_distance += accurate_distance(current_halt->get_basis_pos(), previous_halt->get_basis_pos());
				}
				if(start_counting && current_halt == halt)
				{
					goal_found = true;
				}
				if(current_step < max_steps - 1)
				{
					current_step ++;
				}
				else
				{
					current_step = 0;
				}
			}
			// Journey time in *tenths* of minutes.
			new_connexion->journey_time = (((float)journey_distance / (float)average_speed) * welt->get_einstellungen()->get_journey_time_multiplier() * 600);
			new_connexion->best_convoy = cnv;
			new_connexion->best_line = line;

			// Check whether this is the best connexion so far, and, if so, add it.
			if(!connexions[type->get_catg_index()].put(halt, new_connexion))
			{
				// The key exists in the hashtable already - check whether this entry is better.
				connexion* existing_connexion = connexions[type->get_catg_index()].get(halt);
				if(existing_connexion->journey_time > new_connexion->journey_time)
				{
					// The new connexion is better - replace it.
					delete existing_connexion;
					connexions[type->get_catg_index()].set(halt, new_connexion);
				}
				else
				{
					delete new_connexion;
				}
				//TODO: Consider whether to add code for comfort here, too.

				if(  waren[type->get_catg_index()] == NULL  ) 
				{
					// indicates that this can route those goods
					waren[type->get_catg_index()] = new vector_tpl<ware_t>(0);
				}
			}
		}
	}
}

linehandle_t haltestelle_t::get_preferred_line(halthandle_t transfer, uint8 category) const
{
	if(connexions[category].empty() || connexions[category].get(transfer) == NULL)
	{
		linehandle_t dummy;
		return dummy;
	}
	linehandle_t best_line = connexions[category].get(transfer)->best_line;
	return best_line;
}

convoihandle_t haltestelle_t::get_preferred_convoy(halthandle_t transfer, uint8 category) const
{
	if(connexions[category].empty() || connexions[category].get(transfer) == NULL)
	{
		convoihandle_t dummy;
		return dummy;
	}
	convoihandle_t best_convoy  = connexions[category].get(transfer)->best_convoy;
	return best_convoy;
}

#endif

#ifndef NEW_PATHING

/**
 * Rebuilds the list of reachable destinations
 *
 * @author Hj. Malthaner
 */
void haltestelle_t::rebuild_destinations()
{
	// Hajo: first, remove all old entries
	for (uint8 i=0; i<warenbauer_t::get_max_catg_index(); i++){
		warenziele[i].clear();
	};
	rebuilt_destination_counter = welt->get_schedule_counter();
	resort_freight_info = true;	// might result in error in routing

	const bool i_am_public = get_besitzer()==welt->get_spieler(1);

// DBG_MESSAGE("haltestelle_t::rebuild_destinations()", "Adding new table entries");

	// first all single convois without lines
	vector_tpl<uint8> add_catg_index(4);
	for (vector_tpl<convoihandle_t>::const_iterator i = welt->convois_begin(), end = welt->convois_end(); i != end; ++i) {
		convoihandle_t cnv = *i;
		if(cnv->get_line().is_bound()) {
			continue;
		}

		// DBG_MESSAGE("haltestelle_t::rebuild_destinations()", "convoi %d %p", cnv.get_id(), cnv.get_rep());
		if(i_am_public  ||  cnv->get_besitzer()==get_besitzer()) {

			INT_CHECK("simhalt.cc 612");

			schedule_t *fpl = cnv->get_schedule();
			const spieler_t *cnv_owner = cnv->get_besitzer();
			if(fpl) {
				for(int i=0; i<fpl->get_count(); i++) {

					// Hajo: H�lt dieser convoi hier?
					if (get_halt(welt, fpl->eintrag[i].pos,cnv_owner) == self) {

						// what goods can this line transport?
						add_catg_index.clear();
						for(uint i=0;  i<cnv->get_vehikel_anzahl();  i++  ) {
							// Only consider vehicles that really transport something
							// this helps against routing errors through passenger
							// trains pulling only freight wagons
							if (cnv->get_vehikel(i)->get_fracht_max() == 0) {
								continue;
							}
							const ware_besch_t *ware=cnv->get_vehikel(i)->get_fracht_typ();
							if(ware!=warenbauer_t::nichts  &&  !add_catg_index.is_contained(ware->get_catg_index())) {
								// now add the freights
								hat_gehalten( ware, fpl, cnv_owner );
								add_catg_index.append_unique(ware->get_catg_index());
							}
						}
					}
				}
			}
		}
	}

	// now for the lines
	for(uint i=0; i<registered_lines.get_count(); i++) {
		const linehandle_t line = registered_lines[i];
		schedule_t *fpl = line->get_schedule();
		const spieler_t *line_owner = line->get_besitzer();
		assert(fpl);
		// ok, now add line to the connections
		if(line->count_convoys()>0  &&  (i_am_public  ||  line_owner==get_besitzer())) {
			for( uint j=0; j<line->get_goods_catg_index().get_count();  j++  ) {
				hat_gehalten( warenbauer_t::get_info_catg_index(line->get_goods_catg_index()[j]), fpl, line_owner );
			}
		}
	}
}

#else

void haltestelle_t::reset_connexions(uint8 category)
{
	if(connexions[category].empty())
	{
		// Nothing to do here
		return;
	}
	
	quickstone_hashtable_iterator_tpl<haltestelle_t, connexion*> iter(connexions[category]);

	// Delete the connexions.
	while(iter.next())
	{
		delete iter.get_current_value();
	}

	
	// Finally, clear the collection class.
	connexions[category].clear();
}

//@author: jamespetts (although much is taken from the original rebuild_destinations())
void haltestelle_t::rebuild_connexions(uint8 category)
{	
	reset_connexions(category);
	connexions_timestamp[category] = welt->get_base_pathing_counter();
	if(connexions_timestamp[category] == 0 || reschedule[category])
	{
		// Spread the load of rebuilding this with pathing - advance by half the interval.
		connexions_timestamp[category] += (welt->get_einstellungen()->get_max_rerouting_interval_months() / 2);
	}

	reschedule[category] = false;
	
	rebuilt_destination_counter = welt->get_schedule_counter();
	resort_freight_info = true;	// might result in error in routing

	const bool i_am_public = get_besitzer() == welt->get_spieler(1);

	// first all single convois without lines
	vector_tpl<uint8> add_catg_index(4);
	halthandle_t tmp_halt;
	const linehandle_t dummy_line;
	const convoihandle_t dummy_convoy;
	schedule_t *fpl = NULL;
	convoihandle_t cnv;
	for (vector_tpl<convoihandle_t>::const_iterator i = welt->convois_begin(), end = welt->convois_end(); i != end; ++i) 
	{
		cnv = *i;
		if(cnv->get_line().is_bound()) 
		{
			// Deal with lines later.
			continue;
		}

		if(i_am_public || cnv->get_besitzer() == get_besitzer()) 
		{
			INT_CHECK("simhalt.cc 612");

			fpl = cnv->get_schedule();
			if(fpl != NULL) 
			{				
				ITERATE_PTR(fpl, i)
				{
					// Hajo: H�lt dieser convoi hier?
					// "If this Convoi here?" (Google)
					tmp_halt = haltestelle_t::get_halt(welt, fpl->eintrag[i].pos, besitzer_p);
					if(!tmp_halt.is_bound())
					{
						// Try a public player halt
						spieler_t* sp = welt->get_spieler(0);
						tmp_halt = haltestelle_t::get_halt(welt, fpl->eintrag[i].pos, sp);
					}
					if (tmp_halt == self) 
					{
						// what goods can this convoy transport?
						add_catg_index.clear();
						for(uint i = 0;  i < cnv->get_vehikel_anzahl();  i++) 
						{
							// Only consider vehicles that really transport something
							// this helps against routing errors through passenger
							// trains pulling only freight wagons
							const ware_besch_t* ware = cnv->get_vehikel(i)->get_fracht_typ();

							if (cnv->get_vehikel(i)->get_fracht_max() == 0 || ware->get_catg_index() != category) 
							{
								continue;
							}
							
							if(ware != warenbauer_t::nichts)
							{
								// now add the freights
								add_connexion(ware, fpl, cnv, dummy_line);
								if(!add_catg_index.is_contained(ware->get_catg_index()))
								{
									add_catg_index.append_unique(category);
								}
							}
						}
					}
				}
			}
		}
	}
	// now for the lines
	ITERATE(registered_lines,i)
	{
		const linehandle_t line = registered_lines[i];
		fpl = line->get_schedule();
		assert(fpl);
		// ok, now add line to the connections
		if(line->count_convoys( )> 0 && (i_am_public || line->get_convoy(0)->get_besitzer() == get_besitzer()))
		{
			INT_CHECK("simhalt.cc 613");

			if(fpl != NULL) 
			{
				halthandle_t tmp_halt;

				ITERATE_PTR(fpl, i)
				{
					tmp_halt = haltestelle_t::get_halt(welt, fpl->eintrag[i].pos, besitzer_p);
					if(!tmp_halt.is_bound())
					{
						// Try a public player halt
						spieler_t* sp = welt->get_spieler(0);
						tmp_halt = haltestelle_t::get_halt(welt, fpl->eintrag[i].pos, sp);
					}
					if (tmp_halt == self) 
					{
						// what goods can this line transport?
						add_catg_index.clear();
						const minivec_tpl<uint8> &goods = line->get_goods_catg_index();
						ITERATE(goods, j)
						{
							const ware_besch_t* ware = warenbauer_t::get_info_catg_index(goods[j]);

							if (ware->get_catg_index() != category) 
							{
								continue;
							}
							
							if(ware != warenbauer_t::nichts) 
							{
								// now add the freights
								add_connexion(ware, fpl, dummy_convoy, line);
								if(!add_catg_index.is_contained(ware->get_catg_index()))
								{
									add_catg_index.append_unique(category);
								}
							}
						}
					}
				}
			}
		}
	}
}



//@author: jamespetts
void haltestelle_t::calculate_paths(halthandle_t goal, uint8 category)
{
	// Use Dijkstra's Algorithm to find all the best routes from here at once
	// http://en.wikipedia.org/wiki/Dijkstra%27s_algorithm

	// Only recalculate when necessary (i.e., on schedule change, or
	// after a certain period of time has elapsed), and in any event
	// not until a destination from this halt is requested, to prevent
	// the game pausing and becoming unresponsive whilst everything is
	// re-calculated all at the same time.

	if((category == 0 && !get_pax_enabled()) || (category == 1 && !get_post_enabled()) || (category > 1 && !get_ware_enabled()))
	{
		// Cannot route from this station: do not try.
		return;
	}
	
	if(reschedule[category] || connexions_timestamp[category] <= welt->get_base_pathing_counter() - welt->get_einstellungen()->get_max_rerouting_interval_months() || welt->get_base_pathing_counter() >= (65535 - welt->get_einstellungen()->get_max_rerouting_interval_months()))
	{
		// Connexions are stale. Recalculate.
		rebuild_connexions(category);
		// Not clearing the open list would be faster, but could give anomalous results.
		// Thus, if connexions are stale, all new paths will be recalculated from scratch.
		if(!open_list.empty())
		{
			open_list.clear();
			delete[] path_nodes;
		}
	}
	if(paths_timestamp[category] <= welt->get_base_pathing_counter() - welt->get_einstellungen()->get_max_rerouting_interval_months() || welt->get_base_pathing_counter() >= (65535 - welt->get_einstellungen()->get_max_rerouting_interval_months()))
	{
		// List is stale. Recalculate.
		// If this is false, then this is only being called to finish 
		// calculating paths that have not yet been calculated.
		if(!open_list.empty())
		{
			open_list.clear();
			delete[] path_nodes;
		}

		// Reset the timestamp.
		paths_timestamp[category] = welt->get_base_pathing_counter();
	}

	if(open_list.empty())
	{
		// Only reset the list if it is empty, so as to allow for re-using the open
		// list on subsequent occasions of finding a path. 
		const uint32 total_halts = alle_haltestellen.get_count();
		const sint32 max_transfers = welt->get_einstellungen()->get_max_transfers();
		const uint32 max_depth = total_halts * max_transfers;
		max_iterations = max_depth <= 65535 ? max_depth : 65535;
		
		path_nodes = new path_node[max_iterations];

		iterations = 0;
		path_node* starting_node = &path_nodes[iterations++];
		starting_node->halt = self;
		starting_node->journey_time = 0;
		starting_node->link = NULL;
		open_list.insert(starting_node);
	}

	path_node* current_node = NULL;
	quickstone_hashtable_tpl<haltestelle_t, connexion*> *current_connexions = NULL;
	connexion* current_connexion = NULL;
	connexion* previous_connexion = NULL;
	path_node* new_node = NULL;
	halthandle_t next_halt;

	while(open_list.get_count() > 0 && iterations < max_iterations)
	{	
		current_node = open_list.pop();

		if(!current_node->halt.is_bound() || paths[category].get(current_node->halt).journey_time != 65535)
		{
			// Only insert into the open list if the 
			// item is not already on the closed list,
			// and the halt has not been deleted since 
			// being added to the open list.
			continue;
		}

		current_connexions = current_node->halt->get_connexions(category);
		quickstone_hashtable_iterator_tpl<haltestelle_t, connexion*> iter(*current_connexions);
		linehandle_t previous_best_line;
		convoihandle_t previous_best_convoy;
		if(current_node->link != NULL)
		{
			previous_connexion = current_node->link->halt->get_connexions(category)->get(current_node->halt);
			if(previous_connexion != NULL)
			{
				previous_best_line = previous_connexion->best_line;
				previous_best_convoy = previous_connexion->best_convoy;
			}
		}
		while(iter.next() && iterations <= max_iterations)
		{
			next_halt = iter.get_current_key();
			if(paths[category].get(next_halt).journey_time != 65535)
			{
				// Only insert into the open list if the 
				// item is not already on the closed list.
				continue;
			}

			current_connexion = iter.get_current_value();

			if(previous_best_line == current_connexion->best_line && previous_best_convoy == current_connexion->best_convoy)
			{
				continue;
			}
			new_node = &path_nodes[iterations++];
			new_node->halt = next_halt;
			new_node->journey_time = current_connexion->journey_time + current_connexion->waiting_time + current_node->journey_time;
			new_node->link = current_node;

			open_list.insert(new_node);

		}
		
		path new_path;
		new_path.journey_time = current_node->journey_time;

		// Add only if not already contained and it is not the head node.
		if(current_node->link != NULL && paths[category].put(current_node->halt, new_path))
		{
			// Track the path back to get the next transfer from this halt
			path_node* track_node = current_node;
			//while(track_node->link != NULL)
			//for(uint8 depth = 0; depth <= max_transfers; depth ++)
			for(uint8 depth = 0; depth <= 255; depth ++)
			{
				if(track_node->halt.is_bound())
				{

					if(track_node->link->link == NULL)
					{
						paths[category].access(current_node->halt)->next_transfer = track_node->halt;
						//path tmp = *paths[category].access(current_node->halt);
						//tmp.next_transfer = track_node->halt;
						//tmp = NULL;
						// End of search.
						break;
					}
				}
				
				track_node = track_node->link;
			}
			if(!paths[category].get(current_node->halt).next_transfer.is_bound())
			{
				// Remove bad paths (transfer depth too great, so aborted)
				paths[category].remove(current_node->halt);
			}
		}
		
		INT_CHECK( "simhalt 1694" );

		if(current_node->halt == goal)
		{
			// Abort the search early if the goal stop is found.
			// Because the open list is stored on the heap, the search
			// can be resumed where it left off if another goal, as yet
			// not found, is searched for, unless the index is stale.

			if(open_list.empty() || iterations == max_iterations)
			{
				open_list.clear();
				delete[] path_nodes;
				search_complete = true;
			}
			return;
		}	
		
		current_node = NULL;
	}
	
	// If the code has reached here without returning, the search is complete.
	search_complete = true;
	
	// Thanks to Knightly for this suggestion.
	open_list.clear();
	delete[] path_nodes;
}



haltestelle_t::path haltestelle_t::get_path_to(halthandle_t goal, uint8 category) 
{
	assert(goal.is_bound());
	path destination_path;
	
	if(reschedule[category] || paths_timestamp[category] <= welt->get_base_pathing_counter() - welt->get_einstellungen()->get_max_rerouting_interval_months() || welt->get_base_pathing_counter() >= (65535 - welt->get_einstellungen()->get_max_rerouting_interval_months()))
	{
		// If the paths hashtable is stale, clear it.
		// This will mean that all the paths will need to be recalculated.
		// Must always recalculate if the schedules change.
		paths[category].clear();
		search_complete = false;
	}

	destination_path = paths[category].get(goal);
	/*char* self_name = (char*)self->get_name();
	char* goal_name = (char*)goal->get_name();
	char* next_transfer_name = "NULL";
	if(destination_path.next_transfer.is_bound())
	{
		next_transfer_name = (char*)destination_path.next_transfer->get_name();
	}*/

	if(!destination_path.next_transfer.is_bound() && !search_complete)
	{
		// The pathfinding is incomplete or stale - recalculate
		// If the next transfer is not bound even though the search
		// is complete, then there is no admissible path to the goal.
		calculate_paths(goal, category);
		destination_path = paths[category].get(goal);
	}

	/*if(destination_path.next_transfer.is_bound())
	{
		next_transfer_name = (char*)destination_path.next_transfer->get_name();
	}*/

	return destination_path;
}

quickstone_hashtable_tpl<haltestelle_t, haltestelle_t::connexion*>* haltestelle_t::get_connexions(uint8 c)
{ 

	if(reschedule[c] || connexions->get_count() == 0 || paths_timestamp[c] <= welt->get_base_pathing_counter() - welt->get_einstellungen()->get_max_rerouting_interval_months() || welt->get_base_pathing_counter() >= (65535 - welt->get_einstellungen()->get_max_rerouting_interval_months()))
	{
		// Rebuild the connexions if they are stale.
		rebuild_connexions(c);
	}
	
	return &connexions[c]; 
}

void haltestelle_t::force_paths_stale(const uint8 category)
{
	if(paths_timestamp[category] > welt->get_einstellungen()->get_max_rerouting_interval_months())
	{
		paths_timestamp[category] -= welt->get_einstellungen()->get_max_rerouting_interval_months();
	}
	else
	{
		// Prevent overflows.
		paths_timestamp[category] = 0;
	}
}

minivec_tpl<halthandle_t>* haltestelle_t::build_destination_list(ware_t &ware)
{
	const ware_besch_t * warentyp = ware.get_besch();
	const uint8 ware_catg_index = warentyp->get_catg_index();

	if(ware.get_zielpos() == koord::invalid && ware.get_ziel().is_bound())
	{
		ware.set_zielpos(ware.get_ziel()->get_basis_pos());
	}
		
	const koord ziel = ware.get_zielpos();

	// since also the factory halt list is added to the ground, we can use just this ...
	const planquadrat_t *plan = welt->lookup(ziel);
	const halthandle_t *halt_list = plan->get_haltlist();
	// but we can only use a subset of these
	minivec_tpl<halthandle_t> *ziel_list = new minivec_tpl<halthandle_t>(plan->get_haltlist_count());

	for(uint16 h = 0; h < plan->get_haltlist_count(); h++) 
	{
		halthandle_t halt = halt_list[h];
		if(halt->is_enabled(warentyp)) 
		{
			ziel_list->append(halt);
		}
	}
	return ziel_list;
}

uint16 haltestelle_t::find_route(minivec_tpl<halthandle_t> *ziel_list, ware_t &ware, const uint16 previous_journey_time)
{
	uint16 journey_time = previous_journey_time;

	if(ziel_list->empty()) 
	{
		//no target station found
		ware.set_ziel(halthandle_t());
		ware.set_zwischenziel(halthandle_t());
		return 65535;
	}

	// check, if the shortest connection is not right to us ...
	if(ziel_list->is_contained(self)) 
	{
		ware.set_ziel(self);
		ware.set_zwischenziel( halthandle_t());
		return 65535;
	}

	sint16 best_destination = -1;

	// Now, find the best route from here.
	ITERATE_PTR(ziel_list,i)
	{
		path test_path = get_path_to(ziel_list->get_element(i), ware.get_besch()->get_catg_index());
		if(test_path.journey_time != 65535 && test_path.next_transfer.is_bound())
		{
			journey_time = test_path.journey_time;
			best_destination = i;
		}
	}
	
	if(journey_time < previous_journey_time && best_destination >= 0)
	{
		// If we are comparing this with other routes from different start halts,
		// only set the details if it is the best route so far.
		ware.set_ziel(ziel_list->get_element(best_destination));
		path final_path = get_path_to(ziel_list->get_element(best_destination), ware.get_besch()->get_catg_index());
		if(final_path.next_transfer.is_bound())
		{
			ware.set_zwischenziel(final_path.next_transfer);
			return final_path.journey_time;
		}
		// If the next transfer is not bound, something has gone wrong.
		return 65535;
	}
	
	return journey_time;
}

uint16 haltestelle_t::find_route (ware_t &ware, const uint16 previous_journey_time)
{
	minivec_tpl<halthandle_t> *ziel_list = build_destination_list(ware);
	const uint16 journey_time = find_route(ziel_list, ware, previous_journey_time);
	delete ziel_list;
	return journey_time;
}

#endif

#ifndef NEW_PATHING


/* HNode is used for route search */
struct HNode {
	halthandle_t halt;
	uint16 depth;
	HNode *link;
	HNode *next;	// for linked list
};

/**
 * This routine tries to find a route for a good packet (ware)
 * it will be called for
 *  - new goods (either from simcity.cc or simfab.cc)
 *  - goods that transfer and cannot be joined with other goods
 *  - during rerouting
 * Therefore this routine eats up most of the performance in
 * later games. So all changes should be done with this in mind!
 *
 * If no route is found, ziel and zwischenziel are unbound handles.
 * If next_to_ziel is not NULL, it will get the koordinate of the stop
 * previous to target. Can be used to create passengers/mail back the
 * same route back
 *
 * if USE_ROUTE_SLIST_TPL is defined, the list template will be used.
 * However, this is about 50% slower.
 *
 * @author Hj. Malthaner/prissi/gerw
 */
int haltestelle_t::suche_route( ware_t &ware, koord *next_to_ziel, bool avoid_overcrowding )
{
	const ware_besch_t * warentyp = ware.get_besch();
	const uint8 ware_catg_index = warentyp->get_catg_index();
	const koord ziel = ware.get_zielpos();

	// since also the factory halt list is added to the ground, we can use just this ...
	const planquadrat_t *plan = welt->lookup(ziel);
	const halthandle_t *halt_list = plan->get_haltlist();
	// but we can only use a subset of these
	vector_tpl<halthandle_t> ziel_list(plan->get_haltlist_count());
	for( unsigned h=0;  h<plan->get_haltlist_count();  h++ ) 
	{
		halthandle_t halt = halt_list[h];
		if(  halt->is_enabled(warentyp)  ) 
		{
			ziel_list.append(halt);
		}
		else 
		{
//DBG_MESSAGE("suche_route()","halt %s near (%i,%i) does not accept  %s!",halt->get_name(),ziel.x,ziel.y,warentyp->get_name());
		}
	}

	if(  ziel_list.empty()  ) 
	{
		//no target station found
		ware.set_ziel( halthandle_t() );
		ware.set_zwischenziel( halthandle_t() );
		// printf("keine route zu %d,%d nach %d steps\n", ziel.x, ziel.y, step);
		if(  next_to_ziel != NULL  ) 
		{
			*next_to_ziel = koord::invalid;
		}
		return NO_ROUTE;
	}

	// check, if the shortest connection is not right to us ...
	if(  ziel_list.is_contained(self)  ) 
	{
		ware.set_ziel( self );
		ware.set_zwischenziel( halthandle_t() );
		if(  next_to_ziel != NULL  ) 
		{
			*next_to_ziel = koord::invalid;
		}
	}

	// single threading makes some things easier
	static uint32 current_mark = 0;
	static HNode nodes[65535];

	/* Need to clean up?
	 * Otherwise we just incease the mark => less time for cleanups
	 */
	if(  current_mark == 0xFFFFFFFFu  )
	{
		slist_iterator_tpl<halthandle_t > halt_iter (alle_haltestellen);
		while(  halt_iter.next()  ) 
		{
			halt_iter.get_current()->marke = 0;
		}
		current_mark = 0;
	}
	current_mark ++;

	// die Berechnung erfolgt durch eine Breitensuche fuer Graphen
	// Warteschlange fuer Breitensuche
	// "The calculation is performed by a wide search for graphs queued for Beam Search" (Google)
	const uint16 max_transfers = welt->get_einstellungen()->get_max_transfers();
#ifdef USE_ROUTE_SLIST_TPL
	slist_tpl<HNode *> queue;
#else
	// we need just need to know the current bottom of the list with respect to the nodes array
	uint32 bottom_of_the_list = 0;
#endif
	uint32 step = 1;
	HNode *tmp;

	nodes[0].halt = self;
	nodes[0].link = NULL;
	nodes[0].depth = NULL;

#ifdef USE_ROUTE_SLIST_TPL
	queue.insert( &nodes[0] );	// init queue mit erstem feld "with the first field" (Google)
#endif
	self->marke = current_mark;

	const uint32 max_hops = welt->get_einstellungen()->get_max_hops();
	// here the normal routing with overcrowded stops is done
	do 
	{
#ifdef USE_ROUTE_SLIST_TPL
		tmp = queue.remove_first();
#else
		tmp = &nodes[bottom_of_the_list++];
#endif

		const halthandle_t halt = tmp->halt;

		// we end this loop always with this jump (if sucessful)
		if(ziel_list.is_contained(halt)) 
		{
			goto found;
		}

		// Hajo: check for max transfers -> don't add more stations
		//      to queue if the limit is reached
		if(tmp->depth < max_transfers  &&  step<64000u  ) 
		{
			const vector_tpl<halthandle_t> *wz = halt->get_warenziele(ware_catg_index);
			for(  uint32 i=0;  i<wz->get_count();  i++  ) 
			{
				// since these are precalculated, they should be always pointing to a valid ground
				// (if not, we were just under construction, and will be fine after 16 steps)
				const halthandle_t &tmp_halt = (*wz)[i];
				if(tmp_halt.is_bound() &&  tmp_halt->marke!=current_mark) 
				{
					HNode *node = &nodes[step++];
					node->halt = tmp_halt;
					node->depth = tmp->depth + 1;
					node->link = tmp;

#ifdef USE_ROUTE_SLIST_TPL
					queue.append( node );
#endif
					// betretene Haltestellen markieren
					tmp_halt->marke = current_mark;
				}
			}
		} // max transfers

#ifdef USE_ROUTE_SLIST_TPL
	} while (!queue.empty() && step < welt->get_einstellungen()->get_max_hops());
#else
	} while(  bottom_of_the_list < step  &&  step < max_hops  );
#endif

	// if the loop ends, nothing was found
	tmp = 0;

found:

	if(tmp) 
	{
		// ziel gefunden
		// "target found"  (Google)
		ware.set_ziel( tmp->halt );

		if(tmp->link == NULL) 
		{
			// kein zwischenziel
			// "no between target" (Google)
			ware.set_zwischenziel(ware.get_ziel());
			if(next_to_ziel!=NULL) 
			{
				// for reverse route the next hop, but not hop => enter start
				*next_to_ziel = self->get_basis_pos();
			}
		}
		else 
		{
			if(next_to_ziel!=NULL) 
			{
				// for reverse route the next hop
				*next_to_ziel = tmp->link->halt->get_basis_pos();
			}
			// find the intermediate stops
			while(tmp->link->link) 
			{
				tmp = tmp->link;
				if(  avoid_overcrowding  &&  tmp->halt->is_overcrowded(ware_catg_index)  ) 
				{
					return ROUTE_OVERCROWDED;
				}
			}
			ware.set_zwischenziel(tmp->halt);
		}
		return ROUTE_OK;
	}
	else 
	{
		// no suitable target station found
		ware.set_ziel( halthandle_t() );
		ware.set_zwischenziel( halthandle_t() );
		if(next_to_ziel!=NULL) {
			*next_to_ziel = koord::invalid;
		}
		return NO_ROUTE;
	}
}

#endif

/**
 * Found route and station uncrowded
 * @author Hj. Malthaner
 */
void haltestelle_t::add_pax_happy(int n)
{
	pax_happy += n;
	book(n, HALT_HAPPY);
}



/**
 * Found no route
 * @author Hj. Malthaner
 */
void haltestelle_t::add_pax_no_route(int n)
{
	pax_no_route += n;
	book(n, HALT_NOROUTE);
}



/**
 * Station crowded
 * @author Hj. Malthaner
 */
void haltestelle_t::add_pax_unhappy(int n)
{
	pax_unhappy += n;
	book(n, HALT_UNHAPPY);
}



void haltestelle_t::liefere_an_fabrik(const ware_t& ware) //"deliver to the factory" (Google)
{
	slist_iterator_tpl<fabrik_t *> fab_iter(fab_list);

	while(fab_iter.next()) {
		fabrik_t * fab = fab_iter.get_current();

		const vector_tpl<ware_production_t>& eingang = fab->get_eingang();
		for (uint32 i = 0; i < eingang.get_count(); i++) {
			if (eingang[i].get_typ() == ware.get_besch() && ware.get_zielpos() == fab->get_pos().get_2d()) {
				fab->liefere_an(ware.get_besch(), ware.menge);
				return;
			}
		}
	}
}



/* retrieves a ware packet for any destination in the list
 * needed, if the factory in question wants to remove something
 */
bool haltestelle_t::recall_ware( ware_t& w, uint32 menge )
{
	w.menge = 0;
	vector_tpl<ware_t> *warray = waren[w.get_besch()->get_catg_index()];
	if(warray!=NULL) {

		for(  uint32 i=0;  i<warray->get_count();  i++ ) {
			ware_t &tmp = (*warray)[i];

			// skip empty entries
			if(tmp.menge==0  ||  w.get_index()!=tmp.get_index()  ||  w.get_zielpos()!=tmp.get_zielpos()) {
				continue;
			}

			// not too much?
			if(tmp.menge > menge) {
				// not all can be loaded
				tmp.menge -= menge;
				w.menge = menge;
			}
			else {
				// leave an empty entry => joining will more often work
				w.menge = tmp.menge;
				tmp.menge = 0;
			}
			book(w.menge, HALT_ARRIVED);
			resort_freight_info = true;
			return true;
		}
	}
	// nothing to take out
	return false;
}


// will load something compatible with wtyp into the car which schedule is fpl
ware_t haltestelle_t::hole_ab(const ware_besch_t *wtyp, uint32 maxi, const schedule_t *fpl, const spieler_t *sp, convoi_t* cnv) //"hole from" (Google)
{
	// prissi: first iterate over the next stop, then over the ware
	// might be a little slower, but ensures that passengers to nearest stop are served first
	// this allows for separate high speed and normal service
	const uint8 count = fpl->get_count();
	vector_tpl<ware_t> *warray = waren[wtyp->get_catg_index()];

	if(warray != NULL) 
	{

		// da wir schon an der aktuellem haltestelle halten
		// startet die schleife ab 1, d.h. dem naechsten halt

		// because we have to keep the current haltestelle
		// loop starts from 1, i.e. the next stop (Google)

		for(uint8 i = 1; i < count; i++) 
		{
			const uint8 wrap_i = (i + fpl->get_aktuell()) % count; //aktuell = "current" (Google)

			const halthandle_t plan_halt = haltestelle_t::get_halt(welt, fpl->eintrag[wrap_i].pos, sp); //eintrag = "entry" (Google)
			if(plan_halt == self) 
			{
				// we will come later here again ...
				break;
			}
			else if(plan_halt.is_bound() && warray->get_count() > 0) 
			{

				// The random offset will ensure that all goods have an equal chance to be loaded.
				sint32 offset = simrand(warray->get_count());
				for(uint32 i = 0;  i < warray->get_count();  i++) 
				{
					ware_t &tmp = (*warray)[ i+offset ];

					// prevent overflow (faster than division)
					if(i + offset + 1 >= warray->get_count()) 
					{
						offset -= warray->get_count();
					}

					// skip empty entries
					if(tmp.menge == 0) 
					{
						continue;
					}
					
					

					// Checks to see whether the freight has been waiting too long.
					// If so, discard it.
					if(tmp.get_besch()->get_speed_bonus() > 0)
					{
						// Only consider for discarding if the goods care about their timings.
						// Goods/passengers' maximum waiting times are proportionate to the length of the journey.
						const uint16 base_max_minutes = (welt->get_einstellungen()->get_passenger_max_wait() / tmp.get_besch()->get_speed_bonus()) * 10;  // Minutes are recorded in tenths
						//const uint16 thrice_journey = connexions[tmp.get_besch()->get_catg_index()].get(tmp.get_zwischenziel()) != NULL ? connexions[tmp.get_besch()->get_catg_index()].get(tmp.get_zwischenziel())->journey_time * 3 : base_max_minutes;
						//const uint16 max_minutes = base_max_minutes < thrice_journey ? base_max_minutes : thrice_journey;
						const uint16 waiting_minutes = get_waiting_minutes(welt->get_zeit_ms() - tmp.arrival_time);
						/* 
						 *  This is depracated here, since it is now called every 256 steps instead.
						 *
						if(waiting_minutes > max_minutes)
						{
							// Waiting too long: discard
							if(tmp.is_passenger())
							{
								// Passengers - use unhappy graph.
								add_pax_unhappy(tmp.menge);
							}
							
							// The goods/passengers leave.
							tmp.menge = 0;
						}
						*/
#ifdef NEW_PATHING
						// Skip if the goods have recently arrived, and this is not their preferred line/convoy
						// After waiting some time (1/3rd of their maximum wait), they will board anything.
						const uint16 third_minutes = base_max_minutes / 3;
						const uint16 twice_journey = connexions[tmp.get_besch()->get_catg_index()].get(tmp.get_zwischenziel()) != NULL ? connexions[tmp.get_besch()->get_catg_index()].get(tmp.get_zwischenziel())->journey_time * 2 : base_max_minutes;
						const uint16 max_best_minutes = third_minutes > twice_journey ? twice_journey : third_minutes;
						if(cnv != NULL && waiting_minutes <= third_minutes)
						{
							if(cnv->get_line().is_bound())
							{
								linehandle_t best_line = get_preferred_line(tmp.get_zwischenziel(), tmp.get_besch()->get_catg_index());
								if(best_line.is_bound() && best_line != cnv->get_line())
								{
									continue;
								}
							}
							else
							{
								convoihandle_t best_convoy = get_preferred_convoy(tmp.get_zwischenziel(), tmp.get_besch()->get_catg_index());
								if(best_convoy.is_bound() && best_convoy.get_rep() != cnv)
								{
									continue;
								}
							}
						}
#endif
					}					

					// compatible car and right target stop?
					if(  tmp.get_zwischenziel() == plan_halt  ) 
					{
						// Overcrowding is now dealt with elsewhere. This method creates deadlocks.
						
						/*if(  plan_halt->is_overcrowded(wtyp->get_catg_index())  ) 
						{
							if(  welt->get_einstellungen()->is_avoid_overcrowding()  &&  !(tmp.get_ziel()==plan_halt)  ) 
							{
								// do not go for transfer to overcrowded transfer stop
								continue;
							}
						}*/

						// not too much?
						ware_t neu(tmp);
						if(  tmp.menge > maxi  ) 
						{
							// not all can be loaded
							neu.menge = maxi;
							tmp.menge -= maxi;
						}
						else 
						{
							// leave an empty entry => joining will more often work
							tmp.menge = 0;
						}
				
						book(neu.menge, HALT_DEPARTED);
						uint16 waiting_minutes = get_waiting_minutes(welt->get_zeit_ms() - neu.arrival_time);
						if(waiting_minutes == 0 && welt->get_zeit_ms() != neu.arrival_time)
						{ 
							/*const sint64 TEST_1 = welt->get_zeit_ms();
							const sint64 TEST_2 = neu.arrival_time;
							const sint64 DIFFERENCE = TEST_1 - TEST_2;
							uint8 a = 1 + 1;*/

							waiting_minutes = 1;
						}
						if(waiting_minutes > 0)
						{
							add_waiting_time(waiting_minutes, neu.get_zwischenziel(), neu.get_besch()->get_catg_index());
						}
						resort_freight_info = true;
						return neu;
					}
				}

				// nothing there to load
			}
		}
	}

	// empty quantity of required type -> no effect
	return ware_t (wtyp);
}

inline uint16 haltestelle_t::get_waiting_minutes(uint32 waiting_ticks) const
{
	// Waiting time is reduced (2* ...) instead of (3 * ...) because, in real life, people
	// can organise their journies according to timetables, so waiting is more efficient.

	// Note: waiting times now in *tenths* of minutes (hence difference in arithmetic)
	//uint16 test_minutes_1 = ((float)1 / (1 / (waiting_ticks / 4096.0) * 20) * welt->get_einstellungen()->get_journey_time_multiplier() * 600);
	//uint16 test_minutes_2 = (2 * welt->get_einstellungen()->get_journey_time_multiplier() * waiting_ticks) / 409.6;
	return (2 * welt->get_einstellungen()->get_journey_time_multiplier() * waiting_ticks) / 409.6F;
	//const uint32 value = (2 * welt->get_einstellungen()->get_journey_time_multiplier() * waiting_ticks) / 409.6F;
	//return value <= 65535 ? value : 65535;

	//Old method (both are functionally equivalent, except for reduction in time. Would be fully equivalent if above was 3 * ...):
	//return ((float)1 / (1 / (waiting_ticks / 4096.0) * 20) * welt->get_einstellungen()->get_journey_time_multiplier() * 60);
}

uint32 haltestelle_t::get_ware_summe(const ware_besch_t *wtyp) const
{
	int sum = 0;
	vector_tpl<ware_t> * warray = waren[wtyp->get_catg_index()];
	if(warray!=NULL) {
		for(unsigned i=0;  i<warray->get_count();  i++ ) {
			if(wtyp->get_index()==(*warray)[i].get_index()) {
				sum += (*warray)[i].menge;
			}
		}
	}
	return sum;
}



uint32 haltestelle_t::get_ware_fuer_zielpos(const ware_besch_t *wtyp, const koord zielpos) const
{
	vector_tpl<ware_t> * warray = waren[wtyp->get_catg_index()];
	if(warray!=NULL) {
		for(unsigned i=0;  i<warray->get_count();  i++ ) {
			ware_t &ware = (*warray)[i];
			if(wtyp->get_index()==ware.get_index()  &&  ware.get_zielpos()==zielpos) {
				return ware.menge;
			}
		}
	}
	return 0;
}



uint32 haltestelle_t::get_ware_fuer_zwischenziel(const ware_besch_t *wtyp, const halthandle_t zwischenziel) const
{
	uint32 sum = 0;
	vector_tpl<ware_t> * warray = waren[wtyp->get_catg_index()];
	if(warray!=NULL) {
		for(unsigned i=0;  i<warray->get_count();  i++ ) {
			ware_t &ware = (*warray)[i];
			if(wtyp->get_index()==ware.get_index()  &&  ware.get_zwischenziel()==zwischenziel) {
				sum += ware.menge;
			}
		}
	}
	return sum;
}



/**
 * @returns the sum of all waiting goods (100t coal + 10
 * passengers + 2000 liter oil = 2110)
 * @author Markus Weber
 */
uint32 haltestelle_t::sum_all_waiting_goods() const      //15-Feb-2002    Markus Weber    Added
{
	uint32 sum = 0;

	for(unsigned i=0; i<warenbauer_t::get_max_catg_index(); i++) {
		if(waren[i]) {
			for( unsigned j=0;  j<waren[i]->get_count();  j++  ) {
				sum += (*(waren[i]))[j].menge;
			}
		}
	}
	return sum;
}



bool haltestelle_t::vereinige_waren(const ware_t &ware) //"unite were" (Google)
{
	// pruefen ob die ware mit bereits wartender ware vereinigt werden kann
	// "examine whether the ware with software already waiting to be united" (Google)

	vector_tpl<ware_t> * warray = waren[ware.get_besch()->get_catg_index()];
	if(warray != NULL) 
	{
		for(uint32 i = 0; i < warray->get_count(); i++) 
		{
			ware_t &tmp = (*warray)[i];

			/*
			* OLD SYSTEM - did not take account of origins and timings when merging.
			*
			* // es wird auf basis von Haltestellen vereinigt
			* // prissi: das ist aber ein Fehler f�r alle anderen G�ter, daher Zielkoordinaten f�r alles, was kein passagier ist ...
			*
			* //it is based on uniting stops. 
			* //prissi: but that is a mistake for all other goods, therefore, target coordinates for everything that is not a passenger ...
			* // (Google)
			*
			* if(ware.same_destination(tmp)) {
			*/

			// NEW SYSTEM
			// Adds more checks.
			// @author: jamespetts
			if(ware.can_merge_with(tmp))
			{
				if(  ware.get_zwischenziel().is_bound()  &&  ware.get_zwischenziel()!=self  ) 
				{
					// update route if there is newer route
					tmp.set_zwischenziel( ware.get_zwischenziel() );
				}

				// Merge waiting times.
				if(ware.menge > 0)
				{
					//The waiting time for ware will always be zero.
					tmp.arrival_time = welt->get_zeit_ms() - ((welt->get_zeit_ms() - tmp.arrival_time) * tmp.menge) / (tmp.menge + ware.menge);
				}

				tmp.menge += ware.menge;
				resort_freight_info = true;
				return true;
			}
		}
	}
	return false;
}



// put the ware into the internal storage
// take care of all allocation neccessary
void haltestelle_t::add_ware_to_halt(ware_t ware, bool from_saved)
{
	//@author: jamespetts
	if(!from_saved)
	{
		ware.arrival_time = welt->get_zeit_ms();
	}

	// now we have to add the ware to the stop
	vector_tpl<ware_t> * warray = waren[ware.get_besch()->get_catg_index()];
	if(warray==NULL) 
	{
		// this type was not stored here before ...
		warray = new vector_tpl<ware_t>(4);
		waren[ware.get_besch()->get_catg_index()] = warray;
	}
	// the ware will be put into the first entry with menge==0
	resort_freight_info = true;
	ITERATE_PTR(warray,i)
	{
		if((*warray)[i].menge==0) 
		{
			(*warray)[i] = ware;
			return;
		}
	}
	// here, if no free entries found
	warray->append(ware);
}



/* same as liefere an, but there will be no route calculated,
 * since it hase be calculated just before
 * (execption: route contains us as intermediate stop)
 * @author prissi
 */
uint32 haltestelle_t::starte_mit_route(ware_t ware)
{
	if(ware.get_ziel()==self) {
		if(ware.is_freight()) {
			// muss an fabrik geliefert werden
			liefere_an_fabrik(ware);
		}
		// already there: finished (may be happen with overlapping areas and returning passengers)
		return ware.menge;
	}

	// no valid next stops? Or we are the next stop?
	if(ware.get_zwischenziel() == self) 
	{
		dbg->error("haltestelle_t::starte_mit_route()","route cannot contain us as first transfer stop => recalc route!");
#ifdef NEW_PATHING
		if(find_route(ware) == 65535)
		{
#else
		if(  suche_route( ware, NULL, false )==NO_ROUTE  ) 
		{
#endif
			// no route found?
			dbg->error("haltestelle_t::starte_mit_route()","no route found!");
			return ware.menge;
		}
	}

	// passt das zu bereits wartender ware ?
	if(vereinige_waren(ware)) {
		// dann sind wir schon fertig;
		return ware.menge;
	}

	// add to internal storage
	add_ware_to_halt(ware);

	return ware.menge;
}



/* Recieves ware and tries to route it further on
 * if no route is found, it will be removed
 * @author prissi
 */
uint32 haltestelle_t::liefere_an(ware_t ware)
{
	// no valid next stops?
	if(!ware.get_ziel().is_bound()  ||  !ware.get_zwischenziel().is_bound()) 
	{
		// write a log entry and discard the goods
dbg->warning("haltestelle_t::liefere_an()","%d %s delivered to %s have no longer a route to their destination!", ware.menge, translator::translate(ware.get_name()), get_name() );
		return ware.menge;
	}

	// did we arrived?
	if(welt->lookup(ware.get_zielpos())->is_connected(self)) 
	{
		if(ware.is_freight()) 
		{
			// muss an fabrik geliefert werden
			liefere_an_fabrik(ware);
		}
		else if(ware.get_besch()==warenbauer_t::passagiere) 
		{
			// arriving passenger may create pedestrians
			if(welt->get_einstellungen()->get_show_pax())
			{
				int menge = ware.menge;
				for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); menge > 0 && i != end; ++i)
				{
					grund_t* gr = i->grund;
					menge = erzeuge_fussgaenger(welt, gr->get_pos(), menge);
				}
				INT_CHECK("simhalt 938");
			}
		}
		return ware.menge;
	}

	// do we have already something going in this direction here?
	if(  vereinige_waren(ware)  ) 
	{
		return ware.menge;
	}

#ifdef NEW_PATHING
	if(find_route(ware) == 65535)
	{
#else
	// not near enough => we need to do a rerouting
	if(  suche_route( ware, NULL, false )==NO_ROUTE  ) 
	{
#endif
		// target no longer there => delete

		INT_CHECK("simhalt 1364");

		DBG_MESSAGE("haltestelle_t::liefere_an()","%s: delivered goods (%d %s) to ??? via ??? could not be routed to their destination!",get_name(), ware.menge, translator::translate(ware.get_name()) );
		return ware.menge;
	}
#if 1
	// passt das zu bereits wartender ware ?
	if(vereinige_waren(ware)) 
	{
		// dann sind wir schon fertig;
		return ware.menge;
	}
#endif
	// add to internal storage
	add_ware_to_halt(ware);

	return ware.menge;
}



#ifdef USE_QUOTE
// rating of this place ...
const char *
haltestelle_t::quote_bezeichnung(int quote, convoihandle_t cnv) const
{
    const char *str = "unbekannt";

    if(quote < 0) {
	str = translator::translate("miserabel");
    } else if(quote < 30) {
	str = translator::translate("schlecht");
    } else if(quote < 60) {
	str = translator::translate("durchschnitt");
    } else if(quote < 90) {
	str = translator::translate("gut");
    } else if(quote < 120) {
	str = translator::translate("sehr gut");
    } else if(quote < 150) {
	str = translator::translate("bestens");
    } else if(quote < 180) {
	str = translator::translate("excellent");
    } else {
	str = translator::translate("spitze");
    }

    return str;
}
#endif



void haltestelle_t::info(cbuffer_t & buf) const
{
	char tmp [512];

	sprintf(tmp,
		translator::translate("Passengers %d %c, %d %c, %d no route"),
		pax_happy,
		30,
		pax_unhappy,
		31,
		pax_no_route
		);
	buf.append(tmp);
}


/**
 * @param buf the buffer to fill
 * @return Goods description text (buf)
 * @author Hj. Malthaner
 */
void haltestelle_t::get_freight_info(cbuffer_t & buf)
{
	if(resort_freight_info) {
		// resort only inf absolutely needed ...
		resort_freight_info = false;
		buf.clear();

		for(unsigned i=0; i<warenbauer_t::get_max_catg_index(); i++) {
			vector_tpl<ware_t> * warray = waren[i];
			if(warray) {
				freight_list_sorter_t::sort_freight(warray, buf, (freight_list_sorter_t::sort_mode_t)sortierung, NULL, "waiting");
			}
		}
	}
}



void haltestelle_t::get_short_freight_info(cbuffer_t & buf)
{
	bool got_one = false;

	for(unsigned int i=0; i<warenbauer_t::get_waren_anzahl(); i++) {
		const ware_besch_t *wtyp = warenbauer_t::get_info(i);
		if(gibt_ab(wtyp)) {

			// ignore goods with sum=zero
			const int summe=get_ware_summe(wtyp);
			if(summe>0) {

				if(got_one) {
					buf.append(", ");
				}

				buf.append(summe);
				buf.append(translator::translate(wtyp->get_mass()));
				buf.append(" ");
				buf.append(translator::translate(wtyp->get_name()));

				got_one = true;
			}
		}
	}

	if(got_one) {
		buf.append(" ");
		buf.append(translator::translate("waiting"));
		buf.append("\n");
	}
	else {
		buf.append(translator::translate("no goods waiting"));
		buf.append("\n");
	}
}



void haltestelle_t::zeige_info()
{
	create_win(new halt_info_t(welt, self), w_info, (long)this );
}



// changes this to a publix transfer exchange stop
sint64 haltestelle_t::calc_maintenance()
{
	sint64 maintenance = 0;
	for(slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		grund_t* gr = i->grund;
		gebaeude_t* gb = gr->find<gebaeude_t>();
		if(gb) {
			maintenance += welt->get_einstellungen()->maint_building*gb->get_tile()->get_besch()->get_level();
		}
	}
	return maintenance;
}


// changes this to a public transfer exchange stop
bool haltestelle_t::make_public_and_join( spieler_t *sp )
{
	spieler_t *public_owner=welt->get_spieler(1);
	sint64 total_costs = 0;
	slist_tpl<halthandle_t> joining;

	// only something to do if not yet owner ...
	if(besitzer_p!=public_owner) {
		// now recalculate maintenance
		for(slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
			grund_t* gr = i->grund;
			gebaeude_t* gb = gr->find<gebaeude_t>();
			if(gb) {
				spieler_t *gb_sp=gb->get_besitzer();
				sint64 costs = welt->get_einstellungen()->maint_building*gb->get_tile()->get_besch()->get_level();
				total_costs += costs;
				if(!sp->can_afford((total_costs*60)<<(welt->ticks_bits_per_tag-18)))
				{
					return false;
				}
				spieler_t::add_maintenance( gb_sp, -costs );
				gb->set_besitzer(public_owner);
				spieler_t::add_maintenance(public_owner, costs );
			}
			// ok, valid start, now we can join them
			for( uint8 i=0;  i<8;  i++  ) {
				const planquadrat_t *pl2 = welt->lookup(gr->get_pos().get_2d()+koord::neighbours[i]);
				if(  pl2  ) {
					halthandle_t halt = pl2->get_halt();
					if(  halt.is_bound()  &&  halt->get_besitzer()==public_owner  &&  !joining.is_contained(halt)) {
						joining.append(halt);
					}
				}
			}
		}
		// transfer ownership
		spieler_t::accounting( sp, -((total_costs*60)<<(welt->ticks_bits_per_tag-18)), get_basis_pos(), COST_CONSTRUCTION);
		besitzer_p->halt_remove(self);
		besitzer_p = public_owner;
		public_owner->halt_add(self);
	}

	while(!joining.empty()) {
		// join this halt with me
		halthandle_t halt = joining.remove_first();

		// now with the second stop
		while(halt.is_bound()  &&  halt!=self) {
			// we always take the first remaining tile and transfer it => more safe
			koord3d t = halt->get_basis_pos3d();
			grund_t *gr = welt->lookup(t);
			gebaeude_t* gb = gr->find<gebaeude_t>();
			if(gb) {
				// there are also water tiles, which may not have a buidling
				spieler_t *gb_sp=gb->get_besitzer();
				if(public_owner!=gb_sp) {
					spieler_t *gb_sp=gb->get_besitzer();
					sint64 costs = welt->get_einstellungen()->maint_building*gb->get_tile()->get_besch()->get_level();
					spieler_t::add_maintenance( gb_sp, -costs );
					spieler_t::accounting(gb_sp, -((costs*60)<<(welt->ticks_bits_per_tag-18)), gr->get_pos().get_2d(), COST_CONSTRUCTION);
					gb->set_besitzer(public_owner);
					spieler_t::add_maintenance(public_owner, costs );
				}
			}
			// transfer tiles to us
			halt->rem_grund(gr);
			add_grund(gr);
			// and check for existence
			if(!halt->existiert_in_welt()) {
				destroy(halt);
			}
		}
	}

	recalc_station_type();
	return true;
}



/*
 * recalculated the station type(s)
 * since it iterates over all ground, this is better not done too often, because line management and station list
 * queries this information regularely; Thus, we do this, when adding new ground
 * This recalculates also the capacity from the building levels ...
 * @author Weber/prissi
 */
void haltestelle_t::recalc_station_type()
{
	int new_station_type = 0;
	capacity[0] = 0;
	capacity[1] = 0;
	capacity[2] = 0;
	enables &= CROWDED;	// clear flags

	// iterate over all tiles
	for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		grund_t* gr = i->grund;
		const gebaeude_t* gb = gr->find<gebaeude_t>();
		const haus_besch_t *besch=gb?gb->get_tile()->get_besch():NULL;

		if(gr->ist_wasser()) {
			// may happend around oil rigs and so on
			new_station_type |= dock;
			// for water factories
			if(besch) {
				// enabled the matching types
				enables |= besch->get_enabled();
				if(  welt->get_einstellungen()->is_seperate_halt_capacities()  ) {
					if(besch->get_enabled()&1) {
						capacity[0] += besch->get_level()*32;
					}
					if(besch->get_enabled()&2) {
						capacity[1] += besch->get_level()*32;
					}
					if(besch->get_enabled()&4) {
						capacity[2] += besch->get_level()*32;
					}
				}
				else {
					// no sperate capacities: sum up all
					capacity[0] += besch->get_level()*32;
					capacity[2] = capacity[1] = capacity[0];
				}
			}
			continue;
		}

		if(besch==NULL) {
			// no besch, but solid gound?!?
			dbg->error("haltestelle_t::get_station_type()","ground belongs to halt but no besch?");
			continue;
		}
//if(besch) DBG_DEBUG("haltestelle_t::get_station_type()","besch(%p)=%s",besch,besch->get_name());

		// there is only one loading bay ...
		switch (besch->get_utyp()) {
			case haus_besch_t::ladebucht:    new_station_type |= loadingbay;   break;
			case haus_besch_t::hafen:
			case haus_besch_t::binnenhafen:  new_station_type |= dock;         break;
			case haus_besch_t::bushalt:      new_station_type |= busstop;      break;
			case haus_besch_t::airport:      new_station_type |= airstop;      break;
			case haus_besch_t::monorailstop: new_station_type |= monorailstop; break;

			case haus_besch_t::bahnhof:
				if (gr->hat_weg(monorail_wt)) {
					new_station_type |= monorailstop;
				} else {
					new_station_type |= railstation;
				}
				break;

			case haus_besch_t::generic_stop:
				switch (besch->get_extra()) {
					case road_wt:
						new_station_type |= (besch->get_enabled()&3)!=0 ? busstop : loadingbay;
						break;
					case water_wt:       new_station_type |= dock;            break;
					case air_wt:         new_station_type |= airstop;         break;
					case monorail_wt:    new_station_type |= monorailstop;    break;
					case track_wt:       new_station_type |= railstation;     break;
					case tram_wt:        new_station_type |= tramstop;        break;
					case maglev_wt:      new_station_type |= maglevstop;      break;
					case narrowgauge_wt: new_station_type |= narrowgaugestop; break;
				}
				break;
			default: break;
		}


		// enabled the matching types
		enables |= besch->get_enabled();
		if(  welt->get_einstellungen()->is_seperate_halt_capacities()  ) {
			if(besch->get_enabled()&1) {
				capacity[0] += besch->get_level()*32;
			}
			if(besch->get_enabled()&2) {
				capacity[1] += besch->get_level()*32;
			}
			if(besch->get_enabled()&4) {
				capacity[2] += besch->get_level()*32;
			}
		}
		else {
			// no sperate capacities: sum up all
			capacity[0] += besch->get_level()*32;
			capacity[2] = capacity[1] = capacity[0];
		}
	}
	station_type = (haltestelle_t::stationtyp)new_station_type;

//DBG_DEBUG("haltestelle_t::recalc_station_type()","result=%x, capacity[0]=%i, capacity[1], capacity[2]",new_station_type,capacity[0],capacity[1],capacity[2]);
}



int haltestelle_t::erzeuge_fussgaenger(karte_t *welt, koord3d pos, int anzahl)
{
	fussgaenger_t::erzeuge_fussgaenger_an(welt, pos, anzahl);
	for(int i=0; i<4 && anzahl>0; i++) {
		fussgaenger_t::erzeuge_fussgaenger_an(welt, pos+koord::nsow[i], anzahl);
	}
	return anzahl;
}



void haltestelle_t::rdwr(loadsave_t *file)
{
	xml_tag_t h( file, "haltestelle_t" );

	sint32 spieler_n;
	koord3d k;

	if(file->is_saving()) {
		spieler_n = welt->sp2num( besitzer_p );
	}

	if(file->get_version()<99008) {
		init_pos.rdwr( file );
	}
	file->rdwr_long(spieler_n, "\n");

	if(file->get_version()<=88005) {
		bool dummy;
		file->rdwr_bool(dummy, " "); // pax
		file->rdwr_bool(dummy, " "); // post
		file->rdwr_bool(dummy, "\n");	// ware
	}

	if(file->is_loading()) {
		besitzer_p = welt->get_spieler(spieler_n);
		k.rdwr( file );
		slist_tpl <grund_t *>grund_list;
		while(k!=koord3d::invalid) {
			grund_t *gr = welt->lookup(k);
			if(!gr) {
				dbg->error("haltestelle_t::rdwr()", "invalid position %s", k.get_str() );
				gr = welt->lookup(k.get_2d())->get_kartenboden();
				dbg->error("haltestelle_t::rdwr()", "setting to %s", gr->get_pos().get_str() );
			}
			// during loading and saving halts will be referred by their base postion
			// so we may alrady be defined ...
			if(gr->get_halt().is_bound()) {
				dbg->warning( "haltestelle_t::rdwr()", "bound to ground twice at (%i,%i)!", k.x, k.y );
			}
			// prissi: now check, if there is a building -> we allow no longer ground without building!
			const gebaeude_t* gb = gr->find<gebaeude_t>();
			const haus_besch_t *besch=gb?gb->get_tile()->get_besch():NULL;
			if(besch) {
				add_grund( gr );
			}
			else {
				dbg->warning("haltestelle_t::rdwr()", "will no longer add ground without building at %s!", k.get_str() );
			}
			k.rdwr( file );
		}
	} else {
		for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
			k = i->grund->get_pos();
			k.rdwr( file );
		}
		k = koord3d::invalid;
		k.rdwr( file );
	}

	short count;
	const char *s;
	init_pos = tiles.empty() ? koord::invalid : tiles.front().grund->get_pos().get_2d();
	if(file->is_saving()) {
		for(unsigned i=0; i<warenbauer_t::get_max_catg_index(); i++) {
			vector_tpl<ware_t> *warray = waren[i];
			if(warray) {
				s = "y";	// needs to be non-empty
				file->rdwr_str(s);
				count = warray->get_count();
				file->rdwr_short(count, " ");
				for(unsigned i=0;  i<warray->get_count();  i++ ) {
					ware_t &ware = (*warray)[i];
					ware.rdwr(welt,file);
				}
			}
		}
		s = "";
		file->rdwr_str(s);

	}
	else 
	{
		// restoring all goods in the station
		char s[256];
		file->rdwr_str(s,256);
		while(*s) 
		{
			file->rdwr_short(count, " ");
			if(count > 0) 
			{
				for(int i = 0; i < count; i++) 
				{
					// add to internal storage (use this function, since the old categories were different)
					ware_t ware(welt, file);
					if(ware.menge > 0) 
					{
						add_ware_to_halt(ware, file->get_experimental_version() >= 2);
					}
				}
			}
			file->rdwr_str(s,256);
		}

		// old games save the list with stations
		// however, we have to rebuilt them anyway for the new format
		if(file->get_version()<99013) 
		{
			file->rdwr_short(count, " ");

			for(int i=0; i<count; i++) 
			{
#ifndef NEW_PATHING
				warenziel_t wz (file);
#else
				if(file->is_loading())
				{
					// Dummy loading and saving to maintain backwards compatibility
					koord dummy_koord;
					dummy_koord.rdwr(file);

					char dummy[256];
					file->rdwr_str(dummy,256);
				}
#endif
			}
			
		}

	}

	for (int j = 0; j<MAX_HALT_COST; j++) {
		for (int k = MAX_MONTHS-1; k>=0; k--) {
			file->rdwr_longlong(financial_history[k][j], " ");
		}
	}

	if(file->get_experimental_version() >= 2)
	{
		for(uint8 i = 0; i < warenbauer_t::get_max_catg_index(); i ++)
		{
			if(file->is_saving())
			{
				uint16 halts_count;
				halts_count = waiting_times[i].get_count();
				file->rdwr_short(halts_count, "");
			
				koordhashtable_iterator_tpl<koord, fixed_list_tpl<uint16, 16> > iter(waiting_times[i]);

				while(iter.next())
				{
					koord save_koord = iter.get_current_key();
					save_koord.rdwr(file);
					uint8 waiting_time_count = iter.get_current_value().get_count();
					file->rdwr_byte(waiting_time_count, "");
					ITERATE(iter.get_current_value(),i)
					{
						// Store each waiting time
						uint16 current_time = iter.access_current_value().get_element(i);
						file->rdwr_short(current_time, "");
					}
				}
			}
			else
			{
				uint16 halts_count;
				file->rdwr_short(halts_count, "");
				for(uint16 k = 0; k < halts_count; k ++)
				{
					koord halt_position;
					halt_position.rdwr(file);
					if(halt_position != koord::invalid)
					{
						fixed_list_tpl<uint16, 16> list;
						uint8 waiting_time_count;
						file->rdwr_byte(waiting_time_count, "");
						for(uint8 j = 0; j < waiting_time_count; j ++)
						{
							uint16 current_time;
							file->rdwr_short(current_time, "");
							list.add_to_tail(current_time);
						}
						waiting_times[i].put(halt_position, list);
					}
					else
					{
						// The list was not properly saved.
						uint8 waiting_time_count;
						file->rdwr_byte(waiting_time_count, "");
						for(uint8 j = 0; j < waiting_time_count; j ++)
						{
							uint16 current_time;
							file->rdwr_short(current_time, "");
						}
					}
				}
			}
		}
	}

	if(file->get_experimental_version() >= 2)
	{
		if(file->get_experimental_version() < 3)
		{
			uint16 old_paths_timestamp = 0;
			uint16 old_connexions_timestamp = 0;
			bool old_reschedule = false;
			file->rdwr_short(old_paths_timestamp, "");
			file->rdwr_short(old_connexions_timestamp, "");
			file->rdwr_bool(old_reschedule, "");
			for(uint8 i = 0; i < warenbauer_t::get_max_catg_index(); i ++)
			{
				paths_timestamp[i] = old_paths_timestamp;
				connexions_timestamp[i] = old_connexions_timestamp;
				reschedule[i] = old_reschedule;
			}
		}
		else
		{
			for(uint8 i = 0; i < warenbauer_t::get_max_catg_index(); i ++)
			{
				file->rdwr_short(paths_timestamp[i], "");
				file->rdwr_short(connexions_timestamp[i], "");
				file->rdwr_bool(reschedule[i], "");
			}
		}
	}
}



//"Load lock" (Google)
void haltestelle_t::laden_abschliessen()
{
	if(besitzer_p==NULL) 
	{
		return;
	}

	// fix good destination coordinates
	for(unsigned i=0; i<warenbauer_t::get_max_catg_index(); i++) 
	{
		if(waren[i]) 
		{
			vector_tpl<ware_t> * warray = waren[i];
			ITERATE_PTR(warray,j) 
			{
				(*warray)[j].laden_abschliessen(welt);
			}
			// merge identical entries (should only happen with old games)
			ITERATE_PTR(warray,j)
			{
				if(  (*warray)[j].menge==0  ) 
				{
					continue;
				}
				for(unsigned k=j+1; k<warray->get_count(); k++) 
				{
					if(  (*warray)[k].menge > 0  &&  (*warray)[j].can_merge_with( (*warray)[k] )  ) 
					{
						(*warray)[j].menge += (*warray)[k].menge;
						(*warray)[k].menge = 0;
					}
				}
			}
		}
	}

	// what kind of station here?
	recalc_station_type();
#ifdef LAGER_NOT_IN_USE
	for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		koord3d k(i->grund->get_pos());
		// nach sondergebaeuden suchen

		lagerhaus_t* l = welt->lookup(k)->find<lagerhaus_t>();
		if  (l != NULL) {
			lager = l;
		break;
		}
	}
#endif

	// handle name for old stations which don't exist in kartenboden
	grund_t* bd = welt->lookup(get_basis_pos3d());
	if(bd!=NULL  &&  !bd->get_flag(grund_t::has_text) ) {
		// restore label und bridges
		grund_t* bd_old = welt->lookup_kartenboden(get_basis_pos());
		if(bd_old) {
			// transfer name (if there)
			const char *name = bd->get_text();
			if(name) {
				set_name( name );
				bd_old->set_text( NULL );
			}
			else {
				set_name( "Unknown" );
			}
		}
	}
	else {
		if(!all_names.put(bd->get_text(),self)) {
			DBG_MESSAGE("haltestelle_t::set_name()","name %s already used!",bd->get_text());
		}
	}
	recalc_status();
}



void haltestelle_t::book(sint64 amount, int cost_type)
{
	assert(cost_type <= MAX_HALT_COST);
	financial_history[0][cost_type] += amount;
	recalc_status();
}



void haltestelle_t::init_financial_history()
{
	for (int j = 0; j<MAX_HALT_COST; j++)
	{
		for (int k = MAX_MONTHS-1; k>=0; k--)
		{
			financial_history[k][j] = 0;
		}
	}
	financial_history[0][HALT_HAPPY] = pax_happy;
	financial_history[0][HALT_UNHAPPY] = pax_unhappy;
	financial_history[0][HALT_NOROUTE] = pax_no_route;
}



/**
 * Calculates a status color for status bars
 * @author Hj. Malthaner
 */
void haltestelle_t::recalc_status()
{
	status_color = financial_history[0][HALT_CONVOIS_ARRIVED] > 0 ? COL_GREEN : COL_YELLOW;

	// since the status is ored ...
	uint8 status_bits = 0;

	memset( overcrowded, 0, 8 );

	uint32 total_sum = 0;
	if(get_pax_enabled()) {
		const uint32 max_ware = get_capacity(0);
		total_sum += get_ware_summe(warenbauer_t::passagiere);
		if(total_sum>max_ware) {
			overcrowded[0] |= 1;
		}
		if(get_pax_unhappy() > 40 ) {
			status_bits = (total_sum>max_ware+200 || get_pax_unhappy()>200) ? 2 : 1;
		} else if(total_sum>max_ware) {
			status_bits = total_sum>max_ware+200 ? 2 : 1;
		}
	}

	if(get_post_enabled()) {
		const uint32 max_ware = get_capacity(1);
		const uint32 post = get_ware_summe(warenbauer_t::post);
		total_sum += post;
		if(post>max_ware) {
			status_bits |= post>max_ware+200 ? 2 : 1;
			overcrowded[0] |= 2;
		}
	}

	// now for all goods
	if(status_color!=COL_RED  &&  get_ware_enabled()) {
		const int count = warenbauer_t::get_waren_anzahl();
		const uint32 max_ware = get_capacity(2);
		for( int i=2; i+1<count; i++) {
			const ware_besch_t *wtyp = warenbauer_t::get_info(i+1);
			const uint32 ware_sum = get_ware_summe(wtyp);
			total_sum += ware_sum;
			if(ware_sum>max_ware) {
				status_bits |= (ware_sum>max_ware+32  ||  CROWDED) ? 2 : 1;
				overcrowded[wtyp->get_catg_index()/8] |= 1<<(wtyp->get_catg_index()%8);
			}
		}
	}

	// take the worst color for status
	if(  status_bits  ) {
		status_color = status_bits&2 ? COL_RED : COL_ORANGE;
	}
	else {
		status_color = (financial_history[0][HALT_WAITING]+financial_history[0][HALT_DEPARTED] == 0) ? COL_YELLOW : COL_GREEN;
	}

	financial_history[0][HALT_WAITING] = total_sum;
}



/**
 * Draws some nice colored bars giving some status information
 * @author Hj. Malthaner
 */
void haltestelle_t::display_status(sint16 xpos, sint16 ypos) const
{
	// ignore freight that cannot reach to this station
	sint16 count = 0;
	for( unsigned i=0;  i<warenbauer_t::get_waren_anzahl(); i++) {
		if(i==2) continue;	// ignore freight none
		if(gibt_ab(warenbauer_t::get_info(i))) {
			count ++;
		}
	}

	ypos -= 11;
	xpos -= (count*4 - get_tile_raster_width())/2;
	sint16 x = xpos;
	uint32 max_capacity;

	for( unsigned i=0;  i<warenbauer_t::get_waren_anzahl(); i++) {
		if(i==2) continue;	// ignore freight none
		const ware_besch_t *wtyp = warenbauer_t::get_info(i);
		if(gibt_ab(wtyp)) {
			if(i<2) {
				max_capacity = get_capacity(i);
			}
			else {
				max_capacity = get_capacity(2);
			}
			const uint32 sum = get_ware_summe(wtyp);
			uint32 v = min(sum, max_capacity);
			if(max_capacity>512) {
				v = 2+(v*128)/max_capacity;
			}
			else {
				v = (v/4)+2;
			}

			display_fillbox_wh_clip(xpos, ypos-v-1, 1, v, COL_GREY4, true);
			display_fillbox_wh_clip(xpos+1, ypos-v-1, 2, v, wtyp->get_color(), true);
			display_fillbox_wh_clip(xpos+3, ypos-v-1, 1, v, COL_GREY1, true);

			// Hajo: show up arrow for capped values
			if(sum > max_capacity) {
				display_fillbox_wh_clip(xpos+1, ypos-v-6, 2, 4, COL_WHITE, true);
				display_fillbox_wh_clip(xpos, ypos-v-5, 4, 1, COL_WHITE, true);
			}

			xpos += 4;
		}
	}

	// status color box below
	display_fillbox_wh_clip(x-1-4, ypos, count*4+12-2, 4, get_status_farbe(), true);
}



bool haltestelle_t::add_grund(grund_t *gr)
{
	assert(gr!=NULL);

	// neu halt?
	if (tiles.is_contained(gr)) {
		return false;
	}

	koord pos=gr->get_pos().get_2d();
	gr->set_halt(self);
	tiles.append(gr);

	// appends this to the ground
	// after that, the surrounding ground will know of this station
	int cov = welt->get_einstellungen()->get_station_coverage();
	for (int y = -cov; y <= cov; y++) {
		for (int x = -cov; x <= cov; x++) {
			koord p=pos+koord(x,y);
			if(welt->ist_in_kartengrenzen(p)) {
				welt->access(p)->add_to_haltlist( self );
				welt->lookup(p)->get_kartenboden()->set_flag(grund_t::dirty);
			}
		}
	}
	welt->access(pos)->set_halt(self);

	//DBG_MESSAGE("haltestelle_t::add_grund()","pos %i,%i,%i to %s added.",pos.x,pos.y,pos.z,get_name());

	vector_tpl<fabrik_t*>& fablist = fabrik_t::sind_da_welche(welt, pos - koord(cov, cov), pos + koord(cov, cov));
	for(unsigned i=0; i<fablist.get_count(); i++) {
		fabrik_t* fab = fablist[i];
		if(!fab_list.is_contained(fab)) {
			fab_list.insert(fab);
			fab->link_halt(self);
		}
	}

	// check, if we have to add a line to this coordinate
	vector_tpl<linehandle_t> check_line(0);
	if(get_besitzer()==welt->get_spieler(1)) {
		// must iterate over all players lines ...
		for(  int i=0;  i<MAX_PLAYER_COUNT;  i++  ) {
			if(welt->get_spieler(i)) {
				welt->get_spieler(i)->simlinemgmt.get_lines(simline_t::line, &check_line);
				for(  uint j=0;  j<check_line.get_count();  j++  ) {
					// only add unknow lines
					if(  !registered_lines.is_contained(check_line[j])  ) {
						const schedule_t *fpl = check_line[j]->get_schedule();
						for(  int k=0;  k<fpl->get_count();  k++  ) {
							if(get_halt(welt,fpl->eintrag[k].pos,check_line[j]->get_besitzer())==self) {
								registered_lines.append(check_line[j]);
								break;
							}
						}
					}
				}
			}
		}
	}
	else {
		get_besitzer()->simlinemgmt.get_lines(simline_t::line, &check_line);
		for(  uint32 j=0;  j<check_line.get_count();  j++  ) {
			// only add unknow lines
			if(  !registered_lines.is_contained(check_line[j])  ) {
				const schedule_t *fpl = check_line[j]->get_schedule();
				for(  int k=0;  k<fpl->get_count();  k++  ) {
					if(get_halt(welt,fpl->eintrag[k].pos,get_besitzer())==self) {
						registered_lines.append(check_line[j]);
						break;
					}
				}
			}
		}
	}

	assert(welt->lookup(pos)->get_halt() == self  &&  gr->is_halt());
	init_pos = tiles.front().grund->get_pos().get_2d();
	return true;
}



void haltestelle_t::rem_grund(grund_t *gr)
{
	// namen merken
	if(gr) {
		slist_tpl<tile_t>::iterator i = std::find(tiles.begin(), tiles.end(), gr);
		if (i == tiles.end()) {
			// was not part of station => do nothing
			dbg->error("haltestelle_t::rem_grund()","removed illegal ground from halt");
			return;
		}

		// first tile => remove name from this tile ...
		char buf[256];
		const char* station_name_to_transfer = NULL;
		if (i == tiles.begin()  &&  (*i).grund->get_name()) {
			tstrncpy(buf, get_name(), lengthof(buf));
			station_name_to_transfer = buf;
			set_name(NULL);
		}

		// now remove tile from list
		tiles.erase(i);
		init_pos = tiles.empty() ? koord::invalid : tiles.front().grund->get_pos().get_2d();

		// re-add name
		if (station_name_to_transfer != NULL  &&  !tiles.empty()) {
			label_t *lb = tiles.front().grund->find<label_t>();
			if(lb) {
				delete lb;
			}
			set_name( station_name_to_transfer );
		}

		planquadrat_t *pl = welt->access( gr->get_pos().get_2d() );
		if(pl) {
			// no longer connected (upper level)
			gr->set_halt(halthandle_t());
			// still connected elsewhere?
			for(unsigned i=0;  i<pl->get_boden_count();  i++  ) {
				if(pl->get_boden_bei(i)->get_halt()==self) {
					// still connected with other ground => do not remove from plan ...
					DBG_DEBUG("haltestelle_t::rem_grund()", "keep floor, count=%i", tiles.get_count());
					return;
				}
			}
			DBG_DEBUG("haltestelle_t::rem_grund()", "remove also floor, count=%i", tiles.get_count());
			// otherwise remove from plan ...
			pl->set_halt(halthandle_t());
			pl->get_kartenboden()->set_flag(grund_t::dirty);
		}

		int cov = welt->get_einstellungen()->get_station_coverage();
		for (int y = -cov; y <= cov; y++) {
			for (int x = -cov; x <= cov; x++) {
				planquadrat_t *pl = welt->access( gr->get_pos().get_2d()+koord(x,y) );
				if(pl) {
					pl->remove_from_haltlist(welt,self);
					pl->get_kartenboden()->set_flag(grund_t::dirty);
				}
			}
		}

		// factory reach may have been changed ...
		verbinde_fabriken();

		// remove lines eventually
		for(  int j=registered_lines.get_count()-1;  j>=0;  j--  ) {
			const schedule_t *fpl = registered_lines[j]->get_schedule();
			bool ok=false;
			for(  int k=0;  k<fpl->get_count();  k++  ) {
				if(get_halt(welt,fpl->eintrag[k].pos,registered_lines[j]->get_besitzer())==self) {
					ok = true;
					break;
				}
			}
			// need removal?
			if(!ok) {
				registered_lines.remove_at(j);
			}
		}

	}
}



bool haltestelle_t::existiert_in_welt()
{
	return !tiles.empty();
}


/* return the closest square that belongs to this halt
 * @author prissi
 */
koord haltestelle_t::get_next_pos( koord start ) const
{
	koord find = koord::invalid;

	if (!tiles.empty()) {
		// find the closest one
		int	dist = 0x7FFF;
		for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
			koord p = i->grund->get_pos().get_2d();
			int d = abs_distance(start, p );
			if(d<dist) {
				// ok, this one is closer
				dist = d;
				find = p;
			}
		}
	}
	return find;
}



/* marks a coverage area
 * @author prissi
 */
void haltestelle_t::mark_unmark_coverage(const bool mark) const
{
	// iterate over all tiles
	for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		koord size( welt->get_einstellungen()->get_station_coverage()*2+1, welt->get_einstellungen()->get_station_coverage()*2+1);
		welt->mark_area( i->grund->get_pos()-size/2, size, mark );
	}
}



/* Find a tile where this type of vehicle could stop
 * @author prissi
 */
const grund_t *haltestelle_t::find_matching_position(const waytype_t w) const
{
	// iterate over all tiles
	for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		if(i->grund->hat_weg(w)) {
			return i->grund;
		}
	}
	return NULL;
}



/* checks, if there is an unoccupied loading bay for this kind of thing
 * @author prissi
 */
bool haltestelle_t::find_free_position(const waytype_t w,convoihandle_t cnv,const ding_t::typ d) const
{
	// iterate over all tiles
	for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		if (i->reservation == cnv || !i->reservation.is_bound()) {
			// not reseved
			grund_t* gr = i->grund;
			assert(gr);
			// found a stop for this waytype but without object d ...
			if(gr->hat_weg(w)  &&  gr->suche_obj(d)==NULL) {
				// not occipied
				return true;
			}
		}
	}
	return false;
}



/* reserves a position (caution: railblocks work differently!
 * @author prissi
 */
bool haltestelle_t::reserve_position(grund_t *gr,convoihandle_t cnv)
{
	slist_tpl<tile_t>::iterator i = std::find(tiles.begin(), tiles.end(), gr);
	if (i != tiles.end()) {
		if (i->reservation == cnv) {
//DBG_MESSAGE("haltestelle_t::reserve_position()","gr=%d,%d already reserved by cnv=%d",gr->get_pos().x,gr->get_pos().y,cnv.get_id());
			return true;
		}
		// not reseved
		if (!i->reservation.is_bound()) {
			grund_t* gr = i->grund;
			if(gr) {
				// found a stop for this waytype but without object d ...
				if(gr->hat_weg(cnv->get_vehikel(0)->get_waytype())  &&  gr->suche_obj(cnv->get_vehikel(0)->get_typ())==NULL) {
					// not occipied
//DBG_MESSAGE("haltestelle_t::reserve_position()","sucess for gr=%i,%i cnv=%d",gr->get_pos().x,gr->get_pos().y,cnv.get_id());
					i->reservation = cnv;
					return true;
				}
			}
		}
	}
//DBG_MESSAGE("haltestelle_t::reserve_position()","failed for gr=%i,%i, cnv=%d",gr->get_pos().x,gr->get_pos().y,cnv.get_id());
	return false;
}



/* frees a reserved  position (caution: railblocks work differently!
 * @author prissi
 */
bool haltestelle_t::unreserve_position(grund_t *gr, convoihandle_t cnv)
{
	slist_tpl<tile_t>::iterator i = std::find(tiles.begin(), tiles.end(), gr);
	if (i != tiles.end()) {
		if (i->reservation == cnv) {
			i->reservation = convoihandle_t();
			return true;
		}
	}
DBG_MESSAGE("haltestelle_t::unreserve_position()","failed for gr=%p",gr);
	return false;
}



/* can a convoi reserve this position?
 * @author prissi
 */
bool haltestelle_t::is_reservable(const grund_t *gr, convoihandle_t cnv) const
{
	for (slist_tpl<tile_t>::const_iterator i = tiles.begin(), end = tiles.end(); i != end; ++i) {
		if(gr==i->grund) {
			if (i->reservation == cnv) {
DBG_MESSAGE("haltestelle_t::is_reservable()","gr=%d,%d already reserved by cnv=%d",gr->get_pos().x,gr->get_pos().y,cnv.get_id());
				return true;
			}
			// not reseved
			if (!i->reservation.is_bound()) {
				// found a stop for this waytype but without object d ...
				if(gr->hat_weg(cnv->get_vehikel(0)->get_waytype())  &&  gr->suche_obj(cnv->get_vehikel(0)->get_typ())==NULL) {
					// not occipied
					return true;
				}
			}
			return false;
		}
	}
DBG_MESSAGE("haltestelle_t::reserve_position()","failed for gr=%i,%i, cnv=%d",gr->get_pos().x,gr->get_pos().y,cnv.get_id());
	return false;
}
