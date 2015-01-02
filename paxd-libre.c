#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <glib.h>

#include "flags.h"

#define UNUSED __attribute__((unused))

static int inotify = -1;
static int watch_conf = -1;
static int watch_conf_dir = -1;

// int fd -> char *path (owned by path_table)
static GHashTable *watch_to_path = NULL;

// char *path -> struct dir_watch *info
static GHashTable *path_table = NULL;

// char *path -> char *exception
static GHashTable *exception_table = NULL;

struct dir_watch {
    // char *path -> NULL
    GHashTable *child_set;
    int watch;
};

static void dir_watch_destroy(void *data) {
    struct dir_watch *info = data;
    g_hash_table_destroy(info->child_set);
    g_free(info);
}

static void reinitialize_table(GHashTable **table, GHashFunc hash_fn, GEqualFunc key_equal_fn,
                               GDestroyNotify key_destroy_fn, GDestroyNotify value_destroy_fn) {
    if (*table) {
        g_hash_table_remove_all(*table);
    } else {
        *table = g_hash_table_new_full(hash_fn, key_equal_fn, key_destroy_fn, value_destroy_fn);
    }
}

static void add_dir_watch(struct dir_watch *info, char *dir) {
    info->watch = inotify_add_watch(inotify, dir, IN_ONLYDIR | IN_CREATE | IN_MOVED_TO);
    if (info->watch != -1) {
        g_hash_table_insert(watch_to_path, GINT_TO_POINTER(info->watch), dir);
    }
}

static void handler(const char *flags, size_t flags_len, const char *path) {
    if (g_hash_table_contains(exception_table, path)) {
        return; // duplicate exception
    }

    char *value = g_strndup(flags, flags_len);
    g_hash_table_insert(exception_table, g_strdup(path), value);
    set_pax_flags(flags, flags_len, path);

    char *path_segment = g_strdup(path);
    char *path_scratch = g_strdup(path);

    do {
        char *dir = dirname(path_scratch);

        struct dir_watch *info = g_hash_table_lookup(path_table, dir);
        if (info) {
            if (!g_hash_table_contains(info->child_set, path_segment)) {
                g_hash_table_insert(info->child_set, g_strdup(path_segment), NULL);
            }
        } else {
            char *dir_copy = g_strdup(dir);
            info = g_malloc(sizeof(struct dir_watch));
            g_hash_table_insert(path_table, dir_copy, info);

            info->child_set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
            g_hash_table_insert(info->child_set, g_strdup(path_segment), NULL);

            add_dir_watch(info, dir_copy);
        }

        strcpy(path_segment, dir);
        strcpy(path_scratch, path_segment);
    } while (strcmp(path_segment, "/") && strcmp(path_segment, "."));

    g_free(path_scratch);
    g_free(path_segment);
}

static void reinitialize(const char *config) {
    if (inotify != -1) {
        close(inotify);
    }

    inotify = inotify_init();
    if (inotify == -1) {
        perror("inotify");
        return;
    }

    watch_conf = inotify_add_watch(inotify, config, IN_MODIFY);

    char *tmp = g_strdup(config);
    watch_conf_dir = inotify_add_watch(inotify, dirname(tmp), IN_CREATE | IN_MOVED_TO);
    g_free(tmp);

    reinitialize_table(&watch_to_path, NULL, NULL, NULL, NULL);
    reinitialize_table(&path_table, g_str_hash, g_str_equal, g_free, dir_watch_destroy);
    reinitialize_table(&exception_table, g_str_hash, g_str_equal, g_free, g_free);

    fprintf(stderr, "loading configuration and applying all exceptions\n");
    update_attributes(config, handler);
}

static void reinitialize_watch_tree(const char *path);

static void reinitialize_watch_tree_cb(void *key, UNUSED void *value, UNUSED void *user_data) {
    reinitialize_watch_tree(key);
}

static void reinitialize_watch_tree(const char *path) {
    void *key, *value;
    if (g_hash_table_lookup_extended(path_table, path, &key, &value)) {
        struct dir_watch *info = value;
        g_hash_table_remove(watch_to_path, GINT_TO_POINTER(info->watch));
        add_dir_watch(info, key);
        g_hash_table_foreach(info->child_set, reinitialize_watch_tree_cb, NULL);
    } else {
        char *flags = g_hash_table_lookup(exception_table, path);
        if (flags) {
            fprintf(stderr, "setting `%s` on `%s`\n", flags, path);
            set_pax_flags(flags, strlen(flags), path);
        }
    }
}

static void handle_exception_event(struct inotify_event *event) {
    char *path_prefix = g_hash_table_lookup(watch_to_path, GINT_TO_POINTER(event->wd));
    if (!path_prefix) {
        return;
    }
    if (!strcmp(path_prefix, ".")) {
        reinitialize_watch_tree(event->name);
    } else {
        char *path = g_build_filename(path_prefix, event->name, NULL);
        reinitialize_watch_tree(path);
        g_free(path);
    }
}

int main(int argc, char **argv) {
    static const struct option opts[] = {
        { "apply", no_argument, 0, 'a' },
        { "user", no_argument, 0, 'u' },
        { 0, 0, 0, 0 }
    };

    bool apply = false;
    bool user = false;
    for (;;) {
        int opt = getopt_long(argc, argv, "au", opts, NULL);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'a':
            apply = true;
            break;
        case 'u':
            user = true;
            break;
        }
    }

    const char *config;
    const char *working_directory;

    if (user) {
        config = g_build_filename(g_get_user_config_dir(), "paxd-libre.conf", NULL);
        working_directory = g_get_home_dir();
    } else {
        config = "/etc/paxd-libre.conf";
        working_directory = "/";
    }

    if (chdir(working_directory) == -1) {
        perror("chdir");
        return EXIT_FAILURE;
    }

    if (apply) {
        update_attributes(config, set_pax_flags);
        return EXIT_SUCCESS;
    }

    reinitialize(config);

    alignas(struct inotify_event) char buf[(sizeof(struct inotify_event) + NAME_MAX + 1) * 64];

    for (;;) {
        ssize_t bytes_read = read(inotify, buf, sizeof buf);
        if (bytes_read == -1) {
            perror("read");
            return EXIT_FAILURE;
        }

        for (char *p = buf; p < buf + bytes_read; ) {
            struct inotify_event *event = (struct inotify_event *)p;
            p += sizeof (struct inotify_event) + event->len;

            if (event->wd == -1) {
                fprintf(stderr, "event queue overflowed\n");
                reinitialize(config);
            } else if (event->wd == watch_conf) {
                fprintf(stderr, "configuration file modified\n");
                reinitialize(config);
            } else if (event->wd == watch_conf_dir && !strcmp(event->name, "paxd-libre.conf")) {
                fprintf(stderr, "configuration file created or replaced\n");
                reinitialize(config);
            } else {
                handle_exception_event(event);
            }
        }
    }
}
