/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include "../simcolor.h"
#include "../simwin.h"
#include "../simworld.h"

#include "../dataobj/translator.h"
#include "../utils/simstring.h"
#include "../vehicle/simvehikel.h"

#include "replace_frame.h"


replace_frame_t::replace_frame_t(convoihandle_t cnv, const char *name):
	gui_frame_t(translator::translate("Replace"), cnv->get_besitzer()),
	cnv(cnv),
	replace_line(false), replace_all(false), depot(false), autostart(true),
	state(state_replace), replaced_so_far(0),
	lb_convoy(cnv, true, true),
	lb_to_be_replaced(NULL, COL_BLACK, gui_label_t::centered),
	lb_money(NULL, COL_BLACK, gui_label_t::money),
	lb_replace_cycle(NULL, COL_BLACK, gui_label_t::right),
	lb_replace(NULL, COL_BLACK, gui_label_t::left),
	lb_sell(NULL, COL_BLACK, gui_label_t::left),
	lb_skip(NULL, COL_BLACK, gui_label_t::left),
	lb_n_replace(NULL, COL_BLACK, gui_label_t::left),
	lb_n_sell(NULL, COL_BLACK, gui_label_t::left),
	lb_n_skip(NULL, COL_BLACK, gui_label_t::left)

{
	const uint32 a_button_height = 14;
	const uint32 margin = 6;
	lb_money.set_text_pointer(txt_money);
	add_komponente(&lb_money);
	
	lb_convoy.set_text_pointer(name);
	add_komponente(&lb_convoy);

	lb_to_be_replaced.set_text_pointer(translator::translate("To be replaced by:"));
	add_komponente(&lb_to_be_replaced);

	lb_replace_cycle.set_text_pointer(translator::translate("Replace cycle:"));
	lb_replace.set_text_pointer(translator::translate("Replace"));
	lb_sell.set_text_pointer(translator::translate("Sell"));
	lb_skip.set_text_pointer(translator::translate("Skip"));
	numinp[state_replace].set_value( 1 );
	numinp[state_replace].set_limits( 0, 99 );
	numinp[state_replace].set_increment_mode( 1 );
	numinp[state_replace].add_listener(this);
	numinp[state_sell].set_value( 0 );
	numinp[state_sell].set_limits( 0, 99 );
	numinp[state_sell].set_increment_mode( 1 );
	numinp[state_sell].add_listener(this);
	numinp[state_skip].set_value( 0 );
	numinp[state_skip].set_limits( 0, 99 );
	numinp[state_skip].set_increment_mode( 1 );
	numinp[state_skip].add_listener(this);
	lb_n_replace.set_text_pointer(txt_n_replace);
	lb_n_sell.set_text_pointer(txt_n_sell);
	lb_n_skip.set_text_pointer(txt_n_skip);
	add_komponente(&lb_replace_cycle);
	add_komponente(&lb_replace);
	add_komponente(&numinp[state_replace]);
	add_komponente(&lb_n_replace);
	add_komponente(&lb_sell);
	add_komponente(&numinp[state_sell]);
	add_komponente(&lb_n_sell);
	add_komponente(&lb_skip);
	add_komponente(&numinp[state_skip]);
	add_komponente(&lb_n_skip);

	waytype_t wt = road_wt;
	if (cnv->get_vehikel_anzahl()>0) { // Should always be true
		wt=cnv->get_vehikel(0)->get_besch()->get_waytype();
	}
	const weg_t *way = cnv->get_welt()->lookup(cnv->get_vehikel(0)->get_pos())->get_weg(wt);
	const bool weg_electrified = way == NULL ? false : way->is_electrified();
	convoy_assembler = new gui_convoy_assembler_t(cnv->get_welt(), wt, weg_electrified,  cnv->get_besitzer()->get_player_nr() );
	convoy_assembler->set_convoy_tabs_skip(-2*LINESPACE+3*LINESPACE+2*margin+a_button_height);
	convoy_assembler->add_listener(this);
	convoy_assembler->set_vehicles(cnv->get_replacing_vehicles());
	add_komponente(convoy_assembler);

	bt_replace_line.set_typ(button_t::square);
	bt_replace_line.set_text("replace all in line");
	bt_replace_line.set_tooltip("Replace all convoys like this belonging to this line");
	bt_replace_line.add_listener(this);
	add_komponente(&bt_replace_line);
	bt_replace_all.set_typ(button_t::square);
	bt_replace_all.set_text("replace all");
	bt_replace_all.set_tooltip("Replace all convoys like this");
	bt_replace_all.add_listener(this);
	add_komponente(&bt_replace_all);

	bt_autostart.set_typ(button_t::roundbox);
	bt_autostart.set_text("Full replace");
	bt_autostart.set_tooltip("Send convoy to depot, replace and restart it automatically");
	bt_autostart.add_listener(this);
	add_komponente(&bt_autostart);
	bt_depot.set_typ(button_t::roundbox);
	bt_depot.set_text("Replace but stay");
	bt_depot.set_tooltip("Send convoy to depot, replace it and stay there");
	bt_depot.add_listener(this);
	add_komponente(&bt_depot);

	bt_mark.set_typ(button_t::roundbox);
	bt_mark.set_text("Mark for replacing");
	bt_mark.set_tooltip("Mark for replacing. The convoy will replace when manually sent to depot");
	bt_mark.add_listener(this);
	add_komponente(&bt_mark);

	koord gr = koord(0,0);
	layout(&gr);
	update_data();
	gui_frame_t::set_fenstergroesse(gr);

	// Hajo: Trigger layouting
	set_resizemode(diagonal_resize);

	convoy_assembler->set_replace_frame(this);
}


