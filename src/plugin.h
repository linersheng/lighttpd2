#ifndef _LIGHTTPD_PLUGIN_H_
#define _LIGHTTPD_PLUGIN_H_

struct plugin;
typedef struct plugin plugin;

struct plugin_option;
typedef struct plugin_option plugin_option;

struct server_option;
typedef struct server_option server_option;

#define INIT_FUNC(x) \
		LI_EXPORT void * x(server *srv, plugin *)

#define PLUGIN_DATA \
	size_t id; \
	ssize_t option_base_ndx

#include "base.h"
#include "options.h"
#include "module.h"

typedef void     (*PluginInit)        (server *srv, plugin *p);
typedef void     (*PluginFree)        (server *srv, plugin *p);
typedef gboolean (*PluginParseOption) (server *srv, gpointer p_d, size_t ndx, option *opt, gpointer *value);
typedef void     (*PluginFreeOption)  (server *srv, gpointer p_d, size_t ndx, gpointer value);

struct plugin {
	size_t version;
	const gchar *name; /**< name of the plugin */

	gpointer data;    /**< private plugin data */

	PluginFree free; /**< called before plugin is unloaded */

	const plugin_option *options;
};

struct plugin_option {
	const gchar *key;
	option_type type;

	PluginParseOption parse_option;
	PluginFreeOption free_option;
};

struct server_option {
	plugin *p;

	/** the plugin must free the _content_ of the option
	  * opt is zero to get the global default value if nothing is specified
	  * save result in value
	  *
	  * Default behaviour (NULL) is to just use the option as value
	  */
	PluginParseOption parse_option;
	PluginFreeOption free_option;

	size_t index, module_index;
	option_type type;
};

LI_API void plugin_free(plugin *p);
LI_API gboolean plugin_register(server *srv, const gchar *name, PluginInit init);

LI_API gboolean parse_option(server *srv, const char *key, option *opt, option_set *mark);
LI_API void release_option(server *srv, option_set *mark); /**< Does not free the option_set memory */

#endif
