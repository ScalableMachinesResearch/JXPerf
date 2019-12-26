#ifndef SPLAY_TREE_H
#define SPLAY_TREE_H
#include "context.h"

struct splay_interval_s {
  struct splay_interval_s *next;
  struct splay_interval_s *prev;

  void *start;
  void *end;

  Context *node_ctxt;
};

typedef struct splay_interval_s interval_tree_node;

#define START(n)   (n)->start
#define END(n)     (n)->end
#define RIGHT(n)   (n)->next
#define LEFT(n)    (n)->prev
#define CTXT(n)    (n)->node_ctxt

#define SSTART(n)  (n).start
#define SEND(n)    (n).end
#define SRIGHT(n)  (n).next
#define SLEFT(n)   (n).prev

class SplayTree {
public:
    static interval_tree_node *interval_tree_lookup(interval_tree_node **root, void *addr, void** startaddress);
    static int interval_tree_insert(interval_tree_node **root, interval_tree_node *node);
    static void interval_tree_delete(interval_tree_node **root, interval_tree_node **del_tree, interval_tree_node *node);
    static interval_tree_node* node_make(void *start, void *end, Context *ctxt);

private:
    static interval_tree_node* interval_tree_splay(interval_tree_node *root, void *addr);
};

#endif
