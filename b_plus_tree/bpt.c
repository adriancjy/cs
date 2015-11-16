#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include "bpt.h"

/*
 *  All bpt_nodes have an array of keys, and an array of values.
 *  In non-leaf bpt_nodes, values[i] points to the bpt_node that is either
 *  greater than or equal to key[i-1] or less than key[i].
 */


void (*repair_func)(struct bpt_node *, struct bpt_node *) = NULL;
void (*append_func)(void *, void **) = NULL;
uint32_t ORDER = 4;

//{{{ struct bpt_node *bpt_to_node(void *n)
struct bpt_node *bpt_to_node(void *n)
{
    return (struct bpt_node*)n;
}
//}}}

//{{{int b_search(int key, int *D, int D_size)
int b_search(uint32_t key, uint32_t *D, uint32_t D_size)
{
    int lo = -1, hi = D_size, mid;
    while ( hi - lo > 1) {
        mid = (hi + lo) / 2;
        if ( D[mid] < key )
            lo = mid;
        else
            hi = mid;
    }

    return hi;
}
//}}}

//{{{ struct bpt_node *bpt_new_node()
struct bpt_node *bpt_new_node()
{
    struct bpt_node *n = (struct bpt_node *)malloc(sizeof(struct bpt_node));
    n->parent = NULL;
    n->keys = (uint32_t *) 
            malloc( (ORDER+1) * sizeof(uint32_t)); //adding one help bpt_insert
    n->num_keys = 0;
    n->is_leaf = 0;
    n->pointers = (void **) malloc((ORDER+2) * sizeof(void *));
    n->next = NULL;

    return n;
}
//}}}

//{{{ struct bpt_node *bpt_find_leaf(struct bpt_node *curr, int key)
struct bpt_node *bpt_find_leaf(struct bpt_node *curr, int key)
{
    while (curr->is_leaf != 1) {
        int i = bpt_find_insert_pos(curr, key);
        if ((i < curr->num_keys) && (curr->keys[i] == key))
            i+=1;
        curr = (struct bpt_node *)curr->pointers[i];
    }
    return curr;
}
//}}}

//{{{int bpt_find_insert_pos(struct bpt_node *leaf, int key)
int bpt_find_insert_pos(struct bpt_node *leaf, int key)
{
    return b_search(key, leaf->keys, leaf->num_keys);
}
//}}}

//{{{struct bpt_node *bpt_place_new_key_value(struct bpt_node *root,
struct bpt_node *bpt_place_new_key_value(struct bpt_node *root,
                                 struct bpt_node **target_bpt_node,
                                 int *target_key_pos,
                                 int key,
                                 void *value)
{
    int bpt_insert_key_pos = bpt_find_insert_pos(*target_bpt_node, key);

    int bpt_insert_value_pos = bpt_insert_key_pos;

    if ((*target_bpt_node)->is_leaf == 0)
        bpt_insert_value_pos += 1;

    if ((*target_bpt_node)->is_leaf == 1)
        *target_key_pos = bpt_insert_key_pos;


    if (((*target_bpt_node)->is_leaf == 1) &&
         (*target_key_pos < ((*target_bpt_node)->num_keys)) &&
        ((*target_bpt_node)->keys[*target_key_pos] == key )) {

        if (append_func != NULL)
            append_func(value, &((*target_bpt_node)->pointers[*target_key_pos]));
        else
            (*target_bpt_node)->pointers[*target_key_pos] = value;

        return root;
    }

    // move everything over
    int i;

#if DEBUG
    fprintf(stderr, "bpt_place_new_key_value:\t"
                    "target_bpt_node:%p\t"
                    "target_bpt_node->num_keys:%d\t"
                    "bpt_insert_key_pos:%d\t"
                    "target_bpt_node->is_leaf:%d\n",
                    *target_bpt_node,
                    (*target_bpt_node)->num_keys,
                    bpt_insert_key_pos,
                    (*target_bpt_node)->is_leaf
                    );
#endif

    for (i = (*target_bpt_node)->num_keys; i > bpt_insert_key_pos; --i) {
#if DEBUG
        fprintf(stderr, "bpt_place_new_key_value:\ti:%d\tk:%d\n", 
                        i,
                        (*target_bpt_node)->keys[i-1]);
#endif
        (*target_bpt_node)->keys[i] = (*target_bpt_node)->keys[i-1];
    }

    if ((*target_bpt_node)->is_leaf == 1) {
        for (i = (*target_bpt_node)->num_keys; i > bpt_insert_value_pos; --i) 
            (*target_bpt_node)->pointers[i] = (*target_bpt_node)->pointers[i-1];
    } else {
        for (i = (*target_bpt_node)->num_keys+1; i > bpt_insert_value_pos; --i) {

#if DEBUG
            fprintf(stderr, "bpt_place_new_key_value:\ti:%d\tv:%p\n", 
                            i,
                            (*target_bpt_node)->pointers[i-1]);
#endif

            (*target_bpt_node)->pointers[i] = (*target_bpt_node)->pointers[i-1];
        }
    }


#if DEBUG
    fprintf(stderr, "bpt_place_new_key_value:\ti:%d\tkey:%d\n",
                    bpt_insert_key_pos,
                    key);
    fprintf(stderr, "bpt_place_new_key_value:\ti:%d\tvalue:%p\n",
                    bpt_insert_value_pos,
                    value);
#endif

    (*target_bpt_node)->keys[bpt_insert_key_pos] = key;
    (*target_bpt_node)->pointers[bpt_insert_value_pos] = value;

    (*target_bpt_node)->num_keys += 1;

    // If there are now too many values in the bpt_node, split it
    if ((*target_bpt_node)->num_keys > ORDER) {
        struct bpt_node *lo_result_bpt_node = NULL, *hi_result_bpt_node = NULL;
        int lo_hi_split_point = 0;
        struct bpt_node *r = bpt_split_node(root,
                                            *target_bpt_node,
                                            &lo_result_bpt_node,
                                            &hi_result_bpt_node,
                                            &lo_hi_split_point,
                                            repair_func);
        if ((*target_bpt_node)->is_leaf) {
            if (bpt_insert_key_pos < lo_hi_split_point)
                *target_bpt_node = lo_result_bpt_node;
            else {
                *target_bpt_node = hi_result_bpt_node;
                *target_key_pos = bpt_insert_key_pos - lo_hi_split_point;
            }
        }

        return r;
    } else {
        return root;
    }
}
//}}}