replace_frame_t::~replace_frame_t()
{
	delete convoy_assembler;
}


void replace_frame_t::update_total_height(uint32 height)
{
	total_height+=height;
	min_total_height+=height;
}


void replace_frame_t::update_total_width(uint32 width)
{
	total_width=max(total_width,width);
	min_total_width=max(min_total_width,width);
}


void replace_frame_t::layout(koord *gr)
{
	const uint32 window_bar_height=16;
	const uint32 margin=6;
	const uint32 a_button_width=96;
	const uint32 a_button_height=14;

	/**
	 * Let's calculate the space and min space
	 */
	koord fgr = (gr!=NULL)? *gr : get_fenstergroesse();
	min_total_width=0;
	total_width=fgr.x;
	total_height=window_bar_height+2*margin;
	min_total_height=total_height;

	// Width at least to see labels ok
	update_total_width(400);

	// Convoy label: name+image+specs
	koord img_size=lb_convoy.get_size();
	update_total_width(img_size.x);
	update_total_height(img_size.y);

	// Label to be replaced
	update_total_height(LINESPACE);

	// 3 buttons
	update_total_width(2*margin+3*a_button_width);
	// No update height needed, convoy assembler

	// Rest of the vertical space, if any, for convoy_assembler
	update_total_width(convoy_assembler->get_convoy_image_width());
	convoy_assembler->set_panel_rows(gr  &&  gr->y==0?-1:fgr.y-total_height);
	total_height+=convoy_assembler->get_height()+margin;
	min_total_height+=convoy_assembler->get_min_height()+margin;

	set_min_windowsize(koord(min_total_width, min_total_height));
	if(fgr.x<total_width) {
		gui_frame_t::set_fenstergroesse(koord(min_total_width, max(fgr.y,min_total_height) ));
	}
	if(gr  &&  gr->x==0) {
		gr->x = total_width;
	}
	if(gr  &&  gr->y==0) {
		gr->y = total_height;
	}

	/**
	 * Now do the layout
	 */
	uint32 current_y=margin;
	if (gr) {
		fgr=*gr;
	} else {
		fgr=koord(total_width,total_height);
	}

	lb_convoy.set_pos(koord(fgr.x/2,current_y));
	current_y+=lb_convoy.get_size().y;

	lb_to_be_replaced.set_pos(koord(fgr.x/2,current_y));
	current_y+=LINESPACE;

	convoy_assembler->set_pos(koord(0,current_y));
	convoy_assembler->set_groesse(koord(fgr.x,convoy_assembler->get_height()));
	convoy_assembler->layout();

	uint32 buttons_y=current_y+convoy_assembler->get_convoy_height()-2*LINESPACE+8;
	uint32 buttons_width=(fgr.x-2*margin)/3;
	bt_autostart.set_groesse(koord(buttons_width, a_button_height));
	bt_depot.set_groesse(koord(buttons_width, a_button_height));
	bt_mark.set_groesse(koord(buttons_width, a_button_height));
	bt_autostart.set_pos(koord(margin,buttons_y));
	bt_depot.set_pos(koord(margin+buttons_width,buttons_y));
	bt_mark.set_pos(koord(margin+buttons_width+buttons_width,buttons_y));
	current_y=buttons_y+a_button_height+margin;
	lb_money.set_pos(koord(110,current_y));
	lb_replace_cycle.set_pos(koord(fgr.x-170,current_y));
	lb_replace.set_pos(koord(fgr.x-166,current_y));
	numinp[state_replace].set_pos( koord( fgr.x-110, current_y ) );

	numinp[state_replace].set_groesse( koord( 50, a_button_height ) );
	lb_n_replace.set_pos( koord( fgr.x-50, current_y ) );
	current_y+=LINESPACE+2;
	bt_replace_line.set_pos(koord(margin,current_y));
	lb_sell.set_pos(koord(fgr.x-166,current_y));
	numinp[state_sell].set_groesse( koord( 50, a_button_height ) );
	lb_n_sell.set_pos( koord( fgr.x-50, current_y ) );
	current_y+=LINESPACE+2;
	bt_replace_all.set_pos(koord(margin,current_y));
	lb_skip.set_pos(koord(fgr.x-166,current_y));
	numinp[state_skip].set_pos( koord( fgr.x-110, current_y ) );
	numinp[state_skip].set_groesse( koord( 50, a_button_height ) );
	lb_n_skip.set_pos( koord( fgr.x-50, current_y ) );
	current_y+=LINESPACE+margin;
}


