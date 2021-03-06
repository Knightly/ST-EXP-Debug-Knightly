/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include "components/list_button.h"

#include "colors.h"

#include "../simdebug.h"
#include "../simworld.h"
#include "../simimg.h"
#include "../simintr.h"
#include "../simcolor.h"
#include "../dataobj/einstellungen.h"
#include "../dataobj/umgebung.h"
#include "../dataobj/translator.h"
#include "../simgraph.h"

#include "../utils/simstring.h"

#define MAX_BUTTONS (20)

// y coordinates
#define GRID_MODE			(0*13+6)
#define UNDERGROUND		(1*13+6)
#define DAY_NIGHT			(2*13+6)
#define BRIGHTNESS		(3*13+6)
#define SCROLL_INVERS (4*13+6)
#define SCROLL_SPEED	(5*13+6)

#define SEPERATE1 (6*13+6)

#define USE_TRANSPARENCY	(6*13+6+4)
#define HIDE_TREES				(7*13+6+4)
#define HIDE_CITY_HOUSES	(8*13+6+4)

#define SEPERATE2 (9*13+6+4)

#define USE_TRANSPARENCY_STATIONS	(9*13+6+2*4)
#define SHOW_STATION_COVERAGE			(10*13+6+2*4)
#define SHOW_STATION_SIGNS				(11*13+6+2*4)
#define SHOW_STATION_GOODS				(12*13+6+2*4)

#define SEPERATE3	(13*13+6+2*4)

#define CITY_WALKER								(13*13+6+3*4)
#define STOP_WALKER								(14*13+6+3*4)
#define DENS_TRAFFIC							(15*13+6+3*4)
#define CONVOI_TOOLTIPS							(16*13+6+3*4)

#define SEPERATE4	(17*13+6+3*4)

#define FPS_DATA (17*13+6+4*4)
#define IDLE_DATA (18*13+6+4*4)
#define FRAME_DATA (19*13+6+4*4)
#define LOOP_DATA (20*13+6+4*4)

#define BOTTOM (21*13+6+12+5*4)

// x coordinates
#define RIGHT_WIDTH (220)



color_gui_t::color_gui_t(karte_t *welt) :
	gui_frame_t("Helligk. u. Farben")
{
	this->welt = welt;

	// brightness
	brightness.set_pos( koord(RIGHT_WIDTH-10-40,BRIGHTNESS) );
	brightness.set_groesse( koord( 40, BUTTON_HEIGHT-1 ) );
	brightness.set_value( umgebung_t::daynight_level );
	brightness.set_limits( 0, 9 );
	brightness.add_listener(this);
	add_komponente(&brightness);

	// scrollspeed
	scrollspeed.set_pos( koord(RIGHT_WIDTH-10-40,SCROLL_SPEED) );
	scrollspeed.set_groesse( koord( 40, BUTTON_HEIGHT-1 ) );
	scrollspeed.set_value( abs(umgebung_t::scroll_multi) );
	scrollspeed.set_limits( 1, 9 );
	scrollspeed.add_listener(this);
	add_komponente(&scrollspeed);

	// traffic density
	traffic_density.set_pos( koord(RIGHT_WIDTH-10-50,DENS_TRAFFIC) );
	traffic_density.set_groesse( koord( 50, BUTTON_HEIGHT-1 ) );
	traffic_density.set_value( welt->get_einstellungen()->get_verkehr_level() );
	traffic_density.set_limits( 0, 16 );
	traffic_density.add_listener(this);
	add_komponente(&traffic_density);

	// other settings
	buttons[6].set_pos( koord(10,SCROLL_INVERS) );
	buttons[6].set_typ(button_t::square_state);
	buttons[6].set_text("4LIGHT_CHOOSE");
	buttons[6].pressed = umgebung_t::scroll_multi < 0;

	buttons[7].set_pos( koord(10,STOP_WALKER) );
	buttons[7].set_typ(button_t::square_state);
	buttons[7].set_text("5LIGHT_CHOOSE");
	buttons[7].pressed = welt->get_einstellungen()->get_show_pax();

	buttons[8].set_pos( koord(10,CITY_WALKER) );
	buttons[8].set_typ(button_t::square_state);
	buttons[8].set_text("6LIGHT_CHOOSE");
	buttons[8].pressed = welt->get_einstellungen()->get_random_pedestrians();

	buttons[9].set_pos( koord(10,DAY_NIGHT) );
	buttons[9].set_typ(button_t::square_state);
	buttons[9].set_text("8WORLD_CHOOSE");
	buttons[9].pressed = umgebung_t::night_shift;

	buttons[10].set_pos( koord(10,USE_TRANSPARENCY) );
	buttons[10].set_typ(button_t::square_state);
	buttons[10].set_text("hide transparent");
	buttons[10].pressed = umgebung_t::hide_with_transparency;

	buttons[11].set_pos( koord(10,HIDE_TREES) );
	buttons[11].set_typ(button_t::square_state);
	buttons[11].set_text("hide trees");

	// left right for hide messages
	buttons[12].set_pos( koord(10,HIDE_CITY_HOUSES) );
	buttons[12].set_typ(button_t::arrowleft);
	buttons[13].set_pos( koord(RIGHT_WIDTH-10-10,HIDE_CITY_HOUSES) );
	buttons[13].set_typ(button_t::arrowright);

	buttons[14].set_pos( koord(10,USE_TRANSPARENCY_STATIONS) );
	buttons[14].set_typ(button_t::square_state);
	buttons[14].set_text("transparent station coverage");
	buttons[14].pressed = umgebung_t::use_transparency_station_coverage;

	buttons[15].set_pos( koord(10,SHOW_STATION_COVERAGE) );
	buttons[15].set_typ(button_t::square_state);
	buttons[15].set_text("show station coverage");

	buttons[16].set_pos( koord(10,UNDERGROUND) );
	buttons[16].set_typ(button_t::square_state);
	buttons[16].set_text("underground mode");

	buttons[17].set_pos( koord(10,GRID_MODE) );
	buttons[17].set_typ(button_t::square_state);
	buttons[17].set_text("show grid");

	buttons[18].set_pos( koord(10,SHOW_STATION_SIGNS) );
	buttons[18].set_typ(button_t::square_state);
	buttons[18].set_text("show station names");
	buttons[18].pressed = umgebung_t::show_names&1;

	buttons[19].set_pos( koord(10,SHOW_STATION_GOODS) );
	buttons[19].set_typ(button_t::square_state);
	buttons[19].set_text("show waiting bars");
	buttons[19].pressed = umgebung_t::show_names&1;

	// left/right for convoi tooltips
	buttons[0].set_pos( koord(10,CONVOI_TOOLTIPS) );
	buttons[0].set_typ(button_t::arrowleft);
	buttons[1].set_pos( koord(RIGHT_WIDTH-10-10,CONVOI_TOOLTIPS) );
	buttons[1].set_typ(button_t::arrowright);

	for(int i=0;  i<MAX_BUTTONS;  i++ ) {
		buttons[i].add_listener(this);
		add_komponente( buttons+i );
	}

	set_fenstergroesse( koord(RIGHT_WIDTH, BOTTOM) );
}