//{{{struct bpt_node *bpt_split_node(struct bpt_node *root, struct bpt_node *bpt_node)
struct bpt_node *bpt_split_node(struct bpt_node *root,
                        struct bpt_node *bpt_node,
                        struct bpt_node **lo_result_bpt_node,
                        struct bpt_node **hi_result_bpt_node,
                        int *lo_hi_split_point,
                        void (*repair)(struct bpt_node *, struct bpt_node *))
{

#if DEBUG
    fprintf(stderr, "bpt_split_node():\n");
#endif

    struct bpt_node *n = bpt_new_node();

#if DEBUG
    fprintf(stderr, "bpt_split_node():\tbpt_new_node:%p\n", n);
#endif

    // set the split location
    int split_point = (ORDER + 1)/2;

    *lo_result_bpt_node = bpt_node;
    *hi_result_bpt_node = n;
    *lo_hi_split_point = split_point;

    // copy the 2nd 1/2 of the values over to the new bpt_node
    int bpt_node_i, new_bpt_node_i = 0;
    for (bpt_node_i = split_point; bpt_node_i < bpt_node->num_keys; ++bpt_node_i) {
        n->keys[new_bpt_node_i] = bpt_node->keys[bpt_node_i];
        n->pointers[new_bpt_node_i] = bpt_node->pointers[bpt_node_i];
        n->num_keys += 1;
        new_bpt_node_i += 1;
    }

    // if the bpt_node is not a leaf, the far right pointer must be coppied too
    if (bpt_node->is_leaf == 0) {
        n->pointers[new_bpt_node_i] = bpt_node->pointers[bpt_node_i];
        n->pointers[0] = NULL;
    }

    // set status of new bpt_node
    n->is_leaf = bpt_node->is_leaf;
    n->parent = bpt_node->parent;

    bpt_node->num_keys = split_point;

    if (bpt_node->is_leaf == 0) {
#if DEBUG
        fprintf(stderr, "bpt_split_node():\tsplit non-leaf\n");
#endif
        // if the bpt_node is not a leaf, then update the parent pointer of the 
        // children
        for (bpt_node_i = 1; bpt_node_i <= n->num_keys; ++bpt_node_i) 
            ( (struct bpt_node *)n->pointers[bpt_node_i])->parent = n;
    } else {
        // if the bpt_node is a leaf, then connect the two
        n->next = bpt_node->next;
        bpt_node->next = n;

#if DEBUG
        fprintf(stderr,
                "bpt_split_node():\tsplit leaf old->next:%p new->next:%p\n",
                bpt_node->next,
                n->next);
#endif
    }

    if (repair != NULL) {
        repair(bpt_node, n);
    }

    if (bpt_node == root) {
#if DEBUG
            fprintf(stderr, "bpt_split_node():\tsplit root\tk:%d\n", n->keys[0]);
#endif

        // if we just split the root, create a new root witha single value
        struct bpt_node *new_root = bpt_new_node();
        new_root->is_leaf = 0;
        new_root->keys[0] = n->keys[0];
        new_root->pointers[0] = (void *)bpt_node; 
        new_root->pointers[1] = (void *)n; 
        new_root->num_keys += 1;
        bpt_node->parent = new_root;
        n->parent = new_root;
        return new_root;
    } else {
#if DEBUG
            fprintf(stderr, "bpt_split_node():\tsplit non-root\n");
#endif
        // if we didnt split the root, place the new value in the parent bpt_node
        return bpt_place_new_key_value(root,
                                       &(bpt_node->parent),
                                       NULL,
                                       n->keys[0],
                                       (void *)n);
    }
}
//}}}

