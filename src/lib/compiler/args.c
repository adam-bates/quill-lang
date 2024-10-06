#include <stdio.h>
#include <string.h>

#include "./args.h"
#include "../utils/utils.h"

typedef struct {
    bool is_path;
    Strings patterns;
    String* arg;
} ArgMatcher;

static ArgMatcher matcher_for(Arena* arena, Strings args, QuillcOption opt) {
    switch (opt) {        
        case QO_MAIN: {
            static size_t const patterns_len = 2;
            Strings patterns = { patterns_len, arena_calloc(arena, patterns_len, sizeof(Strings)) };
            patterns.strings[0] = c_str("-m");
            patterns.strings[1] = c_str("--main");
            return (ArgMatcher){
                .is_path = true,
                .patterns = patterns,
                .arg = args.strings + opt,
            };
        }

        case QO_OUTPUT: {
            static size_t const patterns_len = 2;
            Strings patterns = { patterns_len, arena_calloc(arena, patterns_len, sizeof(Strings)) };
            patterns.strings[0] = c_str("-o");
            patterns.strings[1] = c_str("--output");
            return (ArgMatcher){
                .is_path = true,
                .patterns = patterns,
                .arg = args.strings + opt,
            };
        }

        case QO_LSTD: {
            static size_t const patterns_len = 1;
            Strings patterns = { patterns_len, arena_calloc(arena, patterns_len, sizeof(Strings)) };
            patterns.strings[0] = c_str("-lstd");
            return (ArgMatcher){
                .is_path = true,
                .patterns = patterns,
                .arg = args.strings + opt,
            };
        }

        case QO_LLIBC: {
            static size_t const patterns_len = 1;
            Strings patterns = { patterns_len, arena_calloc(arena, patterns_len, sizeof(Strings)) };
            patterns.strings[0] = c_str("-llibc");
            return (ArgMatcher){
                .is_path = true,
                .patterns = patterns,
                .arg = args.strings + opt,
            };
        }

        case QO_BUILD_DIR: {
            static size_t const patterns_len = 2;
            Strings patterns = { patterns_len, arena_calloc(arena, patterns_len, sizeof(Strings)) };
            patterns.strings[0] = c_str("-D");
            patterns.strings[1] = c_str("--build-dir");
            return (ArgMatcher){
                .is_path = true,
                .patterns = patterns,
                .arg = args.strings + opt,
            };
        }

        default: assert(false);
    }
}

static bool match_arg(char* const argv[], uint8_t* pi, ArgMatcher* matcher) {
    uint8_t i = *pi;
    char* arg = argv[i];

    for (size_t p = 0; p < matcher->patterns.length; ++p) {
        String pattern = matcher->patterns.strings[p];

        if (strncmp(arg, pattern.chars, pattern.length) == 0) {
            assert(matcher->arg != NULL);
            assert(matcher->arg->length == 0);

            char* m_arg;
            if (arg[pattern.length] == '=') {
                m_arg = arg + pattern.length + 1;
            } else {
                m_arg = argv[i + 1];
                *pi += 1;
            }
            *matcher->arg = c_str(m_arg);

            if (matcher->is_path) {
                *matcher->arg = normalize_path(*matcher->arg);
            }

            assert(matcher->arg->length > 0);

            return true;
        }
    }

    return false;
}

void parse_args(Arena* arena, QuillcArgs* out, int const argc, char* const argv[]) {
    assert(argc <= ARGC_MAX);

    out->opt_args.strings = arena_calloc(arena, QO_COUNT, sizeof(String));
    out->paths_to_include.strings = arena_calloc(arena, argc, sizeof(String));

    uint8_t paths_len = 0;

    ArgMatcher arg_matchers[QO_COUNT] = {0};
    for (uint8_t i = 0; i < QO_COUNT; ++i) {
        arg_matchers[i] = matcher_for(arena, out->opt_args, i);
    }

    bool has_opts = false;
    for (uint8_t i = 1; i < argc; ++i) {
        char* arg = argv[i];

        if (*arg != '-') {
            out->paths_to_include.strings[paths_len++] = normalize_path(c_str(arg));
            continue;
        }

        for (uint8_t j = 0; j < QO_COUNT; ++j) {
            if (match_arg(argv, &i, &arg_matchers[j])) {
                has_opts = true;
                break;
            }
        }
    }

    if (out->opt_args.strings[QO_MAIN].chars) {
        out->paths_to_include.strings[paths_len++] = out->opt_args.strings[QO_MAIN];
    }
    if (out->opt_args.strings[QO_LSTD].chars) {
        out->paths_to_include.strings[paths_len++] = out->opt_args.strings[QO_LSTD];
    }
    if (out->opt_args.strings[QO_LLIBC].chars) {
        out->paths_to_include.strings[paths_len++] = out->opt_args.strings[QO_LLIBC];
    }

    out->opt_args.length = QO_COUNT;
    out->paths_to_include.length = paths_len;

    // remove duplicates
    for (size_t i = 0; i < out->paths_to_include.length - 1; ++i) {
        String const istr = out->paths_to_include.strings[i];

        for (size_t j = i + 1; j < out->paths_to_include.length; ++j) {
            String const jstr = out->paths_to_include.strings[j];

            if (
                istr.length == jstr.length
                && strncmp(istr.chars, jstr.chars, istr.length) == 0
            ) {
                strs_remove(&out->paths_to_include, j);
            }
        }
    }

    for (size_t i = 0; i < out->paths_to_include.length; ++i) {
        out->paths_to_include.strings[i] = arena_strcpy(arena, out->paths_to_include.strings[i]);
    }

    // debug print args
    {
        printf("\n");

        if (has_opts) { printf("Options:\n"); }
        for (uint8_t i = 0; i < QO_COUNT; ++i) {
            if (!out->opt_args.strings[i].chars) { continue; }

            char const* arg = out->opt_args.strings[i].chars;
            printf("- [%s] %s\n", arg_matchers[i].patterns.strings[0].chars, arg);
        }

        if (out->paths_to_include.length > 0) {
            printf("- source paths:\n");
            for (uint8_t i = 0; i < out->paths_to_include.length; ++i) {
                printf("  - \"%s\"\n", out->paths_to_include.strings[i].chars);
            }
        }

        printf("\n");
    }
}
