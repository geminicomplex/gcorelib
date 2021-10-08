/**
 * @file avl.h
 * @author Joe Wingbermuehle
 * @date 2007-06-11
 *
 * @brief AVL-tree implementation.
 *
 */

#ifndef AVL_H
#define AVL_H

/** AVL tree data type. */
typedef void *AVLTree;

/** Comparator for AVL tree keys.
 * @param key1 The first key.
 * @param key2 The second key.
 * @return The result of the comparison.
 *    < 0 if key1 is less than key2.
 *    = 0 if key1 equals key2.
 *    > 0 if key1 is greater than key2.
 */
typedef int (*AVLComparator)(const void *key1, const void *key2);

/** Create an empty AVL tree.
 * @param comparator The comparator to use.
 * @return An empty AVL tree.
 */
AVLTree *CreateAVL(AVLComparator comparator);

/** Destroy an AVL tree.
 * @param tree The AVL tree to destroy.
 */
void DestroyAVL(AVLTree *tree);

/** Insert an item to the AVL tree.
 * Note that if the key is already in the tree the value
 * will not be inserted.
 * @param tree The AVL tree.
 * @param key The key.
 * @param value The value.
 * @return The value currently in the tree, if any.
 */
void *InsertAVL(AVLTree *tree, const void *key, void *value);

/** Remove an item from the AVL tree.
 * @param tree The AVL tree.
 * @param key The key of the item to remove.
 * @return The removed item (NULL if not found).
 */
void *RemoveAVL(AVLTree *tree, const void *key);

/** Find an item in the AVL tree.
 * @param tree The AVL tree.
 * @param key The key.
 * @return The item (NULL if not found).
 */
void *FindAVL(const AVLTree *tree, const void *key);

/** Traverse each element of the tree.
 * @param tree The AVL tree.
 * @param func The traversal function.
 * @param data A value to be passed to the traversal function.
 */
void TraverseAVL(const AVLTree *tree,
   void (*func)(const void *key, const void *value, const void *data),
   void *data);

/** Get the number of elements in the tree.
 * @param tree The AVL tree.
 * @return The number of elements in the tree.
 */
int GetAVLSize(const AVLTree *tree);

/** Get the height of the AVL tree.
 * @param tree The AVL tree.
 * @return The height of the tree.
 */
int GetAVLHeight(const AVLTree *tree);

#endif /* AVL_H */