//{{{struct bpt_node *bpt_insert(struct bpt_node *root, int key, void *value)
struct bpt_node *bpt_insert(struct bpt_node *root,
                    int key,
                    void *value,
                    struct bpt_node **leaf,
                    int *pos)
{

#if DEBUG
    fprintf(stderr, "bpt_insert():\tk:%d\n", key);
#endif

    if (root == NULL) {
        root = bpt_new_node();
        root->is_leaf = 1;
        root->keys[0] = key;
        root->pointers[0] = value;
        root->num_keys += 1;

        *leaf = root;
        *pos = 0;

        return root;
    } else {
        *leaf = bpt_find_leaf(root, key);
#if DEBUG
        fprintf(stderr, "bpt_insert():\tleaf:%p\n", *leaf);
#endif
        root = bpt_place_new_key_value(root,
                                   leaf,
                                   pos,
                                   key,
                                   value);
        return root;
    }
}
//}}}

//{{{void bpt_print_tree(struct bpt_node *curr, int level)
void bpt_print_tree(struct bpt_node *curr, int level)
{
    int i;
    for (i = 0; i < level; ++i)
        printf(" ");

    printf("keys %p(%d)(%d):", curr,curr->num_keys, curr->is_leaf);

    for (i = 0; i < curr->num_keys; ++i)
        printf(" %d", curr->keys[i]);
    printf("\n");

    for (i = 0; i < level; ++i)
        printf(" ");
    printf("values:");

    if (curr->is_leaf == 0) {
        for (i = 0; i <= curr->num_keys; ++i)
            printf(" %p", (struct bpt_node *)curr->pointers[i]);
    } else {
        for (i = 0; i < curr->num_keys; ++i) {
            int *v = (int *)curr->pointers[i];
            printf(" %d", *v);
        }
    }
    printf("\n");

    if (curr->is_leaf == 0) {
        for (i = 0; i <= curr->num_keys; ++i)
            bpt_print_tree((struct bpt_node *)curr->pointers[i], level+1);
    }
}
//}}}

//{{{ void bpt_print_node(struct bpt_node *n)
void bpt_print_node(struct bpt_node *n)
{
    int i;
    if (n->is_leaf)
        printf("+");
    else
        printf("-");
    printf("\t%p\t%p\t",n, n->parent);

    for (i = 0; i < n->num_keys; ++i) {
        if (i!=0)
            printf(" ");
        printf("%u", n->keys[i]);
    }
    printf("\n");
}
//}}}

//{{{void print_values(struct bpt_node *root)
void print_values(struct bpt_node *root)
{
    struct bpt_node *curr = root;
    while (curr->is_leaf != 1) {
        curr = (struct bpt_node *)curr->pointers[0];
    }

    printf("values: ");
    int i, *v;
    while (1) {
        for (i = 0; i < curr->num_keys; ++i) {
            v = (int *)curr->pointers[i];
            printf(" %d", *v);
        }
        if (curr->next == NULL)
            break;
        else
            curr = curr->next;
    }
    printf("\n");
}
//}}}

//{{{ void *find(struct bpt_node *root, int key) 
void *bpt_find(struct bpt_node *root, struct bpt_node **leaf, int key) 
{
    *leaf = bpt_find_leaf(root, key);
    int bpt_insert_key_pos = bpt_find_insert_pos(*leaf, key);
#if DEBUG
    fprintf(stderr,
            "key:%d pos:%d num_key:%d\n",
            key,
            bpt_insert_key_pos,
            (*leaf)->num_keys);
#endif

    if ((bpt_insert_key_pos + 1) > (*leaf)->num_keys)
        return NULL;
    else if (key != (*leaf)->keys[bpt_insert_key_pos])
        return NULL;
    else
        return (*leaf)->pointers[bpt_insert_key_pos];
}
//}}}

//{{{ void bpt_destroy_tree(struct bpt_node **curr)
void bpt_destroy_tree(struct bpt_node **curr)
{
#if 1
    if (*curr == NULL) {
        return;
    } else if ((*curr)->is_leaf == 1) {
        free((*curr)->keys);
        free((*curr)->pointers);
        free(*curr);
        *curr = NULL;
        return;
    } else {
        uint32_t i;
        for (i = 0; i <= (*curr)->num_keys; ++i) 
            bpt_destroy_tree((struct bpt_node **)&((*curr)->pointers[i]));
        free((*curr)->keys);
        free((*curr)->pointers);
        free(*curr);
        *curr = NULL;
        return;
    } 
#endif
}
//}}}

#if 0
//{{{void store(struct bpt_node *root, char *file_name)
void store(struct bpt_node *root, char *file_name)
{
    FILE *f = fopne(file_name, "wb");

    if (!f)
        err(EX_IOERR, "Could not write to '%s'.", file_name);
}
//}}}
#endif
