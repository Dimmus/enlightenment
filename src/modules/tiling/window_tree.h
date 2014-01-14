#ifndef WINDOW_TREE_H
#define WINDOW_TREE_HO
#include <e.h>

/* XXX: Gotta be 0 and 1 because it's calculated using level % 2. */
typedef enum {
     TILING_SPLIT_HORIZONTAL = 0,
     TILING_SPLIT_VERTICAL = 1
} Tiling_Split_Type;

typedef struct _Window_Tree Window_Tree;

/* Root is level 0. Each node's split type is (level % 2). */
struct _Window_Tree
{
   EINA_INLIST;
   Window_Tree *parent;
   /* FIXME: client is falid iff children is null. Sholud enforce it. */
   Eina_Inlist *children; /* Window_Tree * type */
   E_Client *client;
   float weight;
};

void tiling_window_tree_free(Window_Tree *root);
void tiling_window_tree_walk(Window_Tree *root, void (*func)(void *));

Window_Tree *tiling_window_tree_add(Window_Tree *root, Window_Tree *parent, E_Client *client, Tiling_Split_Type split_type);

Window_Tree *tiling_window_tree_remove(Window_Tree *root, Window_Tree *item);

Window_Tree *tiling_window_tree_client_find(Window_Tree *root, E_Client *client);

void tiling_window_tree_apply(Window_Tree *root, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);

#endif
