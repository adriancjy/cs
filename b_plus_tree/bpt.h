#ifndef __BPT_H__
#define __BPT_H__

//#define ORDER 4
uint32_t ORDER;

struct bpt_node 
{
    struct bpt_node *parent;
    uint32_t *keys, num_keys, is_leaf;
    void **pointers;
    struct bpt_node *next;
};

void (*repair_func)(struct bpt_node *, struct bpt_node *);

void (*append_func)(void *, void **);

struct bpt_node *bpt_to_node(void *n);

struct bpt_node *bpt_insert(struct bpt_node *root,
                            int key,
                            void *value,
                            struct bpt_node **leaf,
                            int *pos);

struct bpt_node *bpt_place_new_key_value(struct bpt_node *root,
                                         struct bpt_node **target_bpt_node,
                                         int *target_key_pos,
                                         int key,
                                         void *value);

struct bpt_node *bpt_new_node();

void bpt_print_tree(struct bpt_node *curr, int level);

void pbt_print_node(struct bpt_node *bpt_node);

int b_search(uint32_t key, uint32_t *D, uint32_t D_size);

struct bpt_node *bpt_find_leaf(struct bpt_node *curr, int key);

int bpt_find_insert_pos(struct bpt_node *leaf, int key);

struct bpt_node *bpt_split_node(struct bpt_node *root,
                                struct bpt_node *bpt_node,
                                struct bpt_node **lo_result_bpt_node,
                                struct bpt_node **hi_result_bpt_node,
                                int *lo_hi_split_point,
                                void (*repair)(struct bpt_node *,
                                               struct bpt_node *));

void *bpt_find(struct bpt_node *root, struct bpt_node **leaf, int key);

void bpt_destroy_tree(struct bpt_node **root);

#endif
