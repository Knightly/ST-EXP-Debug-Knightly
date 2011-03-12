/*
 * Copyright (c) 1997 - 2001 Hansjörg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef simfab_h
#define simfab_h

#include "dataobj/koord3d.h"
#include "dataobj/translator.h"

#include "tpl/slist_tpl.h"
#include "tpl/vector_tpl.h"
#include "besch/fabrik_besch.h"
#include "halthandle_t.h"
#include "simworld.h"


class spieler_t;
class stadt_t;


/**
 * Factory statistics
 * @author Knightly
 */
#define MAX_MONTH                  (12)
#define FAB_PRODUCTION              (0)
#define FAB_POWER                   (1)
#define FAB_BOOST_ELECTRIC          (2)
#define FAB_BOOST_PAX               (3)
#define FAB_BOOST_MAIL              (4)
#define FAB_PAX_DEPARTED            (5)
#define FAB_PAX_ARRIVED             (6)
#define FAB_PAX_AVERAGE             (7)
#define FAB_MAIL_DEPARTED           (8)
#define FAB_MAIL_ARRIVED            (9)
#define FAB_MAIL_AVERAGE           (10)
#define MAX_FAB_STAT               (11)

// reference lines
#define FAB_REF_MAX_BOOST_ELECTRIC  (0)
#define FAB_REF_MAX_BOOST_PAX       (1)
#define FAB_REF_MAX_BOOST_MAIL      (2)
#define FAB_REF_DEMAND_ELECTRIC     (3)
#define FAB_REF_DEMAND_PAX          (4)
#define FAB_REF_DEMAND_MAIL         (5)
#define MAX_FAB_REF_LINE            (6)

// statistics for goods
#define MAX_FAB_GOODS_STAT          (3)
// common to both input and output goods
#define FAB_GOODS_STORAGE           (0)
// input goods
#define FAB_GOODS_RECEIVED          (1)
#define FAB_GOODS_CONSUMED          (2)
// output goods
#define FAB_GOODS_DELIVERED         (1)
#define FAB_GOODS_PRODUCED          (2)


// production happens in every second
#define PRODUCTION_DELTA_T (1024)
// default production factor
#define DEFAULT_PRODUCTION_FACTOR_BITS (8)
#define DEFAULT_PRODUCTION_FACTOR (1 << DEFAULT_PRODUCTION_FACTOR_BITS)
// precision of apportioned demand (i.e. weights of factories at target cities)
#define DEMAND_BITS (4)


// to prepare for 64 precision ...
class ware_production_t
{
private:
	const ware_besch_t *type;
	// Knightly : statistics for each goods
	sint64 statistics[MAX_MONTH][MAX_FAB_GOODS_STAT];
	sint64 weighted_sum_storage;
public:
	const ware_besch_t* get_typ() const { return type; }
	void set_typ(const ware_besch_t *t) { type=t; }

	// Knightly : functions for manipulating goods statistics
	void init_stats();
	void roll_stats();
	void rdwr_stats(loadsave_t *file);
	const sint64* get_stats() const { return *statistics; }
	void book_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_GOODS_STAT); statistics[0][stat_type] += value; }
	void set_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_GOODS_STAT); statistics[0][stat_type] = value; }
	sint64 get_stat(int month, int stat_type) const { assert(stat_type<MAX_FAB_GOODS_STAT); return statistics[month][stat_type]; }
	void recalc_weighted_average_storage(sint64 weighted_value, sint64 aggregate_weight);

	sint32 menge;	// in internal untis shifted by precision (see produktion)
	sint32 max;
};


/**
 * Eine Klasse für Fabriken in Simutrans. Fabriken produzieren und
 * verbrauchen Waren und beliefern nahe Haltestellen.
 *
 * Die Abfragefunktionen liefern -1 wenn eine Ware niemals
 * hergestellt oder verbraucht wird, 0 wenn gerade nichts
 * hergestellt oder verbraucht wird und > 0 sonst
 * (entspricht Vorrat/Verbrauch).
 *
 * @date 1998
 * @see haltestelle_t
 * @author Hj. Malthaner
 */