void replace_frame_t::set_fenstergroesse( koord gr )
{
	koord g=gr;
	layout(&g);
	update_data();
	gui_frame_t::set_fenstergroesse(gr);
}


void replace_frame_t::update_data()
{
	convoy_assembler->update_data();

	txt_n_replace[0]='\0';
	txt_n_sell[0]='\0';
	txt_n_skip[0]='\0';
	uint32 n[3];
	n[0]=0;
	n[1]=0;
	n[2]=0;
	money = 0;
	sint32 base_total_cost = calc_total_cost();
	if (replace_line || replace_all) {
		start_replacing();
	} else {
		money -= base_total_cost;
	}
	if (replace_line) {
		linehandle_t line=cnv.is_bound()?cnv->get_line():linehandle_t();
		if (line.is_bound()) {
			for (uint32 i=0; i<line->count_convoys(); i++) {
				convoihandle_t cnv_aux=line->get_convoy(i);
				if (cnv->has_same_vehicles(cnv_aux)) {
					uint32 present_state=get_present_state();
					if (present_state==-1) {
						continue;
					}
					switch(convoy_assembler->get_action())
					{
						
					case gui_convoy_assembler_t::clear_convoy_action:
						money = 0;
						n[present_state]++;
						break;

					case gui_convoy_assembler_t::remove_vehicle_action:
						money += base_total_cost;
						n[present_state]++;
						break;

					default:
						money -= base_total_cost;
						n[present_state]++;
					};
				}
			}
		}
	} else if (replace_all) {
		karte_t *welt=cnv->get_welt();
		for (uint32 i=0; i<welt->get_convoi_count(); i++) {
			convoihandle_t cnv_aux=welt->get_convoi(i);
			if (cnv_aux.is_bound() && cnv_aux->get_besitzer()==cnv->get_besitzer() && cnv->has_same_vehicles(cnv_aux)) 
			{
				uint32 present_state=get_present_state();
				if (present_state==-1) 
				{
					continue;
				}

				switch(convoy_assembler->get_action())
				{
					
				case gui_convoy_assembler_t::clear_convoy_action:
					money = 0;
					n[present_state]++;
					break;

				case gui_convoy_assembler_t::remove_vehicle_action:
					money += base_total_cost;
					n[present_state]++;
					break;

				default:
					money -= base_total_cost;
					n[present_state]++;
				};	
			}
		}
	}
	if (replace_all || replace_line) {
		sprintf(txt_n_replace,"%d",n[0]);
		sprintf(txt_n_sell,"%d",n[1]);
		sprintf(txt_n_skip,"%d",n[2]);
	}
	if (convoy_assembler->get_vehicles()->get_count()>0) {
		money_to_string(txt_money,money/100.0);
		lb_money.set_color(money>=0?MONEY_PLUS:MONEY_MINUS);
	} else {
		txt_money[0]='\0';
	}

}