bool
color_gui_t::action_triggered( gui_action_creator_t *komp, value_t v)
{
	einstellungen_t * sets = welt->get_einstellungen();

	if(&brightness==komp) {
		umgebung_t::daynight_level = v.i;
	} else if(&traffic_density==komp) {
		sets->set_verkehr_level( v.i );
	} else if(&scrollspeed==komp) {
		umgebung_t::scroll_multi = buttons[6].pressed ? -v.i : v.i;
	} else if((buttons+0)==komp) {
		umgebung_t::show_vehicle_states = (umgebung_t::show_vehicle_states+2)%3;
	} else if((buttons+1)==komp) {
		umgebung_t::show_vehicle_states = (umgebung_t::show_vehicle_states+1)%3;
	} else if((buttons+6)==komp) {
		buttons[6].pressed ^= 1;
		umgebung_t::scroll_multi = -umgebung_t::scroll_multi;
	} else if((buttons+7)==komp) {
		welt->get_einstellungen()->set_show_pax( !welt->get_einstellungen()->get_show_pax() );
		buttons[7].pressed ^= 1;
	} else if((buttons+8)==komp) {
		welt->get_einstellungen()->set_random_pedestrians( !welt->get_einstellungen()->get_random_pedestrians() );
		buttons[8].pressed ^= 1;
	} else if((buttons+9)==komp) {
		umgebung_t::night_shift = !umgebung_t::night_shift;
		buttons[9].pressed ^= 1;
	} else if((buttons+10)==komp) {
		umgebung_t::hide_with_transparency = !umgebung_t::hide_with_transparency;
		buttons[10].pressed ^= 1;
	} else if((buttons+11)==komp) {
		umgebung_t::hide_trees = !umgebung_t::hide_trees;
	} else if((buttons+12)==komp) {
		umgebung_t::hide_buildings = (umgebung_t::hide_buildings+2)%3;
	} else if((buttons+13)==komp) {
		umgebung_t::hide_buildings = (umgebung_t::hide_buildings+1)%3;
	} else if((buttons+14)==komp) {
		umgebung_t::use_transparency_station_coverage = !umgebung_t::use_transparency_station_coverage;
		buttons[14].pressed ^= 1;
	} else if((buttons+15)==komp) {
		umgebung_t::station_coverage_show = umgebung_t::station_coverage_show==0 ? 0xFF : 0;
	} else if((buttons+16)==komp) {
		grund_t::underground_mode = !grund_t::underground_mode;
		for(int y=0; y<welt->get_groesse_y(); y++) {
			for(int x=0; x<welt->get_groesse_x(); x++) {
				const planquadrat_t *plan = welt->lookup(koord(x,y));
				const int boden_count = plan->get_boden_count();
				for(int schicht=0; schicht<boden_count; schicht++) {
					grund_t *gr = plan->get_boden_bei(schicht);
					gr->calc_bild();
				}
			}
		}
	} else if((buttons+17)==komp) {
		grund_t::toggle_grid();
	} else if((buttons+18)==komp) {
		umgebung_t::show_names ^= 1;
	} else if((buttons+19)==komp) {
		umgebung_t::show_names ^= 2;
	}
	welt->set_dirty();
	return true;
}




