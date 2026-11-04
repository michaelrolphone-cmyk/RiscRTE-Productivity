#ifndef PRODUCTIVITY_POINTS_SCENE_MODEL_H
#define PRODUCTIVITY_POINTS_SCENE_MODEL_H
/* Points domain state and semantic content only. The scene service owns all
 * layout, input interpretation and presentation. Storage, clocks, alarm
 * refresh and public text entry are explicit effects owned by the caller.
 * A bounded document window is not a catalog limit: navigation traverses the
 * complete dynamically allocated controller catalog using stable record IDs. */
#include "points_catalog_controller.h"
#include "RiscSceneComponentsV1.h"

#define POINTS_SCENE_PAGE_ITEMS 20u
#define POINTS_SCENE_ROUTE 1u

enum points_scene_page {
    POINTS_SCENE_LIST, POINTS_SCENE_EDIT, POINTS_SCENE_TYPES,
    POINTS_SCENE_TIME, POINTS_SCENE_DAYS, POINTS_SCENE_MODE,
    POINTS_SCENE_DURATION, POINTS_SCENE_CUSTOM, POINTS_SCENE_COLOR,
    POINTS_SCENE_NAME
};
enum points_scene_effect {
    POINTS_SCENE_EFFECT_NONE, POINTS_SCENE_EFFECT_SAVE_EVENT,
    POINTS_SCENE_EFFECT_DELETE_EVENT, POINTS_SCENE_EFFECT_SAVE_TYPE,
    POINTS_SCENE_EFFECT_DELETE_TYPE, POINTS_SCENE_EFFECT_RETRY,
    POINTS_SCENE_EFFECT_NAME, POINTS_SCENE_EFFECT_EXIT
};
enum points_scene_action {
    POINTS_SCENE_BACK=1, POINTS_SCENE_ADD, POINTS_SCENE_OPEN_EVENT,
    POINTS_SCENE_TYPES_ACTION, POINTS_SCENE_SELECT_TYPE, POINTS_SCENE_EDIT_TYPE,
    POINTS_SCENE_ADD_TYPE, POINTS_SCENE_PREVIOUS, POINTS_SCENE_NEXT,
    POINTS_SCENE_TIME_ACTION, POINTS_SCENE_DAYS_ACTION,
    POINTS_SCENE_DURATION_ACTION, POINTS_SCENE_MODE_ACTION,
    POINTS_SCENE_NOTIFY_END, POINTS_SCENE_WARN3, POINTS_SCENE_ENABLED,
    POINTS_SCENE_SAVE, POINTS_SCENE_DELETE, POINTS_SCENE_DONE,
    POINTS_SCENE_TIME_VALUE, POINTS_SCENE_DURATION_VALUE,
    POINTS_SCENE_MODE_VALUE, POINTS_SCENE_DAY_VALUE, POINTS_SCENE_DAY_PRESET,
    POINTS_SCENE_NAME_ACTION, POINTS_SCENE_COLOR_ACTION,
    POINTS_SCENE_COLOR_VALUE, POINTS_SCENE_DURATION_ENABLED,
    POINTS_SCENE_RETRY_ACTION
};
typedef struct {
    uint32_t node, action, kind, record;
    int32_t minimum, maximum;
    bool enabled;
} points_scene_binding;
typedef struct {
    points_editor *editor;
    enum points_scene_page page, return_page;
    enum points_scene_effect pending_effect;
    uint32_t first, revision, committed_revision, screen_key;
    uint32_t change_revision, candidate_change_revision;
    uint64_t last_sequence;
    unsigned time_format;
    uint32_t event_limit, custom_type_limit, type_name_limit;
    bool legacy_type_metadata_only, retry_writes;
    bool ready, clock_valid, service_valid, service_busy, visual_only;
    bool delete_confirm, accepting, dirty;
    points_catalog_item subpage_event;
    points_catalog_type subpage_type;
    points_scene_binding bindings[RISC_COMPONENTS_MAX_NODES];
    uint32_t binding_count;
    points_scene_binding candidate_bindings[RISC_COMPONENTS_MAX_NODES];
    uint32_t candidate_count;
    char notice[RISC_SCENE_TEXT];
    char clock_text[64], schedule_status[64];
    uint32_t next_event_id;
} points_scene_model;