uint8 replace_frame_t::get_present_state() {
	if (numinp[state_replace].get_value()==0 && numinp[state_sell].get_value()==0 && numinp[state_skip].get_value()==0) {
		return -1;
	}
	for (uint32 i=0; i<n_states; ++i) {
		if (replaced_so_far>=numinp[state].get_value()) {
			replaced_so_far=0;
			state=(state+1)%n_states;
		} else {
			break;
		}
	}
	replaced_so_far++;
		return state;
	}


void replace_frame_t::replace_convoy(convoihandle_t cnv)
{
	uint32 state=get_present_state();
	if (!cnv.is_bound() || cnv->in_depot() || state==-1) {
		return;
	}

	switch (state) {
	case state_replace:
		if(!cnv->get_welt()->get_active_player()->can_afford(0 - money))
		{
			const char *err = "That would exceed\nyour credit limit.";
			news_img *box = new news_img(err);
			create_win(box, w_time_delete, magic_none);
			break;
		}

		cnv->set_replacing_vehicles(convoy_assembler->get_vehicles());
		cnv->set_depot_when_empty( (depot || autostart) && convoy_assembler->get_vehicles()->get_count()>0 );
		cnv->set_autostart(autostart);
		cnv->set_no_load( cnv->get_depot_when_empty() && convoy_assembler->get_vehicles()->get_count()>0 );
		cnv->set_replace(convoy_assembler->get_vehicles()->get_count()>0);
		// If already empty, no need to be emptied
		if(cnv->get_replace() && cnv->get_depot_when_empty() && cnv->has_no_cargo()) {
			cnv->set_depot_when_empty(false);
			cnv->set_no_load(false);
			cnv->go_to_depot(false);
		}
		break;
	
	case state_sell:
		cnv->set_replace(false);
		cnv->set_withdraw(true);
		cnv->set_no_load(true);
		break;
	case state_skip:
	break;
	}

	replaced_so_far++;
}

bool replace_frame_t::action_triggered( gui_action_creator_t *komp,value_t p)
{
	if(komp != NULL) {	// message from outside!
		if(komp == convoy_assembler) {
			//const koord k=*static_cast<const koord *>(p.p);
			//switch (k.x) {
			//	case gui_convoy_assembler_t::clear_convoy_action:
			//		new_convoy_cost=0;
			//		break;
			//	case gui_convoy_assembler_t::remove_vehicle_action:
			//		new_convoy_cost-=convoy_assembler->get_last_changed_vehicle()->get_preis();
			//		break;
			//	default: // append/insert_in_front
			//		new_convoy_cost+=convoy_assembler->get_last_changed_vehicle()->get_preis();
			//		break;
			//}

		} else if(komp == &bt_replace_line) {
			replace_line=!replace_line;
			replace_all=false;
		} else if(komp == &bt_replace_all) {
			replace_all=!replace_all;
			replace_line=false;

		} else if(komp == numinp+state_replace) {
		} else if(komp == numinp+state_sell) {
		} else if(komp == numinp+state_skip) {
		} else if(komp==&bt_autostart || komp== &bt_depot || komp == &bt_mark) {
			depot=(komp==&bt_depot);
			autostart=(komp==&bt_autostart);

			start_replacing();
			if (!replace_line && !replace_all) {
				replace_convoy(cnv);
			} else if (replace_line) {
				linehandle_t line=cnv.is_bound()?cnv->get_line():linehandle_t();
				if (line.is_bound()) {
					for (uint32 i=0; i<line->count_convoys(); i++) {
						convoihandle_t cnv_aux=line->get_convoy(i);
						if (cnv->has_same_vehicles(cnv_aux)) {
							replace_convoy(cnv_aux);
						}
					}
				}
			} else if (replace_all) {
				karte_t *welt=cnv->get_welt();
				for (uint32 i=0; i<welt->get_convoi_count(); i++) {
					convoihandle_t cnv_aux=welt->get_convoi(i);
					if (cnv_aux.is_bound() && cnv_aux->get_besitzer()==cnv->get_besitzer() && cnv->has_same_vehicles(cnv_aux)) {
						replace_convoy(cnv_aux);
					}
				}
			}
			destroy_win(this);
			return true;
		}
	}
	convoy_assembler->build_vehicle_lists();
	update_data();
	layout(NULL);
	return true;
}