class fabrik_t
{
public:
	/**
	 * Konstanten
	 * @author Hj. Malthaner
	 */
	enum { precision_bits = 10, old_precision_bits = 10, precision_mask = 1023 };

private:
	/**
	 * Factory statistics
	 * @author Knightly
	 */
	sint64 statistics[MAX_MONTH][MAX_FAB_STAT];
	sint64 weighted_sum_production;
	sint64 weighted_sum_boost_electric;
	sint64 weighted_sum_power;
	sint64 aggregate_weight;

	// Knightly : Functions for manipulating factory statistics
	void init_stats();
	void book_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_STAT); statistics[0][stat_type] += value; }
	void set_stat(sint64 value, int stat_type) { assert(stat_type<MAX_FAB_STAT); statistics[0][stat_type] = value; }

	// Knightly : For recalculating average statistics by dividing weighted sums with sum of delta time
	void recalc_average_stats(sint64 delta);

	// used for haltlist and lieferziele searches in verteile_waren to produce round robin results
	uint32 index_offset;

	/**
	 * Die möglichen Lieferziele
	 * @author Hj. Malthaner
	 */
	vector_tpl <koord> lieferziele;

	/**
	 * suppliers to this factry
	 * @author hsiegeln
	 */
	vector_tpl <koord> suppliers;

	/**
	 * fields of this factory (only for farms etc.)
	 * @author prissi/Knightly
	 */
	struct field_data_t
	{
		koord location;
		uint16 field_class_index;

		field_data_t() : field_class_index(0) {}
		explicit field_data_t(const koord loc) : location(loc), field_class_index(0) {}
		field_data_t(const koord loc, const uint16 class_index) : location(loc), field_class_index(class_index) {}

		bool operator==(const field_data_t &other) const { return location==other.location; }
	};
	vector_tpl <field_data_t> fields;

	/**
	 * Die erzeugten waren auf die Haltestellen verteilen
	 * @author Hj. Malthaner
	 */
	void verteile_waren(const uint32 produkt);

	// List of target cities
	vector_tpl<stadt_t *> target_cities;

	spieler_t *besitzer_p;
	karte_t *welt;

	const fabrik_besch_t *besch;

	/**
	 * Bauposition gedreht?
	 * @author V.Meyer
	 */
	uint8 rotate;

	/**
	 * produktionsgrundmenge
	 * @author Hj. Malthaner
	 */
	sint32 prodbase;

	/**
	 * multiplikator für die Produktionsgrundmenge
	 * @author Hj. Malthaner
	 */
	sint32 prodfactor_electric;
	sint32 prodfactor_pax;
	sint32 prodfactor_mail;

	vector_tpl<ware_production_t> eingang; //< das einganslagerfeld
	vector_tpl<ware_production_t> ausgang; //< das ausgangslagerfeld

	/**
	 * Zeitakkumulator für Produktion
	 * @author Hj. Malthaner
	 */
	sint32 delta_sum;
	uint32 delta_menge;

	// true if the factory has a transformer adjacent
	bool transformer_connected;

	// true, if the factory did produce enough in the last step to require power
	bool currently_producing;

	// power that can be currently drawn from this station (or the amount delivered)
	uint32 power;

	// power requested for next step
	uint32 power_demand;

	uint32 total_input, total_output;
	uint8 status;

	/**
	 * Die Koordinate (Position) der fabrik
	 * @author Hj. Malthaner
	 */
	koord3d pos;

	/**
	 * Number of times the factory has expanded so far
	 * Only for factories without fields
	 * @author Knightly
	 */
	uint16 times_expanded;

	/**
	 * Electricity amount scaled with prodbase and month length
	 * @author TurfIt
	 */
	uint32 scaled_electric_amount;

	/**
	 * Pax/mail demand scaled with prodbase and month length
	 * @author Knightly
	 */
	uint32 scaled_pax_demand;
	uint32 scaled_mail_demand;

	/**
	 * Update scaled electricity amount
	 * @author TurfIt
	 */
	void update_scaled_electric_amount();

	/**
	 * Update scaled pax/mail demand
	 * @author Knightly
	 */
	void update_scaled_pax_demand();
	void update_scaled_mail_demand();

	/**
	 * Update production multipliers for pax and mail
	 * @author Knightly
	 */
	void update_prodfactor_pax();
	void update_prodfactor_mail();

	/**
	 * Re-calculate the pax/mail demands of factory at target cities
	 * @author Knightly
	 */
	void recalc_demands_at_target_cities();

	/**
	 * Recalculate storage capacities based on prodbase and month length
	 * @author Knightly
	 */
	void recalc_storage_capacities();

	void recalc_factory_status();

	// create some smoke on the map
	void smoke() const;

	/**
	 * increase the amount for a time delta_t scaled to a fixed time PRODUCTION_DELTA_T
	 * @author Hj. Malthaner - original
	 */
	uint32 produktion(uint32 produkt, long delta_t) const;