static inline void points_scene_changed(points_scene_model *m) {
    m->dirty=true;++m->change_revision;
}
static inline void points_scene_notice(points_scene_model *m,const char *s) {
    snprintf(m->notice,sizeof(m->notice),"%s",s?s:"");points_scene_changed(m);
}
static inline void points_scene_init(points_scene_model *m,points_editor *editor) {
    memset(m,0,sizeof(*m));m->editor=editor;m->screen_key=1;
    m->ready=m->clock_valid=m->service_valid=true;points_scene_changed(m);
    m->type_name_limit=POINTS_CATALOG_NAME_MAX;
}
/* Call only after closing the old scene and opening a fresh input session. */
static inline void points_scene_begin_session(points_scene_model *m) {
    m->revision=m->committed_revision=0;m->last_sequence=0;m->accepting=false;points_scene_changed(m);
}
static inline bool points_scene_subpage(enum points_scene_page page) {
    return page==POINTS_SCENE_TIME||page==POINTS_SCENE_DAYS||
        page==POINTS_SCENE_MODE||page==POINTS_SCENE_DURATION||page==POINTS_SCENE_COLOR;
}
static inline void points_scene_page_to(points_scene_model *m,enum points_scene_page next) {
    if(points_scene_subpage(next)) {
        m->subpage_event=m->editor->event;m->subpage_type=m->editor->type;
        m->return_page=m->page;
    }
    m->page=next;m->first=0;m->delete_confirm=false;points_scene_changed(m);
    m->screen_key=m->screen_key==UINT32_MAX?1:m->screen_key+1;
}
static inline const points_catalog_type *points_scene_type(const points_scene_model *m,uint32_t id) {
    return points_catalog_find_type(&m->editor->store.saved,id);
}
static inline const char *points_scene_type_name(const points_scene_model *m,uint32_t id) {
    const points_catalog_type *t=points_scene_type(m,id);return t?t->name:"Unknown type";
}
static inline bool points_scene_duration(const points_scene_model *m) {
    const points_catalog_type *t=points_scene_type(m,m->editor->event.type_id);
    return t&&(t->flags&POINTS_TYPE_DURATION);
}
static inline const char *points_scene_status(const points_scene_model *m) {
    const points_editor *e=m->editor;
    if(!m->ready)return "Dependencies unavailable: retry";
    if(e->store.retained)return "Storage custody uncertain: restart required";
    if(e->store.uncertain)return m->retry_writes?"Save unconfirmed: retry pending save":"Save unconfirmed: check storage";
    if(e->conflict)return "Storage changed: reload before saving";
    if(!e->store.loaded)return "Saved data unavailable: retry";
    if(!e->index_ready)return "List unavailable: retry memory";
    if(e->error==RISC_APP_DATA_NO_SPACE)return "Storage full: draft kept";
    if(e->error==POINTS_STORAGE_MEMORY)return "Not enough memory: draft kept";
    if(!m->clock_valid)return "Time unavailable: retry";
    if(!m->service_valid)return "Alarm service unavailable: retry";
    if(m->service_busy)return "Finish the alert before saving";
    if(m->schedule_status[0])return m->schedule_status;
    return m->notice;
}
static inline bool points_scene_save_allowed(const points_scene_model *m) {
    return m->ready&&points_editor_editable(m->editor)&&m->service_valid&&
        !m->service_busy&&(m->page==POINTS_SCENE_CUSTOM||m->clock_valid);
}
static inline enum points_scene_effect points_scene_emit(points_scene_model *m,enum points_scene_effect effect) {
    m->pending_effect=effect;points_scene_changed(m);return effect;
}
static inline enum points_scene_effect points_scene_back(points_scene_model *m) {
    points_editor *e=m->editor;
    if(e->store.uncertain||e->store.retained) {
        points_scene_notice(m,m->retry_writes?"Retry save before leaving":"Check save before leaving");return POINTS_SCENE_EFFECT_NONE;
    }
    if(m->page==POINTS_SCENE_LIST)return points_scene_emit(m,POINTS_SCENE_EFFECT_EXIT);
    if(m->page==POINTS_SCENE_EDIT) {
        points_editor_cancel_event(e);points_scene_page_to(m,POINTS_SCENE_LIST);
        points_scene_notice(m,"Draft discarded");
    } else if(m->page==POINTS_SCENE_CUSTOM) {
        points_editor_cancel_type(e);points_scene_page_to(m,POINTS_SCENE_TYPES);
        points_scene_notice(m,"Type draft discarded");
    } else if(m->page==POINTS_SCENE_TYPES) {
        points_scene_page_to(m,e->editing_event?POINTS_SCENE_EDIT:POINTS_SCENE_LIST);
    } else if(points_scene_subpage(m->page)) {
        e->event=m->subpage_event;e->type=m->subpage_type;
        points_scene_page_to(m,m->return_page);
    }
    return POINTS_SCENE_EFFECT_NONE;
}
static inline enum points_scene_effect points_scene_save(points_scene_model *m,bool remove) {
    points_editor *e=m->editor;
    if(!points_scene_save_allowed(m))return POINTS_SCENE_EFFECT_NONE;
    if(m->page==POINTS_SCENE_CUSTOM) {
        size_t n=strlen(e->type.name);while(n&&e->type.name[n-1]==' ')e->type.name[--n]=0;
        if(!remove&&!n){points_scene_notice(m,"Choose a nonempty type name");return POINTS_SCENE_EFFECT_NONE;}
        return points_scene_emit(m,remove?POINTS_SCENE_EFFECT_DELETE_TYPE:POINTS_SCENE_EFFECT_SAVE_TYPE);
    }
    if(m->page!=POINTS_SCENE_EDIT)return POINTS_SCENE_EFFECT_NONE;
    if(!remove&&e->event.enabled&&!e->event.weekdays) {
        points_scene_notice(m,"Choose at least one weekday");points_scene_page_to(m,POINTS_SCENE_DAYS);
        return POINTS_SCENE_EFFECT_NONE;
    }
    return points_scene_emit(m,remove?POINTS_SCENE_EFFECT_DELETE_EVENT:POINTS_SCENE_EFFECT_SAVE_EVENT);
}
/* The service owner calls the existing controller once, then supplies its
 * result. Uncertain outcomes retain the draft. The negotiated retry capability
 * distinguishes read-only catalog Check from legacy pending-write Retry.
 * A successful save additionally requires the service owner to refresh alarms. */
