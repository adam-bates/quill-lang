#include <stdio.h>
#include <string.h>

#include "./arena_alloc.h"
#include "./path.h"
#include "./string_buffer.h"
#include "./utils.h"

typedef struct {
    bool is_dots;
    String str;
} Section;

static Arena arena = {0};

size_t remove_section(Section* sections, size_t sections_len, size_t idx) {
    if (idx >= sections_len) {
        return sections_len;
    }
    
    for (size_t i = idx + 1; i < sections_len; ++i) {
        sections[i - 1] = sections[i];
    }

    return sections_len - 1;
}

String normalize_path(String path) {
    // Count slashes
    size_t slashes = 0;
    bool is_dots = true;
    for (size_t i = 0; i < path.length; ++i) {
        if (path.chars[i] != '.') {
            is_dots = false;
        }

        if (path.chars[i] == '/') {
            slashes += 1;
        }
    }

    // Handle 0 slashes
    if (slashes == 0) {
        if (is_dots && path.length > 0) {
            /*    . => ./
                 .. => ../
                ... => .../
            */
            StringBuffer sb = strbuf_create_with_capacity(&arena, path.length + 1);
            strbuf_append_str(&sb, path);
            strbuf_append_char(&sb, '/');
            return strbuf_to_str(sb);
        } else {
            /*      foo => ./foo
                foo.bar => ./foo.bar
            */
            StringBuffer sb = strbuf_create_with_capacity(&arena, path.length + 2);
            strbuf_append_chars(&sb, "./");
            strbuf_append_str(&sb, path);
            return strbuf_to_str(sb);
        }
    }
    assert(slashes > 0);

    bool start_slash = path.chars[0] == '/';

    // Build sections (delimited by slashes)
    size_t sections_len = 0;
    size_t sections_cap = slashes + 1;
    Section* sections = arena_alloc(&arena, sizeof(*sections) * sections_cap);

    is_dots = true;

    StringBuffer sb = strbuf_create(&arena);
    for (size_t i = 0; i < path.length; ++i) {
        if (path.chars[i] == '/') {
            if (sb.length > 0) {
                sections[sections_len++] = (Section){
                    .is_dots = is_dots,
                    .str = strbuf_to_strcpy(sb),
                };
                strbuf_reset(&sb);
            }

            is_dots = true;
            continue;
        }

        if (is_dots && path.chars[i] != '.') {
            is_dots = false;
        }

        strbuf_append_char(&sb, path.chars[i]);
    }
    sections[sections_len++] = (Section){
        .is_dots = is_dots && sb.length > 0,
        .str = strbuf_to_strcpy(sb),
    };
    strbuf_reset(&sb);

    // printf("\n[1] \"%s\"\n", path.chars);
    // for (size_t i = 0; i < sections_len; ++i) {
    //     printf("- \"%s\"\n", arena_strcpy(&arena, sections[i].str).chars);
    // }
    // printf("\n");

    // Resolve dots
    while (true) {
        for (size_t i = 1; i < sections_len; ++i) {
            Section section = sections[i];

            if (section.is_dots) {
                bool made_changes = false;

                for (size_t r = 0; r < section.str.length; ++r) {
                    size_t to_remove = i - r;

                    if (to_remove > 0) {                        
                        // Back-track dots: ./foo/../bar => ./bar
                        sections_len = remove_section(sections, sections_len, to_remove);
                    } else {
                        if (sections[0].is_dots) {
                            // Merge dots at start of path: ../.. => .../
                            strbuf_append_str(&sb, sections[0].str);
                        } else {
                            // Merge dots at start of path: foo/.../bar => ../bar
                            sections[0].is_dots = true;
                        }

                        for (size_t d = 0; d < section.str.length - r; ++d) {
                            strbuf_append_char(&sb, '.');
                        }
                        sections[0].str = strbuf_to_strcpy(sb);
                        strbuf_reset(&sb);
                    }

                    made_changes = true;
                }

                if (made_changes) {
                    continue;
                }
            }
        }

        // no changes, all done :)
        break;
    }

    if (start_slash && sections[0].is_dots && sections[0].str.length == 1) {
        sections_len = remove_section(sections, sections_len, 0);
    }

    if (sections_len > 1 && sections[sections_len - 1].str.length == 0) {
        sections_len = remove_section(sections, sections_len, sections_len - 1);
    }

    // ./ , ../ , .../ , etc
    if (!start_slash && sections_len == 1 && sections[0].is_dots) {
        strbuf_append_str(&sb, sections[0].str);
        strbuf_append_char(&sb, '/');
        return strbuf_to_str(sb);
    }

    // printf("\n[2] \"%s\"\n", path.chars);
    // for (size_t i = 0; i < sections_len; ++i) {
    //     printf("- \"%s\"\n", arena_strcpy(&arena, sections[i].str).chars);
    // }
    // printf("\n");

    if (start_slash) {
        strbuf_append_char(&sb, '/');
    } else if (!sections[0].is_dots && sections[0].str.length > 0) {
        strbuf_append_chars(&sb, "./");
    }

    for (size_t i = 0; i < sections_len; ++i) {
        strbuf_append_str(&sb, sections[i].str);

        if (i + 1 < sections_len) {
            strbuf_append_char(&sb, '/');
        }
    }

    return strbuf_to_str(sb);
}

static bool is_normalize_expected(char const* const input, char const* const expected) {
    String actual = normalize_path(c_str(input));

    bool res =
        strlen(expected) == actual.length
        && strncmp(expected, actual.chars, actual.length) == 0;

    if (!res) {
        printf("Test Failed!\n- Exptected: \"%s\"\n- Actual \"%s\"\n\n", expected, arena_strcpy(&arena, actual).chars);
    }

    return res;
}

void test_normalize(void) {
    assert(is_normalize_expected(".", "./"));
    assert(is_normalize_expected("..", "../"));
    assert(is_normalize_expected("...", ".../"));

    assert(is_normalize_expected("./", "./"));
    assert(is_normalize_expected("../", "../"));
    assert(is_normalize_expected(".../", ".../"));

    assert(is_normalize_expected("./.", "./"));
    assert(is_normalize_expected(".//.", "./"));
    assert(is_normalize_expected("././", "./"));
    assert(is_normalize_expected("../..", ".../"));
    assert(is_normalize_expected(".../.", ".../"));

    assert(is_normalize_expected("foo", "./foo"));
    assert(is_normalize_expected("foo.ql", "./foo.ql"));
    assert(is_normalize_expected("./foo.ql", "./foo.ql"));

    assert(is_normalize_expected("/foo", "/foo"));
    assert(is_normalize_expected("/foo.ql", "/foo.ql"));
    assert(is_normalize_expected("/foo/", "/foo"));
    assert(is_normalize_expected("/foo//", "/foo"));
    assert(is_normalize_expected("//foo//", "/foo"));

    assert(is_normalize_expected("/foo/bar.ql", "/foo/bar.ql"));
    assert(is_normalize_expected("foo/bar.ql", "./foo/bar.ql"));
    assert(is_normalize_expected("./foo/bar.ql", "./foo/bar.ql"));

    assert(is_normalize_expected("/foo/../bar.ql", "/bar.ql"));
    assert(is_normalize_expected("foo/../bar.ql", "./bar.ql"));
    assert(is_normalize_expected("./foo/../bar.ql", "./bar.ql"));

    assert(is_normalize_expected("/foo/.../bar.ql", "/../bar.ql"));
    assert(is_normalize_expected("foo/.../bar.ql", "../bar.ql"));
    assert(is_normalize_expected("./foo/.../bar.ql", "../bar.ql"));
}
