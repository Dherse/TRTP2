#ifndef HT_H

#define HT_H

#include "global.h"

#define INITIAL_SIZE 16
#define IP_LEN 16

typedef struct item {
    bool used;

    void *value;

    uint16_t port;
    uint8_t ip[16];
} item_t;

/**
 * /!\ READ THIS CAREFULLY IF YOU DON'T UNDERSTAND HASH-TABLES
 * 
 * ## Problem
 * 
 * After looking at the documentation, it is clear that
 * the port and IP the client connects with is the best way to
 * identify different clients. But the problem persists
 * that the port "space" ranges from 0 to 65535 (16 bits)
 * and the IP space is 128 bits long and it would be ludicrous 
 * to create a single array all the many **MANY** entries.
 * 
 * The goal is then to map a `uint16_t` from this range
 * to a much smaller range (the size of the array). This
 * can be simply accomplish with the simplest 
 * hashing method: the modulo.
 * 
 * This method simply consists as doing the modulo of the
 * port by the maximum size of the hash table. It will
 * then give a number in the range 0 to `size - 1` which
 * is the same range as the indices in an array of length
 * `size`.
 * 
 * A problem still persists that unlike in buffer.h we
 * have no way to ensure that numbers are contained
 * within a smaller range. This means that the module
 * of two different ports could collide e.g be the same.
 * This is an undesirable situation as we could overwrite
 * existing values if we're not careful.
 * 
 * ## The solution
 * 
 * The solution is to use a hash-table. 
 * It's a table that has a small number of elements and 
 * can contain values indexed using a very large range of 
 * values (in this case 16 bits). It solves the issue of 
 * collision using different methods and is automatically 
 * widened when the capacity becomes too low.
 * 
 * In our case, we have decided to use alinear probing
 * hash table.
 * 
 * ## Linear probing
 * 
 * The idea behind linear probing is simple, after computing
 * the hash of the key we're trying to insert, we try to
 * insert the value at that index in the array. If there is 
 * already a value at that index we move over by one until
 * we find an empty space.
 * 
 * And for reading, we repeat this process: we compute the
 * hash of the key we're trying to get, and we iterate in the
 * array starting at the hash until we either find an item
 * with the same key as the desired one or we find an empty
 * item which indicates that the key is not present in the
 * hash table.
 * 
 * ## Performance
 * 
 * Although this part (and the previous ones as well) is
 * discussed at length in the algorithmic course, a simple
 * reminder never hurts!
 * 
 * The time complexity of this method in big O notation is
 * on average O(1) for put, get and delete and at worse
 * of O(N). This means the function is almost always in
 * constant time which is perfect to preserve the performance
 * of our implementation.
 * 
 * The spatial complexity of this method is O(N) which is
 * also good.
 * 
 * ## Implementation problem
 * 
 * Hashing both the IP and the port makes the code a lot
 * slower and more complicated to understand. For this reason,
 * we dediced to only hash the port using a simple modulo
 * and then to simply compare the IPs until we find the right
 * values. This means the chances of collision for the keys
 * is higher but in the vast majority of situations, the performance
 * will be higher than a complex hashing algorithm.
 * 
 * ## For anybody still reading
 * 
 * It's one of the algorithm teacher's favourite for the
 * exam so don't forget to study this method !
 * 
 * ## Sources
 * 
 * - [IPv6](https://en.wikipedia.org/wiki/IPv6)
 * - [Port](https://en.wikipedia.org/wiki/Port_(computer_networking))
 * - [Hash table](https://en.wikipedia.org/wiki/Hash_table)
 * - [Hashing function](https://en.wikipedia.org/wiki/Hash_function)
 * - [Linear probing](https://en.wikipedia.org/wiki/Linear_probing)
 * 
 */
typedef struct hash_table {
    pthread_mutex_t *lock;

    /** Items contained within the hash table */
    item_t *items;

    /** Capacity of the hash table */
    size_t size;

    /** Number of elements in the hash table */
    size_t length;
} ht_t;

/**
 * ## Use :
 * 
 * Allocates a new hash table.
 * 
 * ## Arguments :
 *
 * - `table` - a pointer to an already allocated hash table
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int allocate_ht(ht_t *table);

/**
 * ## Use :
 * 
 * Deallocates an existing hash table.
 * 
 * ## Arguments :
 *
 * - `table` - a pointer to an already-allocated hash table.
 *  The hash table does not have to be fully initialized!
 *
 */
int dealloc_ht(ht_t *table);

/**
 * ## Use :
 * 
 * Computes the hash of a given port
 * 
 * ## Arguments :
 *
 * - `table` - a pointer to a hash table
 * - `port`  - the port to hash
 * - `ip`    - the ip to compare
 *
 * ## Return value:
 * 
 * An index between 0 and `table->size-1`.
 * 
 */
uint16_t ht_hash(ht_t *table, uint16_t port);

/**
 * ## Use :
 * 
 * Checks if a key is contained in the hash table
 * 
 * ## Arguments :
 *
 * - `table` - a pointer to a hash table
 * - `port`  - the port to hash
 * - `ip`    - the ip to compare
 *
 * ## Return value:
 * 
 * 1 if the value is found, 0 otherwise.
 * 
 */
bool ht_contains(ht_t* table, uint16_t port, uint8_t *ip);

/**
 * ## Use :
 * 
 * Gets a value from the hash table
 * 
 * ## Arguments :
 *
 * - `table` - a pointer to a hash table
 * - `port`  - the port to hash
 * - `ip`    - the ip to compare
 *
 * ## Return value:
 * 
 * a value if the key is in the hashtable,
 * NULL otherwise
 * 
 */
void *ht_get(ht_t *table, uint16_t port, uint8_t *ip);

/**
 * ## Use :
 * 
 * Gets a value from the hash table
 * 
 * ## Arguments :
 *
 * - `table` - a pointer to a hash table
 * - `port`  - the port to hash
 * - `ip`    - the ip to compare
 * - `item`  - the item to insert
 *
 * ## Return value:
 * 
 * NULL if no other value previously had this port, a value otherwise.
 * NULL & errono != 0 if there was an error.
 * 
 */
void *ht_put(ht_t *table, uint16_t port, uint8_t *ip, void *item);

/**
 * ## Use :
 * 
 * Removes a value from the hashtable
 * 
 * ## Arguments :
 *
 * - `table` - a pointer to a hash table
 * - `port`  - the port to hash
 * - `ip`    - the ip to compare
 *
 * ## Return value:
 * 
 * a value if the key is in the hashtable,
 * NULL otherwise
 * 
 */
void *ht_remove(ht_t *table, uint16_t port, uint8_t *ip);

/**
 * ## Use :
 * 
 * Resizes (up **and** down) the hashtable.
 * Note that sizing down without checking the size first
 * can lead to undefined behaviour.
 * 
 * ## Arguments :
 *
 * - `table`   - a pointer to a hash table
 * - `new_size - the new capacity oif the hashtable
 *
 * ## Return value:
 * 
 * 0 if it was a success, -1 otherwise.
 * 
 */
int ht_resize(ht_t *table, size_t new_size);

/**
 * ## Use :
 * 
 * Returns the number of elements in the hash table
 * 
 * ## Arguments :
 *
 * - `table`   - a pointer to a hash table
 *
 * ## Return value:
 * 
 * the number of elements in the hash table
 * 
 */
size_t ht_length(ht_t *table);

#endif