static inline void points_scene_saved(points_scene_model *m,int32_t rc,unsigned action) {
    m->pending_effect=POINTS_SCENE_EFFECT_NONE;m->accepting=false;points_scene_changed(m);
    points_editor *e=m->editor;
    if(e->store.retained)return;
    if(rc==RISC_APP_DATA_OK) {
        if(action==POINTS_EDITOR_TYPE) {
            if(e->editing_event)(void)points_catalog_apply_type(&e->event,&e->store.saved,e->type.id);
            points_scene_page_to(m,e->editing_event?POINTS_SCENE_EDIT:POINTS_SCENE_TYPES);
            points_scene_notice(m,"Type saved");
        } else if(action==POINTS_EDITOR_DELETE_TYPE) {
            points_scene_page_to(m,POINTS_SCENE_TYPES);points_scene_notice(m,"Type deleted");
        } else {
            points_scene_page_to(m,POINTS_SCENE_LIST);
            points_scene_notice(m,action==POINTS_EDITOR_DELETE?"Point deleted":"Point saved");
        }
    } else if(e->store.uncertain)points_scene_notice(m,m->retry_writes?"Retry writes the pending saved draft, then verifies it":"Check reads storage; it never rewrites");
    else if(rc==RISC_APP_DATA_STALE)points_scene_notice(m,"Review the reloaded draft before saving");
    else if(rc==RISC_APP_DATA_UNAVAILABLE)points_scene_notice(m,"Type is used by saved points");
    else if(rc==RISC_APP_DATA_INVALID)points_scene_notice(m,"Check name, days and duration");
    else points_scene_notice(m,"Save failed: draft kept");
}
static inline void points_scene_retry_done(points_scene_model *m) {
    m->pending_effect=POINTS_SCENE_EFFECT_NONE;m->accepting=false;points_scene_changed(m);
}
static inline bool points_scene_name_result(points_scene_model *m,bool accepted,const char *name) {
    if(!m||m->pending_effect!=POINTS_SCENE_EFFECT_NAME||m->page!=POINTS_SCENE_NAME)return false;
    m->pending_effect=POINTS_SCENE_EFFECT_NONE;m->accepting=false;points_scene_changed(m);
    bool valid=true,nonspace=false;size_t n=0;
    if(accepted) {
        if(!name)valid=false;
        else for(;n<=POINTS_CATALOG_NAME_MAX&&name[n];n++) {
            unsigned char ch=(unsigned char)name[n];
            if(ch<32||ch>126)valid=false;
            if(ch!=' ')nonspace=true;
        }
        if(n>POINTS_CATALOG_NAME_MAX||n>m->type_name_limit||!nonspace)valid=false;
        if(valid) {memset(m->editor->type.name,0,sizeof(m->editor->type.name));memcpy(m->editor->type.name,name,n);}
    }
    points_scene_page_to(m,POINTS_SCENE_CUSTOM);
    points_scene_notice(m,!accepted?"Name draft discarded":valid?"Name draft updated":"Choose a nonempty type name");
    return !accepted||valid;
}
static inline void points_scene_set_duration(points_scene_model *m,uint16_t value) {
    points_editor *e=m->editor;
    if(m->return_page==POINTS_SCENE_CUSTOM) {
        e->type.duration_minutes=value;
        if(!value)e->type.flags&=(uint8_t)~POINTS_TYPE_NOTIFY_END;
        if(value<3)e->type.flags&=(uint8_t)~POINTS_TYPE_WARN3;
    } else {
        e->event.duration_minutes=value;if(!value)e->event.notify_end=0;if(value<3)e->event.warn3=0;
    }
}
static inline uint32_t points_scene_custom_count(const points_scene_model *m) {
    uint32_t count=0;
    for(uint32_t i=0;i<m->editor->store.saved.type_count;i++)
        if(m->editor->store.saved.types[i].id>POINTS_BEDTIME)count++;
    return count;
}
static inline uint32_t points_scene_count(const points_scene_model *m) {
    return m->page==POINTS_SCENE_TYPES?m->editor->store.saved.type_count:m->editor->order_count;
}
static inline enum points_scene_effect points_scene_reduce(points_scene_model *m,const risc_scene_event_v1 *event) {
    if(!m||!m->editor||!event||event->struct_size<sizeof(*event)||!m->accepting||
       m->pending_effect||m->editor->store.retained||event->document_revision!=m->committed_revision||
       !event->sequence||event->sequence<=m->last_sequence)return POINTS_SCENE_EFFECT_NONE;
    const points_scene_binding *binding=NULL;
    if(event->node==0&&event->action==POINTS_SCENE_BACK&&event->kind==RISC_SCENE_ACTION_EVENT&&!event->value) {
        static const points_scene_binding back={.action=POINTS_SCENE_BACK};binding=&back;
    } else for(uint32_t i=0;i<m->binding_count;i++) {
        const points_scene_binding *b=&m->bindings[i];
        if(b->enabled&&b->node==event->node&&b->action==event->action&&b->kind==event->kind&&
           event->value>=b->minimum&&event->value<=b->maximum){binding=b;break;}
    }
    if(!binding)return POINTS_SCENE_EFFECT_NONE;
    m->last_sequence=event->sequence;m->accepting=false;points_scene_changed(m);
    points_editor *e=m->editor;uint32_t record=binding->record;unsigned action=event->action;
    if(action!=POINTS_SCENE_DELETE)m->delete_confirm=false;
    if(action==POINTS_SCENE_BACK)return points_scene_back(m);
    if(action==POINTS_SCENE_RETRY_ACTION)return points_scene_emit(m,POINTS_SCENE_EFFECT_RETRY);
    if(!m->ready||!points_editor_editable(e))return POINTS_SCENE_EFFECT_NONE;
    switch(action) {
    case POINTS_SCENE_ADD:
    case POINTS_SCENE_OPEN_EVENT:
        if(points_editor_begin_event(e,action==POINTS_SCENE_ADD?0:record)) {
            points_scene_page_to(m,POINTS_SCENE_EDIT);points_scene_notice(m,"Draft only: Save keeps changes");
        }break;
    case POINTS_SCENE_TYPES_ACTION:points_scene_page_to(m,POINTS_SCENE_TYPES);break;
    case POINTS_SCENE_SELECT_TYPE:
        if(e->editing_event&&points_catalog_apply_type(&e->event,&e->store.saved,record)==POINTS_CATALOG_OK) {
            points_scene_page_to(m,POINTS_SCENE_EDIT);points_scene_notice(m,"Type defaults applied to draft");
        }break;
    case POINTS_SCENE_ADD_TYPE:
    case POINTS_SCENE_EDIT_TYPE:
        if(points_editor_begin_type(e,action==POINTS_SCENE_ADD_TYPE?0:record)) {
            points_scene_page_to(m,POINTS_SCENE_CUSTOM);points_scene_notice(m,"Type draft only");
        }break;
    case POINTS_SCENE_PREVIOUS:
        m->first=m->first>POINTS_SCENE_PAGE_ITEMS?m->first-POINTS_SCENE_PAGE_ITEMS:0;
        m->screen_key=m->screen_key==UINT32_MAX?1:m->screen_key+1;break;
    case POINTS_SCENE_NEXT:
        if(points_scene_count(m)>m->first&&points_scene_count(m)-m->first>POINTS_SCENE_PAGE_ITEMS)m->first+=POINTS_SCENE_PAGE_ITEMS;
        m->screen_key=m->screen_key==UINT32_MAX?1:m->screen_key+1;break;
    case POINTS_SCENE_TIME_ACTION:points_scene_page_to(m,POINTS_SCENE_TIME);break;
    case POINTS_SCENE_DAYS_ACTION:points_scene_page_to(m,POINTS_SCENE_DAYS);break;
    case POINTS_SCENE_DURATION_ACTION:points_scene_page_to(m,POINTS_SCENE_DURATION);break;
    case POINTS_SCENE_MODE_ACTION:points_scene_page_to(m,POINTS_SCENE_MODE);break;
    case POINTS_SCENE_COLOR_ACTION:points_scene_page_to(m,POINTS_SCENE_COLOR);break;
    case POINTS_SCENE_NOTIFY_END:
        if(m->page==POINTS_SCENE_CUSTOM)e->type.flags=(uint8_t)((e->type.flags&~POINTS_TYPE_NOTIFY_END)|(event->value?POINTS_TYPE_NOTIFY_END:0));
        else e->event.notify_end=(uint8_t)event->value;
        break;
    case POINTS_SCENE_WARN3:
        if(m->page==POINTS_SCENE_CUSTOM)e->type.flags=(uint8_t)((e->type.flags&~POINTS_TYPE_WARN3)|(event->value?POINTS_TYPE_WARN3:0));
        else e->event.warn3=(uint8_t)event->value;
        break;
    case POINTS_SCENE_ENABLED:e->event.enabled=(uint8_t)event->value;break;
    case POINTS_SCENE_DURATION_ENABLED:
        if(event->value)e->type.flags|=POINTS_TYPE_DURATION;
        else {e->type.flags=0;e->type.duration_minutes=0;}break;
    case POINTS_SCENE_SAVE:return points_scene_save(m,false);
    case POINTS_SCENE_DELETE:
        if(!(m->page==POINTS_SCENE_CUSTOM?e->type.id:e->event.id))return points_scene_back(m);
        if(m->delete_confirm)return points_scene_save(m,true);
        m->delete_confirm=true;points_scene_notice(m,"Choose Delete again to confirm");break;
    case POINTS_SCENE_DONE:points_scene_page_to(m,m->return_page);break;
    case POINTS_SCENE_TIME_VALUE:e->event.hour=(uint8_t)(event->value/60);e->event.minute=(uint8_t)(event->value%60);break;
    case POINTS_SCENE_DURATION_VALUE:points_scene_set_duration(m,(uint16_t)event->value);break;
    case POINTS_SCENE_MODE_VALUE:
        if(m->return_page==POINTS_SCENE_CUSTOM)e->type.mode=(uint8_t)event->value;
        else e->event.mode=(uint8_t)event->value;
        break;
    case POINTS_SCENE_DAY_VALUE:
        e->event.weekdays=(uint8_t)((e->event.weekdays&~(1u<<record))|(event->value?(1u<<record):0));break;
    case POINTS_SCENE_DAY_PRESET:e->event.weekdays=(uint8_t)record;break;
    case POINTS_SCENE_COLOR_VALUE:e->type.color=record;break;
    case POINTS_SCENE_NAME_ACTION:
        points_scene_page_to(m,POINTS_SCENE_NAME);return points_scene_emit(m,POINTS_SCENE_EFFECT_NAME);
    default:break;
    }
    return POINTS_SCENE_EFFECT_NONE;
}