void color_gui_t::zeichnen(koord pos, koord gr)
{
	const int x = pos.x;
	const int y = pos.y+16;	// compensate for title bar
	const einstellungen_t * sets = welt->get_einstellungen();
	char buf[128];

	// can be changed also with keys ...
	buttons[11].pressed = umgebung_t::hide_trees;
	buttons[15].pressed = umgebung_t::station_coverage_show;
	buttons[16].pressed = grund_t::underground_mode;
	buttons[17].pressed = grund_t::show_grid;
	buttons[18].pressed = umgebung_t::show_names&1;
	buttons[19].pressed = (umgebung_t::show_names&2)!=0;

	gui_frame_t::zeichnen(pos, gr);

	// seperator
	display_ddd_box_clip(x+10, y+SEPERATE1, RIGHT_WIDTH-20, 0, MN_GREY0, MN_GREY4);
	display_ddd_box_clip(x+10, y+SEPERATE2, RIGHT_WIDTH-20, 0, MN_GREY0, MN_GREY4);
	display_ddd_box_clip(x+10, y+SEPERATE3, RIGHT_WIDTH-20, 0, MN_GREY0, MN_GREY4);
	display_ddd_box_clip(x+10, y+SEPERATE4, RIGHT_WIDTH-20, 0, MN_GREY0, MN_GREY4);

	display_proportional_clip(x+10, y+BRIGHTNESS+1, translator::translate("1LIGHT_CHOOSE"), ALIGN_LEFT, COL_BLACK, true);

	display_proportional_clip(x+10, y+SCROLL_SPEED+1, translator::translate("3LIGHT_CHOOSE"), ALIGN_LEFT, COL_BLACK, true);

	display_proportional_clip(x+10, y+DENS_TRAFFIC+1, translator::translate("6WORLD_CHOOSE"), ALIGN_LEFT, COL_BLACK, true);

	const char *hhc = translator::translate( umgebung_t::hide_buildings==0 ? "no buildings hidden" : (umgebung_t::hide_buildings==1 ? "hide city building" : "hide all building") );
	display_proportional_clip(x+10+16, y+HIDE_CITY_HOUSES+1, hhc, ALIGN_LEFT, COL_BLACK, true);

	const char *ctc = translator::translate( umgebung_t::show_vehicle_states==0 ? "convoi error tooltips" : (umgebung_t::show_vehicle_states==1 ? "convoi mouseover tooltips" : "all convoi tooltips") );
	display_proportional_clip(x+10+16, y+CONVOI_TOOLTIPS+1, ctc, ALIGN_LEFT, COL_BLACK, true);

	int len=15+display_proportional_clip(x+10, y+FPS_DATA, translator::translate("Frame time:"), ALIGN_LEFT, COL_BLACK, true);
	sprintf(buf,"%ld ms", get_frame_time() );
	display_proportional_clip(x+len, y+FPS_DATA, buf, ALIGN_LEFT, COL_WHITE, true);

	len = 15+display_proportional_clip(x+10, y+IDLE_DATA, translator::translate("Idle:"), ALIGN_LEFT, COL_BLACK, true);
	display_proportional_clip(x+len, y+IDLE_DATA, ntos(welt->get_schlaf_zeit(), "%d ms"), ALIGN_LEFT, COL_WHITE, true);

	uint8 farbe;
	uint32 loops;
	loops=welt->get_realFPS();
	farbe = COL_WHITE;
	uint32 target_fps = welt->is_fast_forward() ? 10 : umgebung_t::fps;
	if(loops<(target_fps*3)/4) {
		farbe = (loops<=target_fps/2) ? COL_RED : COL_YELLOW;
	}
	len = 15+display_proportional_clip(x+10, y+FRAME_DATA, translator::translate("FPS:"), ALIGN_LEFT, COL_BLACK, true);
	sprintf(buf,"%d fps", loops );
	display_proportional_clip(x+len, y+FRAME_DATA, buf, ALIGN_LEFT, farbe, true);

	loops=welt->get_simloops();
	farbe = COL_WHITE;
	if(loops<=30) {
		farbe = (loops<=20) ? COL_RED : COL_YELLOW;
	}
	len = 15+display_proportional_clip(x+10, y+LOOP_DATA, translator::translate("Sim:"), ALIGN_LEFT, COL_BLACK, true);
	sprintf( buf, "%d%c%d", loops/10, get_fraction_sep(), loops%10 );
	display_proportional_clip(x+len, y+LOOP_DATA, buf, ALIGN_LEFT, farbe, true);
}
