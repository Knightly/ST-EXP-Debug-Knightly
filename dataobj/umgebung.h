/*
 * Hier werden die Kommandozeilenparametr in f�r das Spiel
 * nutzbarer Form gespeichert.
 *
 * von Hansj�rg Malthaner, November 2000
 */

#ifndef dataobj_umgebung_h
#define dataobj_umgebung_h

#include "../simtypes.h"
#include "../simconst.h"
#include "../simcolor.h"
#include "einstellungen.h"

class cstring_t;

/**
 * Diese Klasse bildet eine Abstraktion der Kommandozeilenparameter.
 * Alle Attribute sind statisch, damit sie �berall zug�nglich sind.
 * Das ist kein Problem, denn sie existieren garantiert nur einmal!
 *
 * The game specific stuff is in default_einstellungen, to keep them centralized ...
 *
 * @author Hj. Malthaner
 */
class umgebung_t
{
public:
	// points to the current simutrans data directory
	static char program_dir[1024];

	//points to the current directory user for loading and saving
	static const char *user_dir;

	static cstring_t objfilename;

	// scrollrichtung
	static sint16 scroll_multi;

	/**
	* tag-nacht wechsel zeigen ?
	*
	* @author Hj. Malthaner
	*/
	static bool night_shift;

	/**
	* Stationsabdeckung zeigen
	* @author prissi
	*/
	static bool use_transparency_station_coverage;
	static uint8 station_coverage_show;
	enum { NOT_SHOWN_COVERAGE=0, SHOW_MY_COVERAGE, SHOW_ALL_COVERAGE };

	// use transparency to hide buildings and trees
	static bool hide_with_transparency;

	/**
	 * three states:
	 */
	enum { NOT_HIDE=0, SOME_HIDDEN_BUIDLING, ALL_HIDDEN_BUIDLING };
	static uint8 hide_buildings;

	/**
	 * Set to true to hide all trees. "Hiding" is implemented by showing the
	 * first pic which should be very small.
	 * @author Volker Meyer
	 * @date  10.06.2003
	 */
	static bool hide_trees;

	/**
	* Namen (St�dte, Haltestellen) anzeigen? (0 .. 3)
	*
	* @author Hj. Malthaner
	*/
	static sint32 show_names;

	/**
	* which messages to display where?
	*
	* @author prissi
	*/
	static sint32 message_flags[4];

	/* time per water animation fram (0=off)
	 * @author prissi
	 */
	static uint32 water_animation;

	/* probability for ground objects (if exists)
	 * @author prissi
	 */
	static uint32 ground_object_probability;

	/* probability for moving objects (if there)
	 * @author prissi
	 */
	static uint32 moving_object_probability;

	/**
	* Info-Fenster f�r Fussg�nger und Privatfahrzeuge
	*
	* @author Hj. Malthaner
	*/
	static bool verkehrsteilnehmer_info;

	/**
	* Info-Fenster f�r B�ume
	* @author prissi
	*/
	static bool tree_info;

	/**
	* Info-Fenster for all grounds
	* @author prissi
	*/
	static bool ground_info;

	/**
	* Info-Fenster f�r Townhall
	* @author prissi
	*/
	static bool townhall_info;

	/**
	* Only one info window
	* @author prissi
	*/
	static bool single_info;

	/**
	* window button at right corner (like Windows)
	* @author prissi
	*/
	static bool window_buttons_right;

	/*
	 * frame active window
	 */
	static bool window_frame_active;

	/**
	* Produce more debug info ?
	*
	* @author Hj. Malthaner
	*/
	static uint8 verbose_debug;

	// how to sort stations/convois
	static uint8 default_sortmode;

	// what is selected for maps
	static sint8 default_mapmode;

	/**
	* Max. L�nge f�r initiale Stadtverbindungen
	*
	* @author Hj. Malthaner
	*/
	static sint32 intercity_road_length;

	/**
	* Typ (Name) initiale Stadtverbindungen
	*
	* @author Hj. Malthaner
	*/
	static const char *intercity_road_type;

	/**
	 * Name of rivers; first the river with the lowest number
	 * @author prissi
	 */
	static const char *river_type[10];
	static uint8 river_types;

	/**
	* show month in date?
	*
	* @author hsiegeln
	*/
	static uint8 show_month;

	/* prissi: do autosave every month? */
	static sint32 autosave;

	/* prissi: drive on the left side of the road */
	static bool drive_on_left;

	// set the frame rate for the display
	static uint32 fps;

	// maximum acceleration with fast forward
	static sint16 max_acceleration;

	// false to quit the programs
	static bool quit_simutrans;

	// new game start without tree
	static bool no_tree;

	// customize your tooltips
	static bool show_tooltips;
	static uint8 tooltip_color;
	static uint8 tooltip_textcolor;

	// color used for cursor overlay blending
	static uint8 cursor_overlay_color;

	// show error/info tooltips over the vehicles
	static uint8 show_vehicle_states;

	// fixed day/night view level
	static sint8 daynight_level;

	// current language
	static const char *language_iso;

	// midi/sound option
	static sint16 global_volume, midi_volume;
	static bool mute_sound, mute_midi, shuffle_midi;

	static einstellungen_t default_einstellungen;

	// load/saving settings from file
	static void rdwr(loadsave_t *file);
};

#endif
