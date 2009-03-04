/** vim:sw=4:sts=4
 *
 * Bindings for the GeoIP database library by MaxMind.
 * by Wolfgang Oertl 2009
 */

#include <GeoIPCity.h>
#include <lua.h>
#include <lauxlib.h>
#include <unistd.h>
#include <fcntl.h>


/**
 * The "Field" structure describes one field of the result with name and
 * how to extract the data.
 */
struct result_t;
typedef struct _field_t {
    const char *name;		/* name of the field */
    int mode;
    int offset;
    int (*callback)(lua_State *L, struct result_t *r, struct _field_t *f);
} Field;


/**
 * One such structure exists per database type.
 */
typedef struct {
    Field *fields;	/* array of field definitions */
    void (*gc)(lua_State *L, struct result_t *r);
    int (*tostring)(lua_State *L, struct result_t *r);
} ResultMeta;


/**
 * Result is a structure containing all the information on a location
 * and is returned by searching for an IP address or a hostname.
 *
 * Depending on the database used, one of the fields is filled in.  Most of the
 * fields can be read with the appropriate __index method.
 */
typedef struct result_t {
    ResultMeta *meta;
    void *data;		/* some token returned by libGeoIP */
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

static int _access_field(lua_State *L, Result *r, Field *f)
{
    char *p = (char*) r->data;

    switch (f->mode) {
	case 1:
	return _push_opt_string(L, * (char**) (p + f->offset));
	
	case 2:
	lua_pushnumber(L, * (float*) (p + f->offset));
	return 1;
	
	// callback
	case 3:
	return f->callback(L, r, f);
    }

    return 0;
}


/* --------- city database ------------ */

static int city_region_name(lua_State *L, Result *r, Field *f)
{
    GeoIPRecord *g = (GeoIPRecord*) r->data;
    return _push_opt_string(L, GeoIP_region_name_by_code(
	g->country_code, g->region));
}

static int city_time_zone(lua_State *L, Result *r, Field *f)
{
    GeoIPRecord *g = (GeoIPRecord*) r->data;
    return _push_opt_string(L, GeoIP_time_zone_by_country_and_region(
	g->country_code, g->region));
}

static Field city_fields[] = {
    { "city", 1, offsetof(GeoIPRecord, city) },
    { "postal_code", 1, offsetof(GeoIPRecord, postal_code) },
    { "latitude", 2, offsetof(GeoIPRecord, latitude) },
    { "longitude", 2, offsetof(GeoIPRecord, longitude) },
    { "country", 1, offsetof(GeoIPRecord, country_name) },
    { "country_code", 1, offsetof(GeoIPRecord, country_code) },
    { "region", 1, offsetof(GeoIPRecord, region) },
    { "continent", 1, offsetof(GeoIPRecord, continent_code) },
    { "region_name", 3, 0, city_region_name },
    { "time_zone", 3, 0, city_time_zone },
    { NULL, 0, 0 },
};

static int city_tostring(lua_State *L, Result *r)
{
    GeoIPRecord *g = (GeoIPRecord*) r->data;    
    lua_pushfstring(L, "%s, %s (%s)", g->city, g->country_name,
	g->country_code);
    return 1;
}

static void city_gc(lua_State *L, Result *r)
{
    if (r->data) {
	GeoIPRecord_delete((GeoIPRecord*) r->data);
	r->data = NULL;
    }
}

/* ----------- country database ----------- */

typedef const char *(*country_func)(int id);

static country_func country_funcs[] = {
    GeoIP_name_by_id,
    GeoIP_code_by_id,
    GeoIP_continent_by_id
};

static int country_field_access(lua_State *L, Result *r, Field *f)
{
    int id = (int) r->data;
    return _push_opt_string(L, country_funcs[f->offset](id));
}

static Field country_fields[] = {
    { "country", 3, 0, country_field_access },
    { "country_code", 3, 1, country_field_access },
    { "continent", 3, 2, country_field_access },
    { NULL },
};

static int country_tostring(lua_State *L, Result *r)
{
    int id = (int) r->data;
    lua_pushfstring(L, "%s (%s)", GeoIP_name_by_id(id), GeoIP_code_by_id(id));
    return 1;
}

static void country_gc(lua_State *L, Result *r)
{
}

/* ---------- region database ----------- */

static int region_time_zone(lua_State *L, Result *r, Field *f)
{
    GeoIPRegion *g = (GeoIPRegion*) r->data;
    return _push_opt_string(L, GeoIP_time_zone_by_country_and_region(
	g->country_code, g->region));
}

static Field region_fields[] = {
    { "country_code", 1, offsetof(GeoIPRegion, country_code) },
    { "region", 1, offsetof(GeoIPRegion, region) },
    { "time_zone", 3, 0, region_time_zone },
    { NULL },
};

static int region_tostring(lua_State *L, Result *r)
{
    GeoIPRegion *reg = (GeoIPRegion*) r->data;
    lua_pushfstring(L, "%s, %s", reg->region, reg->country_code);
    return 1;
}

static void region_gc(lua_State *L, Result *r)
{
    if (r->data) {
	GeoIPRegion_delete((GeoIPRegion*)r->data);
	r->data = NULL;
    }
}


/* --------------------------------------- */

ResultMeta
    result_meta_city = { city_fields, city_gc, city_tostring },
    result_meta_country = { country_fields, country_gc, country_tostring },
    result_meta_region = { region_fields, region_gc, region_tostring };

/**
 * Handle accesses to fields.
 */
static int l_result_index(lua_State *L)
{
    Result *r = (Result*) luaL_checkudata(L, 1, RESULT);
    const char *name = luaL_checkstring(L, 2);

    Field *f;
    for (f=r->meta->fields; f->name; f++)
	if (!strcmp(f->name, name))
	    return _access_field(L, r, f);
    
    return 0;
}


/**
 * If the whole result is to be converted to a string, use a NULL field name,
 * which then causes a default representation to be returned.
 */
static int l_result_tostring(lua_State *L)
{
    Result *r = (Result*) luaL_checkudata(L, 1, RESULT);
    return r->meta->tostring(L, r);
}

static int l_result_gc(lua_State *L)
{
    Result *r = (Result*) luaL_checkudata(L, 1, RESULT);
    r->meta->gc(L, r);
    return 0;
}


/**
 * Use the result as iterator; return all the fields and values in a loop
 * of this type: for name, value in r do print(name, value) end
 *
 * @param r  The result
 * @param nil  always nil
 * @param name  Name of last accessed field, or nil if first call.
 */
static int l_result_call(lua_State *L)
{
    Result *r = luaL_checkudata(L, 1, RESULT);
    Field *f = r->meta->fields;

    // if not the first call, find the last accessed field, then advance to the
    // next field.
    if (lua_type(L, 3) == LUA_TSTRING) {
	const char *name = lua_tostring(L, 3);
	while (f->name && strcmp(f->name, name))
	    f ++;
	if (f->name)
	    f++;
    }

    if (!f->name)
	return 0;

    lua_pushstring(L, f->name);
    return 1 + _access_field(L, r, f);
}

static const luaL_Reg result_methods[] = {
    { "__tostring", l_result_tostring },
    { "__gc", l_result_gc },
    { "__index", l_result_index },
    { "__call", l_result_call },
    { NULL, NULL }
};


/* ----------------- GeoIP ---------------- */ 

typedef struct {
    GeoIP *gi;
} lua_geoip;


/**
 * Look up a host name or an IP address.  The API functions _by_addr seem
 * to be superfluous, as _by_name works just as well with IP addresses.
 *
 * @param gi  GeoIP object
 * @param name  Host name or IP address to look up
 * @return  On success, a Result object is returned, else nil.
 */
static int l_geoip_lookup(lua_State *L)
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
	gir.meta = &result_meta_country;
	gir.data = (void*) id;
	break;

	case GEOIP_REGION_EDITION_REV1:;
	GeoIPRegion *reg = GeoIP_region_by_name(lgi->gi, hostname);
	if (!reg)
	    return 0;
	gir.meta = &result_meta_region;
	gir.data = (void*) reg;
	break;

	case GEOIP_CITY_EDITION_REV1:;
	GeoIPRecord *r = GeoIP_record_by_name(lgi->gi, hostname);
	if (!r)
	    return 0;
	gir.meta = &result_meta_city;
	gir.data = (void*) r;
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
    { "lookup", l_geoip_lookup },
    { NULL, NULL }
};


