#define _hashtable_h

typedef struct HashNode {
    int key;
    void* value;
    struct HashNode* next;
} HashNode;

typedef struct HashTable {
    int size;
    int count;
    HashNode* head;
} HashTable;

HashTable* InitHashTable(int size);

void PutItemInHashTable(HashTable* table, int key, void* value);

void* GetItemFromHashTable(HashTable* table, int key);

void RemoveItemFromHashTable(HashTable* table, int key);

void DestroyHashTable(HashTable* table);

int Hash(HashTable* table, int key);