static inline void points_scene_node(points_scene_model *m,risc_components_document_v1 *d,
        uint32_t id,uint32_t kind,uint32_t action,const char *label,const char *text,
        int32_t value,int32_t maximum,bool enabled,uint32_t record) {
    if(d->node_count>=RISC_COMPONENTS_MAX_NODES)return;
    uint32_t i=d->node_count++;
    risc_scene_node_v1 *n=&d->nodes[i];
    *n=(risc_scene_node_v1){.id=id,.route=POINTS_SCENE_ROUTE,.kind=kind,.action=action,
        .flags=enabled?0:RISC_SCENE_DISABLED,.value=value,.maximum=maximum,.step=1};
    snprintf(n->label,sizeof(n->label),"%.*s",(int)sizeof(n->label)-1,label?label:"");
    snprintf(n->text,sizeof(n->text),"%s",text?text:"");
    if(action) {
        bool is_value=kind==RISC_COMPONENT_SWITCH||kind==RISC_COMPONENT_STEPPER||
            kind==RISC_COMPONENT_TIME_PICKER||kind==RISC_COMPONENT_SEGMENTS;
        m->candidate_bindings[m->candidate_count++]=(points_scene_binding){.node=id,.action=action,
            .kind=is_value?RISC_SCENE_VALUE_EVENT:RISC_SCENE_ACTION_EVENT,
            .minimum=0,.maximum=is_value?maximum:0,.enabled=enabled,.record=record};
    }
}
static inline void points_scene_action_node(points_scene_model *m,risc_components_document_v1 *d,
        unsigned action,const char *label,bool enabled) {
    points_scene_node(m,d,action,RISC_SCENE_ACTION,action,label,"",0,0,enabled,0);
}
static inline void points_scene_row(points_scene_model *m,risc_components_document_v1 *d,
        unsigned action,const char *label,const char *text,bool enabled) {
    points_scene_node(m,d,action,RISC_COMPONENT_ROW,action,label,text,0,0,enabled,0);
}
static inline void points_scene_switch(points_scene_model *m,risc_components_document_v1 *d,
        unsigned action,const char *label,bool value,bool enabled) {
    points_scene_node(m,d,action,RISC_COMPONENT_SWITCH,action,label,"",value,1,enabled,0);
}
static inline void points_scene_pagination(points_scene_model *m,risc_components_document_v1 *d,uint32_t count,bool enabled) {
    points_scene_action_node(m,d,POINTS_SCENE_PREVIOUS,"Previous",enabled&&m->first>0);
    points_scene_action_node(m,d,POINTS_SCENE_NEXT,"Next",enabled&&count>m->first&&count-m->first>POINTS_SCENE_PAGE_ITEMS);
}
static inline bool points_scene_build(points_scene_model *m,risc_components_document_v1 *d) {
    if(!m||!m->editor||!d||m->editor->store.retained||m->revision==UINT32_MAX)return false;
    points_editor *e=m->editor;
    memset(d,0,sizeof(*d));m->candidate_count=0;m->accepting=false;
    d->api_version=1;d->struct_size=sizeof(*d);d->revision=++m->revision;
    d->root=POINTS_SCENE_ROUTE;d->route_count=1;d->screen_key=m->screen_key;
    d->routes[0].id=POINTS_SCENE_ROUTE;d->routes[0].back_action=POINTS_SCENE_BACK;
    snprintf(d->subtitle,sizeof(d->subtitle),"%s",points_scene_status(m));
    const char *title="Points in Time";
    bool usable=m->ready&&points_editor_editable(e),enabled=usable&&!m->pending_effect;
    if(!usable) {
        points_scene_node(m,d,100,RISC_SCENE_TEXT_NODE,0,"Saved points are preserved",
            e->store.uncertain?(m->retry_writes?"Retry writes the pending saved draft, then verifies it":"Check reads the saved file without rewriting"):"Retry to load the catalog",0,0,true,0);
        points_scene_action_node(m,d,POINTS_SCENE_BACK,"Back",!m->pending_effect&&!e->store.uncertain);
        points_scene_action_node(m,d,POINTS_SCENE_RETRY_ACTION,e->store.uncertain?(m->retry_writes?"Retry save":"Check save"):"Retry",!m->pending_effect);
    } else if(m->page==POINTS_SCENE_LIST||m->page==POINTS_SCENE_TYPES) {
        bool types=m->page==POINTS_SCENE_TYPES;
        title=types?"Point types":"Points in Time";
        uint32_t count=types?e->store.saved.type_count:e->order_count;
        if(m->first>=count)m->first=count?((count-1)/POINTS_SCENE_PAGE_ITEMS)*POINTS_SCENE_PAGE_ITEMS:0;
        char summary[72];snprintf(summary,sizeof(summary),"%u %s; items %u to %u",(unsigned)count,types?"types":"points",
            count?(unsigned)(m->first+1):0,(unsigned)(count-m->first>POINTS_SCENE_PAGE_ITEMS?m->first+POINTS_SCENE_PAGE_ITEMS:count));
        points_scene_node(m,d,100,RISC_SCENE_TEXT_NODE,0,summary,"",0,0,true,0);
        if(!types&&m->clock_text[0])points_scene_node(m,d,102,RISC_SCENE_TEXT_NODE,0,"Local time",m->clock_text,0,0,true,0);
        uint32_t shown=count-m->first;if(shown>POINTS_SCENE_PAGE_ITEMS)shown=POINTS_SCENE_PAGE_ITEMS;
        for(uint32_t i=0;i<shown;i++) {
            char value[72];
            if(types) {
                const points_catalog_type *t=&e->store.saved.types[m->first+i];
                snprintf(value,sizeof(value),"%u min; mode %u%s",t->duration_minutes,t->mode,
                    e->editing_event&&e->event.type_id==t->id?"; selected":"");
                points_scene_node(m,d,1000+i,RISC_COMPONENT_ROW,POINTS_SCENE_SELECT_TYPE,t->name,value,
                    0,0,enabled&&e->editing_event,t->id);
                if(t->id>POINTS_BEDTIME) {
                    snprintf(value,sizeof(value),"Edit %s",t->name);
                    points_scene_node(m,d,2000+i,RISC_SCENE_ACTION,POINTS_SCENE_EDIT_TYPE,value,"",0,0,enabled,t->id);
                }
            } else {
                const points_catalog_item *item=points_editor_at(e,m->first+i);
                char tm[24],days[32];points_editor_time(item,m->time_format,tm,sizeof(tm));points_editor_days(item->weekdays,days,sizeof(days));
                snprintf(value,sizeof(value),"%.20s | %.28s | %u min%s",tm,days,item->duration_minutes,item->enabled?"":" | Off");
                points_scene_node(m,d,1000+i,RISC_COMPONENT_ROW,POINTS_SCENE_OPEN_EVENT,
                    points_scene_type_name(m,item->type_id),value,0,0,enabled,item->id);
                if(m->next_event_id==item->id)snprintf(d->details[d->node_count-1].badge,sizeof(d->details[0].badge),"Next");
            }
        }
        if(!count)points_scene_node(m,d,101,RISC_COMPONENT_EMPTY,0,types?"No types yet":"No points yet","",0,0,true,0);
        points_scene_pagination(m,d,count,enabled);
        points_scene_action_node(m,d,types?POINTS_SCENE_ADD_TYPE:POINTS_SCENE_ADD,types?"Add custom type":"Add point",enabled&&(types?
            !m->legacy_type_metadata_only&&(!m->custom_type_limit||points_scene_custom_count(m)<m->custom_type_limit):
            e->store.saved.type_count&&(!m->event_limit||e->store.saved.event_count<m->event_limit)));
        if(!types)points_scene_action_node(m,d,POINTS_SCENE_TYPES_ACTION,"Manage types",enabled);
        points_scene_action_node(m,d,POINTS_SCENE_BACK,"Back",!m->pending_effect);
    } else if(m->page==POINTS_SCENE_EDIT||m->page==POINTS_SCENE_CUSTOM) {
        bool custom=m->page==POINTS_SCENE_CUSTOM;
        title=custom?"Custom type":e->event.id?"Edit point":"Add point";
        char value[72];uint16_t duration=custom?e->type.duration_minutes:e->event.duration_minutes;
        if(custom) {
            points_scene_row(m,d,POINTS_SCENE_NAME_ACTION,"Name",e->type.name[0]?e->type.name:"Enter a type name",enabled);
            snprintf(value,sizeof(value),"#%06X",(unsigned)e->type.color);
            points_scene_row(m,d,POINTS_SCENE_COLOR_ACTION,"Color",value,enabled);
            if(!m->legacy_type_metadata_only)points_scene_switch(m,d,POINTS_SCENE_DURATION_ENABLED,"Duration option",!!(e->type.flags&POINTS_TYPE_DURATION),enabled);
        } else {
            points_scene_row(m,d,POINTS_SCENE_TYPES_ACTION,"Type",points_scene_type_name(m,e->event.type_id),enabled);
            points_editor_time(&e->event,m->time_format,value,sizeof(value));
            points_scene_row(m,d,POINTS_SCENE_TIME_ACTION,"Time",value,enabled);
            points_editor_days(e->event.weekdays,value,sizeof(value));
            points_scene_row(m,d,POINTS_SCENE_DAYS_ACTION,"Days",value,enabled);
        }
        if(!custom||!m->legacy_type_metadata_only) {
        snprintf(value,sizeof(value),"%u minutes",duration);
        points_scene_row(m,d,POINTS_SCENE_DURATION_ACTION,custom?"Default duration":"Duration",value,
            enabled&&(custom?!!(e->type.flags&POINTS_TYPE_DURATION):points_scene_duration(m)));
        static const char *const modes[]={"System default","Vibrate","Sound","Sound + vibrate"};
        unsigned mode=custom?e->type.mode:e->event.mode;
        points_scene_row(m,d,POINTS_SCENE_MODE_ACTION,custom?"Default mode":"Notify start",
            !custom&&m->visual_only?"Visual only: saved mode kept":mode<4?modes[mode]:"Unknown",enabled&&(custom||!m->visual_only));
        points_scene_switch(m,d,POINTS_SCENE_NOTIFY_END,"Notify at end",custom?!!(e->type.flags&POINTS_TYPE_NOTIFY_END):!!e->event.notify_end,enabled&&duration>0);
        points_scene_switch(m,d,POINTS_SCENE_WARN3,"3 minute warning",custom?!!(e->type.flags&POINTS_TYPE_WARN3):!!e->event.warn3,enabled&&duration>=3);
        }
        if(!custom)points_scene_switch(m,d,POINTS_SCENE_ENABLED,"Enabled",!!e->event.enabled,enabled);
        if(!custom||!m->legacy_type_metadata_only) {
        points_scene_action_node(m,d,POINTS_SCENE_DELETE,m->delete_confirm?"Confirm delete":custom?"Delete type":"Delete point",enabled&&points_scene_save_allowed(m));
        d->nodes[d->node_count-1].flags|=RISC_SCENE_DESTRUCTIVE;
        }
        points_scene_action_node(m,d,POINTS_SCENE_SAVE,custom?"Save type":"Save point",enabled&&points_scene_save_allowed(m));
        points_scene_action_node(m,d,POINTS_SCENE_BACK,"Cancel",!m->pending_effect);
    } else if(points_scene_subpage(m->page)) {
        bool custom=m->return_page==POINTS_SCENE_CUSTOM;
        if(m->page==POINTS_SCENE_TIME) {
            title="Time";points_scene_node(m,d,POINTS_SCENE_TIME_VALUE,RISC_COMPONENT_TIME_PICKER,
                POINTS_SCENE_TIME_VALUE,"Time","",e->event.hour*60+e->event.minute,1439,enabled,0);
        } else if(m->page==POINTS_SCENE_DURATION) {
            title="Duration";points_scene_node(m,d,POINTS_SCENE_DURATION_VALUE,RISC_COMPONENT_STEPPER,
                POINTS_SCENE_DURATION_VALUE,"Minutes","",custom?e->type.duration_minutes:e->event.duration_minutes,720,enabled,0);
        } else if(m->page==POINTS_SCENE_MODE) {
            title="Alert mode";points_scene_node(m,d,POINTS_SCENE_MODE_VALUE,RISC_COMPONENT_SEGMENTS,
                POINTS_SCENE_MODE_VALUE,"Mode","",custom?e->type.mode:e->event.mode,3,enabled&&(custom||!m->visual_only),0);
            snprintf(d->details[d->node_count-1].choices,sizeof(d->details[0].choices),"Default|Vibrate|Sound|Both");
        } else if(m->page==POINTS_SCENE_DAYS) {
            title="Weekdays";static const char *const days[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
            for(unsigned i=0;i<7;i++)points_scene_node(m,d,1000+i,RISC_COMPONENT_SWITCH,POINTS_SCENE_DAY_VALUE,days[i],"",!!(e->event.weekdays&(1u<<i)),1,enabled,i);
            points_scene_node(m,d,2000,RISC_SCENE_ACTION,POINTS_SCENE_DAY_PRESET,"Every day","",0,0,enabled,127);
            points_scene_node(m,d,2001,RISC_SCENE_ACTION,POINTS_SCENE_DAY_PRESET,"Mon-Fri","",0,0,enabled,62);
            points_scene_node(m,d,2002,RISC_SCENE_ACTION,POINTS_SCENE_DAY_PRESET,"Weekends","",0,0,enabled,65);
        } else {
            title="Type color";static const uint32_t colors[]={0xffd24a,0xff7a1a,0xff3d71,0xb24dff,0x6d7bff,0x3d9bff,0x19e3ff,0x3dff9a};
            static const char *const names[]={"Yellow","Orange","Pink","Purple","Indigo","Blue","Cyan","Green"};
            for(unsigned i=0;i<8;i++) {
                char text[24];snprintf(text,sizeof(text),"#%06X%s",(unsigned)colors[i],e->type.color==colors[i]?" selected":"");
                points_scene_node(m,d,1000+i,RISC_COMPONENT_ROW,POINTS_SCENE_COLOR_VALUE,names[i],text,0,0,enabled,colors[i]);
            }
        }
        points_scene_action_node(m,d,POINTS_SCENE_DONE,"Done",enabled);
        points_scene_action_node(m,d,POINTS_SCENE_BACK,"Cancel",!m->pending_effect);
    } else {
        title="Type name";points_scene_node(m,d,100,RISC_SCENE_TEXT_NODE,0,"Enter a type name","Text entry is active",0,0,true,0);
    }
    if(usable&&(!m->clock_valid||!m->service_valid||m->service_busy))
        points_scene_action_node(m,d,POINTS_SCENE_RETRY_ACTION,"Retry services",!m->pending_effect);
    snprintf(d->routes[0].title,sizeof(d->routes[0].title),"%s",title);
    m->candidate_change_revision=m->change_revision;return true;
}
/* Publish may return AGAIN/BUSY. Only the successful publication makes these
 * bindings live; input is fenced while a replacement page is pending. */
static inline bool points_scene_commit(points_scene_model *m,uint32_t revision) {
    if(!m||!revision||revision!=m->revision||revision<=m->committed_revision)return false;
    memcpy(m->bindings,m->candidate_bindings,m->candidate_count*sizeof(m->bindings[0]));
    m->binding_count=m->candidate_count;m->committed_revision=revision;
    m->dirty=m->candidate_change_revision!=m->change_revision;
    m->accepting=!m->pending_effect&&!m->dirty;return true;
}
#endif