/**
 * A GeoIP state has been created; now set up a userdata with metatable
 * to let Lua scripts access it.
 */
static int _open_common(lua_State *L, GeoIP *gi, const char *err_msg)
{
    lua_geoip *lgi;

    if (!gi)
	return luaL_error(L, err_msg);

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
 * libGeoIP prints error messages (e.g. failure to open a data file) on stderr
 * instead of returning it somehow.  To avoid messy output, redirect stderr
 * to a pipe, which is read after the operation.
 */
typedef struct {
    int old_stderr;
    int pipefd[2];
    char buf[200];
} Stderr;

/* set up a pipe to catch stderr */
static void stderr_init(Stderr *e)
{
    pipe(e->pipefd);
    fcntl(e->pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(e->pipefd[1], F_SETFL, O_NONBLOCK);
    e->old_stderr = dup(2);
    dup2(e->pipefd[1], 2);
}

/* read the error message(s) and restore the original stderr */
static void stderr_done(Stderr *e)
{
    int n = read(e->pipefd[0], e->buf, sizeof(e->buf));
    if (n >= 0)
	e->buf[n] = 0;

    close(e->pipefd[0]);
    close(e->pipefd[1]);
    dup2(e->old_stderr, 2);
}


/**
 * Open a GeoIP database file by specifying the desired type(s).  The default
 * file name for each type will be used.  The first successfully opened
 * database is used.  If none of the types are available, raise an error.
 *
 * @param type...  The database type(s) to open.
 * @return  A GeoIP object or nil on error.
 */
static int l_open_type(lua_State *L)
{
    Stderr e;
    int i;
    GeoIPDBTypes type;
    GeoIP *gi = NULL;

    stderr_init(&e);

    for (i = 1; i<=lua_gettop(L); i++) {
	const char *type_name = luaL_checkstring(L, i);

	if (!strcmp(type_name, "city"))
	    type = GEOIP_CITY_EDITION_REV1;
	else if (!strcmp(type_name, "country"))
	    type = GEOIP_COUNTRY_EDITION;
	else if (!strcmp(type_name, "region"))
	    type = GEOIP_REGION_EDITION_REV1;
	else
	    return luaL_error(L, "invalid type (city, country or region)");

	if ((gi = GeoIP_open_type(type, GEOIP_INDEX_CACHE)))
	    break;
    }

    stderr_done(&e);
    return _open_common(L, gi, e.buf);
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
    Stderr e;

    stderr_init(&e);
    GeoIP *gi = GeoIP_open(filename, GEOIP_INDEX_CACHE);
    stderr_done(&e);
    return _open_common(L, gi, e.buf);
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

