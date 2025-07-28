#include "label_map.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void          free_entry(Entry *e);
static void          free_entries(Entry *e);
static unsigned long hash_function(char *s);
static Entry        *entry_init(char *id, Command *command);

bool label_map_init(LabelMap *map, int capacity) {
    map->entries = malloc(capacity * sizeof(Entry *));
    memset(map -> entries, 0, capacity * sizeof(Entry *));
    if (map != NULL) {
        map->capacity = capacity;
        return true;
    }
    return false;
}

/**
 * @brief Frees the resources for a `singular` entry.
 *
 * @param e The pointer to the entry to free.
 */
static void free_entry(Entry *e) {
    // Do not free children; see below
    free(e->id);
    free(e);
}

/**
 * @brief Frees the given list of entries.
 *
 * @param e A pointer to the first entry to free.
 */
static void free_entries(Entry *e) {
    while (e != NULL) {
        Entry *temp = e->next;
        free_entry(e);
        e = temp;
    }
}

void label_map_free(LabelMap *map) {
    // Do not free the pointer itself, as you do not know whether it was allocated on the heap
    if (map != NULL) {
        for (int i = 0; i < map->capacity; i++) {
            if (map->entries[i] != NULL) {
                free_entries(map->entries[i]);
            }
        }
        free(map->entries);
    }
}

/**
 * @brief Returns a hash of the specified id.
 *
 * @param s The string to hash.
 * @return The hash of `s`
 */
static unsigned long hash_function(char *s) {
    unsigned long i = 0;
    for (int j = 0; s[j]; j++) {
        i += s[j];
    }

    return i;
}

/**
 * @brief Initializes the given entry's state.
 *
 * @param id The id associated with this entry.
 * @param command The command associated with this entry.
 * @return True if the entry was initialized successfully, false otherwise.
 */
static Entry *entry_init(char *id, Command *command) {
    Entry *ent = malloc(sizeof(Entry));
    if (ent != NULL) {
        ent->id = malloc(sizeof(char) * (strlen(id) + 1));
        if (ent->id == NULL) {
            return NULL;
        }
        strcpy(ent->id, id);
        ent->command = command;
        ent->next    = NULL;
    }
    return ent;
}

bool put_label(LabelMap *map, char *id, Command *command) {
    // It is okay for the command to be null
    if (id == NULL || map == NULL) {
        return false;
    }
    Entry *ent = entry_init(id, command);
    if (ent == NULL) {
        return false;
    }
    Entry *currentEnt = map->entries[hash_function(id) % map->capacity];
    if (currentEnt == NULL) {
        map->entries[hash_function(id) % map->capacity] = ent;
        return true;
    }
    while (currentEnt->next != NULL) {
        if (strcmp(currentEnt->id, id) == 0) {
            free(ent);
            return false;
        }
        currentEnt = currentEnt->next;
    }
    currentEnt->next = ent;
    return true;
}

Entry *get_label(LabelMap *map, char *id) {
    unsigned long num = hash_function(id) % (map->capacity);
    Entry        *ent = map->entries[num];
    while (ent != NULL) {
        if (strcmp(ent->id, id) == 0) {
            return ent;
        }
        ent = ent->next;
    }
    return NULL;
}
