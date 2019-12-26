#include <stdio.h>
#include <stdlib.h>
#include <cinttypes>
#include "splay.h"

interval_tree_node* SplayTree::interval_tree_splay(interval_tree_node *root, void *addr) {
    interval_tree_node dummy;
    interval_tree_node *ltree_max, *rtree_min, *y;

    if (root == NULL)
    return (NULL);

    ltree_max = rtree_min = &dummy;
    for (;;) {
    /* root is never NULL in here. */
    if (addr < START(root)) {
        if ((y = LEFT(root)) == NULL)
        break;
        if (addr < START(y)) {
        /* rotate right */
        LEFT(root) = RIGHT(y);
        RIGHT(y) = root;
        root = y;
        if ((y = LEFT(root)) == NULL)
            break;
        }
        /* Link new root into right tree. */
        LEFT(rtree_min) = root;
        rtree_min = root;
    } else if (addr >= END(root)) {
        if ((y = RIGHT(root)) == NULL)
        break;
        if (addr >= END(y)) {
        /* rotate left */
        RIGHT(root) = LEFT(y);
        LEFT(y) = root;
        root = y;
        if ((y = RIGHT(root)) == NULL)
            break;
        }
        /* Link new root into left tree. */
        RIGHT(ltree_max) = root;
        ltree_max = root;
    } else
        break;
    root = y;
    }

    /* Assemble the new root. */
    RIGHT(ltree_max) = LEFT(root);
    LEFT(rtree_min) = RIGHT(root);
    LEFT(root) = SRIGHT(dummy);
    RIGHT(root) = SLEFT(dummy);

    return (root);
}

interval_tree_node* SplayTree::interval_tree_lookup(interval_tree_node **root,  /* in/out */
             void *addr, 
             void** startaddress) {
    *root = interval_tree_splay(*root, addr);
    if (*root != NULL && START(*root) <= addr && addr < END(*root)) {
        //printf("in splay.cpp, startaddress: %lx(16) %" PRIu64 "\n", (*root)->start, (*root)->start);
        *startaddress = (*root)->start;
        return (*root);
    }

    return (NULL);
}

int SplayTree::interval_tree_insert(interval_tree_node **root,  /* in/out */
             interval_tree_node *node)
{
    interval_tree_node *t;

    /* Reversed order or zero-length. */
    if (START(node) >= END(node))
    return (1);

    /* Empty tree. */
    if (*root == NULL) {
    LEFT(node) = NULL;
    RIGHT(node) = NULL;
    *root = node;
    return (0);
    }

    /* Insert left of root. */
    *root = interval_tree_splay(*root, START(node));
    if (END(node) <= START(*root)) {
    LEFT(node) = LEFT(*root);
    RIGHT(node) = *root;
    LEFT(*root) = NULL;
    *root = node;
    return (0);
    }

    /* Insert right of root. */
    t = interval_tree_splay(RIGHT(*root), START(*root));
    if (END(*root) <= START(node)
    && (t == NULL || END(node) <= START(t))) {
    LEFT(node) = *root;
    RIGHT(node) = t;
    RIGHT(*root) = NULL;
    *root = node;
    return (0);
    }

    /* Must overlap with something in the tree. */
    RIGHT(*root) = t;
    return (1);
}

void SplayTree::interval_tree_delete(interval_tree_node **root,      /* in/out */
             interval_tree_node **del_tree,  /* out */
             interval_tree_node *node)
{
    interval_tree_node *ltree, *rtree, *t;

    /* Empty tree. */
    if (*root == NULL) {
    *del_tree = NULL;
    return;
    }

    /*
     * Split the tree into three pieces: intervals entirely less than
     * start (ltree), intervals within or overlapping with [start, end)
     * (del_tree), and intervals entirely greater than end (rtree).
     */
    t = interval_tree_splay(*root, node->start);
    if (END(t) <= node->start) {
    ltree = t;
    t = RIGHT(t);
    RIGHT(ltree) = NULL;
    } else {
    ltree = LEFT(t);
    LEFT(t) = NULL;
    }
    t = interval_tree_splay(t, node->end);
    if (t == NULL) {
    *del_tree = NULL;
    rtree = NULL;
    } else if (node->end <= START(t)) {
    *del_tree = LEFT(t);
    rtree = t;
    LEFT(t) = NULL;
    } else {
    *del_tree = t;
    rtree = RIGHT(t);
    RIGHT(t) = NULL;
    }

    /* Combine the left and right pieces to make the new tree. */
    if (ltree == NULL) {
    *root = rtree;
    } else if (rtree == NULL) {
    *root = ltree;
    } else {
    *root = interval_tree_splay(ltree, node->end);
    RIGHT(*root) = rtree;
    }
}

interval_tree_node* SplayTree::node_make(void *start, void *end, Context *ctxt)
{
    interval_tree_node *node = (interval_tree_node*)malloc(sizeof(interval_tree_node));
    START(node) = start;
    END(node) = end;
    RIGHT(node) = NULL;
    LEFT(node) = NULL;
    CTXT(node) = ctxt;
    return node;
}

