#include "confparse.h"
#include "jsmn.h"
#include "utils.h"
#include "log.h"
#include "cello.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _inner_border 0
#define _inner_border_color 1
#define _outer_border 2
#define _outer_border_color 3
#define _border 4
#define _keys 5
#define _buttons 6
#define _focus_gap 7

#define uknw -1

struct token {
    char * val;
    int16_t code;
    jsmntype_t tok_type;
};

#define comp_tok(json, tok, s) (\
        tok.type == JSMN_STRING                                &&\
        (int) strlen(s) == tok.end - tok.start                 &&\
        !strncmp(                                                \
            json + tok.start, s, tok.end - tok.start             \
        )                                                        \
    )

#define copy(i) strndup(jss+t[i].start, t[i].end - t[i].start)
#define comp(tks) comp_tok(jss, t[i], tks)

#define isnum(s)  ((*s == '-' || *s == '+')  || (*s > '0' && *s < '9'))
#define isbool(s) ((*s == 't' || *s == 'f'))

static bool set_inner_border(struct token * tok){
    if (tok->tok_type == JSMN_PRIMITIVE && isnum(tok->val)){
        conf.inner_border = atoi(tok->val);
        return true;
    }
    return false;
}
static bool set_inner_border_color(struct token * tok){
    if (tok->tok_type == JSMN_STRING) { 
        conf.inner_border_color = getncolor(tok->val, strlen(tok->val));
        if (conf.inner_border_color)
            return true;
    }

    return false;
}
static bool set_outer_border(struct token * tok){
    if (tok->tok_type == JSMN_PRIMITIVE && isnum(tok->val)){
        conf.outer_border = atoi(tok->val);
        return true;
    }
    return false;
}
static bool set_outer_border_color(struct token * tok){
    if (tok->tok_type == JSMN_STRING) { 
        conf.outer_border_color = getncolor(tok->val, strlen(tok->val));
        if (conf.outer_border_color)
            return true;
    }

    return false;
}
static bool set_border(struct token * tok){
    if (tok->tok_type == JSMN_PRIMITIVE && isbool(tok->val)){
        conf.border = *tok->val == 't' ? true : false;
        return true;
    }

    return false;
}

static bool set_focus_gap(struct token * tok) {
    if (tok->tok_type == JSMN_PRIMITIVE && isnum(tok->val)){
        conf.focus_gap = atoi(tok->val);
        return true;
    }
    return false;
}

/* todo: implement */
static bool set_keys(struct token * tok __attribute((unused))){
    return false;
}
/* todo: implement */
static bool set_buttons(struct token * tok __attribute((unused))){
    return false;
}


bool (*tok_handlers[])(struct token *) = {
    #define xmacro(o) set_##o,
        #include "tokens.xmacro"
    #undef xmacro
};

struct token get_token(jsmntok_t t[], char * jss, uint32_t i) {
    struct token tok;

    tok.tok_type = t[i+1].type;
    tok.val = copy(i+1);

    if (comp("inner_border")) {
        tok.code = _inner_border;
    } else if (comp("inner_border_color")) {
        tok.code = _inner_border_color;
    } else if (comp("outer_border")) {
        tok.code = _outer_border;
    } else if (comp("outer_border_color")) {
        tok.code = _outer_border_color;
    } else if (comp("border")) {
        tok.code = _border;
    } else if (comp("focus_gap")) {
        tok.code = _focus_gap;
    } else {
        // free(tok.val);
        tok.val = copy(i);
        tok.code = uknw;
    }

    return tok;
}

void parse_json_config(void * vconfig_file) {
    /*don't know if i will definitely use JSON as config file*/
    NLOG("Reading the config file");

    FILE * fp;
    char * config;
    uint32_t config_size;

    conf.config_ok = false;

    fp = fopen((char *)vconfig_file, "r");
    // check if the file was moved or deleted after the access() check
    if (!fp) { 
        ELOG("parse_json_config Config file not found");
        return;
    }

    fseek(fp, 0, SEEK_END);
    config_size = ftell(fp);
    rewind(fp);
    
    config = umalloc(config_size * sizeof(char));

    uint32_t read = fread(config, sizeof(char), config_size, fp);
    if (read != config_size) { 
        ELOG("Could not properly read the configuration file.\nLeaving the configuration phase"); 
        return;
    }

    fclose(fp);

    //__________

    jsmn_parser p;
    jsmntok_t t[128];

    jsmn_init(&p);
    int32_t size = jsmn_parse(&p, config, config_size, t, sizeof(t)/sizeof(*t));

    if (size < 0) { 
        ELOG("Failed to parse the config: %d\n", size);
        return;
    }
    if (!size || t[0].type != JSMN_OBJECT) {
        ELOG("Invalid config file.\n");
        return; 
    }

    uint32_t i;
    for (i = 1; i < (uint32_t) size; i++) {
        struct token tok = get_token(t, config, i);

        if (tok.code < 0) {
            ELOG("Unknown option on config file: %s\n", tok.val);
            return;
        }

        if (!tok_handlers[tok.code](&tok)) {
            if (tok.val)
                ufree(tok.val);
            return;
        };
        
        if (tok.val)
            ufree(tok.val); 
        i = i+1;
    }

    NLOG("Configuration OK");
    conf.config_ok = true;
}