void replace_frame_t::infowin_event(const event_t *ev)
{
	gui_frame_t::infowin_event(ev);
	if(IS_WINDOW_REZOOM(ev)) {
		koord gr = get_fenstergroesse();
		set_fenstergroesse(gr);
	} else if(ev->ev_class == INFOWIN && ev->ev_code == WIN_OPEN) {
		convoy_assembler->build_vehicle_lists();
		update_data();
		layout(NULL);
	}
}


void replace_frame_t::zeichnen(koord pos, koord groesse)
{
	if (get_welt()->get_active_player() != cnv->get_besitzer()) {
		destroy_win(this);
		return;
	}

	// Refresh button state.  Otherwise, they would not show pressed.
	bt_replace_line.pressed=replace_line;
	if (cnv.is_bound() && cnv->get_line().is_bound()) {
		bt_replace_line.enable();
	} else {
		bt_replace_line.disable();
		replace_line=false;
	}
	bt_replace_all.pressed=replace_all;
	
	// Make replace cycle grey if not in use
	uint32 color=(replace_line||replace_all?COL_BLACK:COL_GREY4);
	lb_replace_cycle.set_color(color);
	lb_replace.set_color(color);
	lb_sell.set_color(color);
	lb_skip.set_color(color);

	gui_frame_t::zeichnen(pos, groesse);
}

sint32 replace_frame_t::calc_total_cost()
{
	sint32 total_cost = 0;
	vector_tpl<const vehikel_t*> current_vehicles;
	vector_tpl<uint8> keep_vehicles;
	for(uint8 i = 0; i < cnv->get_vehikel_anzahl(); i ++)
	{
		current_vehicles.append(cnv->get_vehikel(i));
	}
	ITERATE((*convoy_assembler->get_vehicles()),j)
	{
		const vehikel_besch_t* veh = NULL;
		const vehikel_besch_t* test_new_vehicle = (*convoy_assembler->get_vehicles())[j];
		// First - check whether there are any of the required vehicles already
		// in the convoy (free)
		ITERATE(current_vehicles,k)
		{
			const vehikel_besch_t* test_old_vehicle = current_vehicles[k]->get_besch();
			if(!keep_vehicles.is_contained(k) && current_vehicles[k]->get_besch() == (*convoy_assembler->get_vehicles())[j])
			{
				veh = current_vehicles[k]->get_besch();
				keep_vehicles.append_unique(k);
				// No change to price here.
				break;
			}
		}

		// We cannot look up the home depot here, so we cannot check whether there are any 
		// suitable vehicles stored there as is done when the actual replacing takes place.

		if (veh == NULL) 
		{
			// Second - check whether the vehicle can be upgraded (cheap)
			ITERATE(current_vehicles,l)
			{	
				for(uint8 c = 0; c < current_vehicles[l]->get_besch()->get_upgrades_count(); c ++)
				{
					const vehikel_besch_t* possible_upgrade_test = current_vehicles[l]->get_besch()->get_upgrades(c);
					if(!keep_vehicles.is_contained(l) && (*convoy_assembler->get_vehicles())[j] == current_vehicles[l]->get_besch()->get_upgrades(c))
					{
						veh = current_vehicles[l]->get_besch();
						keep_vehicles.append_unique(l);
						total_cost += veh->get_upgrades(c)->get_upgrade_price();
						goto end_loop;
					}
				}
			}
end_loop:	
			if(veh == NULL)
			{
				// Third - if all else fails, buy from new (expensive).
				total_cost += (*convoy_assembler->get_vehicles())[j]->get_preis();
			}
		}
	}
	ITERATE(current_vehicles,m)
	{
		if(!keep_vehicles.is_contained(m))
		{
			// This vehicle will not be kept after replacing - 
			// deduct its resale value from the total cost.
			total_cost -= current_vehicles[m]->calc_restwert();
		}
	}
	
	return total_cost;
}