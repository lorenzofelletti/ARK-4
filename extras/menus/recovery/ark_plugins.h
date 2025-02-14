enum{
    PLACE_ARK_SAVE,
    PLACE_MS0_SEPLUGINS,
    PLACE_EF0_SEPLUGINS,
    MAX_PLUGINS_PLACES
};

typedef struct {
    char* description;
    unsigned char max_options;
    unsigned char selection;
    unsigned char* config_ptr;
    char* options[2];
    unsigned char place;
} plugin_t;

typedef struct {
    string line;
    plugin_t* plugin;
} plugin_line;

char* plugins_path[] = {
    "PLUGINS.TXT",
    "ms0:/SEPLUGINS/PLUGINS.TXT",
    "ef0:/SEPLUGINS/PLUGINS.TXT"
};

// variable length array
std::vector<plugin_line> plugin_lines[MAX_PLUGINS_PLACES];
settings_entry** ark_plugin_entries = NULL;
int ark_plugins_count = 0;
int ark_plugins_max = 0;
#define MAX_INITIAL_PLUGINS 8

static void addPlugin(plugin_t* plugin){
    if (ark_plugin_entries == NULL){ // create initial table
        ark_plugin_entries = (settings_entry**)malloc(MAX_INITIAL_PLUGINS * sizeof(settings_entry*));
        ark_plugins_max = MAX_INITIAL_PLUGINS;
        ark_plugins_count = 0;
    }
    if (ark_plugins_count >= ark_plugins_max){ // resize table
        settings_entry** new_table = (settings_entry**)malloc(2 * ark_plugins_max * sizeof(settings_entry*));
        for (int i=0; i<ark_plugins_count; i++) new_table[i] = ark_plugin_entries[i];
        free(ark_plugin_entries);
        ark_plugin_entries = new_table;
        ark_plugins_max *= 2;
    }
    ark_plugin_entries[ark_plugins_count++] = (settings_entry*)plugin;
}

static plugin_t* createPlugin(const char* description, unsigned char enable, unsigned char place){
    plugin_t* plugin = (plugin_t*)malloc(sizeof(plugin_t));
    plugin->description = strdup(description);
    plugin->max_options = 2;
    plugin->selection = enable;
    plugin->config_ptr = &(plugin->selection);
    plugin->options[0] = "Disable";
    plugin->options[1] = "Enable";
    plugin->place = place;
    return plugin;
}

static void loadPluginsFile(unsigned char place){
    std::ifstream input(plugins_path[place]);
    for( std::string line; getline( input, line ); ){
        plugin_line pl = { line, NULL };
        if (!isComment(line)){
            string description;
            string enabled;
            int pos = line.rfind(',');
            if (pos != string::npos){
                description = line.substr(0, pos);
                enabled = line.substr(pos+1, line.size());
                
                // trim string
                std::stringstream trimmer;
                trimmer << enabled;
                trimmer.clear();
                trimmer >> enabled;
                
                pl.plugin = createPlugin(description.c_str(), isRunlevelEnabled(enabled)?1:0, place);
                addPlugin(pl.plugin);
            }
        }
        plugin_lines[place].push_back(pl);
    }
    input.close();
}

void loadPlugins(){
    loadPluginsFile(PLACE_ARK_SAVE);
    loadPluginsFile(PLACE_MS0_SEPLUGINS);
    loadPluginsFile(PLACE_EF0_SEPLUGINS);
}

void savePlugins(){
    std::ofstream output[MAX_PLUGINS_PLACES];

    for (int i=0; i<MAX_PLUGINS_PLACES; i++){
        output[i].open(plugins_path[i]);
        for (int j=0; j<plugin_lines[i].size(); j++){
            plugin_line pl = plugin_lines[i][j];
            if (pl.plugin != NULL){
                output[i] << pl.plugin->description << ", " << ((pl.plugin->selection)? "on":"off") << endl;
            }
            else{
                output[i] << pl.line << endl;
            }
        }
        output[i].close();
    }
}
