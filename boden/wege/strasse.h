#ifndef boden_wege_strasse_h
#define boden_wege_strasse_h

#include "weg.h"

/**
 * Auf der Strasse k�nnen Autos fahren.
 *
 * @author Hj. Malthaner
 */
class strasse_t : public weg_t
{
public:
	static const weg_besch_t *default_strasse;

	strasse_t(karte_t *welt, loadsave_t *file);
	strasse_t(karte_t *welt);

	inline waytype_t get_waytype() const {return road_wt;}

	void set_gehweg(bool janein);

	virtual void rdwr(loadsave_t *file);
};

#endif
