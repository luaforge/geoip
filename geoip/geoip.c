// vim:sw=4:sts=4
/**
 * Bindings for the GeoIP database library by MaxMind.
 * by Wolfgang Oertl 2009
 *
 * Revisions:
 *  2009-03-02	first version.
 *
 * Installation:
 *  To compile, you need the headers GeoIP.h, GeoIPCity.h and maybe others,
 *  which are contained in the C library that can be downloaded from MaxMind.
 *
 *  The library libGeoIP.so must be available.  If you already have
 *  libGeoIP.so.1 or similar, create a symbolic link.  Otherwise compile
 *  the downloaded library and install it.
 *
 *  Finally: gcc -shared -o geoip.so geoip.c -lGeoIP.  You could also use
 *  the static version of libGeoIP to avoid a runtime dependency.
 *
 * Example Usage:
 *  geoip = require("geoip")
 *  g = geoip.open_type "country"
 *  r = g:lookup("74.125.67.100")
 *  r = g:lookup("google.com")
 *  print(r)
 *  print(r.country_code)
 *
 */

#include <GeoIPCity.h>
#include <lua.h>
#include <lauxlib.h>

/**
 * Result is a structure containing all the information on a location
 * and is returned by searching for an IP address or a hostname.
 *
 * Depending on the database used, one of the fields is filled in.  Most of the
 * fields can be read with the appropriate __index method.
 */

typedef struct {
    GeoIPRecord *gir;	/* for the city edition, the complete record */
    int id;		/* for the country edition, the location id */
    GeoIPRegion *reg;	/* for the country edition, the region */
} Result;

#define RESULT	"GeoIPResult"
#define GEOIP "GeoIP"


/**
 * Push a string if it is not NULL and return 1, else return 0.
 */
static int _push_opt_string(lua_State *L, const char *s)
{
    if (s) {
	lua_pushstring(L, s);
	return 1;
    }

    return 0;
}


/**
 * The result is a GeoIPCity record.  Access all these fields.
 */
static int _record_index_city(lua_State *L, GeoIPRecord *g, const char *field)
{
    if (!field) {
	lua_pushfstring(L, "%s, %s (%s)", g->city, g->country_name,
	    g->country_code);
	return 1;
    }

    if (!strcmp(field, "city"))
	return _push_opt_string(L, g->city);
    if (!strcmp(field, "postal_code"))
	return _push_opt_string(L, g->postal_code);
    if (!strcmp(field, "latitude")) {
	lua_pushnumber(L, g->latitude);
	return 1;
    }
    if (!strcmp(field, "longitude")) {
	lua_pushnumber(L, g->longitude);
	return 1;
    }
    if (!strcmp(field, "country"))
	return _push_opt_string(L, g->country_name);
    if (!strcmp(field, "country_code"))
	return _push_opt_string(L, g->country_code);
    if (!strcmp(field, "region"))
	return _push_opt_string(L, g->region);
    if (!strcmp(field, "region_name"))
	return _push_opt_string(L, GeoIP_region_name_by_code(g->country_code,
	    g->region));
    if (!strcmp(field, "time_zone"))
	return _push_opt_string(L, GeoIP_time_zone_by_country_and_region(
	    g->country_code, g->region));
    if (!strcmp(field, "continent"))
	return _push_opt_string(L, g->continent_code);

    return 0;
}


/**
 * The result is from the country edition.  Provide access to the results.
 */
static int _record_index_country(lua_State *L, Result *g,
    const char *field)
{
    int id = g->id;

    if (!field) {
	lua_pushfstring(L, "%s (%s)",
	    GeoIP_name_by_id(id),
	    GeoIP_code_by_id(id));
	return 1;
    }

    if (!strcmp(field, "country"))
	return _push_opt_string(L, GeoIP_name_by_id(id));
    if (!strcmp(field, "country_code"))
	return _push_opt_string(L, GeoIP_code_by_id(id));
    if (!strcmp(field, "continent"))
	return _push_opt_string(L, GeoIP_continent_by_id(id));

    return 0;
}


/**
 * The result is from the region edition.
 */
static int _record_index_region(lua_State *L, Result *g, const char *field)
{
    GeoIPRegion *reg = g->reg;

    if (!field) {
	lua_pushfstring(L, "%s, %s", reg->region,
	    reg->country_code);
	return 1;
    }

    if (!strcmp(field, "country_code"))
	return _push_opt_string(L, reg->country_code);
    if (!strcmp(field, "region"))
	return _push_opt_string(L, reg->region);
    if (!strcmp(field, "time_zone"))
	return _push_opt_string(L, GeoIP_time_zone_by_country_and_region(
	    reg->country_code, reg->region));

    return 0;
}


/**
 * Call the appropriate handler depending on the type of the result.
 */
static int _result_index_switch(lua_State *L, const char *field)
{
    Result *gir = (Result*) luaL_checkudata(L, 1, RESULT);

    if (gir->gir)
	return _record_index_city(L, gir->gir, field);
    else if (gir->id)
	return _record_index_country(L, gir, field);
    else if (gir->reg)
	return _record_index_region(L, gir, field);

    return 0;
}


/**
 * Handle accesses to fields.
 */
static int l_result_index(lua_State *L)
{
    const char *field = luaL_checkstring(L, 2);
    return _result_index_switch(L, field);
}


