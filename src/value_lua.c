
#include <lighttpd/value_lua.h>
#include <lighttpd/condition_lua.h>
#include <lighttpd/actions_lua.h>

/* replace a negative stack index with a positive one,
 * so that you don't need to care about push/pop
 */
static int lua_fixindex(lua_State *L, int ndx) {
	int top;
	if (ndx < 0 && ndx >= -(top = lua_gettop(L)))
		ndx = top + ndx + 1;
	return ndx;
}

static value* value_from_lua_table(server *srv, lua_State *L, int ndx) {
	value *val = NULL, *sub_option;
	GArray *list = NULL;
	GHashTable *hash = NULL;
	int ikey;
	GString *skey;

	ndx = lua_fixindex(L, ndx);
	lua_pushnil(L);
	while (lua_next(L, ndx) != 0) {
		switch (lua_type(L, -2)) {
		case LUA_TNUMBER:
			if (hash) goto mixerror;
			if (!list) {
				val = value_new_list();
				list = val->data.list;
			}
			ikey = lua_tointeger(L, -2);
			if (ikey < 0) {
				ERROR(srv, "Invalid key < 0: %i - skipping entry", ikey);
				lua_pop(L, 1);
				continue;
			}
			sub_option = value_from_lua(srv, L);
			if (!sub_option) continue;
			if ((size_t) ikey >= list->len) {
				g_array_set_size(list, ikey + 1);
			}
			g_array_index(list, value*, ikey) = sub_option;
			break;

		case LUA_TSTRING:
			if (list) goto mixerror;
			if (!hash) {
				val = value_new_hash();
				hash = val->data.hash;
			}
			skey = lua_togstring(L, -2);
			if (g_hash_table_lookup(hash, skey)) {
				ERROR(srv, "Key already exists in hash: '%s' - skipping entry", skey->str);
				lua_pop(L, 1);
				continue;
			}
			sub_option = value_from_lua(srv, L);
			if (!sub_option) {
				g_string_free(skey, TRUE);
				continue;
			}
			g_hash_table_insert(hash, skey, sub_option);
			break;

		default:
			ERROR(srv, "Unexpted key type in table: %s (%i) - skipping entry", lua_typename(L, -1), lua_type(L, -1));
			lua_pop(L, 1);
			break;
		}
	}

	return val;

mixerror:
	ERROR(srv, "%s", "Cannot mix list with hash; skipping remaining part of table");
	lua_pop(L, 2);
	return val;
}


value* value_from_lua(server *srv, lua_State *L) {
	value *val;

	switch (lua_type(L, -1)) {
	case LUA_TNIL:
		lua_pop(L, 1);
		return value_new_none();

	case LUA_TBOOLEAN:
		val = value_new_bool(lua_toboolean(L, -1));
		lua_pop(L, 1);
		return val;

	case LUA_TNUMBER:
		val = value_new_number(lua_tonumber(L, -1));
		lua_pop(L, 1);
		return val;

	case LUA_TSTRING:
		val = value_new_string(lua_togstring(L, -1));
		lua_pop(L, 1);
		return val;

	case LUA_TTABLE:
		val = value_from_lua_table(srv, L, -1);
		lua_pop(L, 1);
		return val;

	case LUA_TUSERDATA:
		{ /* check for action */
			action *a = lua_get_action(L, -1);
			if (a) {
				action_acquire(a);
				lua_pop(L, 1);
				return val = value_new_action(srv, a);
			}
		}
		{ /* check for condition */
			condition *c = lua_get_condition(L, -1);
			if (c) {
				condition_acquire(c);
				lua_pop(L, 1);
				return val = value_new_condition(srv, c);
			}
		}
		ERROR(srv, "%s", "Unknown lua userdata");
		lua_pop(L, 1);
		return NULL;

	case LUA_TLIGHTUSERDATA:
	case LUA_TFUNCTION:
	case LUA_TTHREAD:
	case LUA_TNONE:
	default:
		ERROR(srv, "Unexpected lua type: %s (%i)", lua_typename(L, -1), lua_type(L, -1));
		lua_pop(L, 1);
		return NULL;
	}
}

GString* lua_togstring(lua_State *L, int ndx) {
	const char *buf;
	size_t len = 0;
	GString *str = NULL;

	if (lua_type(L, ndx) == LUA_TSTRING) {
		buf = lua_tolstring(L, ndx, &len);
		if (buf) str = g_string_new_len(buf, len);
	} else {
		lua_pushvalue(L, ndx);
		buf = lua_tolstring(L, -1, &len);
		if (buf) str = g_string_new_len(buf, len);
		lua_pop(L, 1);
	}

	return str;
}
