#include "../Apps/text_editor.c"
#include <assert.h>
const t5_app_api_v1 *t5_app_get_api(uint32_t v) { (void)v; return NULL; }
const t5_storage_api_v1 *t5_storage_get_api(uint32_t v) { (void)v; return NULL; }
const t5_file_open_api_v1 *t5_file_open_get_api(uint32_t v) { (void)v; return NULL; }
const t5_provider_capability_api_v1 *t5_provider_capability_get_api(uint32_t v) { (void)v; return NULL; }
static int failure_mode;
static bool reload(const char *p, void *buf, size_t cap, size_t *count) {
    assert(!strcmp(p, "/sd/Documents/a.txt"));
    if (failure_mode == 1) return false;
    if (!buf) { *count = failure_mode == 2 ? TE_CAPACITY + 1 : 5; return true; }
    assert(cap >= 5); memcpy(buf, failure_mode == 4 ? "bad\001!" : "saved", 5);
    *count = failure_mode == 3 ? 4 : 5; return true;
}
int main(void) {
    static const t5_storage_api_v1 api = {.read_file=reload}; storage=&api;
    strcpy(path, "/sd/Documents/a.txt"); strcpy(filename, "a.txt");
    for (failure_mode=1; failure_mode<=4; ++failure_mode) {
        assert(te_import(&document,"edits",5)); document.dirty=true;
        mode=UNSAVED; after=DO_EXIT; key_press(0x07,0);
        assert(mode==UNSAVED && document.dirty && !strcmp(document.text,"edits"));
        assert(!strcmp(filename,"a.txt") && strstr(status,"FAILED"));
    }
    failure_mode=0; key_press(0x07,0);
    assert(mode==DONE && !document.dirty && !strcmp(document.text,"saved"));
    path[0]=0; first_row=9; document.dirty=true;
    assert(discard_changes());
    assert(!document.length && !document.dirty && !path[0] && !filename[0] && first_row==0);
    puts("Text Editor actual discard paths PASS"); return 0;
}