/**
 * If the whole result is to be converted to a string, use a NULL field name,
 * which then causes a default representation to be returned.
 */
static int l_result_tostring(lua_State *L)
{
    return _result_index_switch(L, NULL);
}


/**
 * Garbage collect a result.
 */
static int l_result_gc(lua_State *L)
{
    Result *gir = (Result*) luaL_checkudata(L, 1, RESULT);
    if (gir->gir) {
	GeoIPRecord_delete(gir->gir);
	gir->gir = NULL;
    }
    if (gir->reg) {
	GeoIPRegion_delete(gir->reg);
	gir->reg = NULL;
    }
    return 0;
}

static const luaL_Reg result_methods[] = {
    { "__tostring", l_result_tostring },
    { "__gc", l_result_gc },
    { "__index", l_result_index },
    { NULL, NULL }
};


/* ----- GeoIP ----- */

typedef struct {
    GeoIP *gi;
} lua_geoip;


/**
 * Look up a host name or an IP address.  The API functions _by_addr seem
 * to be superfluous, as _by_name works just as well with IP addresses.
 *
 * @param gi  GeoIP object
 * @param name  Host name or IP address to look up
 * @return  On success, a GeoIPRecord is returned, else nil.
 */
static int l_by_name(lua_State *L)
{
    lua_geoip *lgi = (lua_geoip*) luaL_checkudata(L, 1, GEOIP);
    Result gir, *p;
    const char *hostname = luaL_checkstring(L, 2);

    memset(&gir, 0, sizeof(gir));

    switch (lgi->gi->databaseType) {
	case GEOIP_COUNTRY_EDITION:;
	int id = GeoIP_id_by_name(lgi->gi, hostname);
	if (!id)
	    return 0;
	gir.id = id;
	break;

	case GEOIP_REGION_EDITION_REV1:;
	GeoIPRegion *reg = GeoIP_region_by_name(lgi->gi, hostname);
	if (!reg)
	    return 0;
	gir.reg = reg;
	break;

	case GEOIP_CITY_EDITION_REV1:;
	GeoIPRecord *r = GeoIP_record_by_name(lgi->gi, hostname);
	if (!r)
	    return 0;
	gir.gir = r;
	break;

	default:
	return 0;
    }

    // success - create the Result object.
    p = (Result*) lua_newuserdata(L, sizeof(*p));
    memcpy(p, &gir, sizeof(*p));
    if (luaL_newmetatable(L, RESULT))
	luaL_register(L, NULL, result_methods);
    lua_setmetatable(L, -2);
    return 1;
}


/**
 * Returns the database type in use.
 */
static int l_geoip_tostring(lua_State *L)
{
    lua_geoip *lgi = (lua_geoip*) luaL_checkudata(L, 1, GEOIP);
    lua_pushstring(L, GeoIPDBDescription[(int)lgi->gi->databaseType]);
    return 1;
}


/**
 * Clean up after using a GeoIP database.  Even after this, existing
 * results seem to continue to work.
 */
static int l_geoip_gc(lua_State *L)
{
    lua_geoip *lgi = (lua_geoip*) luaL_checkudata(L, 1, GEOIP);
    if (lgi->gi) {
	GeoIP_delete(lgi->gi);
	lgi->gi = NULL;
    }
    return 0;
}

static const luaL_Reg geoip_methods[] = {
    { "__tostring", l_geoip_tostring },
    { "__gc", l_geoip_gc },
    { "lookup", l_by_name },
    { NULL, NULL }
};


static int _open_common(lua_State *L, GeoIP *gi)
{
    lua_geoip *lgi;

    if (!gi)
	return 0;

    lgi = (lua_geoip*) lua_newuserdata(L, sizeof(*lgi));
    lgi->gi = gi;

    if (luaL_newmetatable(L, GEOIP)) {
	luaL_register(L, NULL, geoip_methods);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);

    return 1;
}


/**
 * Open a GeoIP database file by specifying the desired type.  The default
 * file name for this type will be used.
 *
 * @param type  The database type to open.  can be "city" or "country".
 * @return  A GeoIP object or nil on error.
 */
static int l_open_type(lua_State *L)
{
    const char *type_name = luaL_checkstring(L, 1);
    GeoIPDBTypes type;

    if (!strcmp(type_name, "city"))
	type = GEOIP_CITY_EDITION_REV1;
    else if (!strcmp(type_name, "country"))
	type = GEOIP_COUNTRY_EDITION;
    else
	return luaL_error(L, "invalid type (city or country)");

    GeoIP *gi = GeoIP_open_type(type, GEOIP_MEMORY_CACHE);
    return _open_common(L, gi);
}


/**
 * Open a GeoIP database file.
 *
 * @param fname  Name of the file to open (including path)
 * @return  A GeoIP object or nil on error.
 */
static int l_open(lua_State *L)
{
    const char *filename = luaL_checkstring(L, 1);
    GeoIP *gi = GeoIP_open(filename, GEOIP_MEMORY_CACHE);
    return _open_common(L, gi);
}


static const luaL_Reg globals[] = {
    { "open_type", l_open_type },
    { "open", l_open },
    { NULL, NULL },
};


/**
 * Initialize this module.  Note that it doesn't automatically create a
 * global table.
 *
 * @return  A table with this module.
 */
int luaopen_geoip(lua_State *L)
{
    lua_newtable(L);
    luaL_register(L, NULL, globals);
    return 1;
}