public:
	fabrik_t(karte_t *welt, loadsave_t *file);
	fabrik_t(koord3d pos, spieler_t* sp, const fabrik_besch_t* fabesch);
	~fabrik_t();

	/**
	 * Return statistics
	 * @author Knightly
	 */
	const sint64* get_stats() const { return *statistics; }
	sint64 get_stat(int month, int stat_type) const { assert(stat_type<MAX_FAB_STAT); return statistics[month][stat_type]; }

	static fabrik_t * get_fab(const karte_t *welt, const koord pos);

	/**
	 * @return vehicle description object
	 * @author Hj. Malthaner
	 */
	const fabrik_besch_t *get_besch() const {return besch; }

	void laden_abschliessen();

	void set_pos( koord3d p ) { pos = p; }

	void rotate90( const sint16 y_size );

	void link_halt(halthandle_t halt);
	void unlink_halt(halthandle_t halt);

	const vector_tpl<koord>& get_lieferziele() const { return lieferziele; }
	const vector_tpl<koord>& get_suppliers() const { return suppliers; }

	/**
	 * Functions for manipulating the list of connected cities
	 * @author Hj. Malthaner/prissi/Knightly
	 */
	void add_target_city(stadt_t *const city);
	void remove_target_city(stadt_t *const city);
	void clear_target_cities();
	const vector_tpl<stadt_t *>& get_target_cities() const { return target_cities; }

	/**
	 * Fügt ein neues Lieferziel hinzu
	 * @author Hj. Malthaner
	 */
	void  add_lieferziel(koord ziel);
	void  rem_lieferziel(koord pos);

	/**
	 * adds a supplier
	 * @author Hj. Malthaner
	 */
	void  add_supplier(koord pos);
	void  rem_supplier(koord pos);

	/**
	 * @return menge der ware typ
	 *   -1 wenn typ nicht produziert wird
	 *   sonst die gelagerte menge
	 */
	sint32 input_vorrat_an(const ware_besch_t *ware);        // Vorrat von Warentyp
	sint32 vorrat_an(const ware_besch_t *ware);        // Vorrat von Warentyp

	// returns all power and consume it to prevent multiple pumpes
	uint32 get_power() { uint32 p=power; power=0; return p; }

	// returns power wanted by the factory for next step and sets to 0 to prevent multiple senkes on same powernet
	uint32 get_power_demand() { uint32 p=power_demand; power_demand=0; return p; }

	// give power to the factory to consume ...
	void add_power(uint32 p) { power += p; }

	// senkes give back wanted power they can't supply such that a senke on a different powernet can try suppling
	// WARNING: senke stepping order can vary between ingame construction and savegame loading => different results after saveing/loading the game
	void add_power_demand(uint32 p) { power_demand +=p; }

	// true, if there was production requiring power in the last step
	bool is_currently_producing() const { return currently_producing; }

	// used to limit transformers to 1 per factory
	bool is_transformer_connected() const { return transformer_connected; }
	void set_transformer_connected(bool connected) { transformer_connected = connected; }

	/**
	 * @return 1 wenn verbrauch,
	 * 0 wenn Produktionsstopp,
	 * -1 wenn Ware nicht verarbeitet wird
	 */
	sint32 verbraucht(const ware_besch_t *);             // Nimmt fab das an ??
	sint32 liefere_an(const ware_besch_t *, sint32 menge);

	void step(long delta_t);                  // fabrik muss auch arbeiten
	void neuer_monat();

	char const* get_name() const { return translator::translate(besch->get_name()); }
	sint32 get_kennfarbe() const { return besch->get_kennfarbe(); }

	spieler_t *get_besitzer() const
	{
		grund_t const* const p = welt->lookup(pos);
		return p ? p->first_obj()->get_besitzer() : 0;
	}

	void zeige_info() const;
	void info(cbuffer_t& buf) const;

	void rdwr(loadsave_t *file);

	inline koord3d get_pos() const { return pos; }

	/*
	 * Fills the vector with the koords of the tiles.
	 */
	void get_tile_list( vector_tpl<koord> &tile_list ) const;

	/**
	 * gibt eine NULL-Terminierte Liste von Fabrikpointern zurück
	 *
	 * @author Hj. Malthaner
	 */
	static vector_tpl<fabrik_t *> & sind_da_welche(karte_t *welt, koord min, koord max);

	/**
	 * gibt true zurueck wenn sich ein fabrik im feld befindet
	 *
	 * @author Hj. Malthaner
	 */
	static bool ist_da_eine(karte_t *welt, koord min, koord max);
	static bool ist_bauplatz(karte_t *welt, koord pos, koord groesse, bool water, climate_bits cl);

	// hier die methoden zum parametrisieren der Fabrik

	/**
	 * Baut die Gebäude für die Fabrik
	 *
	 * @author Hj. Malthaner, V. Meyer
	 */
	void baue(sint32 rotate);

	sint16 get_rotate() const { return rotate; }

	/* field generation code
	 * spawns a field for sure if probability>=1000
	 * @author Kieron Green
	 */
	bool add_random_field(uint16 probability);

	void remove_field_at(koord pos);

	uint32 get_field_count() const { return fields.get_count(); }

	/**
	 * total and current procduction/storage values
	 * @author Hj. Malthaner
	 */
	const vector_tpl<ware_production_t>& get_eingang() const { return eingang; }
	const vector_tpl<ware_production_t>& get_ausgang() const { return ausgang; }

	/**
	 * Production multipliers
	 * @author Hj. Malthaner
	 */
	sint32 get_prodfactor_electric() const { return prodfactor_electric; }
	sint32 get_prodfactor_pax() const { return prodfactor_pax; }
	sint32 get_prodfactor_mail() const { return prodfactor_mail; }
	sint32 get_prodfactor() const { return DEFAULT_PRODUCTION_FACTOR + prodfactor_electric + prodfactor_pax + prodfactor_mail; }

	/* does not takes month length into account */
	sint32 get_base_production() const { return prodbase; }
	void set_base_production(sint32 p);

	sint32 get_current_production() const { return ((sint64)prodbase * (sint64)(get_prodfactor()))>>(26l-(long)welt->ticks_per_world_month_shift); }

	/* prissi: returns the status of the current factory, as well as output */
	enum { bad, medium, good, inactive, nothing };
	static unsigned status_to_color[5];

	uint8  get_status()    const { return status;       }
	uint32 get_total_in()  const { return total_input;  }
	uint32 get_total_out() const { return total_output; }

	/**
	 * Crossconnects all factories
	 * @author prissi
	 */
	void add_all_suppliers();

	/* adds a new supplier to this factory
	 * fails if no matching goods are there
	 */
	bool add_supplier(fabrik_t* fab);

	/**
	 * Return the scaled electricity amount and pax/mail demand
	 * @author Knightly
	 */
	uint32 get_scaled_electric_amount() const { return scaled_electric_amount; }
	uint32 get_scaled_pax_demand() const { return scaled_pax_demand; }
	uint32 get_scaled_mail_demand() const { return scaled_mail_demand; }

	/**
	 * Book pax/mail being sent to this factory
	 * @author Knightly
	 */
	void book_pax_departed(const uint32 amount) { book_stat(amount, FAB_PAX_DEPARTED); }
	void book_mail_departed(const uint32 amount) { book_stat(amount, FAB_MAIL_DEPARTED); }
};

#endif
